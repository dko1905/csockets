#ifndef USERS_H
#define USERS_H

#include <stddef.h>
#include <netdb.h>

struct chat_user {
	/* Address of client. */
	struct sockaddr_storage addr;
	socklen_t addr_len;

	/* Throatteling infomation. */
	int messages_in5s; /* Messages sent in last 5 seconds. */
};

struct chat_user_queue {
	size_t start, stop, size, cap;
	struct chat_user *buffer;
};

int chat_user_queue_init(size_t _cap, struct chat_user_queue *_queue);
/* Add user to queue, returns 0 on success and 1 if it's full. */
int chat_user_queue_push(struct chat_user_queue *_queue,
    const struct chat_user *_user);
int chat_user_queue_peek(const struct chat_user_queue *_queue,
    struct chat_user *_user);
int chat_user_queue_pop(struct chat_user_queue *_queue,
    struct chat_user *_user);
/* Return: 0 - false, 1 - true, -1 - error*/
int chat_user_queue_includes(const struct chat_user_queue *_queue,
    const struct chat_user *_user);
void chat_user_queue_free(struct chat_user_queue *_queue);

#endif
