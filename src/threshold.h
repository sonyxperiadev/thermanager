#ifndef _THRESHOLD_H_
#define _THRESHOLD_H_

#include "list.h"

struct threshold {
	int trigger;
	int clear;
	struct list mitigations;
	struct list_node list_node;
};

struct threshold *threshold_create(const char *trigger, const char *clear);
void threshold_destroy(struct threshold *t);

int threshold_add_mitigation(struct threshold *t, const char *mit, int level);

void threshold_edges(struct threshold *t, int *lo, int *hi);
int threshold_entered(struct threshold *t, int value);
int threshold_exited(struct threshold *t, int value);
void threshold_activate(struct threshold *t);
void threshold_deactivate(struct threshold *t);

#endif
