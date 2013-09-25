#ifndef _DOM_H_
#define _DOM_H_

#include "list.h"

struct dom_attr {
	char name[256];
	char value[256];
	struct list_node list_node;
};

struct dom_obj {
	char name[256];
	char *content;
	struct list children;
	struct list attributes;
	struct list_node list_node;
};

struct dom {
	struct dom_obj *root;
};

const struct dom_obj *dom_obj_child(const struct dom_obj *obj,
		const char *name);
const struct dom_attr *dom_obj_attribute(const struct dom_obj *obj,
		const char *name);
const char *dom_obj_attribute_value(const struct dom_obj *obj,
		const char *name);

struct dom *dom_load(const char *path);
void dom_destroy(struct dom *dom);

const struct dom_obj *dom_root(const struct dom *dom);
const struct dom_obj *dom_object(const struct dom *dom, const char *path);
const struct dom_attr *dom_attribute(const struct dom *dom,
		const char *obj, const char *attr);
const char *dom_attribute_value(const struct dom *dom,
		const char *obj, const char *attr);

#endif
