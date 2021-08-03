#ifndef CONFIG_H
#define CONFIG_H

/* == Helper macros == */
#define MAX(_a, _b) ((_a > _b) ? _a : _b)
#define MIN(_a, _b) ((_a < _b) ? _a : _b)

/* == Compile time options == */
/* = Logging = */
/* Controls wether the log* functions print anything. */
#ifndef PRINT_DEBUG
#define PRINT_DEBUG (1)
#endif
#ifndef PRINT_INFO
#define PRINT_INFO (1)
#endif
#ifndef PRINT_WARN
#define PRINT_WARN (1)
#endif
#ifndef PRINT_ERROR
#define PRINT_ERROR (1)
#endif
#define PRINT_WRITE_MUTEX 1
/* = net = */
/* Max count of interfaces to listen on. Normally only two are used. */
#ifndef MAX_BIND_COUNT
#define MAX_BIND_COUNT (10)
#endif
/* Max size of message that can be received. */
#ifndef MAX_MSG_SIZE
#define MAX_MSG_SIZE (2048)
#endif
/* Timeout before removeing user from active users (in seconds). */
#ifndef ACTUSER_TIMEOUT
#define ACTUSER_TIMEOUT (70)
#endif
/* = user messages = */
/* Digits in an id (including '\0'). */
#ifndef MSG_ID_LEN
#define MSG_ID_LEN (4) /* "123" + '\0' */
#endif
/* Formatter string length. */
#ifndef MSG_F_LEN
#define MSG_F_LEN (3) /* ": " + '\0' */
#endif
/* Max length of message body. */
#ifndef MSG_BODY_LEN
#define MSG_BODY_LEN (50)
#endif
/* Total length of */
#ifndef MSG_TOTAL_LEN
#define MSG_TOTAL_LEN ( 1 + (MSG_ID_LEN - 1) + (MSG_F_LEN - 1) \
    + (MSG_BODY_LEN) \
    + 0 )
#endif
/* Message to send on timeout and times to send message. */
#ifndef MSG_USR_TIMEOUT
#define MSG_USR_TIMEOUT (1)
#define MSG_USR_TIMEOUT_STR "TIMEOUT: please send any message to join again."
#define MSG_USR_TIMEOUT_COUNT (2)
#endif

#endif
