#include <string.h>
#include <stdlib.h>

#include "dom.h"

extern struct dom_obj *libxml_loaddom(const char *path);

const struct dom_obj *dom_obj_child(const struct dom_obj *obj, const char *name)
{
	const struct list_node *node;
	const struct dom_obj *child;

	for_list_node(&obj->children, node) {
		child = list_entry(node, const struct dom_obj, list_node);
		if (!strcmp(child->name, name))
			return child;
	}

	return NULL;
}

const struct dom_attr *dom_obj_attribute(const struct dom_obj *obj,
		const char *name)
{
	const struct list_node *node;
	const struct dom_attr *attr;

	for_list_node(&obj->attributes, node) {
		attr = list_entry(node, const struct dom_attr, list_node);
		if (!strcmp(attr->name, name))
			return attr;
	}

	return NULL;
}

const char *dom_obj_attribute_value(const struct dom_obj *obj,
		const char *name)
{
	const struct dom_attr *attr;

	attr = dom_obj_attribute(obj, name);
	if (attr == NULL)
		return NULL;

	return attr->value;
}

static void dom_obj_destroy(struct dom_obj *obj)
{
	struct list_node *node;
	struct list_node *safe;
	struct dom_attr *attr;
	struct dom_obj *child;

	for_list_node_safe(&obj->children, node, safe) {
		child = list_entry(node, struct dom_obj, list_node);
		dom_obj_destroy(child);
	}

	for_list_node_safe(&obj->attributes, node, safe) {
		attr = list_entry(node, struct dom_attr, list_node);
		free(attr);
	}

	if (obj->content)
		free(obj->content);

	free(obj);
}

void dom_destroy(struct dom *dom)
{
	if (dom->root != NULL)
		dom_obj_destroy(dom->root);
	free(dom);
}

struct dom *dom_load(const char *path)
{
	struct dom *dom;

	dom = calloc(1, sizeof(*dom));
	if (dom == NULL)
		return NULL;

	dom->root = libxml_loaddom(path);
	if (dom->root == NULL) {
		free(dom);
		return NULL;
	}

	return dom;
}

const struct dom_obj *dom_root(const struct dom *dom)
{
	return dom->root;
}

const struct dom_obj *dom_object(const struct dom *dom, const char *path)
{
	const struct dom_obj *obj;
	char *dup;
	char *tok;
	char *p;

	if (dom->root == NULL)
		return NULL;

	dup = strdup(path);

	obj = dom->root;
	p = strtok_r(dup, ".", &tok);
	while (p != NULL && obj != NULL) {
		obj = dom_obj_child(obj, p);
		p = strtok_r(NULL, ".", &tok);
	}

	free(dup);

	return obj;
}

const struct dom_attr *dom_attribute(const struct dom *dom,
		const char *obj_path, const char *attr)
{
	const struct dom_obj *obj;

	obj = dom_object(dom, obj_path);
	if (obj == NULL)
		return NULL;

	return dom_obj_attribute(obj, attr);
}

const char *dom_attribute_value(const struct dom *dom,
		const char *obj, const char *attr_name)
{
	const struct dom_attr *attr;

	attr = dom_attribute(dom, obj, attr_name);
	if (attr == NULL)
		return NULL;

	return attr->value;
}
