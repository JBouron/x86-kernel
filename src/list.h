#pragma once
#include <types.h>

// This file defines an easy way to handle double linked lists.

// This struct allows a type/struct to be part of a linked list. A la linux
// kernel.
struct list_node {
    // The previous entry in the list.
    struct list_node * prev;
    // The next entry in the list.
    struct list_node * next;
} __attribute__((packed));

// Get a pointer to the structure containing a list_node.
// @param node: The address of the list_node to get the struct address from.
// @param type: The expected type of the container.
// @param member: The name of the list_node in the container.
#define list_entry(node, type, member) \
    ((type*)((void*)node - offsetof(type, member)))

// Initialize a list node. This is equivalent to initializing a queue containing
// only the node as element.
// @param node: The list node to initialize.
void list_init(struct list_node * const node);

// Add an element to a list, after the head node.
// @param head: The node to add the element right after.
// @param n: The element/node to add.
void list_add(struct list_node * const head, struct list_node * const n);

// Add an element to a list, before the head node.
// @param head: The node to add the element right before.
// @param n: The element/node to add.
void list_add_tail(struct list_node * const head, struct list_node * const n);

// Delete an element from a list.
// @param node: The element to remove.
// Note: The node is re-initialized upon deletion.
void list_del(struct list_node * const node);

// Test if a list is empty.
// @param head: The list to test.
// @return: true if the list pointed by head is empty, false otherwise.
bool list_empty(struct list_node const * const head);

uint32_t list_size(struct list_node const * const head);

// Iterate over a list.
// @param cursor: The struct list_node * to use as an iterator.
// @param head: The list to iterate over.
#define list_for_each(cursor, head) \
    for (cursor = (head)->next; cursor != (head); cursor = cursor->next)

// Iterate over the entries of a list.
// @param entry: The type* to use as an iterator.
// @param head: The list to iterate over.
// @param member: The name of the list_node member in the entry to follow.
#define list_for_each_entry(entry, head, member)                            \
    for (entry = list_entry((head)->next, typeof(*entry), member);          \
         &entry->member != (head);                                          \
         entry = list_entry(entry->member.next, typeof(*entry), member))

#define list_first(head) \
    ((head)->next)

#define list_last(head) \
    ((head)->prev)

#define list_first_entry(head, type, member) \
    (list_entry(list_first(head), type, member))

#define list_last_entry(head, type, member) \
    (list_entry(list_last(head), type, member))

// Execute tests related to list manipulation.
void list_test(void);
