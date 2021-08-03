#ifndef MSG_FORMATTER_H
#define MSG_FORMATTER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

const char *msg_formatter(uint16_t _sender_id, uint16_t _receiver_id,
    const char *_body, size_t *_body_len);

#endif
