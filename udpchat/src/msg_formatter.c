#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <stdio.h>

#include "config.h"
#include "msg_formatter.h"

static __thread char return_buffer[MSG_TOTAL_LEN] = {0};
static __thread char id_buffer[MSG_ID_LEN] = {0};
/* body USES NEWLINE AS ZERO TERMINATOR. */
const char *msg_formatter(uint16_t sender_id, uint16_t receiver_id,
    const char *body, size_t *len)
{
	size_t free = 0;

	// if (sender_id != receiver_id)
	// 	return_buffer[free++] = '\n';
	if (body != NULL) {
		/* Copy id to start of buffer without terminator. */
		snprintf(id_buffer, sizeof(id_buffer), "%3" PRIu16,
		    sender_id % 999);
		memcpy(&return_buffer[free], id_buffer, MSG_ID_LEN - 1);
		free += MSG_ID_LEN - 1;
		/* Copy format string after id without terminator. */
		memcpy(&return_buffer[free], "| ", MSG_F_LEN - 1);
		free += MSG_F_LEN - 1;
		/* Copy body after format string without newline and add newline. */
		memcpy(&return_buffer[free], body, MIN(*len, MSG_BODY_LEN) - 1);
		free += MIN(*len, MSG_BODY_LEN) - 1;
		return_buffer[free++] = '\n';
	}
	/* free can be between (MSG_F + MSG_ID - 2) and */
	*len = free;

	/* Put terminator at the end. */
	return_buffer[MSG_TOTAL_LEN - 1] = '\n';

	return return_buffer;
}
