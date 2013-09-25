#include <stdlib.h>
#include <limits.h>
#include <stdio.h>

#include "log.h"
#include "watch.h"
#include "configuration.h"

static LIST(g_configuration_manager_list);

void configuration_manager_add(struct configuration *cfg)
{
	list_append(&g_configuration_manager_list, &cfg->list_node);
}

static void configuration_run(struct configuration *cfg, int value);

void configuration_manager_run(void)
{
	struct configuration *cfg;
	struct list_node *node;
	struct watch *watch;

	if (list_first(&g_configuration_manager_list) == NULL) {
		LOGE("no configurations to run, exiting\n");
		return;
	}

	/* group configurations by sensor */
	for_list_node(&g_configuration_manager_list, node) {
		struct configuration *ocfg;
		struct list_node *onode;
		cfg = list_entry(node, struct configuration, list_node);

		for_list_node_after(node, onode) {
			ocfg = list_entry(onode, struct configuration, list_node);
			if (ocfg->sensor == cfg->sensor) {
				list_remove(&g_configuration_manager_list, onode);
				list_insert(&g_configuration_manager_list, node, onode);
				break;
			}
		}
	}

	watch = watch_create();
	if (watch == NULL)
		return;
	watch_manager_set_watch(watch);

	for_list_node(&g_configuration_manager_list, node) {
		cfg = list_entry(node, struct configuration, list_node);
		resource_enable(cfg->sensor);
	}

	watch_synchronize(watch);

	for (;;) {
		struct resource *res = NULL;
		char buf[32];
		int value;
		int rc;

		rc = value = 0;
		for_list_node(&g_configuration_manager_list, node) {
			cfg = list_entry(node, struct configuration, list_node);
			if (cfg->sensor != res) {
				res = cfg->sensor;
				rc = resource_read_value(cfg->sensor, buf, sizeof(buf));
				if (rc > 0) {
					buf[rc] = 0;
					value = strtol(buf, 0, 0);
				}
			}
			if (rc > 0)
				configuration_run(cfg, value);
		}
		watch_manager_wait();
	}

	for_list_node(&g_configuration_manager_list, node) {
		cfg = list_entry(node, struct configuration, list_node);
		resource_disable(cfg->sensor);
	}

	watch_manager_set_watch(NULL);
	watch_destroy(watch);
}

struct configuration *configuration_create(const char *sensor)
{
	struct configuration *cfg;

	cfg = calloc(1, sizeof(*cfg));
	if (cfg == NULL)
		return NULL;

	cfg->sensor = resource_manager_find(sensor);
	if (cfg->sensor == NULL) {
		free(cfg);
		return NULL;
	}
	cfg->last_value = -1;

	list_init(&cfg->unsatisfied);

	return cfg;
}

void configuration_destroy(struct configuration *cfg)
{
	struct list_node *node;
	struct threshold *t;

	while ((node = list_pop(&cfg->unsatisfied)) != NULL) {
		t = list_entry(node, struct threshold, list_node);
		threshold_destroy(t);
	}
	while ((node = list_pop(&cfg->satisfied)) != NULL) {
		t = list_entry(node, struct threshold, list_node);
		threshold_destroy(t);
	}

	free(cfg);
}

int configuration_add_threshold(struct configuration *cfg, struct threshold *n)
{
	struct list_node *node;
	struct list_node *last;
	struct threshold *t;

	/* sort insert */
	last = NULL;
	for_list_node(&cfg->unsatisfied, node) {
		t = list_entry(node, struct threshold, list_node);
		if (t->trigger > n->trigger) {
			list_insert(&cfg->unsatisfied, last, &n->list_node);
			return 0;
		}
		last = node;
	}
	list_append(&cfg->unsatisfied, &n->list_node);
	return 0;
}

static void configuration_run(struct configuration *cfg, int value)
{
	struct list_node *node;
	struct list_node *safe;
	struct threshold *t;
	int high_edge;
	int low_edge;

	low_edge = INT_MIN;
	high_edge = INT_MAX;

	if (value == cfg->last_value) {
		return;
	} else if (value > cfg->last_value) {
		for_list_node_safe(&cfg->unsatisfied, node, safe) {
			t = list_entry(node, struct threshold, list_node);
			if (threshold_entered(t, value)) {
				list_remove(&cfg->unsatisfied, &t->list_node);
				list_prepend(&cfg->satisfied, &t->list_node);
			}
		}
	} else if (value < cfg->last_value) {
		for_list_node_safe(&cfg->satisfied, node, safe) {
			t = list_entry(node, struct threshold, list_node);
			if (threshold_exited(t, value)) {
				list_remove(&cfg->satisfied, &t->list_node);
				list_prepend(&cfg->unsatisfied, &t->list_node);
			}
		}
	}
	cfg->last_value = value;

	node = list_first(&cfg->satisfied);
	if (node != NULL) {
		t = list_entry(node, struct threshold, list_node);
		if (cfg->current != t) {
			threshold_activate(t);
			if (cfg->current != NULL)
				threshold_deactivate(cfg->current);
			cfg->current = t;
		}
		low_edge = t->clear;
	}
	node = list_first(&cfg->unsatisfied);
	if (node != NULL) {
		t = list_entry(node, struct threshold, list_node);
		high_edge = t->trigger;
	}
	resource_set_edges(cfg->sensor, low_edge, high_edge);
}
