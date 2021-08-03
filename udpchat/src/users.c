#include "users.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include "util/net.h"

/* Allow for custom allocators. */
#ifndef cmalloc
#define cmalloc malloc
#endif
#ifndef cfree
#define cfree free
#endif

int user_table_init(struct user_table *table, time_t timeout)
{
	memset(table, 0, sizeof(*table));
	table->timeout = timeout;

	return 0;
}

int user_table_update(struct user_table *table, const struct user *user,
    user_table_timeout_func_t timeout_func, void *timeout_func_args)
{
	struct user *tmp = NULL;

	/* Cycle throught linked list to check if user is already part. */
	for (struct user *p = table->start; p != NULL;) {
		if (sockaddr_cmp(&p->addr, &user->addr, p->addr_len) == 0) {
			/* Found! */
			p->last_msg = user->last_msg;
			p->recv_fd = user->recv_fd;

			return 0; /* Nothing more to do, just return 0. */
		} else if (p->last_msg + table->timeout < time(NULL)) {
			/* User timed out, remove p from list. */
			if (p->prev != NULL && p->next != NULL) {
				/* prev & next are not NULL. */
				p->prev->next = p->next;
				p->next->prev = p->prev;
			} else if (p->prev != NULL) {
				/* prev is not NULL, next is. */
				p->prev->next = NULL;
				table->end = p->prev;
			} else if (p->next != NULL) {
				/* prev is NULL, next is not. */
				p->next->prev = NULL;
				table->start = p->next;
			} else {
				/* p is only element, remove itself. */
				table->start = NULL;
				table->end = NULL;
			}
			/* Call timeout func before freeing. */
			if (timeout_func != NULL)
				timeout_func(p, timeout_func_args);

			/* Store node to delete in tmp, and change current node
			 * to next, and then free tmp.
			 */
			tmp = p;
			p = p->next;
			memset(tmp, 0, sizeof(*tmp));
			free(tmp);
		} else {
			p = p->next;
		}
	}

	/* If not found, add user to end of list. */
	tmp = malloc(sizeof(*tmp));
	if (tmp == NULL) return 1;
	memcpy(tmp, user, sizeof(*tmp));

	if (table->start == NULL) {
		/* start and end are NULL. */
		table->start = tmp;
		table->end = tmp;
	} else {
		/* start and end are not NULL. */
		table->end->next = tmp;
		tmp->prev = table->end;
		table->end = tmp;
	}

	return 0;
}

int user_table_every(const struct user_table *table,
    user_table_every_func_t every_func, void *every_func_args)
{
	assert(every_func != NULL);

	/* Cycle throught every element and call func with args. */
	for (struct user *p = table->start; p != NULL; p = p->next) {
		every_func(p, every_func_args);
	}

	return 0;
}

void user_table_free(struct user_table *table)
{
	/* Cycle throught and free all elements. */
	for (struct user *p = table->start; p != NULL;) {
		/* Free previous elm, free self if last elm. */
		if (p->prev != NULL) {
			memset(p->prev, 0, sizeof(*p->prev));
			free(p->prev);

			p = p->next;
		} else if (p->next == NULL) {
			memset(p, 0, sizeof(*p));
			free(p);
			break;
		}
	}

	memset(table, 0, sizeof(*table));
}

uint16_t user_calculate_id(const struct user *user)
{
	uint16_t id = 0;

	if (user->addr_family == AF_INET) {
		const struct sockaddr_in *sock4 = (void *)&user->addr;
		/* 4 bytes */
		uint8_t *ip_buffer = (void *)&sock4->sin_addr.s_addr;

		/* 997 is prime, and very close to 999. */
		id += ip_buffer[0];
		id += ip_buffer[1];
		id %= 997;
		id += ip_buffer[2];
		id += ip_buffer[3];

		return id;
	} else if (user->addr_family == AF_INET6) {
		const struct sockaddr_in6 *sock6 = (void *)&user->addr;
		/* 16 bytes */
		uint8_t *ip_buffer = (void *)sock6->sin6_addr.s6_addr;

		for (size_t n = 0; n < 16; ++n) {
			id += ip_buffer[n];
			if (n % 2 == 1)
				id %= 997;
		}

		return id;
	} else {
		return 0;
	}
}
