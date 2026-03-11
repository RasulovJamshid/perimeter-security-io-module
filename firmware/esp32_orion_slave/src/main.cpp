#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include "orion_bus_monitor.h"
#include "orion_master.h"

/* ═══════════════════════════════════════════════════════════════════════
 *  CONFIGURATION — Edit these values for your setup
 * ═══════════════════════════════════════════════════════════════════════ */

/* ─── WiFi Access Point (On-Demand) ────────────────────────────────────
 * WiFi is OFF by default for maximum performance.
 * Press debug button → ESP32 creates its own WiFi network (AP mode)
 * Connect to the AP → access HTTP configuration interface
 * WiFi stays on until reboot.
 */
#define WIFI_AP_SSID      "Orion-Gateway"   /* AP network name */
#define WIFI_AP_PASSWORD  "orion12345"      /* AP password (min 8 chars) */
#define WIFI_AP_CHANNEL   6                 /* WiFi channel (1-13) */
#define HTTP_PORT         80                /* HTTP server port */

/* ═══════════════════════════════════════════════════════════════════════
 *  HARDWARE: ESP32-C3 Super Mini
 *  - Built-in USB-C for programming
 *  - 2 UARTs: UART0 (GPIO20/21), UART1 (GPIO6/7)
 *  - Lower power: 43mA idle vs 80mA (ESP32-WROOM-32)
 *  - Smaller size: 22.52x18mm
 * ═══════════════════════════════════════════════════════════════════════ */

/* Orion bus - configurable parameters (saved to NVS) */
#define ORION_GLOBAL_KEY  0x00   /* Default encryption key */

/* Protocol timing (saved to NVS) */
static uint16_t packet_timeout_ms = 5;      /* Inter-byte timeout for packet framing */
static uint16_t response_gap_ms = 50;       /* Gap to distinguish request from response */
static uint16_t device_timeout_ms = 10000;  /* Device offline timeout */
static uint16_t poll_interval_ms = 0;       /* Minimum time between poll() calls (0=no delay) */

/* Configurable baud rate (saved to NVS) */
static uint32_t orion_baud = 9600;  /* Default: 9600, can be changed via config */

/* RS-485 pins (UART1) — to Orion bus via MAX3485 */
#define RS485_TX_PIN   7     /* GPIO7 - UART1 TX (unused in passive mode) */
#define RS485_RX_PIN   6     /* GPIO6 - UART1 RX */
#define RS485_DE_PIN   2     /* GPIO2 - MAX3485 DE+RE pin. Keep LOW = receive only (passive) */

/* Serial output to external system (UART0) — for cable connection */
#define EXT_SERIAL_TX_PIN  21   /* GPIO21 - UART0 TX */
#define EXT_SERIAL_RX_PIN  20   /* GPIO20 - UART0 RX */
#define EXT_SERIAL_BAUD    115200

/* NOTE: ESP32-C3 UART0 is shared with USB-C
 * When USB is connected: UART0 used for programming/debug
 * When USB disconnected: UART0 available for external serial
 * For production: disconnect USB and use UART0 for external system
 */

/* Watchdog timeout (seconds) — auto-restarts if ESP32 hangs */
#define WDT_TIMEOUT_SEC  30

/* Heartbeat interval (ms) — sent to serial cable so external system knows we're alive */
#define HEARTBEAT_INTERVAL_MS  5000

/* LED indicators */
#define STATUS_LED_PIN  3    /* GPIO3 - Status indicator (green LED via transistor) */
#define DEBUG_LED_PIN   4    /* GPIO4 - Debug mode indicator (red LED via transistor) */

/* Debug button (pull-up, active LOW) */
#define DEBUG_BUTTON_PIN  5  /* GPIO5 - Separate debug button (not BOOT!) - see optimized schematic */

/* Mode switch (pull-up, active LOW) */
#define MODE_SWITCH_PIN   8  /* GPIO8 - Physical mode switch: LOW=MONITOR, HIGH=MASTER */

/* Debug modes */
static bool raw_debug_mode = false;      /* Raw packet hex dump */
static bool verbose_debug_mode = false;  /* Verbose logging to USB serial */
static volatile bool debug_button_pressed = false;  /* ISR flag */
static unsigned long debug_button_press_time = 0;
static unsigned long last_debug_stats_print = 0;

/* WiFi AP state */
static bool wifi_ap_started = false;     /* Track if AP is active */

/* Firmware version */
#define FW_VERSION  "1.0.0"

/* ═══════════════════════════════════════════════════════════════════════
 *  GLOBAL OBJECTS
 * ═══════════════════════════════════════════════════════════════════════ */

OrionBusMonitor busMonitor(Serial1, RS485_DE_PIN);
OrionMaster     busMaster(Serial1, RS485_DE_PIN);
Preferences prefs;
WebServer httpServer(HTTP_PORT);

static unsigned long last_heartbeat = 0;
static unsigned long last_led_toggle = 0;
static bool led_state = false;
static uint32_t boot_count = 0;

/* JSON buffer for serial and HTTP responses */
static char json_buf[2048];

/* ═══════════════════════════════════════════════════════════════════════
 *  DEBUG BUTTON — ISR and handler
 * ═══════════════════════════════════════════════════════════════════════ */

void IRAM_ATTR debugButtonISR()
{
    debug_button_pressed = true;
}

void handleDebugButton()
{
    if (!debug_button_pressed) return;
    debug_button_pressed = false;

    /* Debounce: ignore if pressed within last 500ms */
    unsigned long now = millis();
    if (now - debug_button_press_time < 500) return;
    debug_button_press_time = now;

    /* Toggle debug modes on each press */
    static uint8_t debug_state = 0;
    debug_state = (debug_state + 1) % 4;

    switch (debug_state) {
        case 0:  /* All debug off */
            raw_debug_mode = false;
            verbose_debug_mode = false;
            Serial.println("\n[DEBUG] All debug modes OFF");
            Serial1.println("DEBUG,OFF");
            break;

        case 1:  /* Raw packet dump only */
            raw_debug_mode = true;
            verbose_debug_mode = false;
            Serial.println("\n[DEBUG] Raw packet dump ON");
            Serial1.println("DEBUG,RAW");
            break;

        case 2:  /* Verbose logging only */
            raw_debug_mode = false;
            verbose_debug_mode = true;
            Serial.println("\n[DEBUG] Verbose logging ON");
            Serial1.println("DEBUG,VERBOSE");
            break;

        case 3:  /* Both raw + verbose */
            raw_debug_mode = true;
            verbose_debug_mode = true;
            Serial.println("\n[DEBUG] Raw + Verbose ON");
            Serial1.println("DEBUG,FULL");
            break;
    }

    /* Start WiFi AP when debug mode enabled (if not already started) */
    if (!wifi_ap_started && debug_state > 0) {
        Serial.println("[WiFi] Starting Access Point...");
        WiFi.mode(WIFI_AP);
        WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASSWORD, WIFI_AP_CHANNEL);
        
        IPAddress IP = WiFi.softAPIP();
        Serial.printf("[WiFi] AP started: %s\n", WIFI_AP_SSID);
        Serial.printf("[WiFi] IP address: %s\n", IP.toString().c_str());
        Serial.printf("[WiFi] Password: %s\n", WIFI_AP_PASSWORD);
        Serial1.printf("SYSTEM,WIFI_AP_STARTED,%s,%s\n", WIFI_AP_SSID, IP.toString().c_str());
        
        /* Start HTTP configuration server */
        setupHttpServer();
        
        wifi_ap_started = true;
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  MODE SWITCH — Physical hardware switch for mode selection
 * ═══════════════════════════════════════════════════════════════════════ */

void handleModeSwitch()
{
    /* Read mode switch state:
     * LOW (switch to GND)  = MONITOR mode (safe, passive)
     * HIGH (switch open)   = MASTER mode (active control)
     */
    static bool last_switch_state = LOW;
    static unsigned long last_switch_change = 0;
    
    bool current_state = digitalRead(MODE_SWITCH_PIN);
    
    /* Debounce: ignore changes within 500ms */
    if (current_state != last_switch_state) {
        unsigned long now = millis();
        if (now - last_switch_change < 500) return;
        last_switch_change = now;
        last_switch_state = current_state;
        
        /* Switch position changed — update mode */
        if (current_state == HIGH) {
            /* Switch to MASTER mode */
            if (busMaster.getMode() != ORION_MODE_MASTER) {
                if (busMaster.switchToMaster()) {
                    Serial.println("[MODE SWITCH] Physical switch → MASTER mode");
                    Serial1.println("MODE_SWITCH,MASTER");
                } else {
                    Serial.println("[MODE SWITCH] Cannot switch to MASTER — C2000M detected!");
                    Serial1.println("MODE_SWITCH,MASTER_BLOCKED");
                }
            }
        } else {
            /* Switch to MONITOR mode */
            if (busMaster.getMode() != ORION_MODE_MONITOR) {
                busMaster.switchToMonitor();
                Serial.println("[MODE SWITCH] Physical switch → MONITOR mode");
                Serial1.println("MODE_SWITCH,MONITOR");
            }
        }
    }
}

void printDebugStats()
{
    if (!verbose_debug_mode) return;

    unsigned long now = millis();
    if (now - last_debug_stats_print < 10000) return;  /* Every 10s */
    last_debug_stats_print = now;

    Serial.println("\n========== DEBUG STATS ==========");
    Serial.printf("Uptime:        %lu s\n", now / 1000);
    Serial.printf("Free heap:     %lu bytes\n", (unsigned long)ESP.getFreeHeap());
    Serial.printf("Total packets: %lu\n", (unsigned long)busMonitor.getTotalPackets());
    Serial.printf("CRC errors:    %lu\n", (unsigned long)busMonitor.getCrcErrors());
    Serial.printf("Online devs:   %d\n", busMonitor.getOnlineDeviceCount());
    Serial.printf("Mode:          %s\n",
        busMaster.getMode() == ORION_MODE_MASTER ? "MASTER" : "MONITOR");
    Serial.printf("Master detect: %s\n",
        busMaster.isMasterDetected() ? "Yes" : "No");
    Serial.printf("WiFi AP:       %s\n", wifi_ap_started ? "Active" : "OFF");
    if (wifi_ap_started) {
        Serial.printf("AP IP:         %s\n", WiFi.softAPIP().toString().c_str());
        Serial.printf("AP clients:    %d\n", WiFi.softAPgetStationNum());
    }
    Serial.println("Output:        Serial cable + HTTP (if AP active)");
    Serial.println("================================\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 *  CALLBACKS — Bus monitor notifies us of changes
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Called when any device's zone status changes on the Orion bus.
 * Forward the change to external system via serial cable.
 */
void onStatusChange(uint8_t device_addr, uint8_t zone,
                    uint8_t old_status, uint8_t new_status)
{
    const char *old_str = OrionBusMonitor::statusStr(old_status);
    const char *new_str = OrionBusMonitor::statusStr(new_status);

    Serial.printf("[STATUS] Device %d Zone %d: %s -> %s\n",
                  device_addr, zone, old_str, new_str);

    /* Forward to external system via serial cable */
    Serial1.printf("STATUS,%d,%d,%d,%s\n", device_addr, zone, new_status, new_str);
}

/**
 * Called when a device comes online or goes offline.
 */
void onDeviceOnline(uint8_t device_addr, bool online)
{
    const char *state = online ? "ONLINE" : "OFFLINE";
    uint8_t dev_type = busMonitor.getDeviceType(device_addr);

    Serial.printf("[DEVICE] %d (%s) %s\n", device_addr,
                  OrionBusMonitor::deviceTypeStr(dev_type), state);

    /* Forward to serial cable */
    Serial1.printf("DEVICE,%d,%s,%d,%s\n", device_addr, state,
                   dev_type, OrionBusMonitor::deviceTypeStr(dev_type));
}

/**
 * Called for every event captured from the bus.
 */
void onEvent(const orion_bus_event_t *evt)
{
    const char *code_str = OrionBusMonitor::statusStr(evt->event_code);

    Serial.printf("[EVENT] Device %d Zone %d: %s (0x%02X)\n",
                  evt->device_addr, evt->zone, code_str, evt->event_code);

    /* Forward to serial cable */
    Serial1.printf("EVENT,%d,%d,%d,%s\n",
                   evt->device_addr, evt->zone, evt->event_code, code_str);
}

/**
 * Called for every decoded packet (raw forwarding / debug).
 */
void onPacket(const orion_decoded_packet_t *pkt)
{
    /* Notify master module about bus requests for C2000M presence detection.
     * In MONITOR mode: detects that a real master is active on the bus.
     * In MASTER mode: detects bus collision (another master appeared). */
    if (pkt->is_request && busMaster.getMode() == ORION_MODE_MONITOR) {
        busMaster.notifyBusRequest(pkt->addr, pkt->command);
    }

    if (!raw_debug_mode) return;

    /* Output raw hex dump for protocol debugging */
    Serial1.printf("RAW,%s,%d,%s,0x%02X,%d,",
                   pkt->is_request ? "REQ" : "RSP",
                   pkt->addr,
                   pkt->encrypted ? "ENC" : "CLR",
                   pkt->command,
                   (int)pkt->payload_len);
    for (size_t i = 0; i < pkt->payload_len && i < 32; i++) {
        Serial1.printf("%02X", pkt->payload[i]);
    }
    Serial1.println();
}

/* ═══════════════════════════════════════════════════════════════════════
 *  SERIAL CABLE — External system protocol
 *
 *  Output format (one line per message):
 *    STATUS,<dev>,<zone>,<code>,<string>
 *    DEVICE,<dev>,<ONLINE|OFFLINE>,<type>,<type_string>
 *    EVENT,<dev>,<zone>,<code>,<string>
 *    SYSTEM,<json>
 *
 *  Input commands (from external system):
 *    GET_STATUS         — returns full JSON status
 *    GET_DEVICE,<addr>  — returns single device JSON
 *    GET_EVENTS         — returns recent events
 *    PING               — health check (returns PONG)
 *    VERSION            — returns firmware version + uptime + boot count
 *    DEBUG_ON           — enable raw packet hex dump
 *    DEBUG_OFF          — disable raw packet hex dump
 *    SET_KEY,<hex>      — set global encryption key (e.g. SET_KEY,5A)
 *    REBOOT             — restart ESP32
 * ═══════════════════════════════════════════════════════════════════════ */

static char ext_cmd_buf[128];
static size_t ext_cmd_pos = 0;

void handleExternalSerialInput()
{
    while (Serial1.available()) {
        char c = Serial1.read();
        if (c == '\n' || c == '\r') {
            if (ext_cmd_pos > 0) {
                ext_cmd_buf[ext_cmd_pos] = '\0';

                if (strcmp(ext_cmd_buf, "GET_STATUS") == 0) {
                    busMonitor.toJSON(json_buf, sizeof(json_buf));
                    Serial1.printf("SYSTEM,%s\n", json_buf);
                }
                else if (strncmp(ext_cmd_buf, "GET_DEVICE,", 11) == 0) {
                    int addr = atoi(&ext_cmd_buf[11]);
                    busMonitor.deviceToJSON(addr, json_buf, sizeof(json_buf));
                    Serial1.printf("DEVICE_INFO,%s\n", json_buf);
                }
                else if (strcmp(ext_cmd_buf, "GET_EVENTS") == 0) {
                    uint8_t count = busMonitor.getEventCount();
                    Serial1.printf("EVENTS,%d,[", count);
                    for (uint8_t i = 0; i < count; i++) {
                        const orion_bus_event_t *e = busMonitor.getEvent(i);
                        if (e) {
                            if (i > 0) Serial1.print(",");
                            Serial1.printf("{\"d\":%d,\"z\":%d,\"c\":%d,\"s\":\"%s\"}",
                                e->device_addr, e->zone, e->event_code,
                                OrionBusMonitor::statusStr(e->event_code));
                        }
                    }
                    Serial1.println("]");
                }
                else if (strcmp(ext_cmd_buf, "PING") == 0) {
                    Serial1.printf("PONG,%lu,%d\n",
                        (unsigned long)busMonitor.getTotalPackets(),
                        busMonitor.getOnlineDeviceCount());
                }
                else if (strcmp(ext_cmd_buf, "VERSION") == 0) {
                    Serial1.printf("VERSION,%s,%lu,%lu,%lu,%d\n",
                        FW_VERSION,
                        millis() / 1000,
                        boot_count,
                        (unsigned long)ESP.getFreeHeap(),
                        busMonitor.getOnlineDeviceCount());
                }
                else if (strcmp(ext_cmd_buf, "DEBUG_ON") == 0) {
                    raw_debug_mode = true;
                    Serial1.println("OK,DEBUG_ON");
                    Serial.println("[CMD] Raw debug mode ON");
                }
                else if (strcmp(ext_cmd_buf, "DEBUG_OFF") == 0) {
                    raw_debug_mode = false;
                    Serial1.println("OK,DEBUG_OFF");
                    Serial.println("[CMD] Raw debug mode OFF");
                }
                else if (strncmp(ext_cmd_buf, "SET_KEY,", 8) == 0) {
                    unsigned int key = 0;
                    if (sscanf(&ext_cmd_buf[8], "%x", &key) == 1 && key <= 0xFF) {
                        busMonitor.setGlobalKey((uint8_t)key);
                        prefs.begin("orion", false);
                        prefs.putUChar("global_key", (uint8_t)key);
                        prefs.end();
                        Serial1.printf("OK,SET_KEY,%02X\n", key);
                        Serial.printf("[CMD] Global key set to 0x%02X (saved to NVS)\n", key);
                    } else {
                        Serial1.println("ERR,INVALID_KEY");
                    }
                }
                else if (strcmp(ext_cmd_buf, "REBOOT") == 0) {
                    Serial1.println("OK,REBOOT");
                    Serial.println("[CMD] Rebooting...");
                    delay(100);
                    ESP.restart();
                }
                /* ─── Mode switching commands ─────────────────── */
                else if (strcmp(ext_cmd_buf, "MASTER_ON") == 0) {
                    if (busMaster.switchToMaster()) {
                        Serial1.println("OK,MASTER_ON");
                        Serial.println("[CMD] Switched to MASTER mode");
                    } else {
                        Serial1.printf("ERR,MASTER_ON,master_detected_%lus_ago\n",
                            busMaster.msSinceLastMasterTraffic() / 1000);
                    }
                }
                else if (strcmp(ext_cmd_buf, "MASTER_OFF") == 0) {
                    busMaster.switchToMonitor();
                    Serial1.println("OK,MASTER_OFF");
                    Serial.println("[CMD] Switched to MONITOR mode");
                }
                else if (strcmp(ext_cmd_buf, "MASTER_STATUS") == 0) {
                    const char *mode_str = (busMaster.getMode() == ORION_MODE_MASTER)
                                            ? "MASTER" : "MONITOR";
                    Serial1.printf("MODE,%s,master_detected=%d,last_master=%lus,"
                                   "tx=%lu,tx_err=%lu,timeouts=%lu\n",
                        mode_str,
                        busMaster.isMasterDetected() ? 1 : 0,
                        busMaster.msSinceLastMasterTraffic() / 1000,
                        busMaster.getTxPackets(),
                        busMaster.getTxErrors(),
                        busMaster.getResponseTimeouts());
                }
                /* ─── Master-mode commands (only work when MASTER) ── */
                else if (strncmp(ext_cmd_buf, "ARM,", 4) == 0) {
                    unsigned int addr = 0, zone = 0;
                    if (sscanf(&ext_cmd_buf[4], "%u,%u", &addr, &zone) == 2 &&
                        addr >= 1 && addr <= 127 && zone >= 1) {
                        int ret = busMaster.cmdArmZone((uint8_t)addr, (uint8_t)zone);
                        if (ret == 0) Serial1.printf("OK,ARM,%d,%d\n", addr, zone);
                        else Serial1.printf("ERR,ARM,%d\n", ret);
                    } else {
                        Serial1.println("ERR,ARM,usage:ARM,addr,zone");
                    }
                }
                else if (strncmp(ext_cmd_buf, "DISARM,", 7) == 0) {
                    unsigned int addr = 0, zone = 0;
                    if (sscanf(&ext_cmd_buf[7], "%u,%u", &addr, &zone) == 2 &&
                        addr >= 1 && addr <= 127 && zone >= 1) {
                        int ret = busMaster.cmdDisarmZone((uint8_t)addr, (uint8_t)zone);
                        if (ret == 0) Serial1.printf("OK,DISARM,%d,%d\n", addr, zone);
                        else Serial1.printf("ERR,DISARM,%d\n", ret);
                    } else {
                        Serial1.println("ERR,DISARM,usage:DISARM,addr,zone");
                    }
                }
                else if (strncmp(ext_cmd_buf, "RESET_ALARM,", 12) == 0) {
                    unsigned int addr = 0, zone = 0;
                    if (sscanf(&ext_cmd_buf[12], "%u,%u", &addr, &zone) == 2 &&
                        addr >= 1 && addr <= 127 && zone >= 1) {
                        int ret = busMaster.cmdResetAlarm((uint8_t)addr, (uint8_t)zone);
                        if (ret == 0) Serial1.printf("OK,RESET_ALARM,%d,%d\n", addr, zone);
                        else Serial1.printf("ERR,RESET_ALARM,%d\n", ret);
                    } else {
                        Serial1.println("ERR,RESET_ALARM,usage:RESET_ALARM,addr,zone");
                    }
                }
                else if (strncmp(ext_cmd_buf, "PING_DEV,", 9) == 0) {
                    unsigned int addr = 0;
                    if (sscanf(&ext_cmd_buf[9], "%u", &addr) == 1 &&
                        addr >= 1 && addr <= 127) {
                        int ret = busMaster.cmdPing((uint8_t)addr);
                        if (ret == 0) Serial1.printf("OK,PING_DEV,%d\n", addr);
                        else Serial1.printf("ERR,PING_DEV,%d,no_response\n", addr);
                    } else {
                        Serial1.println("ERR,PING_DEV,usage:PING_DEV,addr");
                    }
                }
                else if (strcmp(ext_cmd_buf, "SCAN") == 0) {
                    if (busMaster.getMode() != ORION_MODE_MASTER) {
                        Serial1.println("ERR,SCAN,not_in_master_mode");
                    } else {
                        Serial1.println("SCAN,START");
                        Serial.println("[CMD] Scanning bus for devices...");
                        for (uint8_t a = 1; a <= 127; a++) {
                            int ret = busMaster.cmdPing(a);
                            if (ret == 0) {
                                busMaster.cmdReadDeviceInfo(a);
                                Serial1.printf("SCAN,FOUND,%d\n", a);
                            }
                            delay(10);
                            esp_task_wdt_reset();
                        }
                        Serial1.println("SCAN,DONE");
                        Serial.println("[CMD] Bus scan complete");
                    }
                }
                else if (strcmp(ext_cmd_buf, "POLL_ON") == 0) {
                    busMaster.setAutoPolling(true);
                    Serial1.println("OK,POLL_ON");
                }
                else if (strcmp(ext_cmd_buf, "POLL_OFF") == 0) {
                    busMaster.setAutoPolling(false);
                    Serial1.println("OK,POLL_OFF");
                }
                else {
                    Serial1.printf("ERR,UNKNOWN_CMD,%s\n", ext_cmd_buf);
                }

                ext_cmd_pos = 0;
            }
        } else if (ext_cmd_pos < sizeof(ext_cmd_buf) - 1) {
            ext_cmd_buf[ext_cmd_pos++] = c;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  HTTP SERVER — Configuration interface (WiFi AP mode)
 * ═══════════════════════════════════════════════════════════════════════ */

void handleHttpRoot()
{
    httpServer.send(200, "text/html",
        "<html><head><title>Orion Gateway Config</title>"
        "<style>body{font-family:Arial;margin:40px;background:#f0f0f0;}"
        "h1{color:#333;}a{display:block;margin:10px 0;padding:10px;background:#007bff;"
        "color:white;text-decoration:none;border-radius:5px;text-align:center;}"
        "a:hover{background:#0056b3;}</style></head><body>"
        "<h1>Orion RS-485 Gateway</h1>"
        "<p>Firmware: " FW_VERSION "</p>"
        "<p>Mode: <span id='mode'>Loading...</span></p>"
        "<a href='/api/status'>System Status (JSON)</a>"
        "<a href='/api/devices'>Online Devices (JSON)</a>"
        "<a href='/api/config'>Configuration (JSON)</a>"
        "<a href='/api/mode'>Mode Status (JSON)</a>"
        "</body></html>");
}

void handleHttpStatus()
{
    busMonitor.toJSON(json_buf, sizeof(json_buf));
    httpServer.send(200, "application/json", json_buf);
}

void handleHttpDevices()
{
    uint8_t addrs[ORION_MAX_DEVICES];
    uint8_t count = busMonitor.getOnlineDevices(addrs, ORION_MAX_DEVICES);

    int pos = snprintf(json_buf, sizeof(json_buf), "{\"count\":%d,\"devices\":[", count);
    for (uint8_t i = 0; i < count; i++) {
        const orion_device_state_t *dev = busMonitor.getDevice(addrs[i]);
        if (i > 0) pos += snprintf(json_buf + pos, sizeof(json_buf) - pos, ",");
        pos += snprintf(json_buf + pos, sizeof(json_buf) - pos,
            "{\"addr\":%d,\"type\":\"%s\",\"fw\":\"%d.%d\",\"zones\":%d}",
            addrs[i],
            OrionBusMonitor::deviceTypeStr(dev ? dev->device_type : 0),
            dev ? dev->fw_major : 0, dev ? dev->fw_minor : 0,
            dev ? dev->zone_count : 0);
    }
    snprintf(json_buf + pos, sizeof(json_buf) - pos, "]}");
    httpServer.send(200, "application/json", json_buf);
}

void handleHttpConfig()
{
    if (httpServer.method() == HTTP_GET) {
        snprintf(json_buf, sizeof(json_buf),
            "{\"packet_timeout_ms\":%d,\"response_gap_ms\":%d,"
            "\"device_timeout_ms\":%d,\"poll_interval_ms\":%d,"
            "\"orion_baud\":%lu,\"global_key\":\"0x%02X\","
            "\"mode\":\"%s\",\"master_detected\":%s}",
            packet_timeout_ms, response_gap_ms, device_timeout_ms, poll_interval_ms,
            orion_baud, busMonitor.getGlobalKey(),
            busMaster.getMode() == ORION_MODE_MASTER ? "MASTER" : "MONITOR",
            busMaster.isMasterDetected() ? "true" : "false");
        httpServer.send(200, "application/json", json_buf);
    }
    else if (httpServer.method() == HTTP_POST) {
        bool changed = false;
        
        if (httpServer.hasArg("global_key")) {
            String keyStr = httpServer.arg("global_key");
            unsigned int key = 0;
            if (sscanf(keyStr.c_str(), "%x", &key) == 1 && key <= 0xFF) {
                busMonitor.setGlobalKey((uint8_t)key);
                busMaster.setGlobalKey((uint8_t)key);
                busMaster.setMessageKey((uint8_t)key);
                prefs.begin("orion", false);
                prefs.putUChar("global_key", (uint8_t)key);
                prefs.end();
                changed = true;
            }
        }
        
        if (changed) {
            httpServer.send(200, "application/json", 
                "{\"status\":\"ok\",\"message\":\"Configuration updated\"}");
            Serial.println("[HTTP] Configuration updated via WiFi AP");
        } else {
            httpServer.send(400, "application/json", 
                "{\"status\":\"error\",\"message\":\"No valid parameters\"}");
        }
    }
}

void handleHttpMode()
{
    snprintf(json_buf, sizeof(json_buf),
        "{\"mode\":\"%s\",\"master_detected\":%s,"
        "\"last_master_traffic_s\":%lu,"
        "\"tx_packets\":%lu,\"tx_errors\":%lu,\"response_timeouts\":%lu}",
        busMaster.getMode() == ORION_MODE_MASTER ? "MASTER" : "MONITOR",
        busMaster.isMasterDetected() ? "true" : "false",
        busMaster.msSinceLastMasterTraffic() / 1000,
        busMaster.getTxPackets(),
        busMaster.getTxErrors(),
        busMaster.getResponseTimeouts());
    httpServer.send(200, "application/json", json_buf);
}

void setupHttpServer()
{
    httpServer.on("/", handleHttpRoot);
    httpServer.on("/api/status", handleHttpStatus);
    httpServer.on("/api/devices", handleHttpDevices);
    httpServer.on("/api/config", HTTP_GET, handleHttpConfig);
    httpServer.on("/api/config", HTTP_POST, handleHttpConfig);
    httpServer.on("/api/mode", handleHttpMode);

    httpServer.onNotFound([]() {
        httpServer.send(404, "application/json", "{\"error\":\"not found\"}");
    });

    httpServer.begin();
    Serial.printf("[HTTP] Server running at http://%s/\n", WiFi.softAPIP().toString().c_str());
}

/* ═══════════════════════════════════════════════════════════════════════
 *  HEARTBEAT + LED — Periodic health indicators
 * ═══════════════════════════════════════════════════════════════════════ */

void sendHeartbeat()
{
    unsigned long now = millis();
    if (now - last_heartbeat < HEARTBEAT_INTERVAL_MS) return;
    last_heartbeat = now;

    Serial1.printf("HEARTBEAT,%lu,%lu,%d,%lu,%lu\n",
        now / 1000,
        (unsigned long)busMonitor.getTotalPackets(),
        busMonitor.getOnlineDeviceCount(),
        (unsigned long)busMonitor.getCrcErrors(),
        (unsigned long)ESP.getFreeHeap());
}

void updateLEDs()
{
    unsigned long now = millis();
    uint8_t online = busMonitor.getOnlineDeviceCount();

    /* STATUS LED (GPIO2) - bus activity and alarm status:
     *   Fast blink (200ms)  = bus active, devices online
     *   Slow blink (1000ms) = no devices found yet
     *   Solid ON            = alarm detected on any zone
     *   OFF                 = no bus activity
     */

    bool any_alarm = false;
    uint8_t addrs[ORION_MAX_DEVICES];
    uint8_t count = busMonitor.getOnlineDevices(addrs, ORION_MAX_DEVICES);
    for (uint8_t i = 0; i < count && !any_alarm; i++) {
        const orion_device_state_t *dev = busMonitor.getDevice(addrs[i]);
        if (!dev) continue;
        for (uint8_t z = 0; z < dev->zone_count; z++) {
            if (dev->zones[z].status == ORION_ST_ALARM ||
                dev->zones[z].status == ORION_ST_FIRE_ALARM) {
                any_alarm = true;
                break;
            }
        }
    }

    if (any_alarm) {
        digitalWrite(STATUS_LED_PIN, HIGH);
    } else if (busMonitor.getTotalPackets() == 0) {
        digitalWrite(STATUS_LED_PIN, LOW);
    } else {
        unsigned long interval = (online > 0) ? 200 : 1000;
        if (now - last_led_toggle > interval) {
            last_led_toggle = now;
            led_state = !led_state;
            digitalWrite(STATUS_LED_PIN, led_state ? HIGH : LOW);
        }
    }

    /* DEBUG LED - mode, debug, and WiFi AP indicator:
     *   Fast blink (100ms)  = MASTER mode active
     *   Slow blink (500ms)  = WiFi AP active
     *   Solid ON            = debug mode active (raw or verbose)
     *   OFF                 = MONITOR mode, no debug, no WiFi
     */
    if (busMaster.getMode() == ORION_MODE_MASTER) {
        static unsigned long last_master_led = 0;
        static bool master_led_state = false;
        if (now - last_master_led > 100) {
            last_master_led = now;
            master_led_state = !master_led_state;
            digitalWrite(DEBUG_LED_PIN, master_led_state ? HIGH : LOW);
        }
    } else if (wifi_ap_started) {
        static unsigned long last_wifi_led = 0;
        static bool wifi_led_state = false;
        if (now - last_wifi_led > 500) {
            last_wifi_led = now;
            wifi_led_state = !wifi_led_state;
            digitalWrite(DEBUG_LED_PIN, wifi_led_state ? HIGH : LOW);
        }
    } else {
        digitalWrite(DEBUG_LED_PIN, (raw_debug_mode || verbose_debug_mode) ? HIGH : LOW);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  ARDUINO SETUP
 * ═══════════════════════════════════════════════════════════════════════ */

void setup()
{
    /* Debug serial (USB) */
    Serial.begin(115200);
    Serial.println("\n================================================");
    Serial.println("  Bolid Orion RS-485 Gateway / Bridge (ESP32-C3)");
    Serial.printf("  Firmware %s\n", FW_VERSION);
    Serial.println("  Dual-mode: MONITOR (passive) / MASTER (active)");
    Serial.println("  Default: MONITOR — safe alongside C2000M");
    Serial.println("================================================\n");

    /* LED indicators */
    pinMode(STATUS_LED_PIN, OUTPUT);
    pinMode(DEBUG_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, HIGH);
    digitalWrite(DEBUG_LED_PIN, LOW);

    /* Debug button with interrupt */
    pinMode(DEBUG_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(DEBUG_BUTTON_PIN), debugButtonISR, FALLING);

    /* Mode switch (physical hardware switch) */
    pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);

    /* Load saved config from NVS flash */
    prefs.begin("orion", true);
    uint8_t saved_key = prefs.getUChar("global_key", ORION_GLOBAL_KEY);
    boot_count = prefs.getULong("boot_count", 0);
    prefs.end();

    /* Increment and save boot counter */
    boot_count++;
    prefs.begin("orion", false);
    prefs.putULong("boot_count", boot_count);
    prefs.end();

    Serial.printf("[NVS] Global key: 0x%02X, Boot count: %lu\n",
                  saved_key, boot_count);

    /* External system serial (UART1 on custom pins) */
    Serial1.begin(EXT_SERIAL_BAUD, SERIAL_8N1, EXT_SERIAL_RX_PIN, EXT_SERIAL_TX_PIN);
    Serial.printf("[EXT] Serial output on GPIO%d(TX)/GPIO%d(RX) @ %d baud\n",
                  EXT_SERIAL_TX_PIN, EXT_SERIAL_RX_PIN, EXT_SERIAL_BAUD);

    /* Orion RS-485 bus (UART2 on custom pins) */
    Serial2.begin(ORION_BAUD, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

    /* Configure bus monitor (passive — DE pin stays LOW) */
    busMonitor.setGlobalKey(saved_key);
    busMonitor.onStatusChange(onStatusChange);
    busMonitor.onDeviceOnline(onDeviceOnline);
    busMonitor.onEvent(onEvent);
    busMonitor.onPacket(onPacket);
    busMonitor.begin(ORION_BAUD);

    /* Configure master controller (starts in MONITOR mode — safe) */
    busMaster.setGlobalKey(saved_key);
    busMaster.setMessageKey(saved_key);
    busMaster.onModeChange([](orion_operating_mode_t old_mode,
                              orion_operating_mode_t new_mode,
                              const char *reason) {
        const char *old_str = (old_mode == ORION_MODE_MASTER) ? "MASTER" : "MONITOR";
        const char *new_str = (new_mode == ORION_MODE_MASTER) ? "MASTER" : "MONITOR";
        Serial1.printf("MODE_CHANGE,%s,%s,%s\n", old_str, new_str, reason);
        Serial.printf("[MODE] %s -> %s (reason: %s)\n", old_str, new_str, reason);
    });
    busMaster.begin();

    /* Watchdog timer — auto-restart if ESP32 hangs */
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);

    /* Send boot notification to external system via serial cable */
    Serial1.printf("BOOT,%s,%lu,wifi_ap_on_demand\n", FW_VERSION, boot_count);

    Serial.println("\n[READY] Listening to Orion bus (MONITOR mode)...");
    Serial.println("  Output: Serial cable (primary)");
    Serial.println("  WiFi AP: OFF (press debug button to start)");
    Serial.printf("  Watchdog: %ds timeout\n", WDT_TIMEOUT_SEC);
    Serial.printf("  Debug button: GPIO%d (press to enable debug + WiFi AP)\n", DEBUG_BUTTON_PIN);
    Serial.printf("  Mode switch: GPIO%d (toggle MONITOR/MASTER)\n", MODE_SWITCH_PIN);
    Serial.println("  Mode commands: MASTER_ON, MASTER_OFF, MASTER_STATUS");
    Serial.println("  Master cmds:   ARM,addr,zone  DISARM,addr,zone  SCAN");
    Serial.println("  Send 'VERSION' or 'GET_STATUS' via serial cable\n");

    digitalWrite(STATUS_LED_PIN, LOW);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  ARDUINO LOOP
 * ═══════════════════════════════════════════════════════════════════════ */

void loop()
{
    /* Feed watchdog — if this loop hangs for >30s, ESP32 auto-restarts */
    esp_task_wdt_reset();

    /* Priority 1: Monitor Orion bus (must not miss packets)
     * In MONITOR mode: passively receives all traffic
     * In MASTER mode: busMonitor still decodes responses to our commands */
    busMonitor.poll();

    /* Priority 1b: Master controller (auto-poll, master detection, safety)
     * In MONITOR mode: only checks if C2000M master went offline
     * In MASTER mode: polls devices round-robin, detects bus collisions */
    busMaster.poll();

    /* Priority 2: Handle commands from external system via serial cable */
    handleExternalSerialInput();

    /* Priority 3: Debug button handler */
    handleDebugButton();

    /* Priority 3b: Physical mode switch handler */
    handleModeSwitch();

    /* Priority 4: Send heartbeat to external system */
    sendHeartbeat();

    /* Priority 5: Print debug stats (if verbose mode enabled) */
    printDebugStats();

    /* Priority 6: LED status indicators */
    updateLEDs();

    /* Priority 7: HTTP server (only if WiFi AP is active) */
    if (wifi_ap_started) {
        httpServer.handleClient();
    }
}
