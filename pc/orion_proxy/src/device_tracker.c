#include "device_tracker.h"
#include "orion_crc.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* ─── Lock helpers ─────────────────────────────────────────────────────── */

static void tracker_lock(device_tracker_t *t)
{
#ifdef _WIN32
    EnterCriticalSection(&t->lock);
#else
    while (__sync_lock_test_and_set(&t->lock_flag, 1)) { /* spin */ }
#endif
}

static void tracker_unlock(device_tracker_t *t)
{
#ifdef _WIN32
    LeaveCriticalSection(&t->lock);
#else
    __sync_lock_release(&t->lock_flag);
#endif
}

/* ─── Init / Destroy ───────────────────────────────────────────────────── */

void tracker_init(device_tracker_t *t)
{
    int i;
    memset(t, 0, sizeof(*t));
    t->start_time = time(NULL);

    for (i = 0; i < TRACKER_MAX_DEVICES; ++i) {
        t->devices[i].address = (uint8_t)i;
        t->devices[i].online = 0;
        t->devices[i].last_seen = 0;
        t->devices[i].packet_count = 0;
    }

#ifdef _WIN32
    InitializeCriticalSection(&t->lock);
#else
    t->lock_flag = 0;
#endif
}

void tracker_destroy(device_tracker_t *t)
{
#ifdef _WIN32
    DeleteCriticalSection(&t->lock);
#endif
}

/* ─── Packet processing ────────────────────────────────────────────────── */

void tracker_on_packet(device_tracker_t *t, const orion_packet_t *pkt,
                       const uint8_t *raw, size_t raw_len)
{
    uint8_t addr;
    tracked_device_t *dev;
    tracked_event_t *evt;
    size_t copy_len;

    addr = pkt->address;
    if (pkt->is_encrypted && addr >= 0x80) {
        addr = addr - 0x80;
    }

    if (addr == 0 || addr > ORION_MAX_ADDRESS) {
        return;
    }

    tracker_lock(t);

    t->total_packets++;
    dev = &t->devices[addr];
    dev->last_seen = time(NULL);
    dev->packet_count++;

    if (!dev->online) {
        dev->online = 1;
        /* Record as event: device came online */
        evt = &t->events[t->event_write_pos % TRACKER_MAX_EVENTS];
        evt->id = t->event_count++;
        evt->timestamp = time(NULL);
        evt->device_addr = addr;
        evt->event_code = 0xD1; /* ORION_STATUS_CONNECTION_OK */
        evt->zone = 0;
        copy_len = raw_len < sizeof(evt->raw_payload) ? raw_len : sizeof(evt->raw_payload);
        memcpy(evt->raw_payload, raw, copy_len);
        evt->raw_len = copy_len;
        t->event_write_pos++;
    }

    /* Try to extract status info from payload if long enough */
    if (pkt->length >= 8) {
        dev->last_status1 = pkt->payload[5];
        dev->last_status2 = pkt->payload[6];
    }

    tracker_unlock(t);
}

void tracker_on_crc_error(device_tracker_t *t)
{
    tracker_lock(t);
    t->total_crc_errors++;
    tracker_unlock(t);
}

/* ─── Timeout check ────────────────────────────────────────────────────── */

void tracker_check_timeouts(device_tracker_t *t)
{
    int i;
    time_t now;
    tracked_device_t *dev;
    tracked_event_t *evt;

    now = time(NULL);

    tracker_lock(t);

    for (i = 1; i <= ORION_MAX_ADDRESS; ++i) {
        dev = &t->devices[i];
        if (dev->online && dev->last_seen > 0) {
            if ((now - dev->last_seen) > TRACKER_OFFLINE_SEC) {
                dev->online = 0;
                /* Record as event: device went offline */
                evt = &t->events[t->event_write_pos % TRACKER_MAX_EVENTS];
                evt->id = t->event_count++;
                evt->timestamp = now;
                evt->device_addr = (uint8_t)i;
                evt->event_code = 0xD0; /* ORION_STATUS_CONNECTION_LOST */
                evt->zone = 0;
                evt->raw_len = 0;
                t->event_write_pos++;
            }
        }
    }

    tracker_unlock(t);
}

/* ─── Query functions ──────────────────────────────────────────────────── */

int tracker_online_count(device_tracker_t *t)
{
    int i, count;

    count = 0;
    tracker_lock(t);
    for (i = 1; i <= ORION_MAX_ADDRESS; ++i) {
        if (t->devices[i].online) {
            count++;
        }
    }
    tracker_unlock(t);
    return count;
}

/* ─── Formatting ───────────────────────────────────────────────────────── */

int tracker_format_status(device_tracker_t *t, char *buf, size_t buf_size)
{
    int n;
    time_t now;
    unsigned long uptime;
    int online;
    double err_rate;

    now = time(NULL);
    tracker_lock(t);
    uptime = (unsigned long)(now - t->start_time);
    online = tracker_online_count(t);

    if (t->total_packets > 0) {
        err_rate = (double)t->total_crc_errors * 100.0 / (double)(t->total_packets + t->total_crc_errors);
    } else {
        err_rate = 0.0;
    }

    n = snprintf(buf, buf_size,
        "STATUS,uptime=%lu,packets=%u,crc_errors=%u,error_rate=%.2f%%,"
        "devices_online=%d,events=%u\n",
        uptime, t->total_packets, t->total_crc_errors, err_rate,
        online, t->event_count);

    tracker_unlock(t);
    return n;
}

int tracker_format_devices(device_tracker_t *t, char *buf, size_t buf_size)
{
    int i, n, total;
    tracked_device_t *dev;

    total = 0;
    tracker_lock(t);

    for (i = 1; i <= ORION_MAX_ADDRESS; ++i) {
        dev = &t->devices[i];
        if (dev->packet_count == 0) {
            continue; /* Never seen on bus */
        }

        n = snprintf(buf + total, buf_size - total,
            "DEVICE,%d,%s,type=0x%02X,status1=0x%02X,status2=0x%02X,"
            "packets=%u,last_seen=%ld\n",
            dev->address,
            dev->online ? "ONLINE" : "OFFLINE",
            dev->device_type,
            dev->last_status1,
            dev->last_status2,
            dev->packet_count,
            (long)dev->last_seen);

        if (n > 0 && (size_t)(total + n) < buf_size) {
            total += n;
        } else {
            break;
        }
    }

    tracker_unlock(t);
    return total;
}

int tracker_format_device(device_tracker_t *t, uint8_t address,
                          char *buf, size_t buf_size)
{
    int n;
    tracked_device_t *dev;

    if (address == 0 || address > ORION_MAX_ADDRESS) {
        return 0;
    }

    tracker_lock(t);
    dev = &t->devices[address];

    if (dev->packet_count == 0) {
        tracker_unlock(t);
        return snprintf(buf, buf_size, "ERROR,device %d not found\n", address);
    }

    n = snprintf(buf, buf_size,
        "DEVICE,%d,%s,type=0x%02X,fw=%d.%d,"
        "status1=0x%02X(%s),status2=0x%02X(%s),"
        "packets=%u,last_seen=%ld\n",
        dev->address,
        dev->online ? "ONLINE" : "OFFLINE",
        dev->device_type,
        dev->fw_major, dev->fw_minor,
        dev->last_status1, orion_status_str(dev->last_status1),
        dev->last_status2, orion_status_str(dev->last_status2),
        dev->packet_count,
        (long)dev->last_seen);

    tracker_unlock(t);
    return n;
}

int tracker_format_events(device_tracker_t *t, char *buf, size_t buf_size,
                          int max_events)
{
    int total, n, count, i;
    uint32_t start_pos;
    tracked_event_t *evt;

    total = 0;
    tracker_lock(t);

    /* Calculate starting position for recent events */
    count = (int)t->event_write_pos;
    if (count > TRACKER_MAX_EVENTS) count = TRACKER_MAX_EVENTS;
    if (count > max_events) count = max_events;

    if (count == 0) {
        tracker_unlock(t);
        return snprintf(buf, buf_size, "EVENTS,count=0\n");
    }

    start_pos = t->event_write_pos - count;

    for (i = 0; i < count; ++i) {
        evt = &t->events[(start_pos + i) % TRACKER_MAX_EVENTS];

        n = snprintf(buf + total, buf_size - total,
            "EVENT,%u,dev=%d,code=0x%02X(%s),zone=%d,time=%ld\n",
            evt->id,
            evt->device_addr,
            evt->event_code, orion_status_str(evt->event_code),
            evt->zone,
            (long)evt->timestamp);

        if (n > 0 && (size_t)(total + n) < buf_size) {
            total += n;
        } else {
            break;
        }
    }

    tracker_unlock(t);
    return total;
}
