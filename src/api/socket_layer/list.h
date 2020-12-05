#ifndef _LIST_H
#define _LIST_H

#include <stddef.h>
#include "compile.h"

#ifndef NULL
#define NULL ((void *)0)
#endif

struct list_head {
    struct list_head *next;
    struct list_head *prev;
};

#define LIST_HEAD(name) \
    struct list_head name = { &(name), &(name) }

static inline void ListInit(struct list_head *head) {
    head->prev = head->next = head;
}

static inline void ListAdd(struct list_head *new_e, struct list_head *head) {
    head->next->prev = new_e;
    new_e->next = head->next;
    new_e->prev = head;
    head->next = new_e;
}

static inline void ListAddTail(struct list_head *new_e, struct list_head *head) {
    head->prev->next = new_e;
    new_e->prev = head->prev;
    new_e->next = head;
    head->prev = new_e;
}

static inline void ListDel(struct list_head *elem) {
    struct list_head *prev = elem->prev;
    struct list_head *next = elem->next;

    prev->next = next;
    next->prev = prev;
}

static inline void ListDelInit(struct list_head *list) {
	ListDel(list);
	list->prev = list;
	list->next = list;
}

#define list_entry(ptr, type, member) \
    ((type *) ((char *) (ptr) - offsetof(type, member)))

#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

#define list_last_entry(ptr, type, member)\
	list_entry((ptr)->prev, type, member)

#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define list_for_each_safe(pos, p, head)    \
    for (pos = (head)->next, p = pos->next; \
         pos != (head);                     \
         pos = p, p = pos->next)

#define list_for_each_safe_from(pos, p, from, head)    \
    for (pos = from, p = pos->next; \
         pos != (head);                     \
         pos = p, p = pos->next)

static inline int ListEmpty(struct list_head *head) {
    return head->next == head;
}
#endif