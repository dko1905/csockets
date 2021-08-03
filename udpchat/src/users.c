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
	struct user *p = table->start, *tmp = NULL;

	/* Cycle throught linked list to check if user is already part. */
	while (p != NULL && p->next != NULL) {
		tmp = p;
		p = p->next;

		if (sockaddr_cmp(&tmp->addr, &user->addr, tmp->addr_len) == 0) {
			/* Found! */
			tmp->last_msg = user->last_msg;
			tmp->recv_fd = user->recv_fd;
			tmp->id = user->id;

			return 0;
		} else if (tmp->last_msg + table->timeout < time(NULL)) {
			/* Call timeout_func before deleting. */
			timeout_func(tmp, timeout_func_args);

			/* Remove tmp from list. */
			if (tmp->prev != NULL && tmp->next != NULL) {
				/* prev & next are not NULL. */
				tmp->prev->next = tmp->next;
				tmp->next->prev = tmp->prev;
			} else if (tmp->prev != NULL) {
				/* prev is not NULL, next is. */
				tmp->prev->next = NULL;
				table->end = tmp->prev;
			} else if (tmp->next != NULL) {
				/* prev is NULL, next is not. */
				tmp->next->prev = NULL;
				table->start = tmp->next;
			} else {
				/*  is only element, remove itself. */
				table->start = NULL;
				table->end = NULL;
			}

			memset(tmp, 0, sizeof(*tmp));
			free(tmp);
		}
	}
	if (p != NULL && sockaddr_cmp(&p->addr, &user->addr, p->addr_len) == 0) {
		p->last_msg = user->last_msg;
		p->recv_fd = user->recv_fd;
		p->id = user->id;

		return 0;
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
	struct user *prev = table->start, *tmp = NULL;

	while (prev->next != NULL) {
		tmp = prev;
		prev = prev->next;

		memset(tmp, 0, sizeof(*tmp));
		free(tmp);
	}
	memset(prev, 0, sizeof(*prev));
	free(prev);

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

		id %= 999;
		if (id == 0) id = 999;

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

		id %= 999;
		if (id == 0) id = 999;

		return id;
	} else {
		return 0;
	}
}
