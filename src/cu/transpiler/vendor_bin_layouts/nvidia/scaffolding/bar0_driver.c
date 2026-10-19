// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// bar0_driver.c: a read-only kernel driver that maps the GPU's BAR0 register aperture and hands its 32-bit registers
// back to the userspace reader by IOCTL (mmio.h, mmio_read.c). It is the kernel half of the most direct listen the run
// channel has on the part: where NVML's Xid stamp and the System log report a fault second-hand and late, BAR0 holds
// the part's own state first, the PGRAPH exception registers an out-of-range register warp exception among them.
//
// The physical base and size of the aperture are read at load from the driver's own service key, Bar0Base and Bar0Size
// under its Parameters (install_bar0.ps1 writes them from the range Windows assigned the card). The aperture is mapped
// PAGE_READONLY | PAGE_NOCACHE: the mapping itself refuses a write, the driver unable to write the part whatever an
// IOCTL asks, matching the run channel's rule that the part is listened to and never written. Only reads are exposed,
// each bounded to the mapped aperture.
//
// The driver is a software driver, bound to no device: it maps a fixed physical range and does not attach to the PCI
// stack; it does not contend with the vendor's own driver for the part. It is loaded on demand and unloaded when its
// service stops, the aperture unmapped with it.
#include <ntddk.h>

#include "mmio.h"

// the aperture mapped at load: its physical base and size as the service key gave them, and the virtual address
// MmMapIoSpaceEx returned, NULL until it is mapped
static PDEVICE_OBJECT s_device = NULL;
static PVOID s_bar0 = NULL;
static ULONG64 s_base = 0ull;
static ULONG64 s_size = 0ull;

// the most the aperture may be, a wrong service value mapping nothing absurd: BAR0 is 16 MiB on this part, and no
// register aperture of the family is near this
#define BAR0_SIZE_MOST 0x10000000ull
#define BAR0_POOL_TAG '0RAB'

// One named value under the opened `key` read as an unsigned number into `out`, a 32-bit or a 64-bit value taken as
// written: its status, STATUS_INVALID_PARAMETER where it is neither width
static NTSTATUS bar0_param(HANDLE key, PCWSTR name, ULONG64 *out)
{
    UNICODE_STRING value_name;
    RtlInitUnicodeString(&value_name, name);
    UCHAR buffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONG64)];
    ULONG got = 0ul;
    const NTSTATUS status =
        ZwQueryValueKey(key, &value_name, KeyValuePartialInformation, buffer, sizeof(buffer), &got);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    const KEY_VALUE_PARTIAL_INFORMATION *const info = (const KEY_VALUE_PARTIAL_INFORMATION *)buffer;
    if (info->DataLength == sizeof(ULONG64))
    {
        RtlCopyMemory(out, info->Data, sizeof(ULONG64));
    }
    else if (info->DataLength == sizeof(ULONG))
    {
        ULONG narrow = 0ul;
        RtlCopyMemory(&narrow, info->Data, sizeof(ULONG));
        *out = narrow;
    }
    else
    {
        return STATUS_INVALID_PARAMETER;
    }
    return STATUS_SUCCESS;
}

// The aperture's base and size read from the service key `registry_path`, under its Parameters: its status. s_base and
// s_size are set where it succeeds
static NTSTATUS bar0_extent_read(PUNICODE_STRING registry_path)
{
    UNICODE_STRING suffix;
    RtlInitUnicodeString(&suffix, L"\\Parameters");
    UNICODE_STRING params;
    params.Length = 0u;
    // the service key, a backslash and Parameters, and a terminator past them
    params.MaximumLength = (USHORT)(registry_path->Length + suffix.Length + sizeof(WCHAR));
    params.Buffer = (PWCH)ExAllocatePool2(POOL_FLAG_PAGED, params.MaximumLength, BAR0_POOL_TAG);
    if (params.Buffer == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    RtlCopyUnicodeString(&params, registry_path);
    RtlAppendUnicodeStringToString(&params, &suffix);
    OBJECT_ATTRIBUTES attributes;
    InitializeObjectAttributes(&attributes, &params, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);
    HANDLE key = NULL;
    NTSTATUS status = ZwOpenKey(&key, KEY_READ, &attributes);
    if (NT_SUCCESS(status))
    {
        status = bar0_param(key, L"Bar0Base", &s_base);
        if (NT_SUCCESS(status))
        {
            status = bar0_param(key, L"Bar0Size", &s_size);
        }
        ZwClose(key);
    }
    ExFreePoolWithTag(params.Buffer, BAR0_POOL_TAG);
    return status;
}

// A create or a close of the device answered: the reader opens the device to read it and closes it when it is done,
// and neither carries work
static NTSTATUS bar0_open(PDEVICE_OBJECT device, PIRP irp)
{
    UNREFERENCED_PARAMETER(device);
    irp->IoStatus.Status = STATUS_SUCCESS;
    irp->IoStatus.Information = 0ull;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// One control request answered: the extent of the mapped aperture, or a read of 32-bit registers from it, bounded to
// the aperture and refused where it would reach past it. METHOD_BUFFERED: the system buffer holds the request on entry
// and the reply on exit, the request copied out before the reply overwrites it
static NTSTATUS bar0_control(PDEVICE_OBJECT device, PIRP irp)
{
    UNREFERENCED_PARAMETER(device);
    const PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(irp);
    const ULONG code = stack->Parameters.DeviceIoControl.IoControlCode;
    const ULONG in_length = stack->Parameters.DeviceIoControl.InputBufferLength;
    const ULONG out_length = stack->Parameters.DeviceIoControl.OutputBufferLength;
    PVOID const buffer = irp->AssociatedIrp.SystemBuffer;
    NTSTATUS status = STATUS_INVALID_DEVICE_REQUEST;
    ULONG_PTR information = 0ull;
    if (code == MMIO_IOCTL_EXTENT)
    {
        if (out_length < sizeof(MmioExtent))
        {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        else
        {
            MmioExtent extent;
            extent.base = s_base;
            extent.size = s_size;
            RtlCopyMemory(buffer, &extent, sizeof(extent));
            information = sizeof(extent);
            status = STATUS_SUCCESS;
        }
    }
    else if (code == MMIO_IOCTL_READ)
    {
        if ((in_length < sizeof(MmioRead)) || (buffer == NULL))
        {
            status = STATUS_BUFFER_TOO_SMALL;
        }
        else
        {
            const MmioRead request = *(const MmioRead *)buffer;
            const ULONG64 span = (ULONG64)request.words * sizeof(ULONG);
            if ((request.words == 0u) || (request.words > MMIO_READ_WORDS_MOST))
            {
                status = STATUS_INVALID_PARAMETER;
            }
            else if (out_length < span)
            {
                status = STATUS_BUFFER_TOO_SMALL;
            }
            else if ((request.offset % sizeof(ULONG)) != 0u)
            {
                status = STATUS_INVALID_PARAMETER;
            }
            else if (((ULONG64)request.offset + span) > s_size)
            {
                status = STATUS_INVALID_PARAMETER;
            }
            else if (s_bar0 == NULL)
            {
                status = STATUS_DEVICE_NOT_READY;
            }
            else
            {
                PULONG const out = (PULONG)buffer;
                PUCHAR const at = (PUCHAR)s_bar0 + request.offset;
                for (ULONG word = 0ul; word < request.words; word += 1ul)
                {
                    out[word] = READ_REGISTER_ULONG((PULONG)(at + (word * sizeof(ULONG))));
                }
                information = span;
                status = STATUS_SUCCESS;
            }
        }
    }
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = information;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

// The driver unloaded: the DOS name let go, the aperture unmapped, and the device deleted. The service stopping brings
// this, and the part is left exactly as it was, nothing of it ever written
static VOID bar0_unload(PDRIVER_OBJECT driver)
{
    UNREFERENCED_PARAMETER(driver);
    UNICODE_STRING symlink;
    RtlInitUnicodeString(&symlink, MMIO_SYMLINK_NAME);
    IoDeleteSymbolicLink(&symlink);
    if (s_bar0 != NULL)
    {
        MmUnmapIoSpace(s_bar0, (SIZE_T)s_size);
        s_bar0 = NULL;
    }
    if (s_device != NULL)
    {
        IoDeleteDevice(s_device);
        s_device = NULL;
    }
}

// The driver loaded: the aperture read from the service key and mapped read-only, the device and its DOS name made,
// and the create, close and control requests wired. Its status; where any step fails nothing is left mapped or made
NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING registry_path)
{
    NTSTATUS status = bar0_extent_read(registry_path);
    if (!NT_SUCCESS(status))
    {
        return status;
    }
    if ((s_size == 0ull) || (s_size > BAR0_SIZE_MOST))
    {
        return STATUS_INVALID_PARAMETER;
    }
    PHYSICAL_ADDRESS physical;
    physical.QuadPart = (LONGLONG)s_base;
    s_bar0 = MmMapIoSpaceEx(physical, (SIZE_T)s_size, PAGE_READONLY | PAGE_NOCACHE);
    if (s_bar0 == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    UNICODE_STRING device_name;
    RtlInitUnicodeString(&device_name, MMIO_DEVICE_NAME);
    status = IoCreateDevice(driver, 0u, &device_name, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &s_device);
    if (!NT_SUCCESS(status))
    {
        MmUnmapIoSpace(s_bar0, (SIZE_T)s_size);
        s_bar0 = NULL;
        return status;
    }
    UNICODE_STRING symlink;
    RtlInitUnicodeString(&symlink, MMIO_SYMLINK_NAME);
    status = IoCreateSymbolicLink(&symlink, &device_name);
    if (!NT_SUCCESS(status))
    {
        IoDeleteDevice(s_device);
        s_device = NULL;
        MmUnmapIoSpace(s_bar0, (SIZE_T)s_size);
        s_bar0 = NULL;
        return status;
    }
    driver->DriverUnload = bar0_unload;
    driver->MajorFunction[IRP_MJ_CREATE] = bar0_open;
    driver->MajorFunction[IRP_MJ_CLOSE] = bar0_open;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = bar0_control;
    s_device->Flags &= ~DO_DEVICE_INITIALIZING;
    return STATUS_SUCCESS;
}
