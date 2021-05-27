#include "elem.h"

element_t *new_element(char *key, void *data, size_t size)
{
	element_t *e = calloc(1, sizeof(struct element));

	if (e == NULL) {
		fprintf(stderr, "malloc() failed\n");
		return NULL;
	}

	if (key != NULL) {
		e->key = strdup(key);
	}
	if (data != NULL) {
		e->data = malloc(size);
		memcpy(e->data, data, size);
	}
	return e;
}

void list_element(element_t *list, void (*cbfunc)(), void *cbarg)
{
	//int i = 0;
	element_t *now = list;
	element_t *next = NULL;
	do {
		next = now->forward;
#if 0
		if (cbfunc != NULL && cbarg != NULL) {
#else
		if (cbfunc != NULL) {
#endif
			/* do callback */
			cbfunc(now, cbarg);
		} else {
			/* just printf */
			//fprintf(stderr, "%d: %s\n", i++, now->key == NULL ? "null" : now->key);
		}
		now = next;
	} while (next != NULL);
}

int add_element(element_t *root, element_t *item)
{
	element_t *target = NULL, *traverse = NULL;
	target = traverse = root;
	do {
		if (traverse->key == NULL || strcmp(item->key, traverse->key) > 0) {
			target = traverse;
		} else if (!strcmp(item->key, traverse->key)) {
			fprintf(stderr, "key[%s] already exist!\n", item->key);
			return -1;
		} else {
			break;
		}
		traverse = traverse->forward;
	} while (traverse != NULL);

	insque(item, target);
	return 0;
}

element_t *get_element(element_t *list, const char *key)
{
	element_t *traverse = list;
	do {
		if (traverse->key != NULL && !strcmp(traverse->key, key)) {
			return traverse;
		}
		traverse = traverse->forward;
	} while (traverse != NULL);

	return NULL;
}

void rem_element(element_t *item)
{
	if (item == NULL) {
		return;
	} else {
		remque(item);
		free(item->key);
		free(item->data);
		free(item);
	}
}
