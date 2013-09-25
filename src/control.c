#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "control.h"

struct mitigation_level {
	struct mitigation *mitigation;
	int nvotes;
	struct list_node list_node;
};

static LIST(g_control_manager_list);

struct control *control_manager_find(const char *name)
{
	struct list_node *node;
	struct control *ctrl;

	for_list_node(&g_control_manager_list, node) {
		ctrl = list_entry(node, struct control, list_node);
		if (!strcmp(name, ctrl->name))
			return ctrl;
	}
	return NULL;
}

void control_manager_add(struct control *ctrl)
{
	list_append(&g_control_manager_list, &ctrl->list_node);
}

void control_manager_remove(struct control *ctrl)
{
	list_remove(&g_control_manager_list, &ctrl->list_node);
}

struct control *control_create(const char *name)
{
	struct control *ctrl;

	ctrl = calloc(1, sizeof(*ctrl));
	if (ctrl == NULL)
		return NULL;

	ctrl->current_level = -1;
	strncpy(ctrl->name, name, sizeof(ctrl->name));
	ctrl->name[sizeof(ctrl->name) - 1] = 0;

	list_init(&ctrl->mitigation_levels);

	return ctrl;
}

void control_destroy(struct control *ctrl)
{
	struct list_node *node;
	struct mitigation_level *l;

	while ((node = list_pop(&ctrl->mitigation_levels)) != NULL) {
		l = list_entry(node, struct mitigation_level, list_node);
		mitigation_destroy(l->mitigation);
		free(l);
	}

	free(ctrl);
}

void control_add_mitigation(struct control *ctrl, struct mitigation *mitigation)
{
	struct mitigation_level *l;

	l = calloc(1, sizeof(*l));
	if (l == NULL)
		return;
	l->mitigation = mitigation;
	list_append(&ctrl->mitigation_levels, &l->list_node);
}

static void control_update_level(struct control *ctrl)
{
	struct mitigation_level *l;
	struct list_node *node;
	int level;

	level = 0;

	for_list_node(&ctrl->mitigation_levels, node) {
		l = list_entry(node, struct mitigation_level, list_node);
		if (l->nvotes > 0 && l->mitigation->level > level)
			level = l->mitigation->level;
	}

	if (level == ctrl->current_level)
		return;

	LOGI("\"%s\" set to level %d\n", ctrl->name, level);

	for_list_node(&ctrl->mitigation_levels, node) {
		l = list_entry(node, struct mitigation_level, list_node);
		if (l->mitigation->level == level)
			mitigation_activate(l->mitigation);
		else if (l->mitigation->level == ctrl->current_level)
			mitigation_deactivate(l->mitigation);
	}
	ctrl->current_level = level;
}

void control_vote_level(struct control *ctrl, int level)
{
	struct mitigation_level *l;
	struct list_node *node;

	for_list_node(&ctrl->mitigation_levels, node) {
		l = list_entry(node, struct mitigation_level, list_node);
		if (l->mitigation->level == level) {
			l->nvotes++;
			break;
		}
	}
	control_update_level(ctrl);
}

void control_unvote_level(struct control *ctrl, int level)
{
	struct mitigation_level *l;
	struct list_node *node;

	for_list_node(&ctrl->mitigation_levels, node) {
		l = list_entry(node, struct mitigation_level, list_node);
		if (l->mitigation->level == level) {
			if (l->nvotes > 0)
				l->nvotes--;
			break;
		}
	}
	control_update_level(ctrl);
}
