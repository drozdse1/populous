#ifndef POPULOUS_POP_LOG_H
#define POPULOUS_POP_LOG_H

/*
 * Lightweight leveled logger for the Populous C port (Linux + macOS).
 *
 * Levels: TRACE < DEBUG < INFO < WARN < ERROR < OFF
 *
 * Control (first match wins at init):
 *   - pop_log_set_level() / pop_log_set_level_by_name()
 *   - env POPULOUS_LOG_LEVEL = trace|debug|info|warn|error|off  (also: 0..5)
 *   - env POPULOUS_LOG_TIMESTAMPS=1  → prefix local time HH:MM:SS
 *
 *   - env POPULOUS_LOG_COLOR=0|1   → force ANSI colors off/on (default: auto
 *     via isatty(); NO_COLOR or TERM=dumb disables them)
 *
 * Header-only (stb style): in exactly ONE translation unit write
 *     #define POP_LOG_IMPLEMENTATION
 *     #include "pop_log.h"
 * All other files just include the header.
 *
 * Default level is INFO so the old [PRG]/[SDL] noise stays quiet unless
 * DEBUG/TRACE is requested. WARN/ERROR always go to stderr; others to stdout.
 */

#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum PopLogLevel {
    POP_LOG_TRACE = 0,
    POP_LOG_DEBUG = 1,
    POP_LOG_INFO  = 2,
    POP_LOG_WARN  = 3,
    POP_LOG_ERROR = 4,
    POP_LOG_OFF   = 5
} PopLogLevel;

void pop_log_init(void);
void pop_log_set_level(PopLogLevel level);
int  pop_log_set_level_by_name(const char *name); /* 0 on success */
PopLogLevel pop_log_get_level(void);
void pop_log_set_timestamps(int enabled);
void pop_log_set_color(int color_mode); /* -1 auto, 0 off, 1 on */

void pop_log_write(PopLogLevel level, const char *tag, const char *fmt, ...);
void pop_log_vwrite(PopLogLevel level, const char *tag, const char *fmt, va_list ap);

#define POP_LOG_TRACE(...) pop_log_write(POP_LOG_TRACE, NULL, __VA_ARGS__)
#define POP_LOG_DEBUG(...) pop_log_write(POP_LOG_DEBUG, NULL, __VA_ARGS__)
#define POP_LOG_INFO(...)  pop_log_write(POP_LOG_INFO,  NULL, __VA_ARGS__)
#define POP_LOG_WARN(...)  pop_log_write(POP_LOG_WARN,  NULL, __VA_ARGS__)
#define POP_LOG_ERROR(...) pop_log_write(POP_LOG_ERROR, NULL, __VA_ARGS__)

#define POP_LOGT(tag, ...) pop_log_write(POP_LOG_TRACE, (tag), __VA_ARGS__)
#define POP_LOGD(tag, ...) pop_log_write(POP_LOG_DEBUG, (tag), __VA_ARGS__)
#define POP_LOGI(tag, ...) pop_log_write(POP_LOG_INFO,  (tag), __VA_ARGS__)
#define POP_LOGW(tag, ...) pop_log_write(POP_LOG_WARN,  (tag), __VA_ARGS__)
#define POP_LOGE(tag, ...) pop_log_write(POP_LOG_ERROR, (tag), __VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif /* POPULOUS_POP_LOG_H */

/* ------------------------------------------------------------------------ */
/* Implementation                                                            */
/* ------------------------------------------------------------------------ */
#if defined(POP_LOG_IMPLEMENTATION) && !defined(POP_LOG_IMPLEMENTATION_DONE)
#define POP_LOG_IMPLEMENTATION_DONE

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif

static PopLogLevel g_level = POP_LOG_INFO;
static int g_timestamps;
static int g_ready;
static int g_color = -1; /* -1 auto (isatty), 0 off, 1 on */

static PopLogLevel parse_level(const char *name)
{
    char buf[16];
    size_t n = 0;
    if (!name || !*name) return POP_LOG_INFO;
    while (name[n] && n + 1 < sizeof buf) {
        buf[n] = (char)tolower((unsigned char)name[n]);
        ++n;
    }
    buf[n] = 0;
    if (!strcmp(buf, "trace") || !strcmp(buf, "0")) return POP_LOG_TRACE;
    if (!strcmp(buf, "debug") || !strcmp(buf, "1")) return POP_LOG_DEBUG;
    if (!strcmp(buf, "info")  || !strcmp(buf, "2")) return POP_LOG_INFO;
    if (!strcmp(buf, "warn")  || !strcmp(buf, "warning") || !strcmp(buf, "3")) return POP_LOG_WARN;
    if (!strcmp(buf, "error") || !strcmp(buf, "4")) return POP_LOG_ERROR;
    if (!strcmp(buf, "off")   || !strcmp(buf, "quiet") || !strcmp(buf, "5")) return POP_LOG_OFF;
    return POP_LOG_INFO;
}

void pop_log_init(void)
{
    const char *env;
    if (g_ready) return;
    g_ready = 1;
    env = getenv("POPULOUS_LOG_LEVEL");
    if (env && *env) g_level = parse_level(env);
    env = getenv("POPULOUS_LOG_TIMESTAMPS");
    if (env && (*env == '1' || *env == 'y' || *env == 'Y' || *env == 't' || *env == 'T'))
        g_timestamps = 1;
    env = getenv("POPULOUS_LOG_COLOR");
    if (env && *env) {
        if (*env == '0' || *env == 'n' || *env == 'N' || *env == 'f' || *env == 'F') g_color = 0;
        else if (*env == '1' || *env == 'y' || *env == 'Y' || *env == 't' || *env == 'T') g_color = 1;
    }
    if (getenv("NO_COLOR")) g_color = 0;
}

void pop_log_set_color(int color_mode)
{
    pop_log_init();
    g_color = color_mode < 0 ? -1 : (color_mode ? 1 : 0);
}

static int pop_log_use_color(FILE *out)
{
    if (g_color >= 0) return g_color;
#if defined(_WIN32)
    (void)out;
    return 0;
#else
    {
        const char *term = getenv("TERM");
        if (term && !strcmp(term, "dumb")) return 0;
    }
    return isatty(fileno(out));
#endif
}

static const char *level_color(PopLogLevel level)
{
    switch (level) {
    case POP_LOG_TRACE: return "\x1b[90m";   /* grey    */
    case POP_LOG_DEBUG: return "\x1b[34m";   /* blue    */
    case POP_LOG_INFO:  return "\x1b[32m";   /* green   */
    case POP_LOG_WARN:  return "\x1b[33;1m"; /* yellow  */
    case POP_LOG_ERROR: return "\x1b[31;1m"; /* red     */
    default:            return "";
    }
}

void pop_log_set_level(PopLogLevel level)
{
    pop_log_init();
    if (level < POP_LOG_TRACE) level = POP_LOG_TRACE;
    if (level > POP_LOG_OFF) level = POP_LOG_OFF;
    g_level = level;
}

int pop_log_set_level_by_name(const char *name)
{
    PopLogLevel level;
    if (!name || !*name) return -1;
    level = parse_level(name);
    /* Reject unknown tokens: parse_level falls back to INFO, detect via strcmp. */
    {
        char buf[16];
        size_t n = 0;
        while (name[n] && n + 1 < sizeof buf) {
            buf[n] = (char)tolower((unsigned char)name[n]);
            ++n;
        }
        buf[n] = 0;
        if (strcmp(buf, "trace") && strcmp(buf, "debug") && strcmp(buf, "info") &&
            strcmp(buf, "warn") && strcmp(buf, "warning") && strcmp(buf, "error") &&
            strcmp(buf, "off") && strcmp(buf, "quiet") &&
            strcmp(buf, "0") && strcmp(buf, "1") && strcmp(buf, "2") &&
            strcmp(buf, "3") && strcmp(buf, "4") && strcmp(buf, "5"))
            return -1;
    }
    pop_log_set_level(level);
    return 0;
}

PopLogLevel pop_log_get_level(void)
{
    pop_log_init();
    return g_level;
}

void pop_log_set_timestamps(int enabled)
{
    pop_log_init();
    g_timestamps = enabled ? 1 : 0;
}

static const char *level_name(PopLogLevel level)
{
    switch (level) {
    case POP_LOG_TRACE: return "TRACE";
    case POP_LOG_DEBUG: return "DEBUG";
    case POP_LOG_INFO:  return "INFO";
    case POP_LOG_WARN:  return "WARN";
    case POP_LOG_ERROR: return "ERROR";
    default:            return "OFF";
    }
}

void pop_log_vwrite(PopLogLevel level, const char *tag, const char *fmt, va_list ap)
{
    FILE *out;
    pop_log_init();
    if (level < POP_LOG_TRACE || level >= POP_LOG_OFF) return;
    if (level < g_level) return;
    if (!fmt) return;

    out = (level >= POP_LOG_WARN) ? stderr : stdout;

    if (g_timestamps) {
        time_t now = time(NULL);
        struct tm tm_now;
#if defined(_WIN32)
        localtime_s(&tm_now, &now);
#else
        localtime_r(&now, &tm_now);
#endif
        if (pop_log_use_color(out))
            fprintf(out, "\x1b[2m%02d:%02d:%02d\x1b[0m ", tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
        else
            fprintf(out, "%02d:%02d:%02d ", tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
    }

    {
        int color = pop_log_use_color(out);
        if (color) fprintf(out, "%s%-5s\x1b[0m ", level_color(level), level_name(level));
        else       fprintf(out, "%-5s ", level_name(level));
        if (tag && *tag) {
            if (color) fprintf(out, "\x1b[36m[%s]\x1b[0m ", tag);
            else       fprintf(out, "[%s] ", tag);
        }
    }
    vfprintf(out, fmt, ap);
    {
        size_t len = strlen(fmt);
        if (len == 0 || fmt[len - 1] != '\n') fputc('\n', out);
    }
    fflush(out);
}

void pop_log_write(PopLogLevel level, const char *tag, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    pop_log_vwrite(level, tag, fmt, ap);
    va_end(ap);
}

#endif /* POP_LOG_IMPLEMENTATION */

