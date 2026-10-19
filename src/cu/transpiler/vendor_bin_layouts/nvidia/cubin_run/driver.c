// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// driver.c: the driver opened by name with every entry point taken, and the part's fault telemetry: the management library and the system's event log opened, drained before a launch and read after it for a fault
#include "cubin_run_internal.h"

// The part's fault telemetry, the driver's management layer (NVML), reached by name and by its own entry points as the
// driver is, and nothing of its vendor's toolchain compiled against. What cubin_safe's rule 7 reads after each launch:
// the Xid critical errors the part stamps against the device
typedef struct
{
    int (*init)(void);
    int (*shutdown)(void);
    int (*device_get)(unsigned int, void **);
    int (*set_create)(void **);
    int (*register_events)(void *, unsigned long long, void *);
    int (*set_wait)(void *, void *, unsigned int);
    int (*set_free)(void *);
} CubinRunTelemetry;

typedef struct
{
    void *device;
    unsigned long long event_type;
    unsigned long long event_data;
    unsigned int gpu_instance;
    unsigned int compute_instance;
    unsigned char room[32];
} CubinRunXidEvent;

// The part's fault telemetry as the system's own log holds it, beside the management layer: every error the display
// driver, the provider nvlddmkm, writes to the System log, read through the system's event log reader, reached by name
// and by its own entry points as the driver is. The driver writes each Xid there whether its management layer
// is reached: the event's ID its Xid, and its text what the part did, as `Graphics SM Warp Exception on (GPC 1, TPC 0,
// SM 0): Out Of Range Register`. The log is subscribed to once as the carrier opens, and each launch pulls off the
// subscription what the driver wrote since: a pull with nothing written costs nothing, where a query of the log would
// cost most of a launch's time
typedef struct
{
    void *(WINAPI *subscribe)(void *, void *, const wchar_t *, const wchar_t *, void *, void *, void *, unsigned long);
    int(WINAPI *next)(void *, unsigned long, void **, unsigned long, unsigned long, unsigned long *);
    int(WINAPI *render)(void *, void *, unsigned long, unsigned long, void *, unsigned long *, unsigned long *);
    int(WINAPI *close)(void *);
} CubinRunEvents;

// the driver opened by name and every entry point taken: 1, or 0 where the driver or an entry point is not there
int cubin_run_driver(CubinRunDriver *driver)
{
#if defined(_WIN32)
    void *const library = (void *)LoadLibraryA("nvcuda.dll");
#else
    void *const library = dlopen("libcuda.so.1", RTLD_NOW);
#endif
    if (library == NULL)
    {
        return 0;
    }
    // each entry point's address is the function the driver exports under that name, which takes what its member
    // says: an object pointer is turned into a function pointer here and nowhere else
    *(void **)&driver->init = cubin_run_entry(library, "cuInit");
    *(void **)&driver->device_get = cubin_run_entry(library, "cuDeviceGet");
    *(void **)&driver->context_retain = cubin_run_entry(library, "cuDevicePrimaryCtxRetain");
    *(void **)&driver->context_set = cubin_run_entry(library, "cuCtxSetCurrent");
    *(void **)&driver->module_load = cubin_run_entry(library, "cuModuleLoadData");
    *(void **)&driver->function_get = cubin_run_entry(library, "cuModuleGetFunction");
    *(void **)&driver->function_attribute = cubin_run_entry(library, "cuFuncGetAttribute");
    *(void **)&driver->allocate = cubin_run_entry(library, "cuMemAlloc_v2");
    *(void **)&driver->copy_in = cubin_run_entry(library, "cuMemcpyHtoD_v2");
    *(void **)&driver->clear = cubin_run_entry(library, "cuMemsetD8_v2");
    *(void **)&driver->launch = cubin_run_entry(library, "cuLaunchKernel");
    *(void **)&driver->synchronize = cubin_run_entry(library, "cuCtxSynchronize");
    *(void **)&driver->copy_out = cubin_run_entry(library, "cuMemcpyDtoH_v2");
    *(void **)&driver->error_name = cubin_run_entry(library, "cuGetErrorName");
    *(void **)&driver->event_create = cubin_run_entry(library, "cuEventCreate");
    *(void **)&driver->event_record = cubin_run_entry(library, "cuEventRecord");
    *(void **)&driver->event_synchronize = cubin_run_entry(library, "cuEventSynchronize");
    *(void **)&driver->event_query = cubin_run_entry(library, "cuEventQuery");
    *(void **)&driver->event_elapsed = cubin_run_entry(library, "cuEventElapsedTime");
    *(void **)&driver->event_destroy = cubin_run_entry(library, "cuEventDestroy_v2");
    *(void **)&driver->module_unload = cubin_run_entry(library, "cuModuleUnload");
    *(void **)&driver->release = cubin_run_entry(library, "cuMemFree_v2");
    return (driver->module_unload != NULL) && (driver->release != NULL) && (driver->init != NULL) &&
           (driver->device_get != NULL) && (driver->context_retain != NULL) && (driver->context_set != NULL) &&
           (driver->module_load != NULL) && (driver->function_get != NULL) &&
           (driver->function_attribute != NULL) && (driver->allocate != NULL) &&
           (driver->copy_in != NULL) && (driver->clear != NULL) && (driver->launch != NULL) &&
           (driver->synchronize != NULL) && (driver->copy_out != NULL) && (driver->error_name != NULL) &&
           (driver->event_create != NULL) && (driver->event_record != NULL) && (driver->event_synchronize != NULL) &&
           (driver->event_query != NULL) && (driver->event_elapsed != NULL) && (driver->event_destroy != NULL);
}

static CubinRunTelemetry s_telemetry;
static void *s_telemetry_device = NULL;
static void *s_telemetry_set = NULL;
int s_telemetry_ready = 0;
static CubinRunEvents s_events;
int s_events_ready = 0;

// the subscription to the driver's errors, pulled a launch at a time: only an error written past the last pull counts
// against the launch to come
static void *s_events_subscription = NULL;

unsigned long long s_xid_data[CUBIN_RUN_XID_KEPT];
char s_xid_text[CUBIN_RUN_XID_KEPT][CUBIN_RUN_XID_TEXT];
unsigned int s_xid_count = 0u;

// an Xid and the text the driver wrote of it kept against the last launch, a `;` of the text written `,`, since the
// listener's account ends in clauses after one
static void cubin_run_xid_kept(unsigned long long xid, const char *text)
{
    if (s_xid_count < CUBIN_RUN_XID_KEPT)
    {
        s_xid_data[s_xid_count] = xid;
        snprintf(s_xid_text[s_xid_count], sizeof(s_xid_text[s_xid_count]), "%s", text);
        for (char *semicolon = strchr(s_xid_text[s_xid_count], ';'); semicolon != NULL;
             semicolon = strchr(semicolon, ';'))
        {
            *semicolon = ',';
        }
    }
    s_xid_count += 1u;
}

// `event` rendered as its XML into `xml`, which holds `room`: 1, or 0 where it could not be
static int cubin_run_event_xml(void *event, char *xml, size_t room)
{
    static wchar_t s_wide[16384];
    unsigned long used = 0ul;
    unsigned long properties = 0ul;
    if (!s_events.render(NULL, event, CUBIN_RUN_EVENTS_XML, (unsigned long)sizeof(s_wide), s_wide, &used, &properties))
    {
        return 0;
    }
    // the room is far below what an int counts
    return WideCharToMultiByte(CP_UTF8, 0u, s_wide, -1, xml, (int)room, NULL, NULL) > 0;
}

// the number `xml` holds in the tag `tag`, past its attributes, or 0 where it holds no such tag
static unsigned long long cubin_run_event_number(const char *xml, const char *tag)
{
    const char *const opened = strstr(xml, tag);
    const char *const closed = (opened != NULL) ? strchr(opened, '>') : NULL;
    return (closed != NULL) ? strtoull(closed + 1, NULL, 10) : 0ull;
}

// the text of the driver's error in `xml` into `text`, which holds `room`: its second datum, the first the device, or
// its first where it holds one alone
static void cubin_run_event_text(const char *xml, char *text, size_t room)
{
    const char *datum = strstr(xml, "<Data>");
    const char *const second = (datum != NULL) ? strstr(datum + 6, "<Data>") : NULL;
    datum = (second != NULL) ? second : datum;
    const char *const end = (datum != NULL) ? strstr(datum, "</Data>") : NULL;
    // a datum is far below what an int counts
    snprintf(text, room, "%.*s", (end != NULL) ? (int)(end - datum - 6) : 0, (datum != NULL) ? datum + 6 : "");
}

// The driver's errors written since the last pull, read off the subscription in the order written, each kept against
// the launch where `keep` (cubin_run_xid_kept): the count read
static unsigned int cubin_run_events_read(int keep)
{
    static char s_xml[32768];
    char text[CUBIN_RUN_XID_TEXT];
    void *event[16];
    unsigned long returned = 0ul;
    unsigned int read = 0u;
    while (s_events.next(s_events_subscription, 16ul, event, 0ul, 0ul, &returned) && (returned != 0ul))
    {
        for (unsigned long at = 0ul; at < returned; at += 1ul)
        {
            if (keep && cubin_run_event_xml(event[at], s_xml, sizeof(s_xml)))
            {
                cubin_run_event_text(s_xml, text, sizeof(text));
                cubin_run_xid_kept(cubin_run_event_number(s_xml, "<EventID"), text);
            }
            s_events.close(event[at]);
            read += 1u;
        }
    }
    return read;
}

// the system's log of the driver's errors subscribed to, the errors written from now on: 1 where it is ready to pull,
// 0 where its reader or an entry point of it is not there, or the system keeps no such log
static int cubin_run_events_open(void)
{
#if defined(_WIN32)
    void *const library = (void *)LoadLibraryA("wevtapi.dll");
    if (library == NULL)
    {
        return 0;
    }
    *(void **)&s_events.subscribe = cubin_run_entry(library, "EvtSubscribe");
    *(void **)&s_events.next = cubin_run_entry(library, "EvtNext");
    *(void **)&s_events.render = cubin_run_entry(library, "EvtRender");
    *(void **)&s_events.close = cubin_run_entry(library, "EvtClose");
    if ((s_events.subscribe == NULL) || (s_events.next == NULL) || (s_events.render == NULL) ||
        (s_events.close == NULL))
    {
        return 0;
    }
    // the subscription pulled, never waited on: the signal the reader sets as an error is written is only what a pull
    // without a callback requires
    void *const signal = CreateEventW(NULL, FALSE, FALSE, NULL);
    if (signal == NULL)
    {
        return 0;
    }
    s_events_subscription =
        s_events.subscribe(NULL, signal, L"System", L"*[System[Provider[@Name='nvlddmkm'] and (Level=1 or Level=2)]]",
                           NULL, NULL, NULL, CUBIN_RUN_EVENTS_FUTURE);
    if (s_events_subscription == NULL)
    {
        return 0;
    }
    s_events_ready = 1;
    return 1;
#else
    return 0;
#endif
}

// The part's management layer opened and its Xid critical errors registered on device 0, the device the carrier runs:
// 1 where it is ready to read, 0 where its library, an entry point, or the registration is not there. A part that
// reports no such error still opens, and the wait then only ever comes back empty
static int cubin_run_management_open(void)
{
#if defined(_WIN32)
    void *const library = (void *)LoadLibraryA("nvml.dll");
#else
    void *const library = dlopen("libnvidia-ml.so.1", RTLD_NOW);
#endif
    if (library == NULL)
    {
        return 0;
    }
    *(void **)&s_telemetry.init = cubin_run_entry(library, "nvmlInit_v2");
    *(void **)&s_telemetry.shutdown = cubin_run_entry(library, "nvmlShutdown");
    *(void **)&s_telemetry.device_get = cubin_run_entry(library, "nvmlDeviceGetHandleByIndex_v2");
    *(void **)&s_telemetry.set_create = cubin_run_entry(library, "nvmlEventSetCreate");
    *(void **)&s_telemetry.register_events = cubin_run_entry(library, "nvmlDeviceRegisterEvents");
    *(void **)&s_telemetry.set_wait = cubin_run_entry(library, "nvmlEventSetWait_v2");
    *(void **)&s_telemetry.set_free = cubin_run_entry(library, "nvmlEventSetFree");
    if ((s_telemetry.init == NULL) || (s_telemetry.device_get == NULL) || (s_telemetry.set_create == NULL) ||
        (s_telemetry.register_events == NULL) || (s_telemetry.set_wait == NULL))
    {
        return 0;
    }
    if ((s_telemetry.init() != 0) || (s_telemetry.device_get(0u, &s_telemetry_device) != 0) ||
        (s_telemetry.set_create(&s_telemetry_set) != 0) ||
        (s_telemetry.register_events(s_telemetry_device, CUBIN_RUN_XID, s_telemetry_set) != 0))
    {
        return 0;
    }
    s_telemetry_ready = 1;
    return 1;
}

// The part's fault telemetry opened: the management layer's Xid critical errors, and the driver's errors in the
// system's log where the system keeps one. 1 where either is ready to read, 0 with a warning where neither is
int cubin_run_telemetry_open(void)
{
    const int management = cubin_run_management_open();
    const int events = cubin_run_events_open();
    if (!management && !events)
    {
        fprintf(stderr, "the part's fault telemetry was not reached: rule 7 does not guard this run\n");
        return 0;
    }
    return 1;
}

// Every fault already stamped or written read off and thrown away: only one stamped or written after this counts
// against the launch to come. Nothing where the telemetry is not ready
void cubin_run_telemetry_drain(void)
{
    CubinRunXidEvent event;
    s_xid_count = 0u;
    while (s_telemetry_ready)
    {
        memset(&event, 0, sizeof(event));
        if (s_telemetry.set_wait(s_telemetry_set, &event, 0u) != 0)
        {
            break;
        }
    }
#if defined(_WIN32)
    if (s_events_ready)
    {
        cubin_run_events_read(0);
    }
#endif
}

// 1 where the part stamped an Xid critical error or the driver wrote an error of it to the system's log, every one
// read without waiting and kept: the launch just carried faulted the part. A launch the driver answered with `status`
// other than 0 waits for the driver's error up to CUBIN_RUN_EVENTS_WAIT_MILLISECONDS, since the driver writes it as the
// fault comes back. 0 where none is stamped or written, or the telemetry is not ready
int cubin_run_telemetry_faulted(CubinRunStatus status)
{
    const unsigned int before = s_xid_count;
    CubinRunXidEvent event;
    while (s_telemetry_ready)
    {
        memset(&event, 0, sizeof(event));
        if (s_telemetry.set_wait(s_telemetry_set, &event, 0u) != 0)
        {
            break;
        }
        if ((event.event_type & CUBIN_RUN_XID) != 0ull)
        {
            cubin_run_xid_kept(event.event_data, "");
        }
    }
#if defined(_WIN32)
    const unsigned int stamped = s_xid_count;
    for (unsigned int waited = 0u; s_events_ready; waited += 25u)
    {
        cubin_run_events_read(1);
        if ((s_xid_count != stamped) || (status == 0) || (waited >= CUBIN_RUN_EVENTS_WAIT_MILLISECONDS))
        {
            break;
        }
        Sleep(25u);
    }
#else
    (void)status;
#endif
    return s_xid_count != before;
}
