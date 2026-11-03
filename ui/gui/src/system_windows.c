// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// system_windows.c: the window on Windows. A popup window with a sizing frame whose non-client area is nothing, so the
// client is the whole window and the system still drags, snaps and resizes it by what the window answers to the
// hit test, the compositor rounding its corners and drawing its one-pixel border; the raster it is handed flipped
// through the graphics device or blitted into the client as a top-down 32-bit bitmap; the thread woken by a message
// posted to the window, inside the system's own drag loop too; the scale and the refresh rate read from the window's
// monitor. The records of the two windows that stand at once are this file's own
#define _WIN32_WINNT 0x0A00
#define UNICODE
#define _UNICODE
#include "../../cli/src/command_line/callback_cfg.h"
#include "system.h"

#include "../../cli/src/command_line/cli_internal.h"

#define COBJMACROS
#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <dwmapi.h>
#include <windowsx.h>

// The graphics device the frames are flipped through: whether it stands ready; the device, its queue, the allocator
// and list a copy is recorded in, the fence a copy is waited on through and its event and last value; the swapchain,
// its two buffers and their size; and the heap opened over the framebuffers' pages, those pages, and a buffer placed
// over each framebuffer with where in the pages it lies
typedef struct
{
    int ready;
    ID3D12Device3 *device;
    ID3D12CommandQueue *queue;
    ID3D12CommandAllocator *allocator;
    ID3D12GraphicsCommandList *list;
    ID3D12Fence *fence;
    HANDLE fence_event;
    unsigned long long fence_value;
    IDXGISwapChain3 *chain;
    ID3D12Resource *back[2];
    unsigned int back_width;
    unsigned int back_height;
    ID3D12Heap *heap;
    void *heap_pages;
    ID3D12Resource *positioned[2];
    unsigned long long positioned_at[2];
} SystemDevice;
struct GuiSystem
{
    int taken;
    HWND window;
    const GuiSystemEvents *events;
    void *context;
    GuiRaster displayed;
    int closed;
    unsigned short high_surrogate;
    unsigned int mods;
    unsigned int frame_ms;
    SystemDevice device;
    int device_tried;
    DWORD leave_watched;
    int owner;
};
// the message another thread posts to wake the window's thread
#define GUI_WAKE_MESSAGE (WM_APP + 1u)
// the rows one notch of the wheel scrolls
#define GUI_WHEEL_ROWS 3
// the windows that stand at once: the main one and Preferences'
#define SYSTEM_WINDOWS 2u

static GuiSystem s_systems[SYSTEM_WINDOWS];

// the boundary a framebuffer's buffer is placed on in its heap, and the bytes a copied row's pitch steps by
#define SYSTEM_PLACE (64ull << 10u)
#define SYSTEM_ROW_STEP 256u

// the COM object released where there is one, and the pointer left NULL
#define SYSTEM_RELEASED(object_, release_)                                                                             \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((object_) != NULL)                                                                                         \
        {                                                                                                              \
            release_(object_);                                                                                         \
            (object_) = NULL;                                                                                          \
        }                                                                                                              \
    } while (0)

typedef struct
{
    SystemDevice *device;
    HWND window;
    const GuiRaster *raster;
    GuiSystem *system;
    const GuiSystem *system_read;
    WPARAM key;
    int down;
    char *name;
    unsigned long long capacity;
    WPARAM unit;
    HDC context;
    unsigned int from;
    unsigned int rows;
    DWORD flags;
    unsigned int hit;
    int active;
    HWND owner;
    void *pages;
    const CliWindowPosition *place;
    const char *title;
    const GuiSystemEvents *events;
    void *context_bytess;
    VerbaLinea *err_msg;
    MSG *message;
} SystemWindowsCtx;
// the device's buffers and the swapchain's let go, the heap with them, after the queue has done every copy
EMBED_INLINE void system_surfaces_release(const SystemWindowsCtx *args)
{
    if ((args->device->queue != NULL) && (args->device->fence != NULL))
    {
        args->device->fence_value += 1ull;
        ID3D12CommandQueue_Signal(args->device->queue, args->device->fence, args->device->fence_value);
        if (ID3D12Fence_GetCompletedValue(args->device->fence) < args->device->fence_value)
        {
            ID3D12Fence_SetEventOnCompletion(args->device->fence, args->device->fence_value, args->device->fence_event);
            WaitForSingleObject(args->device->fence_event, INFINITE);
        }
    }
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        SYSTEM_RELEASED(args->device->positioned[at], ID3D12Resource_Release);
        args->device->positioned_at[at] = 0ull;
    }
    SYSTEM_RELEASED(args->device->heap, ID3D12Heap_Release);
    args->device->heap_pages = NULL;
}

// the swapchain's buffers let go
EMBED_INLINE void system_back_release(const SystemWindowsCtx *args)
{
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        SYSTEM_RELEASED(args->device->back[at], ID3D12Resource_Release);
    }
}

// the device let go whole
EMBED_INLINE void system_device_release(const SystemWindowsCtx *args)
{
    system_surfaces_release(args);
    system_back_release(args);
    SYSTEM_RELEASED(args->device->chain, IDXGISwapChain3_Release);
    SYSTEM_RELEASED(args->device->list, ID3D12GraphicsCommandList_Release);
    SYSTEM_RELEASED(args->device->allocator, ID3D12CommandAllocator_Release);
    SYSTEM_RELEASED(args->device->fence, ID3D12Fence_Release);
    SYSTEM_RELEASED(args->device->queue, ID3D12CommandQueue_Release);
    SYSTEM_RELEASED(args->device->device, ID3D12Device3_Release);
    if (args->device->fence_event != NULL)
    {
        CloseHandle(args->device->fence_event);
        args->device->fence_event = NULL;
    }
    args->device->ready = 0;
}

// the device made for `window`: its queue, its copy's allocator and list, its fence, and a flip swapchain of two
// buffers over the window; first a heap opened over `pages`, which a device that cannot read pages where they lie
// refuses before any swapchain stands over the window: 1, or 0 with nothing made
EMBED_INLINE int system_device_init(const SystemWindowsCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->device, .val = 0, .bytes = sizeof(*args->device)});
    ID3D12Device *made = NULL;
    if (FAILED(D3D12CreateDevice(NULL, D3D_FEATURE_LEVEL_11_0, &IID_ID3D12Device, (void **)&made)))
    {
        return 0;
    }
    const HRESULT requested = ID3D12Device_QueryInterface(made, &IID_ID3D12Device3, (void **)&args->device->device);
    ID3D12Device_Release(made);
    if (FAILED(requested))
    {
        return 0;
    }
    ID3D12Heap *trial_heap = NULL;
    const int pinned = SUCCEEDED(ID3D12Device3_OpenExistingHeapFromAddress(args->device->device, args->pages,
                                                                           &IID_ID3D12Heap, (void **)&trial_heap));
    SYSTEM_RELEASED(trial_heap, ID3D12Heap_Release);
    D3D12_COMMAND_QUEUE_DESC queue_desc;
    memor.set(&(MemoriaCfg){.dst = &queue_desc, .val = 0, .bytes = sizeof(queue_desc)});
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    IDXGIFactory4 *factory = NULL;
    IDXGISwapChain1 *chain = NULL;
    DXGI_SWAP_CHAIN_DESC1 chain_desc;
    memor.set(&(MemoriaCfg){.dst = &chain_desc, .val = 0, .bytes = sizeof(chain_desc)});
    chain_desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    chain_desc.SampleDesc.Count = 1u;
    chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    chain_desc.BufferCount = 2u;
    chain_desc.Scaling = DXGI_SCALING_NONE;
    chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    chain_desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    args->device->fence_event = CreateEventW(NULL, FALSE, FALSE, NULL);
    const int created_all =
        pinned && (args->device->fence_event != NULL) &&
        SUCCEEDED(ID3D12Device3_CreateCommandQueue(args->device->device, &queue_desc, &IID_ID3D12CommandQueue,
                                                   (void **)&args->device->queue)) &&
        SUCCEEDED(ID3D12Device3_CreateCommandAllocator(args->device->device, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                       &IID_ID3D12CommandAllocator,
                                                       (void **)&args->device->allocator)) &&
        SUCCEEDED(ID3D12Device3_CreateCommandList(args->device->device, 0u, D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                  args->device->allocator, NULL, &IID_ID3D12GraphicsCommandList,
                                                  (void **)&args->device->list)) &&
        SUCCEEDED(ID3D12GraphicsCommandList_Close(args->device->list)) &&
        SUCCEEDED(ID3D12Device3_CreateFence(args->device->device, 0ull, D3D12_FENCE_FLAG_NONE, &IID_ID3D12Fence,
                                            (void **)&args->device->fence)) &&
        SUCCEEDED(CreateDXGIFactory2(0u, &IID_IDXGIFactory4, (void **)&factory)) &&
        SUCCEEDED(IDXGIFactory4_CreateSwapChainForHwnd(factory, (IUnknown *)args->device->queue, args->window,
                                                       &chain_desc, NULL, NULL, &chain)) &&
        SUCCEEDED(IDXGISwapChain1_QueryInterface(chain, &IID_IDXGISwapChain3, (void **)&args->device->chain));
    if (factory != NULL)
    {
        IDXGIFactory4_MakeWindowAssociation(factory, args->window, DXGI_MWA_NO_ALT_ENTER);
        IDXGIFactory4_Release(factory);
    }
    SYSTEM_RELEASED(chain, IDXGISwapChain1_Release);
    if (!created_all)
    {
        system_device_release(args);
        return 0;
    }
    args->device->ready = 1;
    return 1;
}

// the swapchain's buffers sized to `window`'s client and taken: 1, or 0 where they could not be
EMBED_INLINE int system_back_resize(const SystemWindowsCtx *args)
{
    RECT client;
    GetClientRect(args->window, &client);
    // the client's sides are not negative and far inside what an unsigned int holds
    const unsigned int width = (unsigned int)(client.right - client.left);
    const unsigned int height = (unsigned int)(client.bottom - client.top);
    if ((width == 0u) || (height == 0u))
    {
        return 0;
    }
    if ((args->device->back[0] != NULL) && (width == args->device->back_width) && (height == args->device->back_height))
    {
        return 1;
    }
    system_surfaces_release(args);
    system_back_release(args);
    if (FAILED(IDXGISwapChain3_ResizeBuffers(args->device->chain, 2u, width, height, DXGI_FORMAT_B8G8R8A8_UNORM, 0u)))
    {
        return 0;
    }
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        if (FAILED(IDXGISwapChain3_GetBuffer(args->device->chain, at, &IID_ID3D12Resource,
                                             (void **)&args->device->back[at])))
        {
            system_back_release(args);
            return 0;
        }
    }
    args->device->back_width = width;
    args->device->back_height = height;
    return 1;
}

// the buffer placed over `raster`'s pixels in the heap over its pages, the heap opened over the pages where the one
// open stands over others: the buffer, or NULL where the device refuses it
EMBED_INLINE ID3D12Resource *system_position(const SystemWindowsCtx *args)
{
    if (args->device->heap_pages != args->raster->pages)
    {
        system_surfaces_release(args);
        if (FAILED(ID3D12Device3_OpenExistingHeapFromAddress(args->device->device, args->raster->pages, &IID_ID3D12Heap,
                                                             (void **)&args->device->heap)))
        {
            return NULL;
        }
        args->device->heap_pages = args->raster->pages;
    }
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        if ((args->device->positioned[at] != NULL) && (args->device->positioned_at[at] == args->raster->positioned))
        {
            return args->device->positioned[at];
        }
    }
    const unsigned int entry = (args->device->positioned[0] == NULL) ? 0u : 1u;
    SYSTEM_RELEASED(args->device->positioned[entry], ID3D12Resource_Release);
    D3D12_RESOURCE_DESC desc;
    memor.set(&(MemoriaCfg){.dst = &desc, .val = 0, .bytes = sizeof(desc)});
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    // the buffer reaches from the raster's place to the region's end, which has the raster at any size its capacity
    // fits: a raster sized again in the same region is copied through the buffer already placed
    desc.Width = (UINT64)(args->raster->pages_bytes - args->raster->positioned);
    desc.Height = 1u;
    desc.DepthOrArraySize = 1u;
    desc.MipLevels = 1u;
    desc.SampleDesc.Count = 1u;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
    if (FAILED(ID3D12Device3_CreatePlacedResource(args->device->device, args->device->heap, args->raster->positioned,
                                                  &desc, D3D12_RESOURCE_STATE_COMMON, NULL, &IID_ID3D12Resource,
                                                  (void **)&args->device->positioned[entry])))
    {
        return NULL;
    }
    args->device->positioned_at[entry] = args->raster->positioned;
    return args->device->positioned[entry];
}

// `raster` copied by the device from where it lies into the swapchain's buffer and flipped onto the screen, the copy
// done before this returns, the raster free to be drawn into again: 1, or 0 where the device cannot, which leaves the
// raster to the system's drawing calls. The device is made as the first raster with pages comes, on the main window
// alone
static int system_flip(const SystemWindowsCtx *args)
{
    SystemDevice *const device = &args->system->device;
    if (!device->ready && !args->system->owner && !args->system->device_tried && (args->raster->pages != NULL))
    {
        (void)EMBED_CALL(system_device_init, SystemWindowsCtx, .device = device, .window = args->system->window,
                         .pages = args->raster->pages);
        args->system->device_tried = 1;
    }
    if (!device->ready || (args->raster->pages == NULL) ||
        (((unsigned long long)args->raster->width * sizeof(GuiColor)) % SYSTEM_ROW_STEP != 0ull) ||
        !EMBED_CALL(system_back_resize, SystemWindowsCtx, .device = device, .window = args->system->window))
    {
        return 0;
    }
    ID3D12Resource *const source =
        EMBED_CALL(system_position, SystemWindowsCtx, .device = device, .raster = args->raster);
    if (source == NULL)
    {
        return 0;
    }
    const UINT index = IDXGISwapChain3_GetCurrentBackBufferIndex(device->chain);
    ID3D12Resource *const target = device->back[index];
    ID3D12CommandAllocator_Reset(device->allocator);
    ID3D12GraphicsCommandList_Reset(device->list, device->allocator, NULL);
    D3D12_RESOURCE_BARRIER barrier;
    memor.set(&(MemoriaCfg){.dst = &barrier, .val = 0, .bytes = sizeof(barrier)});
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = target;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    ID3D12GraphicsCommandList_ResourceBarrier(device->list, 1u, &barrier);
    D3D12_TEXTURE_COPY_LOCATION to;
    memor.set(&(MemoriaCfg){.dst = &to, .val = 0, .bytes = sizeof(to)});
    to.pResource = target;
    to.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    to.SubresourceIndex = 0u;
    D3D12_TEXTURE_COPY_LOCATION from;
    memor.set(&(MemoriaCfg){.dst = &from, .val = 0, .bytes = sizeof(from)});
    from.pResource = source;
    from.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    from.PlacedFootprint.Offset = 0ull;
    from.PlacedFootprint.Footprint.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    from.PlacedFootprint.Footprint.Width = args->raster->width;
    from.PlacedFootprint.Footprint.Height = args->raster->height;
    from.PlacedFootprint.Footprint.Depth = 1u;
    from.PlacedFootprint.Footprint.RowPitch = args->raster->width * (UINT)sizeof(GuiColor);
    const D3D12_BOX box = {0u,
                           0u,
                           0u,
                           (args->raster->width < device->back_width) ? args->raster->width : device->back_width,
                           (args->raster->height < device->back_height) ? args->raster->height : device->back_height,
                           1u};
    ID3D12GraphicsCommandList_CopyTextureRegion(device->list, &to, 0u, 0u, 0u, &from, &box);
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    ID3D12GraphicsCommandList_ResourceBarrier(device->list, 1u, &barrier);
    if (FAILED(ID3D12GraphicsCommandList_Close(device->list)))
    {
        return 0;
    }
    ID3D12CommandList *const lists[1] = {(ID3D12CommandList *)device->list};
    ID3D12CommandQueue_ExecuteCommandLists(device->queue, 1u, lists);
    device->fence_value += 1ull;
    ID3D12CommandQueue_Signal(device->queue, device->fence, device->fence_value);
    const HRESULT displayed = IDXGISwapChain3_Present(device->chain, 1u, 0u);
    if (ID3D12Fence_GetCompletedValue(device->fence) < device->fence_value)
    {
        ID3D12Fence_SetEventOnCompletion(device->fence, device->fence_value, device->fence_event);
        WaitForSingleObject(device->fence_event, INFINITE);
    }
    return SUCCEEDED(displayed);
}

// the milliseconds of one refresh of `window`'s monitor, whole, at least one; 1000 / 60 where the monitor does not
// say its rate
EMBED_INLINE unsigned int system_frame_ms_get(const SystemWindowsCtx *args)
{
    MONITORINFOEXW info;
    memor.set(&(MemoriaCfg){.dst = &info, .val = 0, .bytes = sizeof(info)});
    info.cbSize = sizeof(info);
    DEVMODEW mode;
    memor.set(&(MemoriaCfg){.dst = &mode, .val = 0, .bytes = sizeof(mode)});
    mode.dmSize = sizeof(mode);
    unsigned int hertz = 60u;
    if (GetMonitorInfoW(MonitorFromWindow(args->window, MONITOR_DEFAULTTONEAREST), (MONITORINFO *)&info) &&
        EnumDisplaySettingsW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode) && (mode.dmDisplayFrequency > 1u))
    {
        hertz = mode.dmDisplayFrequency;
    }
    return (1000u + hertz - 1u) / hertz;
}

// the modifier keys down, as system.h names them: read from the keys' own messages as they come, which gives a key
// message posted to the window the modifiers posted before it, and from the keyboard's state as the window takes
// the focus
EMBED_INLINE unsigned int system_mods(const SystemWindowsCtx *args)
{
    return args->system_read->mods;
}

static unsigned int system_mods_of_keyboard(void)
{
    return ((GetKeyState(VK_SHIFT) < 0) ? GUI_MOD_SHIFT : 0u) | ((GetKeyState(VK_CONTROL) < 0) ? GUI_MOD_CONTROL : 0u) |
           ((GetKeyState(VK_MENU) < 0) ? GUI_MOD_ALT : 0u);
}

// a modifier key's message applied to the modifier keys down: 1 where the key is a modifier, else 0
EMBED_INLINE int system_modifier_key(const SystemWindowsCtx *args)
{
    unsigned int bit;
    if ((args->key == VK_SHIFT) || (args->key == VK_LSHIFT) || (args->key == VK_RSHIFT))
    {
        bit = GUI_MOD_SHIFT;
    }
    else if ((args->key == VK_CONTROL) || (args->key == VK_LCONTROL) || (args->key == VK_RCONTROL))
    {
        bit = GUI_MOD_CONTROL;
    }
    else if ((args->key == VK_MENU) || (args->key == VK_LMENU) || (args->key == VK_RMENU))
    {
        bit = GUI_MOD_ALT;
    }
    else
    {
        bit = 0u;
    }
    if (bit == 0u)
    {
        return 0;
    }
    args->system->mods = args->down ? (args->system->mods | bit) : (args->system->mods & ~bit);
    return 1;
}

// a virtual key's name as the page's editor names a key, into `name`, which is sized for `capacity`: 1, or 0 for a key
// that has no name, a modifier among them
EMBED_INLINE int system_key_by_name(const SystemWindowsCtx *args)
{
    WPARAM key = args->key;
    char *name = args->name;
    static const struct
    {
        WPARAM key;
        const char *name;
    } s_named[] = {{VK_LEFT, "Left"},      {VK_RIGHT, "Right"},   {VK_UP, "Up"},         {VK_DOWN, "Down"},
                   {VK_HOME, "Home"},      {VK_END, "End"},       {VK_PRIOR, "PageUp"},  {VK_NEXT, "PageDown"},
                   {VK_BACK, "Backspace"}, {VK_DELETE, "Delete"}, {VK_RETURN, "Enter"},  {VK_TAB, "Tab"},
                   {VK_ESCAPE, "Escape"},  {VK_SPACE, "Space"},   {VK_INSERT, "Insert"}, {VK_OEM_4, "["},
                   {VK_OEM_6, "]"},        {VK_OEM_2, "/"},       {VK_OEM_5, "\\"},      {VK_OEM_PERIOD, "."},
                   {VK_OEM_COMMA, ","},    {VK_OEM_1, ";"},       {VK_OEM_7, "'"},       {VK_OEM_3, "`"},
                   {VK_OEM_MINUS, "-"},    {VK_OEM_PLUS, "="}};
    VerbaLinea line = {name, args->capacity, 0ull};
    for (unsigned int at = 0u; at < sizeof(s_named) / sizeof(s_named[0]); at += 1u)
    {
        if (s_named[at].key == key)
        {
            mmgr_line_put(&line, s_named[at].name);
            return mmgr_line_finish(&line) != 0ull;
        }
    }
    if (((key >= 'A') && (key <= 'Z')) || ((key >= '0') && (key <= '9')))
    {
        // a letter's or a digit's virtual key is its own character
        mmgr_line_ch(&line, (char)key);
        return mmgr_line_finish(&line) != 0ull;
    }
    if ((key >= VK_F1) && (key <= VK_F24))
    {
        mmgr_line_ch(&line, 'F');
        mmgr_line_decimal(&line, (unsigned long long)(key - VK_F1) + 1ull, 1u);
        return mmgr_line_finish(&line) != 0ull;
    }
    return 0;
}

// a key press passed to the window, the modifier keys down written before its name
EMBED_INLINE void system_key_press(const SystemWindowsCtx *args)
{
    char own[32];
    if (!EMBED_CALL(system_key_by_name, SystemWindowsCtx, .key = args->key, .name = own, .capacity = sizeof(own)))
    {
        return;
    }
    const unsigned int mods = EMBED_CALL(system_mods, SystemWindowsCtx, .system_read = args->system);
    char named[48];
    VerbaLinea line = {named, sizeof(named), 0ull};
    mmgr_line_put(&line, (mods & GUI_MOD_CONTROL) ? "Mod+" : "");
    mmgr_line_put(&line, (mods & GUI_MOD_ALT) ? "Alt+" : "");
    mmgr_line_put(&line, (mods & GUI_MOD_SHIFT) ? "Shift+" : "");
    mmgr_line_put(&line, own);
    (void)mmgr_line_finish(&line);
    EMBED_CALL(args->system->events->input, GuiEachInputCfg, .context = args->system->context,
               .input = GUI_KEY_INPUT(named));
}

// a character typed handed to the window as UTF-8, a surrogate pair joined across its two messages; a control
// character and one typed with the control key down are keys, not text
EMBED_INLINE void system_char_type(const SystemWindowsCtx *args)
{
    if ((args->unit < 0x20u) || (args->unit == 0x7fu) || ((args->system->mods & (GUI_MOD_CONTROL | GUI_MOD_ALT)) != 0u))
    {
        return;
    }
    unsigned int letter = (unsigned int)args->unit;
    if ((args->unit >= 0xd800u) && (args->unit <= 0xdbffu))
    {
        // the high surrogate waits for its low one
        args->system->high_surrogate = (unsigned short)args->unit;
        return;
    }
    if ((args->unit >= 0xdc00u) && (args->unit <= 0xdfffu))
    {
        if (args->system->high_surrogate == 0u)
        {
            return;
        }
        letter = 0x10000u + (((unsigned int)args->system->high_surrogate - 0xd800u) << 10u) +
                 ((unsigned int)args->unit - 0xdc00u);
        args->system->high_surrogate = 0u;
    }
    char text[8];
    VerbaLinea line = {text, sizeof(text), 0ull};
    EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &line, .letter = letter);
    EMBED_CALL(args->system->events->input, GuiEachInputCfg, .context = args->system->context,
               .input = GUI_TEXT_INPUT(text, line.at));
}

static const wchar_t s_class[] = L"orior_window";

// the scale of `window`'s monitor
EMBED_INLINE int system_scale_get(const SystemWindowsCtx *args)
{
    // a monitor's dots an inch over 96, as a percent, far inside what an int represents
    return (int)(((GetDpiForWindow(args->window) * 100u) + 48u) / 96u);
}

// the pixels of `window`'s sizing frame a side, at its monitor's scale
EMBED_INLINE int system_frame_get(const SystemWindowsCtx *args)
{
    const UINT dots = GetDpiForWindow(args->window);
    return GetSystemMetricsForDpi(SM_CXFRAME, dots) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dots);
}

// the raster shown blitted into `context`, its `rows` rows from the row `from`, as a bitmap of those rows alone
EMBED_INLINE void system_blit(const SystemWindowsCtx *args)
{
    const GuiRaster *const raster = &args->system->displayed;
    if ((raster->pixels == NULL) || (args->from >= raster->height) || (args->rows == 0u))
    {
        return;
    }
    const unsigned int count = (args->rows < raster->height - args->from) ? args->rows : raster->height - args->from;
    BITMAPINFO info;
    memor.set(&(MemoriaCfg){.dst = &info, .val = 0, .bytes = sizeof(info)});
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    // a raster's sides are far inside what a LONG represents; a negative height lays the rows top down
    info.bmiHeader.biWidth = (LONG)raster->width;
    info.bmiHeader.biHeight = -(LONG)count;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    // the row's place is far inside what an int represents
    SetDIBitsToDevice(args->context, 0, (int)args->from, raster->width, count, 0, 0, 0u, count,
                      &raster->pixels[args->from * raster->width], &info, DIB_RGB_COLORS);
}

// the system asked to say when the pointer leaves, by `flags`, the client's or the frame's, where it is not asked that
// already
EMBED_INLINE void system_leave_watch(const SystemWindowsCtx *args)
{
    if (args->system->leave_watched != args->flags)
    {
        TRACKMOUSEEVENT track = {sizeof(track), args->flags, args->window, 0u};
        args->system->leave_watched = TrackMouseEvent(&track) ? args->flags : 0u;
    }
}

// the pointer gone from the client or the frame: the window told where it stands off the window altogether, as a move
// from the client onto the frame is no leave
EMBED_INLINE void system_pointer_leave(const SystemWindowsCtx *args)
{
    POINT at;
    RECT frame;
    if (GetCursorPos(&at) && GetWindowRect(args->window, &frame) && !PtInRect(&frame, at))
    {
        EMBED_CALL(args->system->events->input, GuiEachInputCfg, .context = args->system->context,
                   .input = GUI_LEFT_INPUT());
    }
}

// the hit test's answer as the system names the places
EMBED_INLINE LRESULT system_hit_code(const SystemWindowsCtx *args)
{
    static const LRESULT s_codes[13] = {HTCLIENT, HTCAPTION, HTLEFT,     HTRIGHT,      HTTOP,
                                        HTBOTTOM, HTTOPLEFT, HTTOPRIGHT, HTBOTTOMLEFT, HTBOTTOMRIGHT,
                                        HTCLIENT, HTCLIENT,  HTCLIENT};
    return (args->hit < 13u) ? s_codes[args->hit] : HTCLIENT;
}

// the window's one-pixel border colored for `active`: the system's accent while the window is active and the user
// shows the accent on borders, as the system's own windows do, else the compositor's default for the state
EMBED_INLINE void system_border_color(const SystemWindowsCtx *args)
{
    COLORREF color = DWMWA_COLOR_DEFAULT;
    DWORD prevalence = 0u;
    DWORD bytes = sizeof(prevalence);
    if (args->active &&
        (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"ColorPrevalence", RRF_RT_REG_DWORD,
                      NULL, &prevalence, &bytes) == ERROR_SUCCESS) &&
        (prevalence != 0u))
    {
        DWORD accent = 0u;
        BOOL opaque = FALSE;
        if (DwmGetColorizationColor(&accent, &opaque) == S_OK)
        {
            // the accent comes as alpha, red, green and blue bytes from the top; a COLORREF holds red in its low byte
            color = RGB((accent >> 16u) & 0xffu, (accent >> 8u) & 0xffu, accent & 0xffu);
        }
    }
    DwmSetWindowAttribute(args->window, DWMWA_BORDER_COLOR, &color, sizeof(color));
}

static LRESULT CALLBACK system_message(HWND window, UINT message, WPARAM pixel_width, LPARAM low)
{
    GuiSystem *const system = (GuiSystem *)GetWindowLongPtrW(window, GWLP_USERDATA);
    if (system == NULL)
    {
        return DefWindowProcW(window, message, pixel_width, low);
    }
    switch (message)
    {
    case WM_NCCALCSIZE:
        if (pixel_width == 0u)
        {
            return DefWindowProcW(window, message, pixel_width, low);
        }
        // the client is the whole window; a maximized window's frame stands past the monitor's edges, which the client
        // leaves out
        if (IsZoomed(window))
        {
            NCCALCSIZE_PARAMS *const sizes = (NCCALCSIZE_PARAMS *)low;
            const int frame = EMBED_CALL(system_frame_get, SystemWindowsCtx, .window = window);
            sizes->rgrc[0].left += frame;
            sizes->rgrc[0].top += frame;
            sizes->rgrc[0].right -= frame;
            sizes->rgrc[0].bottom -= frame;
        }
        return 0;
    case WM_NCACTIVATE:
        // the window taking or losing the focus: its border colored for the state, and the system told not to paint
        // its own frame over the client, which -1 in place of the update region does
        EMBED_CALL(system_border_color, SystemWindowsCtx, .window = window, .active = pixel_width != 0u);
        return DefWindowProcW(window, message, pixel_width, (LPARAM)-1);
    case WM_NCPAINT:
        // there is no non-client area to paint
        return 0;
    case WM_NCHITTEST: {
        POINT at = {GET_X_LPARAM(low), GET_Y_LPARAM(low)};
        ScreenToClient(window, &at);
        return EMBED_CALL(system_hit_code, SystemWindowsCtx,
                          .hit = EMBED_CALL(system->events->hit, GuiEachInputCfg, .context = system->context,
                                            .input = GUI_MOVED_INPUT(at.x, at.y)));
    }
    case WM_SIZE:
        if (pixel_width == SIZE_MINIMIZED)
        {
            EMBED_CALL(system->events->displayed, GuiEachDisplayedCfg, .context = system->context, .displayed = 0);
        }
        else
        {
            EMBED_CALL(system->events->displayed, GuiEachDisplayedCfg, .context = system->context, .displayed = 1);
            // the client's sides, as the message carries them, are not negative
            EMBED_CALL(system->events->sized, GuiEachWidthHeightScaleCfg, .context = system->context,
                       .width = (unsigned int)LOWORD(low), .height = (unsigned int)HIWORD(low),
                       .scale = EMBED_CALL(system_scale_get, SystemWindowsCtx, .window = window),
                       .frame_ms = system->frame_ms);
        }
        return 0;
    case WM_SETTINGCHANGE:
        if (pixel_width == SPI_SETCLIENTAREAANIMATION)
        {
            system->events->motion(system->context);
        }
        return 0;
    case WM_GETMINMAXINFO: {
        // the least size the window is dragged to, at its monitor's scale; a size at a scale is far inside a LONG
        MINMAXINFO *const limits = (MINMAXINFO *)low;
        const int scale = EMBED_CALL(system_scale_get, SystemWindowsCtx, .window = window);
        limits->ptMinTrackSize.x = (LONG)((CLI_POSITION_MIN_WIDTH * scale) / 100);
        limits->ptMinTrackSize.y = (LONG)((CLI_POSITION_MIN_HEIGHT * scale) / 100);
        // maximized it fills the part of its monitor the taskbar leaves, its frame past those edges
        MONITORINFO info;
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info))
        {
            const int frame = EMBED_CALL(system_frame_get, SystemWindowsCtx, .window = window);
            limits->ptMaxPosition.x = (info.rcWork.left - info.rcMonitor.left) - frame;
            limits->ptMaxPosition.y = (info.rcWork.top - info.rcMonitor.top) - frame;
            limits->ptMaxSize.x = (info.rcWork.right - info.rcWork.left) + (2 * frame);
            limits->ptMaxSize.y = (info.rcWork.bottom - info.rcWork.top) + (2 * frame);
            limits->ptMaxTrackSize = limits->ptMaxSize;
        }
        return 0;
    }
    case WM_DPICHANGED: {
        const RECT *const suggested = (const RECT *)low;
        SetWindowPos(window, NULL, suggested->left, suggested->top, suggested->right - suggested->left,
                     suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
        system->frame_ms = EMBED_CALL(system_frame_ms_get, SystemWindowsCtx, .window = window);
        return 0;
    }
    case WM_DISPLAYCHANGE:
        system->frame_ms = EMBED_CALL(system_frame_ms_get, SystemWindowsCtx, .window = window);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC context = BeginPaint(window, &paint);
        if ((system->displayed.pixels != NULL) &&
            !EMBED_CALL(system_flip, SystemWindowsCtx, .system = system, .raster = &system->displayed))
        {
            EMBED_CALL(system_blit, SystemWindowsCtx, .system = system, .context = context, .from = 0u,
                       .rows = system->displayed.height);
        }
        EndPaint(window, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_NCMOUSEMOVE:
        EMBED_CALL(system_leave_watch, SystemWindowsCtx, .system = system, .window = window,
                   .flags = TME_LEAVE | TME_NONCLIENT);
        return DefWindowProcW(window, message, pixel_width, low);
    case WM_MOUSELEAVE:
    case WM_NCMOUSELEAVE:
        system->leave_watched = 0;
        EMBED_CALL(system_pointer_leave, SystemWindowsCtx, .system = system, .window = window);
        return 0;
    case WM_MOUSEMOVE:
        EMBED_CALL(system_leave_watch, SystemWindowsCtx, .system = system, .window = window, .flags = TME_LEAVE);
        EMBED_CALL(system->events->input, GuiEachInputCfg, .context = system->context,
                   .input = &(GuiInput){.type = GUI_INPUT_MOVED,
                                        .x = GET_X_LPARAM(low),
                                        .y = GET_Y_LPARAM(low),
                                        .mods = EMBED_CALL(system_mods, SystemWindowsCtx, .system_read = system)});
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(window);
        EMBED_CALL(system->events->input, GuiEachInputCfg, .context = system->context,
                   .input = GUI_DOWN_INPUT(GET_X_LPARAM(low), GET_Y_LPARAM(low),
                                           EMBED_CALL(system_mods, SystemWindowsCtx, .system_read = system)));
        return 0;
    case WM_MBUTTONDOWN:
        EMBED_CALL(system->events->input, GuiEachInputCfg, .context = system->context,
                   .input = &(GuiInput){.type = GUI_INPUT_DOWN,
                                        .x = GET_X_LPARAM(low),
                                        .y = GET_Y_LPARAM(low),
                                        .button = GUI_BUTTON_MIDDLE,
                                        .mods = EMBED_CALL(system_mods, SystemWindowsCtx, .system_read = system)});
        return 0;
    case WM_RBUTTONDOWN:
        EMBED_CALL(system->events->input, GuiEachInputCfg, .context = system->context,
                   .input = &(GuiInput){.type = GUI_INPUT_DOWN,
                                        .x = GET_X_LPARAM(low),
                                        .y = GET_Y_LPARAM(low),
                                        .button = GUI_BUTTON_RIGHT,
                                        .mods = EMBED_CALL(system_mods, SystemWindowsCtx, .system_read = system)});
        return 0;
    case WM_RBUTTONUP:
        // a browser on Windows opens a menu as the right button is let go
        EMBED_CALL(system->events->input, GuiEachInputCfg, .context = system->context,
                   .input = &(GuiInput){.type = GUI_INPUT_MENU,
                                        .x = GET_X_LPARAM(low),
                                        .y = GET_Y_LPARAM(low),
                                        .button = GUI_BUTTON_RIGHT,
                                        .mods = EMBED_CALL(system_mods, SystemWindowsCtx, .system_read = system)});
        return 0;
    case WM_LBUTTONUP:
        ReleaseCapture();
        EMBED_CALL(system->events->input, GuiEachInputCfg, .context = system->context,
                   .input = &(GuiInput){.type = GUI_INPUT_UP,
                                        .x = GET_X_LPARAM(low),
                                        .y = GET_Y_LPARAM(low),
                                        .mods = EMBED_CALL(system_mods, SystemWindowsCtx, .system_read = system)});
        return 0;
    case WM_SETFOCUS:
        system->mods = system_mods_of_keyboard();
        return 0;
    case WM_KILLFOCUS:
        system->mods = 0u;
        return 0;
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        // Alt+F4 is the system's, as a browser leaves it to the system: it closes the window as it closes any, and no
        // part of the window is handed it
        if ((message == WM_SYSKEYDOWN) && (pixel_width == VK_F4))
        {
            return DefWindowProcW(window, message, pixel_width, low);
        }
        if (!EMBED_CALL(system_modifier_key, SystemWindowsCtx, .system = system, .key = pixel_width, .down = 1))
        {
            EMBED_CALL(system_key_press, SystemWindowsCtx, .system = system, .key = pixel_width);
        }
        return 0;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        (void)EMBED_CALL(system_modifier_key, SystemWindowsCtx, .system = system, .key = pixel_width, .down = 0);
        return 0;
    case WM_CHAR:
        EMBED_CALL(system_char_type, SystemWindowsCtx, .system = system, .unit = pixel_width);
        return 0;
    case WM_MOUSEWHEEL: {
        POINT at = {GET_X_LPARAM(low), GET_Y_LPARAM(low)};
        ScreenToClient(window, &at);
        // a notch is WHEEL_DELTA, positive away from the user, which scrolls up
        const int notches = -GET_WHEEL_DELTA_WPARAM(pixel_width) / WHEEL_DELTA;
        EMBED_CALL(system->events->input, GuiEachInputCfg, .context = system->context,
                   .input = GUI_WHEEL_INPUT(at.x, at.y, notches * GUI_WHEEL_ROWS));
        return 0;
    }
    case GUI_WAKE_MESSAGE:
        system->events->woken(system->context);
        return 0;
    case WM_CLOSE:
        system->events->closing(system->context);
        return 0;
    case WM_DESTROY:
        system->closed = 1;
        // the loop is the owner's: a window owned going leaves it running
        if (!system->owner)
        {
            PostQuitMessage(0);
        }
        return 0;
    default:
        return DefWindowProcW(window, message, pixel_width, low);
    }
}

// each monitor's area put after those seen, the main one first
static BOOL CALLBACK system_monitor_visit(HMONITOR monitor, HDC context, LPRECT rect, LPARAM data)
{
    (void)context;
    (void)rect;
    const GuiSystemAreasCfg *const asked = (const GuiSystemAreasCfg *)data;
    CliWindowArea *const areas = asked->areas;
    unsigned int *const count = asked->count_out;
    MONITORINFO info;
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info) || (count[0] >= asked->most))
    {
        return TRUE;
    }
    // a monitor's area is the part the taskbar leaves: a window fitted to it has its foot in view; its sides are
    // not negative
    const CliWindowArea area = {info.rcWork.left, info.rcWork.top, (unsigned int)(info.rcWork.right - info.rcWork.left),
                                (unsigned int)(info.rcWork.bottom - info.rcWork.top)};
    if ((info.dwFlags & MONITORINFOF_PRIMARY) && (count[0] > 0u))
    {
        areas[count[0]] = areas[0];
        areas[0] = area;
    }
    else
    {
        areas[count[0]] = area;
    }
    count[0] += 1u;
    return TRUE;
}

// the monitor's whole size saved in the two words handed on, where it has more pixels than those seen
static BOOL CALLBACK system_monitor_measure(HMONITOR monitor, HDC context, LPRECT rect, LPARAM data)
{
    (void)context;
    (void)rect;
    unsigned int *const most = (unsigned int *)data;
    MONITORINFO info;
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info))
    {
        // a monitor's sides are not negative
        const unsigned int width = (unsigned int)(info.rcMonitor.right - info.rcMonitor.left);
        const unsigned int height = (unsigned int)(info.rcMonitor.bottom - info.rcMonitor.top);
        if (((unsigned long long)width * height) > ((unsigned long long)most[0] * most[1]))
        {
            most[0] = width;
            most[1] = height;
        }
    }
    return TRUE;
}

void gui_system_display_max(const GuiSystemWidthCfg *args)
{
    // the system's pixels, not its scaled ones: the first the program asks of the system
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    unsigned int *width = args->width;
    unsigned int *height = args->height;
    unsigned int most[2] = {0u, 0u};
    EnumDisplayMonitors(NULL, NULL, system_monitor_measure, (LPARAM)most);
    *width = most[0];
    *height = most[1];
}

unsigned int gui_system_areas(const GuiSystemAreasCfg *args)
{
    unsigned int count = 0u;
    const GuiSystemAreasCfg counted = {args->areas, args->most, &count};
    EnumDisplayMonitors(NULL, NULL, system_monitor_visit, (LPARAM)&counted);
    return count;
}

// a window made at `place` under `owner`, none for a window of its own, its record made with it and left for the caller
// to show: the record, or NULL with why given
static GuiSystem *system_window_init(const SystemWindowsCtx *args)
{
    const GuiSystemEvents *events = args->events;
    void *context = args->context_bytess;
    GuiSystem *system = NULL;
    for (unsigned int at = 0u; (at < SYSTEM_WINDOWS) && (system == NULL); at += 1u)
    {
        system = s_systems[at].taken ? NULL : &s_systems[at];
    }
    if (system == NULL)
    {
        mmgr_line_put(args->err_msg, "the program's two windows already stand");
        return NULL;
    }
    memor.set(&(MemoriaCfg){.dst = system, .val = 0, .bytes = sizeof(*system)});
    system->taken = 1;
    system->events = events;
    system->context = context;
    WNDCLASSEXW class_of;
    memor.set(&(MemoriaCfg){.dst = &class_of, .val = 0, .bytes = sizeof(class_of)});
    class_of.cbSize = sizeof(class_of);
    class_of.style = CS_HREDRAW | CS_VREDRAW;
    class_of.lpfnWndProc = system_message;
    class_of.hInstance = GetModuleHandleW(NULL);
    class_of.hCursor = LoadCursorW(NULL, IDC_ARROW);
    class_of.lpszClassName = s_class;
    if ((RegisterClassExW(&class_of) == 0) && (GetLastError() != ERROR_CLASS_ALREADY_EXISTS))
    {
        system->taken = 0;
        mmgr_line_put(args->err_msg, "the window's class could not be registered");
        return NULL;
    }
    wchar_t width_title[CLI_NAME_MAX_LEN];
    const int wrote = MultiByteToWideChar(CP_UTF8, 0, args->title, -1, width_title, CLI_NAME_MAX_LEN);
    if (wrote <= 0)
    {
        width_title[0] = L'\0';
    }
    // the place's sides are far inside what an int represents
    system->window = CreateWindowExW(0, s_class, width_title,
                                     WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU,
                                     args->place->x, args->place->y, (int)args->place->width, (int)args->place->height,
                                     args->owner, NULL, class_of.hInstance, NULL);
    if (system->window == NULL)
    {
        system->taken = 0;
        mmgr_line_put(args->err_msg, "the window could not be made");
        return NULL;
    }
    SetWindowLongPtrW(system->window, GWLP_USERDATA, (LONG_PTR)system);
    system->frame_ms = EMBED_CALL(system_frame_ms_get, SystemWindowsCtx, .window = system->window);
    // the frame's change is taken up now the client is the whole window
    SetWindowPos(system->window, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    // the compositor rounds the window's corners and draws its one-pixel border in the system's color
    const DWM_WINDOW_CORNER_PREFERENCE corners = DWMWCP_ROUND;
    DwmSetWindowAttribute(system->window, DWMWA_WINDOW_CORNER_PREFERENCE, &corners, sizeof(corners));
    return system;
}

GuiSystem *gui_system_open(const GuiSystemPositionTitleCfg *args)
{
    GuiSystem *const system =
        EMBED_CALL(system_window_init, SystemWindowsCtx, .owner = NULL, .place = args->place, .title = args->title,
                   .events = args->events, .context_bytess = args->context, .err_msg = args->err_msg);
    if (system == NULL)
    {
        return NULL;
    }
    ShowWindow(system->window, args->place->maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
    return system;
}

GuiSystem *gui_system_owner_open(const GuiSystemPositionTitleCfg *args)
{
    GuiSystem *const system = EMBED_CALL(system_window_init, SystemWindowsCtx, .owner = args->owner->window,
                                         .place = args->place, .title = args->title, .events = args->events,
                                         .context_bytess = args->context, .err_msg = args->err_msg);
    if (system == NULL)
    {
        return NULL;
    }
    system->owner = 1;
    ShowWindow(system->window, SW_SHOW);
    return system;
}

// one message dispatched: a key pressed with control or alt down in the window it is sent to is a key, not text, and is
// not translated to a character; the character a key makes is queued behind the messages posted already, which would
// put it past the modifiers' release
EMBED_INLINE void system_message_alloc(const SystemWindowsCtx *args)
{
    const GuiSystem *const system =
        (args->message->hwnd != NULL) ? (const GuiSystem *)GetWindowLongPtrW(args->message->hwnd, GWLP_USERDATA) : NULL;
    const int keyed = (args->message->message == WM_KEYDOWN) || (args->message->message == WM_KEYUP) ||
                      (args->message->message == WM_SYSKEYDOWN) || (args->message->message == WM_SYSKEYUP);
    if (!keyed || (system == NULL) || ((system->mods & (GUI_MOD_CONTROL | GUI_MOD_ALT)) == 0u))
    {
        TranslateMessage(args->message);
    }
    DispatchMessageW(args->message);
}

void gui_system_run(const GuiSystemCfg *args)
{
    MSG message;
    while (!args->system->closed && (GetMessageW(&message, NULL, 0u, 0u) > 0))
    {
        EMBED_CALL(system_message_alloc, SystemWindowsCtx, .message = &message);
    }
}

void gui_system_pumped(const GuiSystemCfg *args)
{
    MSG message;
    while (!args->system->closed && PeekMessageW(&message, NULL, 0u, 0u, PM_REMOVE))
    {
        EMBED_CALL(system_message_alloc, SystemWindowsCtx, .message = &message);
    }
}

void *gui_system_native(const GuiSystemPositionCfg *args)
{
    return (void *)args->system_read->window;
}

void gui_system_close(const GuiSystemCfg *args)
{
    EMBED_CALL(system_device_release, SystemWindowsCtx, .device = &args->system->device);
    if (args->system->window != NULL)
    {
        DestroyWindow(args->system->window);
        args->system->window = NULL;
    }
    args->system->closed = 1;
}

void gui_system_free(const GuiSystemCfg *args)
{
    args->system->taken = 0;
}

void gui_system_present(const GuiSystemRasterCfg *args)
{
    args->system->displayed = *args->raster;
    if (!EMBED_CALL(system_flip, SystemWindowsCtx, .system = args->system, .raster = args->raster))
    {
        HDC context = GetDC(args->system->window);
        EMBED_CALL(system_blit, SystemWindowsCtx, .system = args->system, .context = context, .from = 0u,
                   .rows = args->raster->height);
        ReleaseDC(args->system->window, context);
    }
}

void gui_system_present_part(const GuiSystemRasterCfg *args)
{
    args->system->displayed = *args->raster;
    // a flip shows a whole buffer: the device flips the raster whole, and the system's drawing calls write the part
    if (EMBED_CALL(system_flip, SystemWindowsCtx, .system = args->system, .raster = args->raster))
    {
        return;
    }
    if (!EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = args->part) && (args->part.y >= 0))
    {
        // the part's rows are not negative where it is not empty
        HDC context = GetDC(args->system->window);
        EMBED_CALL(system_blit, SystemWindowsCtx, .system = args->system, .context = context,
                   .from = (unsigned int)args->part.y, .rows = (unsigned int)args->part.height);
        ReleaseDC(args->system->window, context);
    }
}

void gui_system_wake(const GuiSystemCfg *args)
{
    if (args->system->window != NULL)
    {
        PostMessageW(args->system->window, GUI_WAKE_MESSAGE, 0u, 0);
    }
}

// an icon made from `raster`'s pixels with their alpha: a 32-bit section the pixels are copied into and an empty mask
EMBED_INLINE HICON system_icon_init(const SystemWindowsCtx *args)
{
    BITMAPV5HEADER header;
    memor.set(&(MemoriaCfg){.dst = &header, .val = 0, .bytes = sizeof(header)});
    header.bV5Size = sizeof(header);
    // a raster's sides are far inside what a LONG represents; a negative height lays the rows top down
    header.bV5Width = (LONG)args->raster->width;
    header.bV5Height = -(LONG)args->raster->height;
    header.bV5Planes = 1;
    header.bV5BitCount = 32;
    header.bV5Compression = BI_BITFIELDS;
    header.bV5RedMask = 0x00ff0000u;
    header.bV5GreenMask = 0x0000ff00u;
    header.bV5BlueMask = 0x000000ffu;
    header.bV5AlphaMask = 0xff000000u;
    void *bits = NULL;
    HBITMAP color = CreateDIBSection(NULL, (const BITMAPINFO *)&header, DIB_RGB_COLORS, &bits, NULL, 0u);
    if ((color == NULL) || (bits == NULL))
    {
        return NULL;
    }
    memor.cpy(
        &(MemoriaCfg){.dst = bits,
                      .src = args->raster->pixels,
                      .bytes = (unsigned long long)args->raster->width * args->raster->height * sizeof(GuiColor)});
    HBITMAP mask = CreateBitmap((int)args->raster->width, (int)args->raster->height, 1u, 1u, NULL);
    ICONINFO info;
    info.fIcon = TRUE;
    info.xHotspot = 0u;
    info.yHotspot = 0u;
    info.hbmMask = mask;
    info.hbmColor = color;
    HICON icon = CreateIconIndirect(&info);
    DeleteObject(color);
    if (mask != NULL)
    {
        DeleteObject(mask);
    }
    return icon;
}

void gui_system_icon(const GuiSystemTitleIconCfg *args)
{
    if (args->system->window == NULL)
    {
        return;
    }
    HICON title = EMBED_CALL(system_icon_init, SystemWindowsCtx, .raster = args->title_icon);
    HICON taskbar = EMBED_CALL(system_icon_init, SystemWindowsCtx, .raster = args->taskbar_icon);
    if (title != NULL)
    {
        SendMessageW(args->system->window, WM_SETICON, ICON_SMALL, (LPARAM)title);
    }
    if (taskbar != NULL)
    {
        SendMessageW(args->system->window, WM_SETICON, ICON_BIG, (LPARAM)taskbar);
    }
}

void gui_system_act(const GuiSystemCfg *args)
{
    if (args->act == GUI_ACT_MINIMIZE)
    {
        ShowWindow(args->system->window, SW_MINIMIZE);
    }
    else if (args->act == GUI_ACT_MAXIMIZE)
    {
        ShowWindow(args->system->window, IsZoomed(args->system->window) ? SW_RESTORE : SW_MAXIMIZE);
    }
    else
    {
        PostMessageW(args->system->window, WM_CLOSE, 0u, 0);
    }
}

void gui_system_on_position(const GuiSystemPositionCfg *args)
{
    WINDOWPLACEMENT placement;
    placement.length = sizeof(placement);
    GetWindowPlacement(args->system_read->window, &placement);
    const RECT *const rect = &placement.rcNormalPosition;
    args->position_out->x = rect->left;
    args->position_out->y = rect->top;
    // a window's sides are not negative
    args->position_out->width = (unsigned int)(rect->right - rect->left);
    args->position_out->height = (unsigned int)(rect->bottom - rect->top);
    args->position_out->maximized = (placement.showCmd == SW_SHOWMAXIMIZED) ? 1 : 0;
}

int gui_system_motion_reduce(const GuiSystemPositionCfg *args)
{
    (void)args;
    BOOL animates = TRUE;
    return SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0u, &animates, 0u) && !animates;
}

int gui_system_scale(const GuiSystemPositionCfg *args)
{
    return EMBED_CALL(system_scale_get, SystemWindowsCtx, .window = args->system_read->window);
}
