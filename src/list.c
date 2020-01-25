#include <list.h>
#include <types.h>

void list_init(struct list_node * const node) {
    node->prev = node;
    node->next = node;
}

static void list_add_between(struct list_node * const prev,
                             struct list_node * const node,
                             struct list_node * const next) {
    prev->next = node;
    next->prev = node;
    node->prev = prev;
    node->next = next;
}

void list_add(struct list_node * const head, struct list_node * const n) {
    list_add_between(head, n, head->next);
}

void list_add_tail(struct list_node * const head, struct list_node * const n) {
    list_add_between(head->prev, n, head);
}

void list_del(struct list_node * const node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    list_init(node);
}

bool list_empty(struct list_node const * const head) {
    return head->next == head;
}

uint32_t list_size(struct list_node const * const head) {
    uint32_t size = 0;
    struct list_node * cursor;
    list_for_each(cursor, head) {
        size ++;
    }
    return size;
}
#include <list.test>
