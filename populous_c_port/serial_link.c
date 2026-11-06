/* serial_link.c - the byte channel that replaces the Amiga serial cable.
 * See serial_link.h. Non-blocking POSIX TCP plus an in-memory test pair. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "serial_link.h"

#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#define SERIAL_RETRY_MS 500

typedef struct {
    uint8_t *bytes;
    size_t head, size, capacity;
} ByteQueue;

enum { LINK_MEMORY, LINK_TCP_HOST, LINK_TCP_JOIN };

struct SerialLink {
    int kind;
    SerialLinkState state;
    ByteQueue rx, tx;
    SerialLink *peer;             /* memory pair */
    int fd, listen_fd;            /* tcp */
    int ever_open;
    uint64_t retry_at;
    struct sockaddr_storage address;
    socklen_t address_size;
    uint16_t port;
    char description[96];
};

static int queue_push(ByteQueue *queue, const void *data, size_t size)
{
    if (!size) return 1;
    if (queue->head && queue->head == queue->size) queue->head = queue->size = 0;
    if (queue->size + size > queue->capacity) {
        if (queue->head) {
            memmove(queue->bytes, queue->bytes + queue->head, queue->size - queue->head);
            queue->size -= queue->head; queue->head = 0;
        }
        if (queue->size + size > queue->capacity) {
            size_t capacity = queue->capacity ? queue->capacity : 256;
            while (capacity < queue->size + size) capacity *= 2;
            uint8_t *grown = realloc(queue->bytes, capacity);
            if (!grown) return 0;
            queue->bytes = grown; queue->capacity = capacity;
        }
    }
    memcpy(queue->bytes + queue->size, data, size);
    queue->size += size;
    return 1;
}
static size_t queue_pop(ByteQueue *queue, void *data, size_t size)
{
    size_t available = queue->size - queue->head;
    if (size > available) size = available;
    if (data && size) memcpy(data, queue->bytes + queue->head, size);
    queue->head += size;
    if (queue->head == queue->size) queue->head = queue->size = 0;
    return size;
}

static uint64_t now_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000u + (uint64_t)now.tv_nsec / 1000000u;
}

static SerialLink *link_new(int kind)
{
    SerialLink *link = calloc(1, sizeof *link);
    if (!link) return NULL;
    link->kind = kind;
    link->fd = link->listen_fd = -1;
    return link;
}

int serial_link_mem_pair(SerialLink **a, SerialLink **b)
{
    SerialLink *left = link_new(LINK_MEMORY), *right = link_new(LINK_MEMORY);
    if (!left || !right) { free(left); free(right); return 0; }
    left->peer = right; right->peer = left;
    left->state = right->state = SERIAL_LINK_OPEN;
    left->ever_open = right->ever_open = 1;
    snprintf(left->description, sizeof left->description, "MEMORY");
    snprintf(right->description, sizeof right->description, "MEMORY");
    *a = left; *b = right;
    return 1;
}

static void set_error(char *error, size_t size, const char *what, int code)
{
    if (error && size) snprintf(error, size, "%s: %s", what, code ? strerror(code) : "failed");
}
static int make_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}
static void tune_socket(int fd)
{
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
#ifdef SO_NOSIGPIPE
    setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
}

SerialLink *serial_link_tcp_host(uint16_t port, char *error, size_t error_size)
{
    SerialLink *link = link_new(LINK_TCP_HOST);
    if (!link) { set_error(error, error_size, "host", ENOMEM); return NULL; }
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { set_error(error, error_size, "socket", errno); free(link); return NULL; }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in address;
    memset(&address, 0, sizeof address);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    if (bind(fd, (struct sockaddr *)&address, sizeof address) || listen(fd, 1) || !make_nonblocking(fd)) {
        set_error(error, error_size, "listen", errno);
        close(fd); free(link); return NULL;
    }
    socklen_t size = sizeof address;
    if (!getsockname(fd, (struct sockaddr *)&address, &size)) port = ntohs(address.sin_port);
    link->listen_fd = fd; link->port = port; link->state = SERIAL_LINK_CONNECTING;
    snprintf(link->description, sizeof link->description, "HOST :%u", (unsigned)port);
    return link;
}

SerialLink *serial_link_tcp_join(const char *host, uint16_t port, char *error, size_t error_size)
{
    SerialLink *link = link_new(LINK_TCP_JOIN);
    if (!link) { set_error(error, error_size, "join", ENOMEM); return NULL; }
    struct addrinfo hints, *found = NULL;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    char service[8];
    snprintf(service, sizeof service, "%u", (unsigned)port);
    int status = getaddrinfo(host, service, &hints, &found);
    if (status || !found || found->ai_addrlen > sizeof link->address) {
        if (error && error_size) snprintf(error, error_size, "resolve %s: %s", host, status ? gai_strerror(status) : "no address");
        if (found) freeaddrinfo(found);
        free(link); return NULL;
    }
    memcpy(&link->address, found->ai_addr, found->ai_addrlen);
    link->address_size = (socklen_t)found->ai_addrlen;
    freeaddrinfo(found);
    link->port = port; link->state = SERIAL_LINK_CONNECTING;
    snprintf(link->description, sizeof link->description, "JOIN %s:%u", host, (unsigned)port);
    return link;
}

static void tcp_drop(SerialLink *link)
{
    if (link->fd >= 0) close(link->fd);
    if (link->listen_fd >= 0) close(link->listen_fd);
    link->fd = link->listen_fd = -1;
    link->state = SERIAL_LINK_CLOSED;
    link->tx.head = link->tx.size = 0;
}

static void tcp_opened(SerialLink *link)
{
    tune_socket(link->fd);
    link->state = SERIAL_LINK_OPEN;
    link->ever_open = 1;
}

static void tcp_connect_step(SerialLink *link)
{
    if (link->fd < 0) {
        if (now_ms() < link->retry_at) return;
        link->fd = socket(link->address.ss_family, SOCK_STREAM, 0);
        if (link->fd < 0 || !make_nonblocking(link->fd)) { tcp_drop(link); return; }
        if (!connect(link->fd, (struct sockaddr *)&link->address, link->address_size)) { tcp_opened(link); return; }
        if (errno != EINPROGRESS) goto retry;
    }
    struct pollfd wait = {link->fd, POLLOUT, 0};
    if (poll(&wait, 1, 0) <= 0) return;
    int code = 0; socklen_t size = sizeof code;
    if (!getsockopt(link->fd, SOL_SOCKET, SO_ERROR, &code, &size) && !code) { tcp_opened(link); return; }
retry:
    /* Not listening yet: try again until the first connection is made. */
    close(link->fd); link->fd = -1;
    link->retry_at = now_ms() + SERIAL_RETRY_MS;
}

static void tcp_transfer(SerialLink *link)
{
    while (link->tx.size > link->tx.head) {
        ssize_t sent = send(link->fd, link->tx.bytes + link->tx.head, link->tx.size - link->tx.head, MSG_NOSIGNAL);
        if (sent > 0) { queue_pop(&link->tx, NULL, (size_t)sent); continue; }
        if (sent < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) break;
        tcp_drop(link); return;
    }
    uint8_t buffer[1024];
    for (;;) {
        ssize_t got = recv(link->fd, buffer, sizeof buffer, 0);
        if (got > 0) { if (!queue_push(&link->rx, buffer, (size_t)got)) { tcp_drop(link); return; } continue; }
        if (got < 0 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) return;
        tcp_drop(link); return;   /* 0: orderly shutdown by the peer */
    }
}

SerialLinkState serial_link_poll(SerialLink *link)
{
    if (!link) return SERIAL_LINK_CLOSED;
    if (link->kind == LINK_MEMORY || link->state == SERIAL_LINK_CLOSED) return link->state;
    if (link->state == SERIAL_LINK_CONNECTING) {
        if (link->kind == LINK_TCP_HOST) {
            int fd = accept(link->listen_fd, NULL, NULL);
            if (fd >= 0) {
                if (!make_nonblocking(fd)) { close(fd); return link->state; }
                close(link->listen_fd); link->listen_fd = -1;
                link->fd = fd; tcp_opened(link);
            }
        } else tcp_connect_step(link);
    }
    if (link->state == SERIAL_LINK_OPEN) tcp_transfer(link);
    return link->state;
}

SerialLinkState serial_link_state(const SerialLink *link) { return link ? link->state : SERIAL_LINK_CLOSED; }

size_t serial_link_write(SerialLink *link, const void *data, size_t size)
{
    if (!link || link->state != SERIAL_LINK_OPEN) return 0;
    if (link->kind == LINK_MEMORY) {
        if (!link->peer || !queue_push(&link->peer->rx, data, size)) return 0;
        return size;
    }
    if (!queue_push(&link->tx, data, size)) return 0;
    tcp_transfer(link);
    return size;
}

size_t serial_link_read(SerialLink *link, void *data, size_t size)
{
    return link ? queue_pop(&link->rx, data, size) : 0;
}

size_t serial_link_available(const SerialLink *link)
{
    return link ? link->rx.size - link->rx.head : 0;
}

uint16_t serial_link_port(const SerialLink *link) { return link ? link->port : 0; }
const char *serial_link_describe(const SerialLink *link) { return link ? link->description : "NONE"; }

void serial_link_close(SerialLink *link)
{
    if (!link) return;
    if (link->kind == LINK_MEMORY) {
        if (link->peer) { link->peer->peer = NULL; link->peer->state = SERIAL_LINK_CLOSED; }
    } else {
        if (link->state == SERIAL_LINK_OPEN) tcp_transfer(link);   /* best-effort flush */
        tcp_drop(link);
    }
    free(link->rx.bytes); free(link->tx.bytes);
    free(link);
}

int serial_link_parse_endpoint(const char *text, char *host, size_t host_size, uint16_t *port)
{
    if (!text || !*text || !host || !host_size || !port) return 0;
    const char *end, *colon = NULL;
    if (*text == '[') {
        end = strchr(text, ']');
        if (!end || end == text + 1) return 0;
        if (end[1] == ':') colon = end + 1;
        else if (end[1]) return 0;
        ++text;
    } else {
        colon = strrchr(text, ':');
        if (colon && strchr(text, ':') != colon) colon = NULL;   /* bare IPv6 */
        end = colon ? colon : text + strlen(text);
    }
    size_t length = (size_t)(end - text);
    if (!length || length >= host_size) return 0;
    memcpy(host, text, length); host[length] = 0;
    *port = SERIAL_LINK_DEFAULT_PORT;
    if (colon) {
        const char *digits = colon + 1;
        if (!*digits) return 0;
        unsigned long value = 0;
        for (const char *digit = digits; *digit; ++digit) {
            if (*digit < '0' || *digit > '9') return 0;
            value = value * 10 + (unsigned long)(*digit - '0');
            if (value > 65535) return 0;
        }
        if (!value) return 0;
        *port = (uint16_t)value;
    }
    return 1;
}
