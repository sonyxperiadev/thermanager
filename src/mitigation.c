#include <stdlib.h>
#include <string.h>

#include "resource.h"
#include "mitigation.h"

struct mitigation_resource {
	char target_value[256];
	struct resource *resource;
	struct list_node list_node;
};

struct mitigation *mitigation_create(int level)
{
	struct mitigation *m;

	m = calloc(1, sizeof(*m));
	if (m == NULL)
		return NULL;

	m->level = level;
	list_init(&m->resources);

	return m;
}

void mitigation_destroy(struct mitigation *m)
{
	struct mitigation_resource *r;
	struct list_node *node;

	while ((node = list_pop(&m->resources)) != NULL) {
		r = list_entry(node, struct mitigation_resource, list_node);
		free(r);
	}

	free(m);
}

void mitigation_add_resource(struct mitigation *m, const char *name, const char *target_value)
{
	struct mitigation_resource *r;
	struct resource *res;

	res = resource_manager_find(name);
	if (res == NULL)
		return;

	r = calloc(1, sizeof(*r));
	if (r == NULL)
		return;
	strncpy(r->target_value, target_value, sizeof(r->target_value));
	r->target_value[sizeof(r->target_value) - 1] = 0;

	r->resource = res;

	list_append(&m->resources, &r->list_node);
}

void mitigation_activate(struct mitigation *m)
{
	struct mitigation_resource *r;
	struct list_node *node;

	for_list_node(&m->resources, node) {
		r = list_entry(node, struct mitigation_resource, list_node);
		resource_enable(r->resource);
		resource_write_value(r->resource, r->target_value, strlen(r->target_value));
	}
}

void mitigation_deactivate(struct mitigation *m)
{
	struct mitigation_resource *r;
	struct list_node *node;

	for_list_node(&m->resources, node) {
		r = list_entry(node, struct mitigation_resource, list_node);
		resource_disable(r->resource);
	}
}
