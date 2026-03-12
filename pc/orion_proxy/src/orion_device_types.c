#include "orion_device_types.h"

const char *orion_device_type_name(uint8_t type_code)
{
    switch (type_code) {
    case ORION_DEV_C2000M:          return "S2000M system controller (master panel)";
    case ORION_DEV_C2000_ADEM:      return "S2000-ADEM address expansion module";
    case ORION_DEV_SIGNAL_20:       return "Signal-20 fire/security panel (20 zones)";
    case ORION_DEV_SIGNAL_20P:      return "Signal-20P addressable panel (20 zones)";
    case ORION_DEV_SIGNAL_20M:      return "Signal-20M security/fire module (20 zones)";
    case ORION_DEV_S2000_KDL:       return "S2000-KDL addressable loop controller";
    case ORION_DEV_S2000_KDL2I:     return "S2000-KDL-2I dual loop controller";
    case ORION_DEV_S2000_SP1:       return "S2000-SP1 relay output module (1 relay)";
    case ORION_DEV_S2000_SP2:       return "S2000-SP2 relay output module (2 relays)";
    case ORION_DEV_S2000_4:         return "S2000-4 security panel (4 zones)";
    case ORION_DEV_S2000_IT:        return "S2000-IT current loop interface";
    case ORION_DEV_S2000_KPB:       return "S2000-KPB fire relay module (6 relays)";
    case ORION_DEV_S2000_PP:        return "S2000-PP protocol converter";
    case ORION_DEV_C2000_BKI:       return "S2000-BKI indicator panel";
    case ORION_DEV_S2000_2:         return "S2000-2 security panel (2 zones)";
    case ORION_DEV_C2000_GSM:       return "S2000-GSM communicator";
    case ORION_DEV_C2000_ETHERNET:  return "S2000-Ethernet network interface";
    case ORION_DEV_C2000_ASPRPT:    return "S2000-ASPRPT printer interface";
    case ORION_DEV_C2000_USB:       return "S2000-USB interface module";
    case ORION_DEV_SIGNAL_10:       return "Signal-10 fire panel (10 zones)";
    case ORION_DEV_UO_4S:           return "UO-4S output module (4 zones)";
    case ORION_DEV_C2000_PGE:       return "S2000-PGE gas extinguishing module";
    case ORION_DEV_C2000_BKS:       return "S2000-BKS access control module";
    case ORION_DEV_S2000_KDL2:      return "S2000-KDL-2 dual loop controller v2";
    case ORION_DEV_C2000_PF:        return "S2000-PF fire suppression panel";
    case ORION_DEV_C2000_SP4:       return "S2000-SP4 relay output module (4 relays)";
    case ORION_DEV_C2000_BIOACCESS: return "S2000-BIOAccess biometric controller";
    case ORION_DEV_SIGNAL_20M_V2:   return "Signal-20M (v2) security/fire module (20 zones)";
    case ORION_DEV_C2000M_V2:       return "S2000M (v2) system controller";
    default:                        return "Unknown device";
    }
}

const char *orion_device_model(uint8_t type_code)
{
    switch (type_code) {
    case ORION_DEV_C2000M:          return "C2000M";
    case ORION_DEV_C2000_ADEM:      return "C2000-ADEM";
    case ORION_DEV_SIGNAL_20:       return "Signal-20";
    case ORION_DEV_SIGNAL_20P:      return "Signal-20P";
    case ORION_DEV_SIGNAL_20M:      return "Signal-20M";
    case ORION_DEV_S2000_KDL:       return "S2000-KDL";
    case ORION_DEV_S2000_KDL2I:     return "S2000-KDL-2I";
    case ORION_DEV_S2000_SP1:       return "S2000-SP1";
    case ORION_DEV_S2000_SP2:       return "S2000-SP2";
    case ORION_DEV_S2000_4:         return "S2000-4";
    case ORION_DEV_S2000_IT:        return "S2000-IT";
    case ORION_DEV_S2000_KPB:       return "S2000-KPB";
    case ORION_DEV_S2000_PP:        return "S2000-PP";
    case ORION_DEV_C2000_BKI:       return "C2000-BKI";
    case ORION_DEV_S2000_2:         return "S2000-2";
    case ORION_DEV_C2000_GSM:       return "C2000-GSM";
    case ORION_DEV_C2000_ETHERNET:  return "C2000-Ethernet";
    case ORION_DEV_C2000_ASPRPT:    return "C2000-ASPRPT";
    case ORION_DEV_C2000_USB:       return "C2000-USB";
    case ORION_DEV_SIGNAL_10:       return "Signal-10";
    case ORION_DEV_UO_4S:           return "UO-4S";
    case ORION_DEV_C2000_PGE:       return "C2000-PGE";
    case ORION_DEV_C2000_BKS:       return "C2000-BKS";
    case ORION_DEV_S2000_KDL2:      return "S2000-KDL-2";
    case ORION_DEV_C2000_PF:        return "C2000-PF";
    case ORION_DEV_C2000_SP4:       return "C2000-SP4";
    case ORION_DEV_C2000_BIOACCESS: return "C2000-BIOAccess";
    case ORION_DEV_SIGNAL_20M_V2:   return "Signal-20M-v2";
    case ORION_DEV_C2000M_V2:       return "C2000M-v2";
    default:                        return "Unknown";
    }
}

orion_device_category_t orion_device_category(uint8_t type_code)
{
    switch (type_code) {
    case ORION_DEV_C2000M:
    case ORION_DEV_C2000M_V2:
        return DEV_CAT_PANEL;

    case ORION_DEV_SIGNAL_20:
    case ORION_DEV_SIGNAL_20P:
    case ORION_DEV_SIGNAL_20M:
    case ORION_DEV_SIGNAL_20M_V2:
    case ORION_DEV_S2000_4:
    case ORION_DEV_S2000_2:
    case ORION_DEV_SIGNAL_10:
    case ORION_DEV_C2000_ADEM:
        return DEV_CAT_ZONE_EXPANDER;

    case ORION_DEV_S2000_SP1:
    case ORION_DEV_S2000_SP2:
    case ORION_DEV_C2000_SP4:
    case ORION_DEV_UO_4S:
        return DEV_CAT_RELAY_OUTPUT;

    case ORION_DEV_S2000_KDL:
    case ORION_DEV_S2000_KDL2I:
    case ORION_DEV_S2000_KDL2:
        return DEV_CAT_LOOP_CONTROLLER;

    case ORION_DEV_C2000_BKS:
    case ORION_DEV_C2000_BIOACCESS:
        return DEV_CAT_ACCESS_CONTROL;

    case ORION_DEV_C2000_GSM:
    case ORION_DEV_C2000_ETHERNET:
    case ORION_DEV_C2000_USB:
    case ORION_DEV_C2000_ASPRPT:
        return DEV_CAT_COMMUNICATOR;

    case ORION_DEV_C2000_BKI:
        return DEV_CAT_INDICATOR;

    case ORION_DEV_S2000_KPB:
    case ORION_DEV_C2000_PGE:
    case ORION_DEV_C2000_PF:
    case ORION_DEV_S2000_IT:
        return DEV_CAT_FIRE;

    case ORION_DEV_S2000_PP:
        return DEV_CAT_CONVERTER;

    default:
        return DEV_CAT_UNKNOWN;
    }
}

const char *orion_device_category_name(orion_device_category_t cat)
{
    switch (cat) {
    case DEV_CAT_PANEL:           return "Panel/Controller";
    case DEV_CAT_ZONE_EXPANDER:   return "Zone Expander";
    case DEV_CAT_RELAY_OUTPUT:    return "Relay Output";
    case DEV_CAT_LOOP_CONTROLLER: return "Loop Controller";
    case DEV_CAT_ACCESS_CONTROL:  return "Access Control";
    case DEV_CAT_COMMUNICATOR:    return "Communicator";
    case DEV_CAT_INDICATOR:       return "Indicator";
    case DEV_CAT_FIRE:            return "Fire System";
    case DEV_CAT_CONVERTER:       return "Protocol Converter";
    default:                      return "Unknown";
    }
}

int orion_device_zone_count(uint8_t type_code)
{
    switch (type_code) {
    case ORION_DEV_S2000_2:         return 2;
    case ORION_DEV_S2000_4:         return 4;
    case ORION_DEV_UO_4S:           return 4;
    case ORION_DEV_SIGNAL_10:       return 10;
    case ORION_DEV_SIGNAL_20:
    case ORION_DEV_SIGNAL_20P:
    case ORION_DEV_SIGNAL_20M:
    case ORION_DEV_SIGNAL_20M_V2:   return 20;
    case ORION_DEV_S2000_SP1:       return 1;
    case ORION_DEV_S2000_SP2:       return 2;
    case ORION_DEV_C2000_SP4:       return 4;
    case ORION_DEV_S2000_KPB:       return 6;
    default:                        return 0;
    }
}
