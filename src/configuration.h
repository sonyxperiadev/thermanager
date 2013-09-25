#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_

#include "resource.h"
#include "threshold.h"
#include "list.h"

struct configuration {
	int last_value;
	struct resource *sensor;
	struct threshold *current;
	struct list unsatisfied;
	struct list satisfied;
	struct list_node list_node;
};

void configuration_manager_add(struct configuration *cfg);
void configuration_manager_run(void);

struct configuration *configuration_create(const char *sensor);
void configuration_destroy(struct configuration *cfg);
int configuration_add_threshold(struct configuration *cfg, struct threshold *n);

#endif
