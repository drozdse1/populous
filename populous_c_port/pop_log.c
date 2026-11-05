#include "pop_log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static PopLogLevel g_level = POP_LOG_INFO;
static int g_timestamps;
static int g_ready;

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
        fprintf(out, "%02d:%02d:%02d ", tm_now.tm_hour, tm_now.tm_min, tm_now.tm_sec);
    }

    fprintf(out, "%-5s ", level_name(level));
    if (tag && *tag) fprintf(out, "[%s] ", tag);
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
