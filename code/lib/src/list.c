#include "list.h"
#include "stddef.h"

void list_init(struct list *plist)
{
    plist->head.prev = NULL;
    plist->head.next = &plist->tail;
    plist->tail.prev = &plist->head;
    plist->tail.next = NULL;
}

void list_insert(struct list_elem *prev_elem, struct list_elem *elem)
{
    prev_elem->prev->next = elem;
    elem->prev = prev_elem->prev;
    elem->next = prev_elem;
    prev_elem->prev = elem;
}

void list_remove(struct list_elem *elem)
{
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
}

void list_append(struct list *plist, struct list_elem *elem)
{
    list_insert(&plist->tail, elem);
}

void list_push(struct list *plist, struct list_elem *elem)
{
    list_insert(plist->head.next, elem);
}

struct list_elem *list_pop(struct list *plist)
{
    /* Check if the list is empty */
    if (list_empty(plist))
    {
        return NULL;
    }

    /* Pop the element */
    struct list_elem *elem = plist->head.next;
    list_remove(elem);
    return elem;
}

bool list_empty(struct list *plist)
{
    return plist->head.next == &plist->tail;
}

uint32_t list_len(struct list *plist)
{
    struct list_elem *elem = plist->head.next;
    uint32_t length = 0;
    while (elem != &plist->tail)
    {
        elem = elem->next;
        length++;
    }

    return length;
}

bool list_find(struct list *plist, struct list_elem *elem)
{
    struct list_elem *cur_elem = plist->head.next;
    while (cur_elem != &plist->tail)
    {
        if (cur_elem == elem)
        {
            return false;
        }
        cur_elem = cur_elem->next;
    }

    return false;
}

struct list_elem *list_traversal(struct list *plist, function func, int arg)
{
    struct list_elem *elem = plist->head.next;

    /* Check empty */
    if (list_empty(plist))
    {
        return NULL;
    }

    while (elem != &plist->tail)
    {
        if (func(elem, arg))
        {
            return elem;
        }
        elem = elem->next;
    }

    return NULL;
}