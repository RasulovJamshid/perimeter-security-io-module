#ifndef ORION_DEVICE_TYPES_H
#define ORION_DEVICE_TYPES_H

#include <stdint.h>

/**
 * Bolid Orion device type codes.
 * Source: Bolid documentation and reverse-engineering.
 */

#define ORION_DEV_C2000M            0x01  /* С2000М — system controller / master panel */
#define ORION_DEV_C2000_ADEM        0x02  /* С2000-АDEM — address expansion module */
#define ORION_DEV_SIGNAL_20         0x04  /* Сигнал-20 — 20-zone fire/security panel */
#define ORION_DEV_SIGNAL_20P        0x05  /* Сигнал-20П — 20-zone addressable panel */
#define ORION_DEV_SIGNAL_20M        0x06  /* Сигнал-20М — 20-zone security/fire module */
#define ORION_DEV_S2000_KDL         0x0A  /* С2000-КДЛ — addressable loop controller */
#define ORION_DEV_S2000_KDL2I       0x0B  /* С2000-КДЛ-2И — dual loop controller */
#define ORION_DEV_S2000_SP1         0x0E  /* С2000-СП1 — relay output module (1 relay) */
#define ORION_DEV_S2000_SP2         0x0F  /* С2000-СП2 — relay output module (2 relays) */
#define ORION_DEV_S2000_4           0x10  /* С2000-4 — 4-zone security panel */
#define ORION_DEV_S2000_IT          0x11  /* С2000-ИТ — current loop interface */
#define ORION_DEV_S2000_KPB         0x13  /* С2000-КПБ — fire relay module (6 relays) */
#define ORION_DEV_S2000_PP          0x14  /* С2000-ПП — protocol converter */
#define ORION_DEV_C2000_BKI         0x15  /* С2000-БКИ — indicator panel */
#define ORION_DEV_S2000_2           0x16  /* С2000-2 — 2-zone security panel */
#define ORION_DEV_C2000_GSM         0x19  /* С2000-GSM — GSM communicator */
#define ORION_DEV_C2000_ETHERNET    0x1A  /* С2000-Ethernet — network interface */
#define ORION_DEV_C2000_ASPRPT      0x1B  /* С2000-АСПРТ — printer interface */
#define ORION_DEV_C2000_USB         0x1C  /* С2000-USB — USB interface module */
#define ORION_DEV_SIGNAL_10         0x1D  /* Сигнал-10 — 10-zone fire panel */
#define ORION_DEV_UO_4S             0x1E  /* УО-4С — 4-zone output module */
#define ORION_DEV_C2000_PGE         0x22  /* С2000-ПГЕ — gas extinguishing module */
#define ORION_DEV_C2000_BKS         0x24  /* С2000-БКС — access control module */
#define ORION_DEV_S2000_KDL2        0x29  /* С2000-КДЛ-2 — dual loop controller v2 */
#define ORION_DEV_C2000_PF          0x2A  /* С2000-ПФ — fire suppression panel */
#define ORION_DEV_C2000_SP4         0x2C  /* С2000-СП4 — 4-relay output module */
#define ORION_DEV_C2000_BIOACCESS   0x2D  /* С2000-BIOAccess — biometric controller */
#define ORION_DEV_SIGNAL_20M_V2     0x34  /* Сигнал-20М (v2) — updated 20-zone module */
#define ORION_DEV_C2000M_V2         0x37  /* С2000М (v2) — updated system controller */

/**
 * Device category for grouping.
 */
typedef enum {
    DEV_CAT_UNKNOWN = 0,
    DEV_CAT_PANEL,           /* Main controller / master panel */
    DEV_CAT_ZONE_EXPANDER,   /* Zone input modules */
    DEV_CAT_RELAY_OUTPUT,    /* Relay output modules */
    DEV_CAT_LOOP_CONTROLLER, /* Addressable loop controllers */
    DEV_CAT_ACCESS_CONTROL,  /* Access control devices */
    DEV_CAT_COMMUNICATOR,    /* GSM, Ethernet, USB interfaces */
    DEV_CAT_INDICATOR,       /* Display / indicator panels */
    DEV_CAT_FIRE,            /* Fire-specific modules */
    DEV_CAT_CONVERTER        /* Protocol converters */
} orion_device_category_t;

/**
 * Get human-readable device name from type code.
 * @return Static string, never NULL.
 */
const char *orion_device_type_name(uint8_t type_code);

/**
 * Get short model designation (e.g. "C2000M", "Signal-20M").
 * @return Static string, never NULL.
 */
const char *orion_device_model(uint8_t type_code);

/**
 * Get device category.
 */
orion_device_category_t orion_device_category(uint8_t type_code);

/**
 * Get category name string.
 */
const char *orion_device_category_name(orion_device_category_t cat);

/**
 * Get number of zones supported by device type.
 * @return Zone count, or 0 if unknown.
 */
int orion_device_zone_count(uint8_t type_code);

#endif /* ORION_DEVICE_TYPES_H */
