#include <stdlib.h>
#include <limits.h>
#include <poll.h>
#include <sys/time.h>
#include "watch.h"
#include "list.h"

typedef unsigned long long u64;

enum watch_type {
	WATCH_TYPE_NULL,
	WATCH_TYPE_FD,
	WATCH_TYPE_TIMEOUT,
};

struct watch_ticket {
	enum watch_type type;
	union {
		int filedes;
		unsigned int interval;
	};
	struct {
		void (* fn)(void *data, struct watch_ticket *);
		void *data;
	} callback;

	u64 start;
	int updated;
	struct watch *watch;
	struct list_node list_node;
};

struct watch {
	struct list tickets;
	int count;
};

u64 time_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (u64)tv.tv_sec*1000 + tv.tv_usec/1000;
}

struct watch *watch_create(void)
{
	struct watch *w;

	w = calloc(1, sizeof(*w));
	if (w == NULL)
		return NULL;

	list_init(&w->tickets);
	return w;
}

void watch_destroy(struct watch *w)
{
	struct watch_ticket *ticket;
	struct list_node *safe;
	struct list_node *node;

	for_list_node_safe(&w->tickets, node, safe) {
		ticket = list_entry(node, struct watch_ticket, list_node);
		free(ticket);
	}

	free(w);
}

void watch_synchronize(struct watch *w)
{
	struct watch_ticket *oticket;
	struct watch_ticket *ticket;
	struct list_node *node;

	for_list_node(&w->tickets, node) {
		struct list_node *onode;
		ticket = list_entry(node, struct watch_ticket, list_node);

		if (ticket->type != WATCH_TYPE_TIMEOUT)
			continue;

		for_list_node_after(node, onode) {
			oticket = list_entry(onode, struct watch_ticket, list_node);
			if (oticket->type != WATCH_TYPE_TIMEOUT)
				continue;

			if (oticket->interval == ticket->interval) {
				oticket->start = ticket->start;
				break;
			}
		}
	}
}

void watch_wait(struct watch *w)
{
	struct pollfd pfds[w->count];
	struct watch_ticket *ticket;
	struct list_node *node;
	u64 term_time;
	int count;
	int idx;
	u64 now;
	int rc;

	idx = 0;
	term_time = (u64)-1;
	for_list_node(&w->tickets, node) {
		ticket = list_entry(node, struct watch_ticket, list_node);
		switch (ticket->type) {
		case WATCH_TYPE_TIMEOUT:
			if (ticket->start + ticket->interval < term_time)
				term_time = ticket->start + ticket->interval;
			break;
		case WATCH_TYPE_FD:
			pfds[idx].fd = ticket->filedes;
			pfds[idx].events = POLLERR | POLLPRI;
			idx++;
			break;
		case WATCH_TYPE_NULL:
			break;
		}
	}
	count = idx;

	if (term_time == (u64)-1) { /* wait forever */
		rc = poll(pfds, count, -1);
	} else {
		now = time_ms();
		if (now >= term_time) { /* already past timeout, skip poll */
			rc = 0;
		} else {
			u64 delta;

			delta = term_time - now;
			if (delta > INT_MAX)
				delta = INT_MAX;
			rc = poll(pfds, count, (int)delta);
		}
	}

	if (rc < 0)
		return;

	idx = 0;
	now = time_ms();
	for_list_node(&w->tickets, node) {
		int fresh = 0;

		ticket = list_entry(node, struct watch_ticket, list_node);
		switch (ticket->type) {
		case WATCH_TYPE_TIMEOUT:
			if (now >= ticket->start + ticket->interval) {
				ticket->start = now;
				fresh = !ticket->updated;
			}
			break;
		case WATCH_TYPE_FD:
			if (rc == 0) /* timed-out */
				break;
			if (pfds[idx].revents & (POLLERR | POLLPRI)) {
				fresh = !ticket->updated;
			}
			idx++;
			break;
		case WATCH_TYPE_NULL:
			break;
		}
		if (fresh) {
			ticket->updated = 1;
			if (ticket->callback.fn)
				(* ticket->callback.fn)(
						ticket->callback.data,
						ticket
				);
		}
	}
}

void watch_ticket_set_null(struct watch_ticket *ticket)
{
	ticket->type = WATCH_TYPE_NULL;
}

void watch_ticket_set_fd(struct watch_ticket *ticket, int fd)
{
	ticket->type = WATCH_TYPE_FD;
	ticket->filedes = fd;
}

void watch_ticket_set_timeout(struct watch_ticket *ticket, unsigned int ms)
{
	ticket->type = WATCH_TYPE_TIMEOUT;
	ticket->interval = ms;
	ticket->start = time_ms();
}

struct watch_ticket *watch_add_null(struct watch *w)
{
	struct watch_ticket *ticket;

	ticket = calloc(1, sizeof(*ticket));
	if (ticket == NULL)
		return NULL;
	ticket->watch = w;

	list_append(&w->tickets, &ticket->list_node);
	w->count++;

	watch_ticket_set_null(ticket);

	return ticket;
}

struct watch_ticket *watch_add_fd(struct watch *w, int fd)
{
	struct watch_ticket *ticket;

	ticket = watch_add_null(w);
	if (ticket == NULL)
		return NULL;

	watch_ticket_set_fd(ticket, fd);

	return ticket;
}

struct watch_ticket *watch_add_timeout(struct watch *w, unsigned int ms)
{
	struct watch_ticket *ticket;

	ticket = watch_add_null(w);
	if (ticket == NULL)
		return NULL;

	watch_ticket_set_timeout(ticket, ms);

	return ticket;
}

void watch_ticket_delete(struct watch_ticket *ticket)
{
	struct watch *w = ticket->watch;
	list_remove(&w->tickets, &ticket->list_node);
	w->count--;
	free(ticket);
}

void watch_ticket_callback(struct watch_ticket *ticket,
		void (* cb_fn)(void *, struct watch_ticket *), void *data)
{
	ticket->callback.fn = cb_fn;
	ticket->callback.data = data;
}

int watch_ticket_check(struct watch_ticket *ticket)
{
	return -(ticket->updated == 0);
}

int watch_ticket_clear(struct watch_ticket *ticket)
{
	int ret;

	ret = watch_ticket_check(ticket);
	ticket->updated = 0;

	return ret;
}

static struct watch *g_watch_manager_watch;

void watch_manager_set_watch(struct watch *watch)
{
	g_watch_manager_watch = watch;
}

struct watch_ticket *watch_manager_add_null(void)
{
	if (g_watch_manager_watch == NULL)
		return NULL;
	return watch_add_null(g_watch_manager_watch);
}

struct watch_ticket *watch_manager_add_fd(int fd)
{
	if (g_watch_manager_watch == NULL)
		return NULL;
	return watch_add_fd(g_watch_manager_watch, fd);
}

struct watch_ticket *watch_manager_add_timeout(unsigned int ms)
{
	if (g_watch_manager_watch == NULL)
		return NULL;
	return watch_add_timeout(g_watch_manager_watch, ms);
}

void watch_manager_wait(void)
{
	if (g_watch_manager_watch == NULL)
		return;
	watch_wait(g_watch_manager_watch);
}
