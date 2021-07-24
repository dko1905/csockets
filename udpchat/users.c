#include "users.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

/* Allow for custom allocators. */
#ifndef cmalloc
#define cmalloc malloc
#endif
#ifndef cfree
#define cfree free
#endif

int chat_user_queue_init(size_t cap, struct chat_user_queue *queue)
{
	/* Allocate buffer to store queue. */
	struct chat_user *buffer = cmalloc(cap * sizeof(struct chat_user));
	if (buffer == NULL) {
		return 1;
	}

	/* Fill queue with 0. */
	*queue = (struct chat_user_queue){0};

	/* Fill with real data. */
	queue->buffer = buffer;
	queue->start = 0;
	queue->stop = 0;
	queue->cap = cap;
	queue->size = 0;

	return 0;
}

int chat_user_queue_includes(const struct chat_user_queue *queue,
    const struct chat_user *user)
{
	size_t n = queue->start;
	/* If empty cancel. */
	if (queue->size == 0) return 0;

	/* If stop if before start go to the end of the buffer. */
	if (queue->start >= queue->stop) {
		while (n < queue->size) {
			const struct chat_user *user1 = &queue->buffer[n % queue->cap];
			if (memcmp(&user->addr, &user1->addr,
			    sizeof(user->addr)) == 0) {
				return 1; /* true */
			}
			++n;
		}
		n = 0;
	}
	/* Traverse the rest if needed. */
	while (n < queue->stop) {
		const struct chat_user *user1 = &queue->buffer[n % queue->cap];
		if (memcmp(&user->addr, &user1->addr, sizeof(user->addr)) == 0) {
			return 1; /* true */
		}
		++n;
	}
	return 0; /* false */
}

int chat_user_queue_push(struct chat_user_queue *queue,
    const struct chat_user *user)
{
	if (queue->size >= queue->cap) {
		return 1;
	}
	size_t index = queue->stop;
	queue->buffer[index] = *user;
	queue->stop = (index + 1) % queue->cap;
	queue->size++;
	return 0;
}

int chat_user_queue_peek(const struct chat_user_queue *queue,
    struct chat_user *user)
{
	if (queue->size == 0) {
		return 1;
	}
	*user = queue->buffer[queue->start];
	return 0;
}

int chat_user_queue_pop(struct chat_user_queue *queue,
    struct chat_user *user)
{
	if (queue->size == 0) {
		return 1;
	}
	if (user != NULL)
		*user = queue->buffer[queue->start];
	queue->start = (queue->start + 1) % queue->cap;
	queue->size--;
	return 0;
}

void chat_user_queue_free(struct chat_user_queue *queue)
{
	if (queue != NULL && queue->buffer != NULL)
		cfree(queue->buffer);
}
