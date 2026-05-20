#ifndef __KERNEL_INC_LIST_H
#define __KERNEL_INC_LIST_H
#include "stdint.h"
#include "stdbool.h"

/***********
 * Define
 ***********/
#define offset(struct_type, member) \
    (int)(&((struct_type *)0)->member)
#define elem2entry(struct_type, struct_member_name, elem_ptr) \
    (struct_type *)((int)elem_ptr - offset(struct_type, struct_member_name))

/***********
 * Struct
 ***********/
struct list_elem
{
    struct list_elem *prev;
    struct list_elem *next;
};

struct list
{
    struct list_elem head;
    struct list_elem tail;
};

/***********
 * Function
 ***********/
typedef bool(function)(struct list_elem *, int arg);

/**
 * @brief Initialize the linked list, set head and tail sentinel nodes
 *
 * @param plist pointer to the list to initialize
 */
void list_init(struct list *plist);

/**
 * @brief Insert elem before prev_elem
 *
 * @param prev_elem the node that elem will be inserted before
 * @param elem the node to insert
 */
void list_insert(struct list_elem *prev_elem, struct list_elem *elem);

/**
 * @brief Remove elem from the linked list
 *
 * @param elem the node to remove
 */
void list_remove(struct list_elem *elem);

/**
 * @brief Append elem to the back of the list (before tail sentinel)
 *
 * @param plist pointer to the list
 * @param elem the node to append
 */
void list_append(struct list *plist, struct list_elem *elem);

/**
 * @brief Push elem to the front of the list (after head sentinel)
 *
 * @param plist pointer to the list
 * @param elem the node to push
 */
void list_push(struct list *plist, struct list_elem *elem);

/**
 * @brief Remove and return the front element of the list
 *
 * @param plist pointer to the list
 * @return struct list_elem* the front element, or NULL if the list is empty
 */
struct list_elem *list_pop(struct list *plist);

/**
 * @brief Check whether the linked list is empty
 *
 * @param plist pointer to the list
 * @return true the list has no elements
 * @return false the list has at least one element
 */
bool list_empty(struct list *plist);

/**
 * @brief Count the number of elements in the linked list
 *
 * @param plist pointer to the list
 * @return uint32_t number of elements (O(n))
 */
uint32_t list_len(struct list *plist);

/**
 * @brief Find whether elem exists in the linked list
 *
 * @param plist pointer to the list
 * @param elem the node to search for
 * @return true elem was found in the list
 * @return false elem was not found in the list
 */
bool list_find(struct list *plist, struct list_elem *elem);

/**
 * @brief Traverse the list and return the first element for which func returns true
 *
 * @param plist pointer to the list
 * @param func callback invoked on each element; return true to stop traversal
 * @param arg extra argument forwarded to func on every call
 * @return struct list_elem* the matching element, or NULL if no match found
 */
struct list_elem *list_traversal(struct list *plist, function func, int arg);

#endif