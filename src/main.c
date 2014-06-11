#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "configuration.h"
#include "control.h"
#include "threshold.h"
#include "mitigation.h"
#include "resource.h"
#include "log.h"

#include "dom.h"

static int parse_only_X_inner(const struct dom_obj *obj, const char *name,
		int (*fn)(const struct dom_obj *))
{
	const struct list_node *node;
	const struct dom_obj *child;
	int rc;

	for_list_node(&obj->children, node) {
		child = list_entry(node, const struct dom_obj, list_node);
		if (strcmp(child->name, name))
			continue;

		rc = (* fn)(child);
		if (rc)
			return rc;
	}

	return 0;
}

static int parse_only_X(const struct dom_obj *obj, const char *name,
		int (*fn)(const struct dom_obj *))
{
	return parse_only_X_inner(list_entry(obj->children.head, const struct dom_obj, list_node), name, fn);
}

static int parse_multi_X(const struct dom_obj *obj, const char *name,
		int (*fn)(void *, const struct dom_obj *), void *data)
{
	const struct list_node *node;
	const struct dom_obj *child;
	int rc;

	for_list_node(&obj->children, node) {
		child = list_entry(node, const struct dom_obj, list_node);
		if (strcmp(child->name, name)) {
			LOGE("invalid object '%s' within '%s'."
					" should be '%s'\n",
					child->name, obj->name, name);
			return -1;
		}

		rc = (* fn)(data, child);
		if (rc)
			return rc;
	}

	return 0;
}

static int parse_one_resource(void *data, const struct dom_obj *obj)
{
	struct resource *res;
	const char *type;
	const char *name;

	type = dom_obj_attribute_value(obj, "type");
	if (type == NULL) {
		LOGE("resource missing 'type' attribute\n");
		return -1;
	}
	name = dom_obj_attribute_value(obj, "name");
	if (name == NULL) {
		LOGE("resource missing 'name' attribute\n");
		return -1;
	}

	res = NULL;
	if (!strcmp(type, "tz")) {
		if (obj->content == NULL)
			return -1;
		res = resource_tz_open(name, obj->content);
	} else if (!strcmp(type, "alias")) {
		const char *alias;
		alias = dom_obj_attribute_value(obj, "resource");
		if (alias == NULL)
			return -1;
		res = resource_alias_open(name, alias);
	} else if (!strcmp(type, "union")) {
		const struct list_node *node;
		const struct dom_obj *child;
		const char *which;
		const char *cnames[256];
		int count;

		count = 0;
		for_list_node(&obj->children, node) {
			child = list_entry(node, const struct dom_obj, list_node);
			if (strcmp("resource", child->name))
				return -1;
			which = dom_obj_attribute_value(child, "name");
			if (which == NULL)
				return -1;
			cnames[count++] = which;
		}
		res = resource_union_open(name, count, cnames);
	} else if (!strcmp(type, "sysfs")) {
		if (obj->content == NULL)
			return -1;
		res = resource_sysfs_open(name, obj->content);
	} else if (!strcmp(type, "deadband")) {
		const char *alias;
		const char *ssize;
		alias = dom_obj_attribute_value(obj, "resource");
		if (alias == NULL)
			return -1;
		ssize = dom_obj_attribute_value(obj, "size");
		if (ssize == NULL)
			return -1;
		res = resource_deadband_open(name, alias, strtol(ssize, 0, 0));
	} else if (!strcmp(type, "halt")) {
		const char *delay;
		int dval;

		delay = dom_obj_attribute_value(obj, "delay");
		if (delay == NULL)
			dval = 0;
		else
			dval = strtol(delay, 0, 0);
		res = resource_halt_open(name, dval);
	} else if (!strcmp(type, "echo")) {
		res = resource_echo_open(name);
	} else if (!strcmp(type, "msm-adc")) {
		if (obj->content == NULL)
			return -1;
		res = resource_msmadc_open(name, obj->content);
	} else if (!strcmp(type, "intent")) {
		if (obj->content == NULL)
			return -1;
		res = resource_intent_open(name, obj->content);
	} else if (!strcmp(type, "cpufreq")) {
		if (obj->content == NULL)
			return -1;
		res = resource_cpufreq_open(name, obj->content);
	}

	if (res == NULL) {
		LOGW("failed to attach resource \"%s\""
				" [%s], ignoring\n", name, type);
		return 0;
	}
	LOGV("attached resource \"%s\" [%s]\n", name, type);

	resource_manager_add(res);

	return 0;
}

static int parse_resources(const struct dom_obj *obj)
{
	return parse_multi_X(obj, "resource", parse_one_resource, 0);
}

static int parse_one_mitigation_resource(void *data, const struct dom_obj *obj)
{
	struct mitigation *mig;
	const char *name;

	name = dom_obj_attribute_value(obj, "resource");
	if (name == NULL) {
		LOGE("value missing 'resource' attribute\n");
		return -1;
	}

	mig = (struct mitigation *)data;
	mitigation_add_resource(mig, name, obj->content ? obj->content : "");

	return 0;
}

static int parse_one_mitigation(void *data, const struct dom_obj *obj)
{
	struct mitigation *mig;
	struct control *ctrl;
	const char *level;
	int rc;

	level = dom_obj_attribute_value(obj, "level");
	if (level == NULL) {
		LOGE("mitigation missing 'level' attribute\n");
		return -1;
	}

	if (!strcmp(level, "off")) {
		mig = mitigation_create(0);
	} else {
		mig = mitigation_create(strtol(level, 0, 0));
	}
	if (mig == NULL)
		return -1;

	rc = parse_multi_X(obj, "value",
			parse_one_mitigation_resource, mig);
	if (rc) {
		LOGE("failed to parse mitigation values\n");
		mitigation_destroy(mig);
		return rc;
	}

	ctrl = (struct control *)data;
	control_add_mitigation(ctrl, mig);

	return 0;
}

static int parse_control(const struct dom_obj *obj)
{
	struct control *ctrl;
	const char *name;
	int rc;

	name = dom_obj_attribute_value(obj, "name");
	if (name == NULL) {
		LOGE("control section missing 'name' attribute\n");
		return -1;
	}

	ctrl = control_create(name);
	if (ctrl == NULL)
		return -1;

	rc = parse_multi_X(obj, "mitigation", parse_one_mitigation, ctrl);
	if (rc) {
		LOGE("failed to parse control mitigations\n");
		control_destroy(ctrl);
		return rc;
	}

	control_manager_add(ctrl);

	return 0;
}

static int parse_one_target_mitigation(void *data, const struct dom_obj *obj)
{
	struct threshold *t;
	const char *level;
	const char *name;
	int value;

	level = dom_obj_attribute_value(obj, "level");
	if (level == NULL) {
		LOGE("mitigation missing 'level' attribute\n");
		return -1;
	}
	name = dom_obj_attribute_value(obj, "name");
	if (name == NULL) {
		LOGE("mitigation missing 'name' attribute\n");
		return -1;
	}

	if (!strcmp(level, "off")) {
		value = 0;
	} else {
		value = strtol(level, 0, 0);
	}

	t = (struct threshold *)data;
	return threshold_add_mitigation(t, name, value);
}

static int parse_one_threshold(void *data, const struct dom_obj *obj)
{
	struct configuration *cfg;
	struct threshold *t;
	const char *trigger;
	const char *release;
	int rc;

	trigger = dom_obj_attribute_value(obj, "trigger");
	release = dom_obj_attribute_value(obj, "clear");
	if (trigger == NULL)
		trigger = "0";
	if (release == NULL)
		release = "-2147483647";

	t = threshold_create(trigger, release);

	rc = parse_multi_X(obj, "mitigation", parse_one_target_mitigation, t);
	if (rc) {
		LOGE("failed to parse threshold mitigations\n");
		threshold_destroy(t);
		return rc;
	}

	cfg = (struct configuration *)data;
	configuration_add_threshold(cfg, t);

	return 0;
}

static int parse_config(const struct dom_obj *obj)
{
	struct configuration *cfg;
	const char *sensor;
	int rc;

	sensor = dom_obj_attribute_value(obj, "sensor");
	if (sensor == NULL) {
		LOGE("config section missing 'sensor' attribute\n");
		return -1;
	}

	cfg = configuration_create(sensor);
	if (cfg == NULL) {
		LOGE("failed to create configuration with sensor"
				" \"%s\"\n", sensor);
		return -1;
	}

	rc = parse_multi_X(obj, "threshold", parse_one_threshold, cfg);
	if (rc) {
		LOGE("failed to parse thresholds\n");
		configuration_destroy(cfg);
		return rc;
	}

	configuration_manager_add(cfg);

	return 0;
}

static int parse(const char *file)
{
	struct dom *dom;

	dom = dom_load(file);
	if (dom == 0)
		return -1;

	if (parse_only_X(dom->root, "resources", parse_resources)) {
		LOGE("failed to parse resource sections\n");
		return -1;
	}
	resource_manager_prepare();

	if (parse_only_X(dom->root, "control", parse_control)) {
		LOGE("failed to parse control sections\n");
		return -1;
	}
	if (parse_only_X(dom->root, "configuration", parse_config)) {
		LOGE("failed to parse configuration sections\n");
		return -1;
	}

	dom_destroy(dom);

	return 0;
}

int main(int argc, char **argv)
{
	if (argc != 2) {
		LOGE("Usage: %s <config>\n", argv[0]);
		return -1;
	}

	if (parse(argv[1]))
		return -1;

	configuration_manager_run();

	return 0;
}
