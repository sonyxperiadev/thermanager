#include <stdlib.h>

#include "control.h"
#include "threshold.h"

struct threshold_mitigation {
	struct control *ctrl;
	int level;
	struct list_node list_node;
};

struct threshold *threshold_create(const char *trigger, const char *clear)
{
	struct threshold *t;

	t = calloc(1, sizeof(*t));
	if (t == NULL)
		return NULL;

	t->trigger = strtol(trigger, 0, 0);
	t->clear = strtol(clear, 0, 0);

	list_init(&t->mitigations);

	return t;
}

void threshold_destroy(struct threshold *t)
{
	struct list_node *node;
	struct threshold_mitigation *m;

	while ((node = list_pop(&t->mitigations)) != NULL) {
		m = list_entry(node, struct threshold_mitigation, list_node);
		free(m);
	}

	free(t);
}

int threshold_add_mitigation(struct threshold *t, const char *mitigation, int level)
{
	struct threshold_mitigation *m;

	m = calloc(1, sizeof(*m));
	if (m == NULL)
		return -1;

	m->level = level;
	m->ctrl = control_manager_find(mitigation);
	if (m->ctrl == NULL) {
		free(m);
		return -1;
	}
	list_append(&t->mitigations, &m->list_node);
	return 0;
}

int threshold_entered(struct threshold *t, int value)
{
	return value >= t->trigger;
}

int threshold_exited(struct threshold *t, int value)
{
	return value <= t->clear;
}

void threshold_edges(struct threshold *t, int *lo, int *hi)
{
	if (lo) *lo = t->clear;
	if (hi) *hi = t->trigger;
}

void threshold_activate(struct threshold *t)
{
	struct list_node *node;
	struct threshold_mitigation *m;

	for_list_node(&t->mitigations, node) {
		m = list_entry(node, struct threshold_mitigation, list_node);
		control_vote_level(m->ctrl, m->level);
	}
}

void threshold_deactivate(struct threshold *t)
{
	struct list_node *node;
	struct threshold_mitigation *m;

	for_list_node(&t->mitigations, node) {
		m = list_entry(node, struct threshold_mitigation, list_node);
		control_unvote_level(m->ctrl, m->level);
	}
}
