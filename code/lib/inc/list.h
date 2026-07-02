#ifndef __KERNEL_INC_LIST_H
#define __KERNEL_INC_LIST_H
#include "stdint.h"
#include "stdbool.h"

/**
 * @file list.h
 * @brief Intrusive doubly linked list with head/tail sentinels.
 *
 * The list does not own its nodes: a struct that wants to live on a list
 * embeds a @ref list_elem member, and elem2entry() converts the embedded
 * member pointer back to the enclosing struct (same idea as Linux
 * list_head / container_of).
 *
 * @note No internal locking — callers must serialize access themselves
 *       (e.g. thread ready list is protected by disabling interrupts).
 */

/***********
 * Define
 ***********/
/** @brief Byte offset of @p member inside @p struct_type (like offsetof). */
#define offset(struct_type, member) \
    (int)(&((struct_type *)0)->member)
/** @brief Convert a pointer to an embedded list_elem member back to a pointer
 *         to its enclosing @p struct_type (like container_of). */
#define elem2entry(struct_type, struct_member_name, elem_ptr) \
    (struct_type *)((int)elem_ptr - offset(struct_type, struct_member_name))

/***********
 * Struct
 ***********/
/** @brief List node embedded inside the user's struct; carries no data. */
struct list_elem
{
    struct list_elem *prev; /**< Previous node (head sentinel: NULL). */
    struct list_elem *next; /**< Next node (tail sentinel: NULL). */
};

/** @brief List handle. @p head / @p tail are sentinel nodes, not elements:
 *         real elements live strictly between them. */
struct list
{
    struct list_elem head; /**< Sentinel before the first element. */
    struct list_elem tail; /**< Sentinel after the last element. */
};

/***********
 * Function
 ***********/
/** @brief Predicate callback for list_traversal(); return true to stop at the
 *         current element. @p arg is caller-supplied context. */
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