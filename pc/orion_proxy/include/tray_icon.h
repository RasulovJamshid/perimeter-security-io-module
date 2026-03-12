#ifndef TRAY_ICON_H
#define TRAY_ICON_H

#ifdef _WIN32

#include <windows.h>
#include <stdint.h>

#define TRAY_ICON_MSG   (WM_USER + 100)

#define TRAY_CMD_DASHBOARD  1001
#define TRAY_CMD_STATUS     1002
#define TRAY_CMD_START_STOP 1003
#define TRAY_CMD_CONFIG     1004
#define TRAY_CMD_LOG        1005
#define TRAY_CMD_EXIT       1006

typedef struct {
    HWND            hwnd;
    NOTIFYICONDATAA nid;
    HMENU           popup_menu;
    HANDLE          thread;
    volatile int   *stop_flag;
    uint16_t        http_port;
    int             serial_connected;
    int             devices_online;
    uint32_t        packets;
} tray_icon_t;

/**
 * Initialize and show the system tray icon.
 * Starts a background thread for the Win32 message loop.
 */
int tray_icon_init(tray_icon_t *tray, volatile int *stop_flag, uint16_t http_port);

/**
 * Update tray tooltip with current status.
 */
void tray_icon_update(tray_icon_t *tray, int serial_ok, int devices, uint32_t packets);

/**
 * Remove tray icon and clean up.
 */
void tray_icon_destroy(tray_icon_t *tray);

/**
 * Hide the console window (for --tray mode).
 */
void hide_console_window(void);

/**
 * Open the default browser to the dashboard URL.
 */
void open_dashboard_browser(uint16_t port);

#endif /* _WIN32 */
#endif /* TRAY_ICON_H */
