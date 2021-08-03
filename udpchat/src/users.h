#ifndef USERS_H
#define USERS_H

#include <stddef.h>
#include <time.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

struct user {
	/* Address of client. */
	struct sockaddr_storage addr; /* Client address. */
	socklen_t addr_len; /* Client address lenhgt.*/
	int addr_family; /* Client address family. */
	int recv_fd; /* Receive file descriptor. */

	/* Throatteling infomation. */
	time_t last_msg; /* used to kick users. */

	/* Linked list infomation. */
	struct user *next;
	struct user *prev;
};

struct user_table {
	struct user *start, *end;
	time_t timeout;
};

typedef void (*user_table_every_func_t)(const struct user *, void *);

int user_table_init(struct user_table *_table, time_t _timeout);
/* Used to keep user in table, will also kick timeed out users. */
int user_table_update(struct user_table *_table, const struct user *_user);
int user_table_every(const struct user_table *_table,
    user_table_every_func_t _func, void *args);
void user_table_free(struct user_table *_table);

#endif
