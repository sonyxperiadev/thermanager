#ifndef _CONTROL_H_
#define _CONTROL_H_

#include "mitigation.h"
#include "list.h"

struct control {
	char name[256];
	int current_level;
	struct list mitigation_levels;
	struct list_node list_node;
};

struct control *control_manager_find(const char *name);
void control_manager_add(struct control *ctrl);
void control_manager_remove(struct control *ctrl);

struct control *control_create(const char *name);
void control_destroy(struct control *ctrl);

void control_add_mitigation(struct control *ctrl, struct mitigation *m);
void control_vote_level(struct control *ctrl, int level);
void control_unvote_level(struct control *ctrl, int level);

#endif
