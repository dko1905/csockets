#ifndef UTIL_LOGGING_H
#define UTIL_LOGGING_H

#include <stdio.h>

/* if you want to change output */
/* printf will be used for DEBUG & INFO. */
#ifndef log_printf
#define log_printf(...) printf(__VA_ARGS__)
#endif
/* eprintf will be used for WARN & ERROR. */
#ifndef log_eprintf
#define log_eprintf(...) fprintf(stderr, __VA_ARGS__)
#endif
/* if enabled */
#ifndef LOG_DEBUG
#define LOG_DEBUG (0)
#endif
#ifndef LOG_INFO
#define LOG_INFO (0)
#endif
#ifndef LOG_WARN
#define LOG_WARN (0)
#endif
#ifndef LOG_ERROR
#define LOG_ERROR (0)
#endif
/* prefixes, here the colons are aligned. */
#ifndef LOG_DEBUG_PREFIX
#define LOG_DEBUG_PREFIX "DEBUG: "
#endif
#ifndef LOG_INFO_PREFIX
#define LOG_INFO_PREFIX  "INFO : "
#endif
#ifndef LOG_WARN_PREFIX
#define LOG_WARN_PREFIX  "WARN : "
#endif
#ifndef LOG_ERROR_PREFIX
#define LOG_ERROR_PREFIX "ERROR: "
#endif
/* if using write mutex */
#ifndef LOG_WRITE_MUTEX
#define LOG_WRITE_MUTEX 1
#endif
#if LOG_WRITE_MUTEX == 1
#include <pthread.h>
pthread_mutex_t _log_write_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* log macro definitions */
#if LOG_DEBUG == 1
#define log_debug(...) {\
	pthread_mutex_lock(&_log_write_mutex);\
	log_printf(LOG_DEBUG_PREFIX);\
	log_printf(__VA_ARGS__);\
	log_printf("\n");\
	pthread_mutex_unlock(&_log_write_mutex);\
}
#else
#define log_debug(...)
#endif
#if LOG_INFO == 1
#define log_info(...) {\
	pthread_mutex_lock(&_log_write_mutex);\
	log_printf(LOG_INFO_PREFIX);\
	log_printf(__VA_ARGS__);\
	log_printf("\n");\
	pthread_mutex_unlock(&_log_write_mutex);\
}
#else
#define log_info(...)
#endif
#if LOG_WARN == 1
#define log_warn(...) {\
	pthread_mutex_lock(&_log_write_mutex);\
	log_eprintf(LOG_WARN_PREFIX);\
	log_eprintf(__VA_ARGS__);\
	log_eprintf("\n");\
	pthread_mutex_unlock(&_log_write_mutex);\
}
#else
#define log_warn(...)
#endif
#if LOG_ERROR == 1
#define log_error(...) {\
	pthread_mutex_lock(&_log_write_mutex);\
	log_eprintf(LOG_ERROR_PREFIX);\
	log_eprintf(__VA_ARGS__);\
	log_eprintf("\n");\
	pthread_mutex_unlock(&_log_write_mutex);\
}
#else
#define log_error(...)
#endif

#endif
