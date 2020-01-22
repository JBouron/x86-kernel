#pragma once

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

// Execute tests related to list manipulation.
void list_test(void);
