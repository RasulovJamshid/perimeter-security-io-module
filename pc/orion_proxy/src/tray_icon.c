#ifdef _WIN32

#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

#include "tray_icon.h"

/* ─── Window class name ────────────────────────────────────────────────── */

#define TRAY_WND_CLASS "OrionProxyTrayWnd"

/* ─── Forward declarations ─────────────────────────────────────────────── */

static LRESULT CALLBACK tray_wnd_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static tray_icon_t *g_tray = NULL;   /* Single global instance for WndProc */

/* ─── Open browser to dashboard ────────────────────────────────────────── */

void open_dashboard_browser(uint16_t port)
{
    char url[128];
    snprintf(url, sizeof(url), "http://localhost:%u", port);
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

/* ─── Hide console window ──────────────────────────────────────────────── */

void hide_console_window(void)
{
    HWND console = GetConsoleWindow();
    if (console) {
        ShowWindow(console, SW_HIDE);
    }
}

/* ─── Show balloon notification ────────────────────────────────────────── */

static void tray_show_balloon(tray_icon_t *tray, const char *title, const char *msg)
{
    tray->nid.uFlags |= NIF_INFO;
    strncpy(tray->nid.szInfoTitle, title, sizeof(tray->nid.szInfoTitle) - 1);
    strncpy(tray->nid.szInfo, msg, sizeof(tray->nid.szInfo) - 1);
    tray->nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconA(NIM_MODIFY, &tray->nid);
    tray->nid.uFlags &= ~NIF_INFO;
}

/* ─── Build context menu ───────────────────────────────────────────────── */

static HMENU build_popup_menu(tray_icon_t *tray)
{
    HMENU menu = CreatePopupMenu();
    char status_text[128];

    /* Status line (disabled, just for display) */
    if (tray->serial_connected) {
        snprintf(status_text, sizeof(status_text),
                 "Online: %d devices, %u packets",
                 tray->devices_online, tray->packets);
    } else {
        snprintf(status_text, sizeof(status_text),
                 "No RS-485 connection");
    }
    AppendMenuA(menu, MF_STRING | MF_GRAYED, 0, status_text);
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);

    /* Action items */
    AppendMenuA(menu, MF_STRING, TRAY_CMD_DASHBOARD, "Open Dashboard");
    AppendMenuA(menu, MF_STRING, TRAY_CMD_LOG,       "Show Console Log");
    AppendMenuA(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(menu, MF_STRING, TRAY_CMD_EXIT,      "Exit");

    /* Make "Open Dashboard" bold (default action) */
    SetMenuDefaultItem(menu, TRAY_CMD_DASHBOARD, FALSE);

    return menu;
}

/* ─── Window message handler ───────────────────────────────────────────── */

static LRESULT CALLBACK tray_wnd_proc(HWND hwnd, UINT msg,
                                       WPARAM wp, LPARAM lp)
{
    if (msg == TRAY_ICON_MSG) {
        switch (LOWORD(lp)) {
        case WM_LBUTTONDBLCLK:
            /* Double-click → open dashboard in browser */
            if (g_tray) {
                open_dashboard_browser(g_tray->http_port);
            }
            break;

        case WM_RBUTTONUP: {
            /* Right-click → show popup menu */
            POINT pt;
            HMENU menu;
            GetCursorPos(&pt);
            SetForegroundWindow(hwnd);
            menu = build_popup_menu(g_tray);
            TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN,
                           pt.x, pt.y, 0, hwnd, NULL);
            DestroyMenu(menu);
            PostMessage(hwnd, WM_NULL, 0, 0);
            break;
        }
        }
        return 0;
    }

    if (msg == WM_COMMAND) {
        switch (LOWORD(wp)) {
        case TRAY_CMD_DASHBOARD:
            if (g_tray) open_dashboard_browser(g_tray->http_port);
            break;
        case TRAY_CMD_LOG: {
            HWND console = GetConsoleWindow();
            if (console) {
                ShowWindow(console, SW_SHOW);
                SetForegroundWindow(console);
            }
            break;
        }
        case TRAY_CMD_EXIT:
            if (g_tray && g_tray->stop_flag) {
                *g_tray->stop_flag = 1;
            }
            PostQuitMessage(0);
            break;
        }
        return 0;
    }

    if (msg == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(hwnd, msg, wp, lp);
}

/* ─── Message loop thread ──────────────────────────────────────────────── */

static DWORD WINAPI tray_thread_func(LPVOID param)
{
    tray_icon_t *tray = (tray_icon_t *)param;
    MSG msg;
    WNDCLASSA wc;

    /* Register window class */
    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = tray_wnd_proc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = TRAY_WND_CLASS;
    RegisterClassA(&wc);

    /* Create hidden message-only window */
    tray->hwnd = CreateWindowExA(0, TRAY_WND_CLASS, "Orion Proxy",
                                  0, 0, 0, 0, 0,
                                  HWND_MESSAGE, NULL,
                                  GetModuleHandle(NULL), NULL);
    if (!tray->hwnd) {
        printf("[TRAY] Failed to create message window\n");
        return 1;
    }

    /* Set up notification icon data */
    memset(&tray->nid, 0, sizeof(tray->nid));
    tray->nid.cbSize = sizeof(NOTIFYICONDATAA);
    tray->nid.hWnd = tray->hwnd;
    tray->nid.uID = 1;
    tray->nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    tray->nid.uCallbackMessage = TRAY_ICON_MSG;
    tray->nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    strncpy(tray->nid.szTip, "Orion Proxy - Starting...",
            sizeof(tray->nid.szTip) - 1);

    Shell_NotifyIconA(NIM_ADD, &tray->nid);

    /* Show startup balloon */
    tray_show_balloon(tray, "Orion Proxy",
                      "Service is running.\n"
                      "Double-click to open dashboard.\n"
                      "Right-click for options.");

    /* Message loop */
    while (GetMessage(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        /* Check if main app wants to exit */
        if (tray->stop_flag && *tray->stop_flag) {
            break;
        }
    }

    /* Remove icon */
    Shell_NotifyIconA(NIM_DELETE, &tray->nid);
    DestroyWindow(tray->hwnd);
    UnregisterClassA(TRAY_WND_CLASS, GetModuleHandle(NULL));

    return 0;
}

/* ─── Public API ───────────────────────────────────────────────────────── */

int tray_icon_init(tray_icon_t *tray, volatile int *stop_flag, uint16_t http_port)
{
    memset(tray, 0, sizeof(*tray));
    tray->stop_flag = stop_flag;
    tray->http_port = http_port;
    tray->serial_connected = 0;
    tray->devices_online = 0;
    tray->packets = 0;

    g_tray = tray;

    tray->thread = CreateThread(NULL, 0, tray_thread_func, tray, 0, NULL);
    if (!tray->thread) {
        printf("[TRAY] Failed to create tray icon thread\n");
        return -1;
    }

    return 0;
}

void tray_icon_update(tray_icon_t *tray, int serial_ok, int devices, uint32_t packets)
{
    if (!tray || !tray->hwnd) return;

    tray->serial_connected = serial_ok;
    tray->devices_online = devices;
    tray->packets = packets;

    if (serial_ok) {
        snprintf(tray->nid.szTip, sizeof(tray->nid.szTip),
                 "Orion Proxy - %d devices, %u pkts",
                 devices, packets);
        tray->nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    } else {
        snprintf(tray->nid.szTip, sizeof(tray->nid.szTip),
                 "Orion Proxy - No RS-485 connection");
        tray->nid.hIcon = LoadIcon(NULL, IDI_WARNING);
    }

    tray->nid.uFlags = NIF_ICON | NIF_TIP;
    Shell_NotifyIconA(NIM_MODIFY, &tray->nid);
}

void tray_icon_destroy(tray_icon_t *tray)
{
    if (!tray) return;

    if (tray->hwnd) {
        PostMessage(tray->hwnd, WM_CLOSE, 0, 0);
    }

    if (tray->thread) {
        WaitForSingleObject(tray->thread, 3000);
        CloseHandle(tray->thread);
        tray->thread = NULL;
    }

    g_tray = NULL;
}

#endif /* _WIN32 */
