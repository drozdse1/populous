#ifndef SERIAL_LINK_H
#define SERIAL_LINK_H

#include <stddef.h>
#include <stdint.h>

/* Byte channel behind the emulated Amiga serial port (SERDAT/SERDATR,
 * _r_ser_buff/_w_ser_buff asm:20058-20076). The PRG only sees an ordered
 * byte stream; the backend is a non-blocking TCP socket or, for tests, an
 * in-memory pair. Nothing here blocks: serial_link_poll() advances a
 * pending connect/accept, flushes queued output and fills the input queue.
 *
 * Like an unplugged cable, bytes written while not connected are dropped. */

#define SERIAL_LINK_DEFAULT_PORT 2611

typedef enum {
    SERIAL_LINK_CONNECTING = 0,   /* listening, or connect in progress / retrying */
    SERIAL_LINK_OPEN = 1,
    SERIAL_LINK_CLOSED = 2        /* peer gone, error, or closed locally */
} SerialLinkState;

typedef struct SerialLink SerialLink;

/* In-memory pair: what one writes the other reads. */
int serial_link_mem_pair(SerialLink **a, SerialLink **b);
/* TCP host: listen on port (0 = any free port), accept one peer. */
SerialLink *serial_link_tcp_host(uint16_t port, char *error, size_t error_size);
/* TCP join: connect to host:port, retrying until the first connection. */
SerialLink *serial_link_tcp_join(const char *host, uint16_t port, char *error, size_t error_size);

SerialLinkState serial_link_poll(SerialLink *link);
SerialLinkState serial_link_state(const SerialLink *link);
/* Queue bytes; returns count accepted (0 when not open). */
size_t serial_link_write(SerialLink *link, const void *data, size_t size);
/* Take up to size queued input bytes; returns count. */
size_t serial_link_read(SerialLink *link, void *data, size_t size);
size_t serial_link_available(const SerialLink *link);
/* Bound port of a TCP host (after creation), else 0. */
uint16_t serial_link_port(const SerialLink *link);
/* Human-readable description ("HOST :2611", "JOIN 1.2.3.4:2611", "MEMORY"). */
const char *serial_link_describe(const SerialLink *link);
/* Close the backend; the peer sees SERIAL_LINK_CLOSED. Frees the link. */
void serial_link_close(SerialLink *link);

/* "ip", "ip:port", "[v6]:port" -> host and port (default if absent).
 * Returns 1 on success. */
int serial_link_parse_endpoint(const char *text, char *host, size_t host_size, uint16_t *port);

#endif
