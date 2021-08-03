#ifndef UTIL_PRINT_H
#define UTIL_PRINT_H

#include <stdio.h>

/* if you want to change output */
/* printf will be used for DEBUG & INFO. */
#ifndef print_printf
#define print_printf(...) printf(__VA_ARGS__)
#endif
/* eprintf will be used for WARN & ERROR. */
#ifndef print_eprintf
#define print_eprintf(...) fprintf(stderr, __VA_ARGS__)
#endif
/* if enabled */
#ifndef PRINT_DEBUG
#define PRINT_DEBUG (0)
#endif
#ifndef PRINT_INFO
#define PRINT_INFO (0)
#endif
#ifndef PRINT_WARN
#define PRINT_WARN (0)
#endif
#ifndef PRINT_ERROR
#define PRINT_ERROR (0)
#endif
/* prefixes, here the colons are aligned. */
#ifndef PRINT_DEBUG_PREFIX
#define PRINT_DEBUG_PREFIX "DEBUG: "
#endif
#ifndef PRINT_INFO_PREFIX
#define PRINT_INFO_PREFIX  "INFO : "
#endif
#ifndef PRINT_WARN_PREFIX
#define PRINT_WARN_PREFIX  "WARN : "
#endif
#ifndef PRINT_ERROR_PREFIX
#define PRINT_ERROR_PREFIX "ERROR: "
#endif
/* if using write mutex */
#ifndef PRINT_WRITE_MUTEX
#define PRINT_WRITE_MUTEX 0
#endif
#if PRINT_WRITE_MUTEX == 1
#include <pthread.h>
static pthread_mutex_t _print_write_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

/* Long print macro definitions */
#if PRINT_DEBUG == 1
#define print_debug(...) {\
	pthread_mutex_lock(&_print_write_mutex);\
	print_printf(PRINT_DEBUG_PREFIX);\
	print_printf(__VA_ARGS__);\
	print_printf("\n");\
	pthread_mutex_unlock(&_print_write_mutex);\
}
#else
#define print_debug(...)
#endif
#if PRINT_INFO == 1
#define print_info(...) {\
	pthread_mutex_lock(&_print_write_mutex);\
	print_printf(PRINT_INFO_PREFIX);\
	print_printf(__VA_ARGS__);\
	print_printf("\n");\
	pthread_mutex_unlock(&_print_write_mutex);\
}
#else
#define print_info(...)
#endif
#if PRINT_WARN == 1
#define print_warn(...) {\
	pthread_mutex_lock(&_print_write_mutex);\
	print_eprintf(PRINT_WARN_PREFIX);\
	print_eprintf(__VA_ARGS__);\
	print_eprintf("\n");\
	pthread_mutex_unlock(&_print_write_mutex);\
}
#else
#define print_warn(...)
#endif
#if PRINT_ERROR == 1
#define print_error(...) {\
	pthread_mutex_lock(&_print_write_mutex);\
	print_eprintf(PRINT_ERROR_PREFIX);\
	print_eprintf(__VA_ARGS__);\
	print_eprintf("\n");\
	pthread_mutex_unlock(&_print_write_mutex);\
}
#else
#define print_error(...)
#endif

/* Short definitions. */
#if PRINT_DEBUG == 1
#define pdebug(...) {\
	pthread_mutex_lock(&_print_write_mutex);\
	print_printf(PRINT_DEBUG_PREFIX);\
	print_printf(__VA_ARGS__);\
	print_printf("\n");\
	pthread_mutex_unlock(&_print_write_mutex);\
}
#else
#define pdebug(...)
#endif
#if PRINT_INFO == 1
#define pinfo(...) {\
	pthread_mutex_lock(&_print_write_mutex);\
	print_printf(PRINT_INFO_PREFIX);\
	print_printf(__VA_ARGS__);\
	print_printf("\n");\
	pthread_mutex_unlock(&_print_write_mutex);\
}
#else
#define pinfo(...)
#endif
#if PRINT_WARN == 1
#define pwarn(...) {\
	pthread_mutex_lock(&_print_write_mutex);\
	print_eprintf(PRINT_WARN_PREFIX);\
	print_eprintf(__VA_ARGS__);\
	print_eprintf("\n");\
	pthread_mutex_unlock(&_print_write_mutex);\
}
#else
#define pwarn(...)
#endif
#if PRINT_ERROR == 1
#define perror(...) {\
	pthread_mutex_lock(&_print_write_mutex);\
	print_eprintf(PRINT_ERROR_PREFIX);\
	print_eprintf(__VA_ARGS__);\
	print_eprintf("\n");\
	pthread_mutex_unlock(&_print_write_mutex);\
}
#else
#define perror(...)
#endif

#endif
