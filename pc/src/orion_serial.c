#include "orion_serial.h"
#include <string.h>

#ifdef _WIN32
/* ─── Windows implementation ─────────────────────────────────────────── */

orion_port_t orion_serial_open(const orion_serial_config_t *config)
{
    if (!config || !config->port_name) {
        return ORION_INVALID_PORT;
    }

    /* Build the full device path (e.g. "\\\\.\\COM3") */
    char full_path[64];
    snprintf(full_path, sizeof(full_path), "\\\\.\\%s", config->port_name);

    HANDLE handle = CreateFileA(
        full_path,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (handle == INVALID_HANDLE_VALUE) {
        return ORION_INVALID_PORT;
    }

    /* Configure the port */
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);

    if (!GetCommState(handle, &dcb)) {
        CloseHandle(handle);
        return ORION_INVALID_PORT;
    }

    uint32_t baud = config->baud_rate ? config->baud_rate : 9600;
    dcb.BaudRate = baud;
    dcb.ByteSize = 8;
    dcb.Parity   = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary  = TRUE;
    dcb.fParity  = FALSE;
    dcb.fOutxCtsFlow = FALSE;
    dcb.fOutxDsrFlow = FALSE;
    dcb.fDtrControl  = DTR_CONTROL_DISABLE;
    dcb.fRtsControl  = RTS_CONTROL_DISABLE;
    dcb.fOutX = FALSE;
    dcb.fInX  = FALSE;

    if (!SetCommState(handle, &dcb)) {
        CloseHandle(handle);
        return ORION_INVALID_PORT;
    }

    /* Set timeouts */
    COMMTIMEOUTS timeouts;
    memset(&timeouts, 0, sizeof(timeouts));
    uint32_t timeout = config->timeout_ms ? config->timeout_ms : 500;
    timeouts.ReadIntervalTimeout         = 50;
    timeouts.ReadTotalTimeoutConstant    = timeout;
    timeouts.ReadTotalTimeoutMultiplier  = 10;
    timeouts.WriteTotalTimeoutConstant   = timeout;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(handle, &timeouts)) {
        CloseHandle(handle);
        return ORION_INVALID_PORT;
    }

    /* Clear any stale data */
    PurgeComm(handle, PURGE_RXCLEAR | PURGE_TXCLEAR);

    return handle;
}

void orion_serial_close(orion_port_t port)
{
    if (port != ORION_INVALID_PORT) {
        CloseHandle(port);
    }
}

int orion_serial_write(orion_port_t port, const uint8_t *data, size_t length)
{
    if (port == ORION_INVALID_PORT || !data) return -1;

    DWORD written = 0;
    if (!WriteFile(port, data, (DWORD)length, &written, NULL)) {
        return -1;
    }
    return (int)written;
}

int orion_serial_read(orion_port_t port, uint8_t *buffer, size_t max_length)
{
    if (port == ORION_INVALID_PORT || !buffer) return -1;

    DWORD read_count = 0;
    if (!ReadFile(port, buffer, (DWORD)max_length, &read_count, NULL)) {
        return -1;
    }
    return (int)read_count;
}

void orion_serial_flush(orion_port_t port)
{
    if (port != ORION_INVALID_PORT) {
        PurgeComm(port, PURGE_RXCLEAR | PURGE_TXCLEAR);
    }
}

#else
/* ─── POSIX (Linux/macOS) implementation ─────────────────────────────── */

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <errno.h>

static speed_t baud_to_speed(uint32_t baud)
{
    switch (baud) {
        case 9600:   return B9600;
        case 19200:  return B19200;
        case 38400:  return B38400;
        case 57600:  return B57600;
        case 115200: return B115200;
        default:     return B9600;
    }
}

orion_port_t orion_serial_open(const orion_serial_config_t *config)
{
    if (!config || !config->port_name) {
        return ORION_INVALID_PORT;
    }

    int fd = open(config->port_name, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd < 0) {
        return ORION_INVALID_PORT;
    }

    /* Set blocking mode */
    fcntl(fd, F_SETFL, 0);

    struct termios tty;
    memset(&tty, 0, sizeof(tty));

    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return ORION_INVALID_PORT;
    }

    uint32_t baud = config->baud_rate ? config->baud_rate : 9600;
    speed_t speed = baud_to_speed(baud);
    cfsetispeed(&tty, speed);
    cfsetospeed(&tty, speed);

    /* 8N1 */
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;

    /* No flow control */
    tty.c_cflag &= ~CRTSCTS;
    tty.c_cflag |= CREAD | CLOCAL;

    /* Raw mode */
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~OPOST;

    /* Read timeout */
    uint32_t timeout_ds = (config->timeout_ms ? config->timeout_ms : 500) / 100;
    if (timeout_ds == 0) timeout_ds = 1;
    tty.c_cc[VMIN]  = 0;
    tty.c_cc[VTIME] = (cc_t)timeout_ds;

    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return ORION_INVALID_PORT;
    }

    tcflush(fd, TCIOFLUSH);
    return fd;
}

void orion_serial_close(orion_port_t port)
{
    if (port != ORION_INVALID_PORT) {
        close(port);
    }
}

int orion_serial_write(orion_port_t port, const uint8_t *data, size_t length)
{
    if (port == ORION_INVALID_PORT || !data) return -1;
    ssize_t n = write(port, data, length);
    return (n < 0) ? -1 : (int)n;
}

int orion_serial_read(orion_port_t port, uint8_t *buffer, size_t max_length)
{
    if (port == ORION_INVALID_PORT || !buffer) return -1;
    ssize_t n = read(port, buffer, max_length);
    return (n < 0) ? -1 : (int)n;
}

void orion_serial_flush(orion_port_t port)
{
    if (port != ORION_INVALID_PORT) {
        tcflush(port, TCIOFLUSH);
    }
}

#endif /* _WIN32 */
