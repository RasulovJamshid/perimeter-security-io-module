#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include "orion_bus_monitor.h"
#include "orion_master.h"

/* ═══════════════════════════════════════════════════════════════════════
 *  CONFIGURATION — Edit these values for your setup
 * ═══════════════════════════════════════════════════════════════════════ */

/* ─── Feature Enable/Disable Flags ──────────────────────────────────────
 * Disable unused features to maximize performance and reduce memory usage.
 * Serial cable (UART1) is always enabled — it's the primary output.
 */
#define ENABLE_WIFI     true   /* WiFi connection (required for MQTT/HTTP/OTA) */
#define ENABLE_MQTT     true   /* MQTT publishing over WiFi */
#define ENABLE_HTTP     true   /* HTTP REST API server */
#define ENABLE_OTA      true   /* Over-the-air firmware updates */

/* WiFi-on-demand mode: WiFi only starts when debug button pressed
 * Set to true to save power and CPU - WiFi stays off until you need it
 * Requires ENABLE_WIFI=true (WiFi code must be compiled in)
 */
#define WIFI_ON_DEMAND  false  /* true = WiFi off by default, starts on debug button */

/* Performance notes:
 * - Serial-only mode (ENABLE_WIFI=false): ~0.5ms loop time, 100+ devices
 * - WiFi-on-demand (ENABLE_WIFI=true, WIFI_ON_DEMAND=true): ~0.5ms until debug pressed
 * - WiFi+MQTT enabled: ~2-5ms loop time, still handles 60+ devices
 * - All features enabled: ~5-10ms loop time, may miss packets on very busy bus
 */

/* WiFi credentials (only used if ENABLE_WIFI is true) */
#define WIFI_SSID       "YOUR_WIFI_SSID"
#define WIFI_PASSWORD   "YOUR_WIFI_PASSWORD"

/* MQTT broker (optional) */
#define MQTT_SERVER     "192.168.1.100"
#define MQTT_PORT       1883
#define MQTT_USER       ""
#define MQTT_PASS       ""
#define MQTT_CLIENT_ID  "orion_gateway"

/* MQTT topics */
#define MQTT_TOPIC_PREFIX   "orion"
#define MQTT_TOPIC_SYSTEM   "orion/system"
#define MQTT_TOPIC_DEVICE   "orion/device"
#define MQTT_TOPIC_EVENT    "orion/event"
#define MQTT_TOPIC_ZONE     "orion/zone"

/* ═══════════════════════════════════════════════════════════════════════
 *  HARDWARE: ESP32-C3 Super Mini
 *  - Built-in USB-C for programming
 *  - 2 UARTs: UART0 (GPIO20/21), UART1 (GPIO6/7)
 *  - Lower power: 43mA idle vs 80mA (ESP32-WROOM-32)
 *  - Smaller size: 22.52x18mm
 * ═══════════════════════════════════════════════════════════════════════ */

/* Orion bus - configurable parameters (saved to NVS) */
#define ORION_GLOBAL_KEY  0x00   /* Default encryption key */

/* Protocol timing (can be adjusted via WiFi config) */
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

/* HTTP REST API port */
#define HTTP_PORT  80

/* How often to publish full status (ms) */
#define STATUS_PUBLISH_INTERVAL_MS  10000
#define EVENT_PUBLISH_INTERVAL_MS   1000

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
static bool wifi_on_demand = WIFI_ON_DEMAND;  /* WiFi enabled only when debug button pressed */
static bool wifi_started = false;       /* Track if WiFi has been started */
static volatile bool debug_button_pressed = false;  /* ISR flag */
static unsigned long debug_button_press_time = 0;
static unsigned long last_debug_stats_print = 0;

/* Runtime feature enable flags (can be toggled via serial commands) */
static bool mqtt_enabled = ENABLE_MQTT;
static bool http_enabled = ENABLE_HTTP;
static bool ota_enabled = ENABLE_OTA;

/* Firmware version */
#define FW_VERSION  "1.0.0"

/* ═══════════════════════════════════════════════════════════════════════
 *  GLOBAL OBJECTS
 * ═══════════════════════════════════════════════════════════════════════ */

OrionBusMonitor busMonitor(Serial1, RS485_DE_PIN);
OrionMaster     busMaster(Serial1, RS485_DE_PIN);
Preferences prefs;

#if ENABLE_WIFI
WiFiClient wifiClient;
#endif

#if ENABLE_MQTT
PubSubClient mqtt(wifiClient);
#endif

#if ENABLE_HTTP
WebServer httpServer(HTTP_PORT);
#endif

static unsigned long last_status_publish = 0;
static unsigned long last_event_check = 0;
static uint8_t last_event_count = 0;
static unsigned long last_heartbeat = 0;
static unsigned long last_led_toggle = 0;
static bool led_state = false;
static uint32_t boot_count = 0;

/* JSON buffer for large responses */
static char json_buf[4096];

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

    /* If WiFi-on-demand mode and WiFi not started yet, start WiFi when debug enabled */
#if ENABLE_WIFI
    if (wifi_on_demand && !wifi_started && debug_state > 0) {
        Serial.println("[DEBUG] Starting WiFi for debug access...");
        setupWiFi();
        wifi_started = true;
        
        if (WiFi.status() == WL_CONNECTED) {
#if ENABLE_MQTT
            if (mqtt_enabled) {
                mqtt.setServer(MQTT_SERVER, MQTT_PORT);
                mqtt.setBufferSize(1024);
            }
#endif
#if ENABLE_HTTP
            if (http_enabled) setupHttpServer();
#endif
#if ENABLE_OTA
            if (ota_enabled) setupOTA();
#endif
            Serial.printf("[DEBUG] WiFi connected: %s\n", WiFi.localIP().toString().c_str());
            Serial1.printf("SYSTEM,WIFI_STARTED,%s\n", WiFi.localIP().toString().c_str());
        }
    }
#endif
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
#if ENABLE_WIFI
    Serial.printf("WiFi status:   %s\n", WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("WiFi IP:       %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("WiFi RSSI:     %d dBm\n", WiFi.RSSI());
#if ENABLE_MQTT
        Serial.printf("MQTT:          %s (%s)\n", 
            mqtt_enabled ? "Enabled" : "Disabled",
            mqtt.connected() ? "Connected" : "Disconnected");
#endif
#if ENABLE_HTTP
        Serial.printf("HTTP:          %s\n", http_enabled ? "Enabled" : "Disabled");
#endif
#if ENABLE_OTA
        Serial.printf("OTA:           %s\n", ota_enabled ? "Enabled" : "Disabled");
#endif
    }
#else
    Serial.println("WiFi:          Disabled (compile-time)");
#endif
    Serial.println("================================\n");
}

/* ═══════════════════════════════════════════════════════════════════════
 *  CALLBACKS — Bus monitor notifies us of changes
 * ═══════════════════════════════════════════════════════════════════════ */

/**
 * Called when any device's zone status changes on the Orion bus.
 * Forward the change to MQTT, serial, etc.
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

    /* Forward to MQTT */
#if ENABLE_MQTT
    if (mqtt_enabled && mqtt.connected()) {
        char topic[64];
        char payload[128];

        /* Per-device/zone topic */
        snprintf(topic, sizeof(topic), "%s/%d/%d/status",
                 MQTT_TOPIC_DEVICE, device_addr, zone);
        snprintf(payload, sizeof(payload),
                 "{\"device\":%d,\"zone\":%d,\"status\":%d,\"status_str\":\"%s\","
                 "\"prev\":%d,\"prev_str\":\"%s\"}",
                 device_addr, zone, new_status, new_str,
                 old_status, old_str);
        mqtt.publish(topic, payload);

        /* Alarm-specific topic for easy subscription */
        if (new_status == ORION_ST_ALARM || new_status == ORION_ST_FIRE_ALARM) {
            snprintf(topic, sizeof(topic), "%s/%d/alarm", MQTT_TOPIC_DEVICE, device_addr);
            snprintf(payload, sizeof(payload),
                     "{\"device\":%d,\"zone\":%d,\"type\":\"%s\"}",
                     device_addr, zone, new_str);
            mqtt.publish(topic, payload);
        }
    }
#endif
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

    /* Forward to MQTT */
#if ENABLE_MQTT
    if (mqtt_enabled && mqtt.connected()) {
        char topic[64];
        char payload[128];
        snprintf(topic, sizeof(topic), "%s/%d/online", MQTT_TOPIC_DEVICE, device_addr);
        snprintf(payload, sizeof(payload),
                 "{\"device\":%d,\"online\":%s,\"type\":%d,\"type_str\":\"%s\"}",
                 device_addr, online ? "true" : "false",
                 dev_type, OrionBusMonitor::deviceTypeStr(dev_type));
        mqtt.publish(topic, payload, true);
    }
#endif
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

    /* Forward to MQTT */
#if ENABLE_MQTT
    if (mqtt_enabled && mqtt.connected()) {
        char topic[64];
        char payload[128];
        snprintf(topic, sizeof(topic), "%s/%d", MQTT_TOPIC_EVENT, evt->device_addr);
        snprintf(payload, sizeof(payload),
                 "{\"device\":%d,\"zone\":%d,\"code\":%d,\"code_str\":\"%s\","
                 "\"extra\":%d}",
                 evt->device_addr, evt->zone, evt->event_code, code_str,
                 evt->extra);
        mqtt.publish(topic, payload);
    }
#endif
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
#if ENABLE_MQTT
                else if (strcmp(ext_cmd_buf, "MQTT_ON") == 0) {
                    mqtt_enabled = true;
                    Serial1.println("OK,MQTT_ON");
                    Serial.println("[CMD] MQTT enabled");
                }
                else if (strcmp(ext_cmd_buf, "MQTT_OFF") == 0) {
                    mqtt_enabled = false;
                    Serial1.println("OK,MQTT_OFF");
                    Serial.println("[CMD] MQTT disabled");
                }
#endif
#if ENABLE_HTTP
                else if (strcmp(ext_cmd_buf, "HTTP_ON") == 0) {
                    http_enabled = true;
                    Serial1.println("OK,HTTP_ON");
                    Serial.println("[CMD] HTTP enabled");
                }
                else if (strcmp(ext_cmd_buf, "HTTP_OFF") == 0) {
                    http_enabled = false;
                    Serial1.println("OK,HTTP_OFF");
                    Serial.println("[CMD] HTTP disabled");
                }
#endif
#if ENABLE_OTA
                else if (strcmp(ext_cmd_buf, "OTA_ON") == 0) {
                    ota_enabled = true;
                    Serial1.println("OK,OTA_ON");
                    Serial.println("[CMD] OTA enabled");
                }
                else if (strcmp(ext_cmd_buf, "OTA_OFF") == 0) {
                    ota_enabled = false;
                    Serial1.println("OK,OTA_OFF");
                    Serial.println("[CMD] OTA disabled");
                }
#endif
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
 *  HTTP REST API — For direct queries over WiFi
 * ═══════════════════════════════════════════════════════════════════════ */

void handleHttpRoot()
{
    httpServer.send(200, "text/html",
        "<html><head><title>Orion Gateway</title></head><body>"
        "<h1>Orion RS-485 Gateway</h1>"
        "<p><a href='/api/status'>System Status (JSON)</a></p>"
        "<p><a href='/api/devices'>Online Devices</a></p>"
        "<p><a href='/api/events'>Recent Events</a></p>"
        "<p><a href='/api/device/1'>Device 1 Detail</a></p>"
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

void handleHttpDevice()
{
    String uri = httpServer.uri();
    int addr = uri.substring(uri.lastIndexOf('/') + 1).toInt();

    if (addr < 1 || addr > 127) {
        httpServer.send(400, "application/json", "{\"error\":\"invalid address\"}");
        return;
    }

    busMonitor.deviceToJSON(addr, json_buf, sizeof(json_buf));
    httpServer.send(200, "application/json", json_buf);
}

void handleHttpEvents()
{
    uint8_t count = busMonitor.getEventCount();
    int pos = snprintf(json_buf, sizeof(json_buf), "{\"count\":%d,\"events\":[", count);

    for (uint8_t i = 0; i < count; i++) {
        const orion_bus_event_t *e = busMonitor.getEvent(i);
        if (!e) continue;
        if (i > 0) pos += snprintf(json_buf + pos, sizeof(json_buf) - pos, ",");
        pos += snprintf(json_buf + pos, sizeof(json_buf) - pos,
            "{\"device\":%d,\"zone\":%d,\"code\":%d,\"code_str\":\"%s\","
            "\"extra\":%d,\"ms\":%lu}",
            e->device_addr, e->zone, e->event_code,
            OrionBusMonitor::statusStr(e->event_code),
            e->extra, (unsigned long)e->timestamp);
    }

    snprintf(json_buf + pos, sizeof(json_buf) - pos, "]}");
    httpServer.send(200, "application/json", json_buf);
}

void handleHttpConfig()
{
    /* GET /api/config - Return current configuration */
    if (httpServer.method() == HTTP_GET) {
        snprintf(json_buf, sizeof(json_buf),
            "{\"packet_timeout_ms\":%d,\"response_gap_ms\":%d,"
            "\"device_timeout_ms\":%d,\"poll_interval_ms\":%d,"
            "\"orion_baud\":%lu,\"global_key\":%d,"
            "\"mqtt_enabled\":%s,\"http_enabled\":%s,\"ota_enabled\":%s}",
            packet_timeout_ms, response_gap_ms, device_timeout_ms, poll_interval_ms,
            orion_baud, busMonitor.getGlobalKey(),
            mqtt_enabled ? "true" : "false",
            http_enabled ? "true" : "false",
            ota_enabled ? "true" : "false");
        httpServer.send(200, "application/json", json_buf);
    }
    /* POST /api/config - Update configuration */
    else if (httpServer.method() == HTTP_POST) {
        bool changed = false;
        
        if (httpServer.hasArg("packet_timeout_ms")) {
            packet_timeout_ms = httpServer.arg("packet_timeout_ms").toInt();
            changed = true;
        }
        if (httpServer.hasArg("response_gap_ms")) {
            response_gap_ms = httpServer.arg("response_gap_ms").toInt();
            changed = true;
        }
        if (httpServer.hasArg("device_timeout_ms")) {
            device_timeout_ms = httpServer.arg("device_timeout_ms").toInt();
            changed = true;
        }
        if (httpServer.hasArg("poll_interval_ms")) {
            poll_interval_ms = httpServer.arg("poll_interval_ms").toInt();
            changed = true;
        }
        if (httpServer.hasArg("global_key")) {
            uint8_t key = httpServer.arg("global_key").toInt();
            busMonitor.setGlobalKey(key);
            prefs.begin("orion", false);
            prefs.putUChar("global_key", key);
            prefs.end();
            changed = true;
        }
        
        if (changed) {
            /* Save timing config to NVS */
            prefs.begin("orion", false);
            prefs.putUShort("pkt_timeout", packet_timeout_ms);
            prefs.putUShort("rsp_gap", response_gap_ms);
            prefs.putUShort("dev_timeout", device_timeout_ms);
            prefs.putUShort("poll_interval", poll_interval_ms);
            prefs.end();
            
            httpServer.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Config updated and saved\"}");
            Serial.println("[HTTP] Configuration updated via WiFi");
        } else {
            httpServer.send(400, "application/json", "{\"status\":\"error\",\"message\":\"No valid parameters\"}");
        }
    }
}

/* ─── Master mode HTTP API handlers ──────────────────────────────────── */

void handleHttpMode()
{
    if (httpServer.method() == HTTP_GET) {
        snprintf(json_buf, sizeof(json_buf),
            "{\"mode\":\"%s\",\"master_detected\":%s,"
            "\"last_master_traffic_s\":%lu,"
            "\"auto_polling\":%s,"
            "\"tx_packets\":%lu,\"tx_errors\":%lu,\"response_timeouts\":%lu}",
            busMaster.getMode() == ORION_MODE_MASTER ? "MASTER" : "MONITOR",
            busMaster.isMasterDetected() ? "true" : "false",
            busMaster.msSinceLastMasterTraffic() / 1000,
            busMaster.isAutoPolling() ? "true" : "false",
            busMaster.getTxPackets(),
            busMaster.getTxErrors(),
            busMaster.getResponseTimeouts());
        httpServer.send(200, "application/json", json_buf);
    }
    else if (httpServer.method() == HTTP_POST) {
        if (httpServer.hasArg("mode")) {
            String mode = httpServer.arg("mode");
            if (mode == "MASTER" || mode == "master") {
                if (busMaster.switchToMaster()) {
                    httpServer.send(200, "application/json",
                        "{\"status\":\"ok\",\"mode\":\"MASTER\"}");
                } else {
                    httpServer.send(409, "application/json",
                        "{\"status\":\"error\",\"message\":\"C2000M master detected on bus\"}");
                }
            } else if (mode == "MONITOR" || mode == "monitor") {
                busMaster.switchToMonitor();
                httpServer.send(200, "application/json",
                    "{\"status\":\"ok\",\"mode\":\"MONITOR\"}");
            } else {
                httpServer.send(400, "application/json",
                    "{\"status\":\"error\",\"message\":\"Invalid mode. Use MASTER or MONITOR\"}");
            }
        }
        if (httpServer.hasArg("auto_polling")) {
            busMaster.setAutoPolling(httpServer.arg("auto_polling") == "true");
        }
    }
}

void handleHttpMasterCmd()
{
    if (busMaster.getMode() != ORION_MODE_MASTER) {
        httpServer.send(403, "application/json",
            "{\"error\":\"Not in MASTER mode. POST to /api/mode with mode=MASTER first.\"}");
        return;
    }

    String uri = httpServer.uri();

    if (uri == "/api/master/arm") {
        if (!httpServer.hasArg("addr") || !httpServer.hasArg("zone")) {
            httpServer.send(400, "application/json", "{\"error\":\"Need addr and zone params\"}");
            return;
        }
        int addr = httpServer.arg("addr").toInt();
        int zone = httpServer.arg("zone").toInt();
        int ret = busMaster.cmdArmZone(addr, zone);
        snprintf(json_buf, sizeof(json_buf),
            "{\"cmd\":\"arm\",\"addr\":%d,\"zone\":%d,\"result\":%d}", addr, zone, ret);
        httpServer.send(ret == 0 ? 200 : 500, "application/json", json_buf);
    }
    else if (uri == "/api/master/disarm") {
        if (!httpServer.hasArg("addr") || !httpServer.hasArg("zone")) {
            httpServer.send(400, "application/json", "{\"error\":\"Need addr and zone params\"}");
            return;
        }
        int addr = httpServer.arg("addr").toInt();
        int zone = httpServer.arg("zone").toInt();
        int ret = busMaster.cmdDisarmZone(addr, zone);
        snprintf(json_buf, sizeof(json_buf),
            "{\"cmd\":\"disarm\",\"addr\":%d,\"zone\":%d,\"result\":%d}", addr, zone, ret);
        httpServer.send(ret == 0 ? 200 : 500, "application/json", json_buf);
    }
    else if (uri == "/api/master/reset") {
        if (!httpServer.hasArg("addr") || !httpServer.hasArg("zone")) {
            httpServer.send(400, "application/json", "{\"error\":\"Need addr and zone params\"}");
            return;
        }
        int addr = httpServer.arg("addr").toInt();
        int zone = httpServer.arg("zone").toInt();
        int ret = busMaster.cmdResetAlarm(addr, zone);
        snprintf(json_buf, sizeof(json_buf),
            "{\"cmd\":\"reset_alarm\",\"addr\":%d,\"zone\":%d,\"result\":%d}", addr, zone, ret);
        httpServer.send(ret == 0 ? 200 : 500, "application/json", json_buf);
    }
    else if (uri == "/api/master/ping") {
        if (!httpServer.hasArg("addr")) {
            httpServer.send(400, "application/json", "{\"error\":\"Need addr param\"}");
            return;
        }
        int addr = httpServer.arg("addr").toInt();
        int ret = busMaster.cmdPing(addr);
        snprintf(json_buf, sizeof(json_buf),
            "{\"cmd\":\"ping\",\"addr\":%d,\"result\":%d}", addr, ret);
        httpServer.send(ret == 0 ? 200 : 500, "application/json", json_buf);
    }
    else if (uri == "/api/master/scan") {
        int pos = snprintf(json_buf, sizeof(json_buf), "{\"devices\":[");
        bool first = true;
        for (uint8_t a = 1; a <= 127; a++) {
            int ret = busMaster.cmdPing(a);
            if (ret == 0) {
                busMaster.cmdReadDeviceInfo(a);
                if (!first) pos += snprintf(json_buf + pos, sizeof(json_buf) - pos, ",");
                first = false;
                pos += snprintf(json_buf + pos, sizeof(json_buf) - pos, "%d", a);
            }
            esp_task_wdt_reset();
        }
        snprintf(json_buf + pos, sizeof(json_buf) - pos, "]}");
        httpServer.send(200, "application/json", json_buf);
    }
}

void setupHttpServer()
{
    httpServer.on("/", handleHttpRoot);
    httpServer.on("/api/status", handleHttpStatus);
    httpServer.on("/api/devices", handleHttpDevices);
    httpServer.on("/api/events", handleHttpEvents);
    httpServer.on("/api/config", HTTP_GET, handleHttpConfig);
    httpServer.on("/api/config", HTTP_POST, handleHttpConfig);

    /* Master mode API */
    httpServer.on("/api/mode", HTTP_GET, handleHttpMode);
    httpServer.on("/api/mode", HTTP_POST, handleHttpMode);
    httpServer.on("/api/master/arm", HTTP_POST, handleHttpMasterCmd);
    httpServer.on("/api/master/disarm", HTTP_POST, handleHttpMasterCmd);
    httpServer.on("/api/master/reset", HTTP_POST, handleHttpMasterCmd);
    httpServer.on("/api/master/ping", HTTP_GET, handleHttpMasterCmd);
    httpServer.on("/api/master/scan", HTTP_GET, handleHttpMasterCmd);

    /* Wildcard: /api/device/<addr> */
    httpServer.onNotFound([]() {
        String uri = httpServer.uri();
        if (uri.startsWith("/api/device/")) {
            handleHttpDevice();
        } else {
            httpServer.send(404, "application/json", "{\"error\":\"not found\"}");
        }
    });

    httpServer.begin();
    Serial.printf("[HTTP] REST API running on port %d\n", HTTP_PORT);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  WIFI + MQTT
 * ═══════════════════════════════════════════════════════════════════════ */

void setupWiFi()
{
    Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        delay(500);
        Serial.print(".");
        attempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[WiFi] FAILED — running offline (serial cable only)");
    }
}

void reconnectMQTT()
{
    if (!mqtt.connected() && WiFi.status() == WL_CONNECTED) {
        Serial.print("[MQTT] Connecting...");
        if (mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
            Serial.println(" connected!");

            char msg[128];
            snprintf(msg, sizeof(msg),
                     "{\"gateway\":\"orion_bridge\",\"ip\":\"%s\",\"uptime\":%lu}",
                     WiFi.localIP().toString().c_str(), millis() / 1000);
            mqtt.publish(MQTT_TOPIC_SYSTEM, msg, true);
        } else {
            Serial.printf(" failed (rc=%d)\n", mqtt.state());
        }
    }
}

/**
 * Publish full system status periodically via MQTT.
 */
void publishSystemStatus()
{
    if (!mqtt.connected()) return;

    unsigned long now = millis();
    if (now - last_status_publish < STATUS_PUBLISH_INTERVAL_MS) return;
    last_status_publish = now;

    /* Publish individual device statuses */
    uint8_t addrs[ORION_MAX_DEVICES];
    uint8_t count = busMonitor.getOnlineDevices(addrs, ORION_MAX_DEVICES);

    for (uint8_t i = 0; i < count; i++) {
        char topic[64];
        snprintf(topic, sizeof(topic), "%s/%d/state", MQTT_TOPIC_DEVICE, addrs[i]);

        char payload[512];
        busMonitor.deviceToJSON(addrs[i], payload, sizeof(payload));
        mqtt.publish(topic, payload);
    }

    /* Publish system summary */
    char summary[256];
    snprintf(summary, sizeof(summary),
        "{\"online_devices\":%d,\"total_packets\":%lu,\"crc_errors\":%lu,"
        "\"uptime\":%lu,\"free_heap\":%lu}",
        count,
        (unsigned long)busMonitor.getTotalPackets(),
        (unsigned long)busMonitor.getCrcErrors(),
        millis() / 1000,
        (unsigned long)ESP.getFreeHeap());
    mqtt.publish(MQTT_TOPIC_SYSTEM, summary);
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

    /* DEBUG LED - mode and debug indicator:
     *   Fast blink (100ms) = MASTER mode active
     *   Solid ON           = debug mode active (raw or verbose)
     *   OFF                = MONITOR mode, no debug
     */
    if (busMaster.getMode() == ORION_MODE_MASTER) {
        static unsigned long last_master_led = 0;
        static bool master_led_state = false;
        if (now - last_master_led > 100) {
            last_master_led = now;
            master_led_state = !master_led_state;
            digitalWrite(DEBUG_LED_PIN, master_led_state ? HIGH : LOW);
        }
    } else {
        digitalWrite(DEBUG_LED_PIN, (raw_debug_mode || verbose_debug_mode) ? HIGH : LOW);
    }
}

/* ═══════════════════════════════════════════════════════════════════════
 *  OTA — Over-The-Air firmware updates (via WiFi)
 * ═══════════════════════════════════════════════════════════════════════ */

void setupOTA()
{
    ArduinoOTA.setHostname("orion-gateway");
    ArduinoOTA.onStart([]() {
        Serial.println("[OTA] Update starting...");
        Serial1.println("SYSTEM,OTA_START");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("[OTA] Update complete! Rebooting...");
        Serial1.println("SYSTEM,OTA_DONE");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("[OTA] Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("[OTA] Error[%u]\n", error);
        Serial1.printf("SYSTEM,OTA_ERROR,%u\n", error);
    });
    ArduinoOTA.begin();
    Serial.println("[OTA] Ready (hostname: orion-gateway)");
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

    /* WiFi + MQTT + HTTP + OTA (optional) */
#if ENABLE_WIFI
    if (!wifi_on_demand) {
        /* Start WiFi immediately if not in on-demand mode */
        setupWiFi();
        wifi_started = true;

        if (WiFi.status() == WL_CONNECTED) {
#if ENABLE_MQTT
            if (mqtt_enabled) {
                mqtt.setServer(MQTT_SERVER, MQTT_PORT);
                mqtt.setBufferSize(1024);
            }
#endif
#if ENABLE_HTTP
            if (http_enabled) setupHttpServer();
#endif
#if ENABLE_OTA
            if (ota_enabled) setupOTA();
#endif
        }
    } else {
        /* WiFi-on-demand mode: WiFi stays off until debug button pressed */
        Serial.println("[WiFi] On-demand mode - press debug button to start WiFi");
    }
#endif

    /* Watchdog timer — auto-restart if ESP32 hangs */
    esp_task_wdt_init(WDT_TIMEOUT_SEC, true);
    esp_task_wdt_add(NULL);

    /* Send boot notification to external system via serial cable */
#if ENABLE_WIFI
    if (wifi_on_demand && !wifi_started) {
        Serial1.printf("BOOT,%s,%lu,wifi_on_demand\n", FW_VERSION, boot_count);
    } else {
        Serial1.printf("BOOT,%s,%lu,%s\n",
            FW_VERSION, boot_count,
            WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "no_wifi");
    }
#else
    Serial1.printf("BOOT,%s,%lu,serial_only\n", FW_VERSION, boot_count);
#endif

    Serial.println("\n[READY] Listening to Orion bus (MONITOR mode)...");
    Serial.print("  Outputs: Serial cable (UART1)");
#if ENABLE_MQTT
    if (mqtt_enabled) Serial.print(" + MQTT");
#endif
#if ENABLE_HTTP
    if (http_enabled) Serial.print(" + HTTP");
#endif
#if ENABLE_OTA
    if (ota_enabled) Serial.print(" + OTA");
#endif
    Serial.println();
    Serial.printf("  Watchdog: %ds timeout\n", WDT_TIMEOUT_SEC);
    Serial.printf("  Debug button: GPIO%d (press to cycle debug modes)\n", DEBUG_BUTTON_PIN);
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

    /* Priority 7: WiFi services (only if enabled) */
#if ENABLE_WIFI
    if (WiFi.status() == WL_CONNECTED) {
#if ENABLE_MQTT
        /* MQTT */
        if (mqtt_enabled) {
            if (!mqtt.connected()) {
                static unsigned long last_mqtt_attempt = 0;
                if (millis() - last_mqtt_attempt > 5000) {
                    last_mqtt_attempt = millis();
                    reconnectMQTT();
                }
            }
            mqtt.loop();
            publishSystemStatus();
        }
#endif

#if ENABLE_HTTP
        /* HTTP REST API */
        if (http_enabled) {
            httpServer.handleClient();
        }
#endif

#if ENABLE_OTA
        /* OTA firmware update check */
        if (ota_enabled) {
            ArduinoOTA.handle();
        }
#endif

    } else {
        /* WiFi lost — try to reconnect every 30 seconds */
        static unsigned long last_wifi_attempt = 0;
        if (millis() - last_wifi_attempt > 30000) {
            last_wifi_attempt = millis();
            Serial.println("[WiFi] Reconnecting...");
            WiFi.reconnect();
        }
    }
#endif
    /* If WiFi disabled at compile time, loop runs at maximum speed */
}
