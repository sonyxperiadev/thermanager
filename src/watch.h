#ifndef _WATCH_H_
#define _WATCH_H_

struct watch;

struct watch *watch_create(void);
void watch_destroy(struct watch *);
void watch_wait(struct watch *watch);
void watch_synchronize(struct watch *watch);

struct watch_ticket;

struct watch_ticket *watch_add_null(struct watch *watch);
struct watch_ticket *watch_add_fd(struct watch *watch, int fd);
struct watch_ticket *watch_add_timeout(struct watch *watch, unsigned int ms);

void watch_ticket_set_null(struct watch_ticket *ticket);
void watch_ticket_set_fd(struct watch_ticket *ticket, int fd);
void watch_ticket_set_timeout(struct watch_ticket *ticket, unsigned int ms);

void watch_ticket_delete(struct watch_ticket *ticket);
int watch_ticket_check(struct watch_ticket *ticket);
int watch_ticket_clear(struct watch_ticket *ticket);
void watch_ticket_callback(struct watch_ticket *ticket,
		void (* cb_fn)(void *, struct watch_ticket *), void *data);

void watch_manager_set_watch(struct watch *watch);
struct watch_ticket *watch_manager_add_null(void);
struct watch_ticket *watch_manager_add_fd(int fd);
struct watch_ticket *watch_manager_add_timeout(unsigned int ms);
void watch_manager_wait(void);

#endif
