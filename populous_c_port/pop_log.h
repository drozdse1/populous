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
