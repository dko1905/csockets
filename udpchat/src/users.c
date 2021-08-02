#include "users.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

/* Allow for custom allocators. */
#ifndef cmalloc
#define cmalloc malloc
#endif
#ifndef cfree
#define cfree free
#endif

int user_table_init(struct user_table *table, time_t timeout)
{
	table->timeout = timeout;
	table->start = NULL;
	table->end = NULL;

	return 0;
}


int user_table_update(struct user_table *table, const struct user *user)
{
	/* Cycle throught linked list to check if user is already part. */
	for (struct user *p = table->start; p != NULL; p = p->next) {
		if (memcmp(&p->addr, &user->addr, sizeof(user->addr)) == 0) {
			/* Found! */
			p->last_msg = user->last_msg;
			/* TODO: More logic. */

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
			memset(p, 0, sizeof(*p));
			free(p);
		}
	}

	/* If not found, add user to end of list. */
	struct user *user1 = malloc(sizeof(*user1));
	if (user1 == NULL) return 1;

	memcpy(user1, user, sizeof(*user));

	if (table->start == NULL) {
		/* start and end are NULL. */
		table->start = user1;
		table->end = user1;
	} else {
		/* start and end are not NULL. */
		table->end->next = user1;
		user1->prev = table->end;
	}

	return 0;
}

int user_table_every(const struct user_table *table,
    user_table_every_func_t func, void *args)
{
	/* Cycle throught every element and call func with args. */
	for (struct user *p = table->start; p != NULL; p = p->next) {
		func(p, args);
	}

	return 0;
}

void user_table_free(struct user_table *table)
{
	/* Cycle throught and free all elements. */
	for (struct user *p = table->start; p != NULL; p = p->next) {
		/* Free previous elm, free self if last elm. */
		if (p->prev != NULL) free(p->prev);
		else if (p->next == NULL) free(p);
	}

	memset(table, 0, sizeof(*table));
}
