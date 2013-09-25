#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>

#include "dom.h"

#define STR2ARRAY(_array, _string) \
  do { \
    strncpy(_array, _string, sizeof(_array)); \
    _array[sizeof(_array) - 1] = 0; \
  } while (0)
#define XSTR2ARRAY(_array, _xstring) \
  STR2ARRAY(_array, (const char *)(_xstring))

static struct dom_obj *libxml_parse_node(xmlNode *xnode)
{
	struct dom_obj *obj;
	xmlNode *xchild;
	xmlAttr *xattr;

	obj = calloc(1, sizeof(*obj));
	if (obj == NULL)
		return NULL;
	XSTR2ARRAY(obj->name, xnode->name);
	if (xnode->children != NULL && xnode->children->content != NULL)
		obj->content = strdup((const char *)xnode->children->content);

	list_init(&obj->children);
	list_init(&obj->attributes);

	for (xattr = xnode->properties; xattr != NULL; xattr = xattr->next) {
		struct dom_attr *attr;

		attr = calloc(1, sizeof(*attr));
		if (attr == NULL)
			return NULL;
		XSTR2ARRAY(attr->name, xattr->name);
		XSTR2ARRAY(attr->value, xattr->children->content);
		list_append(&obj->attributes, &attr->list_node);
	}

	for (xchild = xnode->children; xchild != NULL; xchild = xchild->next) {
		struct dom_obj *child;
		if (xchild->type != XML_ELEMENT_NODE)
			continue;

		child = libxml_parse_node(xchild);
		if (child != NULL)
			list_append(&obj->children, &child->list_node);
	}

	return obj;
}

static struct dom_obj *libxml_parse_root(xmlNode *xnode)
{
	struct dom_obj *obj;
	xmlNode *xchild;

	obj = calloc(1, sizeof(*obj));
	if (obj == NULL)
		return NULL;
	STR2ARRAY(obj->name, "ROOT");

	list_init(&obj->children);
	list_init(&obj->attributes);

	for (xchild = xnode; xchild != NULL; xchild = xchild->next) {
		struct dom_obj *child;

		child = libxml_parse_node(xchild);
		if (child != NULL)
			list_append(&obj->children, &child->list_node);
	}

	return obj;
}

struct dom_obj *libxml_loaddom(const char *path)
{
	struct dom_obj *obj;
	xmlNode *xnode;
	xmlDoc *xdoc;

	LIBXML_TEST_VERSION

	xdoc = xmlReadFile(path, NULL, 0);
	if (xdoc == NULL)
		return NULL;

	xnode = xmlDocGetRootElement(xdoc);

	obj = libxml_parse_root(xnode);

	xmlFreeDoc(xdoc);

	if (obj == NULL)
		return NULL;

	return obj;
}
