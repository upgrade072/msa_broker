#ifndef _ELEM_H
#define _ELEM_H 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <search.h>

struct element {
    struct element *forward;
    struct element *backward;
    char *key;
    void *data;
};
typedef struct element element_t;

/* ------------------------- elem.c --------------------------- */
element_t       *new_element(char *key, void *data, size_t size);
void    list_element(element_t *list, void (*cbfunc)(), void *cbarg);
int     add_element(element_t *root, element_t *item);
element_t       *get_element(element_t *list, const char *key);
void    rem_element(element_t *item);

#endif /* elem.h */
