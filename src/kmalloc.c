#include <types.h>
#include <debug.h>
#include <memory.h>
#include <frame_alloc.h>
#include <paging.h>
#include <list.h>
#include <kernel_map.h>
#include <math.h>
#include <lock.h>

// Dynamic memory allocator for the kernel.
// The memory allocator has the same interfaces as malloc() and free(). The goal
// is to allocate memory in the virtual address space of the kernel (that is
// addresses above KERNEL_PHY_OFFSET).
// At a high level overview, the memory allocator is a linked list of groups
// containing nodes.
//
// Group:
// ======
// A group is a contiguous memory area in virtual address space of at least 4
// KiB (one page). Groups can extend beyond one page, but their size is always
// a multiple of pages.
//
// Node:
// =====
// Within a group are nodes. Nodes are the atomic unit of allocation. Each
// allocation correspond to a node. A node is simply a subset of a group.
// Nodes have tag which can be either FREE or ALLOCATED. A FREE node corresponds
// to some memory region within the group that is not currently allocated,
// whereas an ALLOCATED is an allocated / in use region of the group.
//
// Allocation:
// ===========
// To allocate memory we first need to find a group that has enough free space
// to contain the requested amount of memory. Once such a group is found we need
// to find a free node within this node that can hold the memory.
// The free node is then marked as ALLOCATED and the address of the data it
// contains is returned.
//
// Free list:
// ==========
// To speed up allocation a free list is constructed within groups. A free list
// is basically a linked list of free node that can be iterated over when
// finding a suitable node for allocation.
//
// Free:
// =====
// Upon freeing memory, the corresponding node is marked as FREE and inserted
// back into the free list.

// This is the structure describing a group of virtual pages.
struct group {
    // The total size of the group in bytes.
    uint32_t size;
    // The number of pages used by the group.
    uint32_t num_pages;
    // The number of free bytes in the group.
    uint32_t free;
    // The element of the linked-list of groups.
    struct list_node group_list;
    // The free list of this group.
    struct list_node free_head;
} __attribute__((packed));

// An allocation node.
struct node {
    // The header of the node. This part is fixed and mandatory to all nodes.
    struct {
        // Indicates the type of the node: FREE or ALLOCATED.
        uint8_t tag : 1;
        // The size of the node.
        uint32_t size : 31;
    } __attribute__((packed)) header;
    union {
        // This part is used for data storage. For free node, it contains state
        // for the free list element. For allocated node this is the actual
        // data.
        // For free node this is where we store the free list element.
        struct list_node free;
        // Variable size.
        uint8_t data[0];
    };
} __attribute__((packed));

// Those are the TAGS of the nodes.
// FREE: This node is currenlty free, ie. its data is not in use.
#define FREE (0)
// ALLOCATED: This node is allocated, it contains data that is currently in use.
#define ALLOCATED (1)

// The size of a node header in bytes.
#define HEADER_SIZE (sizeof((struct node*)NULL)->header)
// The minimum allocation size. The reason behind this minimum is that the
// element of the free list is included in the data section of the node struct.
// Therefore every node need at least this amount of memory to contain this
// struct list_node in case it becomes a free node.
#define MIN_SIZE    (sizeof(struct node) - HEADER_SIZE)

// Allocate a new group. The new group is initially empty.
// @param size: The size of the group in pages.
// @return: The virtual address of the new group.
// Note: This function assumes that KMALLOC_LOCK is not held.
static struct group * create_group(size_t const size) {
    // The group must reside in kernel memory.
    void * const low = KERNEL_PHY_OFFSET;
    // Find `size` contiguous pages in kernel memory.

    // Allocate the requested amount of physical frames and map them to the
    // virtual address space. All groups pages are allocated above
    // KERNEL_PHY_OFFSET. This is to avoid filling the low addresses used by the
    // SMP code.
    void *frames[size];
    for (size_t i = 0; i < size; ++i) {
        frames[i] = alloc_frame();
    }
    void * const pages = paging_map_frames_above(low, frames, size, VM_WRITE);

    // Zero the pages to avoid random garbage.
    memzero(pages, size * PAGE_SIZE);

    // Initialize the new group.
    struct group * const group = pages;

    // Create a new free entry containing the entire group.
    struct node * const free = pages + sizeof(*group);
    free->header.tag = FREE;
    free->header.size = size * PAGE_SIZE - sizeof(*group) - HEADER_SIZE;
    list_init(&free->free);

    // Initialize the group's attributes: size, free, and the list nodes.
    group->size = free->header.size;
    group->num_pages = size;
    group->free = free->header.size;
    list_init(&group->group_list);
    list_init(&group->free_head);

    // Add the first free entry to the free list of the group.
    list_add(&group->free_head, &free->free);

    return group;
}

// Check if a group is empty, that is there are no allocated bytes in the
// physical memory area it describes.
// @param group: The group to test.
// @return: true if `group` is empty, false otherwise.
static inline bool group_is_empty(struct group const * const group) {
    // A group is empty if all of its bytes are free.
    return group->size == group->free;
}

// Deallocate a group.
// @param group: The group to de-allocate.
// Note: The group must not contain any allocated data.
// Note: This function assumes that KMALLOC_LOCK is not held.
static void free_group(struct group * const group) {
    // Check that nothing is left in the group before getting rid of it.
    ASSERT(group_is_empty(group));
    
    // Unmap the pages used by the group.
    void * const addr = (void*)group;
    uint32_t const len = group->num_pages * PAGE_SIZE;
    paging_unmap_and_free_frames(addr, len);
}

// Get the address of the data for a given node.
// @param node: The node to get the address of the data of.
// @return: The address of the data.
static void * node_data_start(struct node * const node) {
    return node->data;
}

// Find a free node in a group capable of holding a certain number of bytes.
// @param group: The group to search into.
// @param size: The minimum size of the free node to look for.
// @return: If such a free node exists then this function returns the virtual
// address of the node, otherwise it returns NULL.
static struct node * find_node_in_group(struct group const * const group,
                                          size_t const size) {
    struct node * ite = NULL;
    list_for_each_entry(ite, &group->free_head, free) {
        if (ite->header.size >= size) {
            return ite;
        }
    }
    return NULL;
}

// Allocate memory within a group.
// @param group: The group to allocate into.
// @param size: The size of the allocation.
// @return: On success, returns the virtual address of the allocated memeory,
// otherwise, returns NULL.
static void * kmalloc_in_group(struct group * const group, size_t size) {
    if (list_empty(&group->free_head)) {
        // The group has no more free nodes, nothing to do here.
        return NULL;
    }

    // Upgrade the size to the minimum size if necessary. This is mandatory as
    // the data part of the node is also used as the free list in case a node is
    // free.
    size = max_u32(size, MIN_SIZE);

    // Find a free node in the current group that would be big enough to
    // contain `size` bytes.
    struct node * const dest = find_node_in_group(group, size);
    if (!dest) {
        // No such nodes exists, abort now.
        return NULL;
    }

    // `dest` is big enough to hold the request, get the start address for it.
    void * const data_start = node_data_start(dest);
    // Fow now, assume we use the entire node, and update group->free
    // accordingly. The case in which we can split the current node into one
    // allocated node and a new free node is handled later.
    group->free -= dest->header.size;

    // This node is now marked as allocated.
    dest->header.tag = ALLOCATED;

    // Save the prev pointer of the dest node so that we can already remove it
    // from the free list. If a new free node is created, the prev pointer will
    // be useful to add it to the list.
    struct list_node * const prev = dest->free.prev;
    list_del(&dest->free);

    // Check if there is enough space between the end of the requested data and
    // the next entry to fit a new free node.
    bool const new_free_node = dest->header.size - size >= sizeof(*dest);
    if (new_free_node) {
        // Initialize the new free node.
        struct node* const node = data_start + size;
        node->header.tag = FREE;
        node->header.size = dest->header.size - size - HEADER_SIZE;
        list_init(&node->free);

        // Add the new free node to the free list at the same position as the
        // node that now contains data. To this end we use the prev pointer that
        // we saved before remove the old node.
        list_add(prev, &node->free);

        // Since we added a new free node to the group we need to update the
        // group->free accordingly.
        group->free += node->header.size;

        // Sanity check that we split the node in two without going over the
        // node's limits.
        ASSERT(dest->header.size == size + HEADER_SIZE + node->header.size);

        // Since we cut the destination node in two parts to insert a new free
        // node, we need to update its size.
        dest->header.size = size;
    }

    // Zero the allocated data before returning.
    memzero(data_start, size);
    return data_start;
}

// Test if a virtual address is part of a group.
// @param g: The group to test.
// @param addr: The virtual address to test.
// @return: true if addr is withing the group g, false otherwise.
static bool addr_in_group(struct group const * const g,
                          void const * const addr) {
    return (void*)g <= addr && addr < (void*)g + g->num_pages * PAGE_SIZE;
}

// Check two nodes can be merged to form a single node.
// @param a: The first node.
// @param b: The second node.
// @return: true if a and b can be merged (in this order) to form a bigger node.
// This is only the case if a and b are neighbours within the group.
static bool can_merge(struct node const * const a,
                      struct node const * const b) {
    // We can merge a with b if a spans all the way to b.
    return (void*)a + a->header.size + HEADER_SIZE == (void*)b;
}

// Check if a node is the first entry in the free list of a group.
// @param group: The group to test.
// @param node: The node to test.
// @return: true if the node is the first node in `group`s free list.
static bool first_in_free_list(struct group const * const group,
                               struct node const * const node) {
    return node == list_first_entry(&group->free_head, struct node, free);
}

// Check if a node is the last entry in the free list of a group.
// @param group: The group to test.
// @param node: The node to test.
// @return: true if the node is the last node in `group`s free list.
static bool last_in_free_list(struct group const * const group,
                              struct node const * const node) {
    return node == list_last_entry(&group->free_head, struct node, free);
}

// Get the previous neighbour of a free node in the free list.
// @param node: The node to get the neighbour from.
// @return: The address of the `prev` neighbour of `node`.
static struct node * prev_node(struct node const * const node) {
    return list_entry(node->free.prev, struct node, free);
}

// Get the next neighbour of a free node in the free list.
// @param node: The node to get the neighbour from.
// @return: The address of the `next` neighbour of `node`.
static struct node * next_node(struct node const * const node) {
    return list_entry(node->free.next, struct node, free);
}

// Get the node corresponding to an allocated address.
// @param addr: The virtual address of the data of the node to look for.
// @return: The address of the node containing `addr`.
// Important node: The approach here assumes that addr is a legitimate allocated
// address. This means that it is not possible to give an address that points in
// the middle of a node. It _must_ be the address of the very first byte of data
// of a node.
static struct node * node_for_addr(void * const addr) {
    return addr - HEADER_SIZE;
}

// Try to merge a free node with its neighbour within a group.
// @param group: The group.
// @param node: The free node to merge (if applicable).
static void merge_free_node(struct group * const group,
                            struct node * node) {
    struct node * const prev = prev_node(node);
    if (!first_in_free_list(group, node)) {
        // This node is not the first node in the free list and therefore has a
        // neighbour before it (prev). Check if we can merge to two nodes to
        // create one bigger free node.
        if (can_merge(prev, node)) {
            // This node can be merged with the neighbour before it. Remove it
            // from the free list and update the neighbour's size to contain
            // this node.
            list_del(&node->free);
            prev->header.size += node->header.size + HEADER_SIZE;

            // Since we end up with one less free node (merged), we gained
            // HEADER_SIZE free bytes.
            group->free += HEADER_SIZE;

            // Set the node to prev for the merge with the next neighbour below.
            node = prev;
        }
    }
    if (!last_in_free_list(group, node)) {
        // This node (or the resulting node after the merge above) is not at the
        // end of the list and therefore has a next neighbour with which it
        // might be able to merge. Do it if we can.
        struct node * const next = next_node(node);
        merge_free_node(group, next);
    }
}

// Insert a node in the group's free list.
// @param group: The group.
// @parma node: The node to insert.
// Note: The node need not to be marked FREE already, as this function will do
// it.
static void insert_in_free_list(struct group * const group,
                                struct node * const node) {
    // Mark the node as FREE.
    node->header.tag = FREE;

    // By releasing this allocation we increase the available space in the
    // group.
    group->free += node->header.size;

    // Now insert in the free list in order of addresses. Find the node in the
    // free list that should follow the freed node.
    struct list_node * next; 
    list_for_each(next, &group->free_head) {
        if (next > &node->free) {
            break;
        }
    }

    // Now insert `node` in the free list right before `next`.
    list_add_tail(next, &node->free);

    // Try to merge this new node in the free list with its neighbours. This
    // reduces fragmentation.
    merge_free_node(group, node);
}

// Release the data of an allocation within a group.
// @param group: The group.
// @param addr: The address of the allocated memory to free.
// Note: The address _must_ point to the very first byte of the allocated
// memory, giving an address in the middle will not work.
static void kfree_in_group(struct group * const group, void * const addr) {
    ASSERT(addr_in_group(group, addr));

    // Get the node for the allocated data.
    struct node * const node = node_for_addr(addr);
    ASSERT(node->header.tag == ALLOCATED);

    // Insert the node in the free list, this will mark it as FREE and
    // relinquish the allocated data.
    insert_in_free_list(group, node);
}

// Allocate memory in the first group available in `group_list`.
// @param group_list: A linked list of groups to try to allocate into. The
// particular group that will contain the allocation is the first group (in the
// order of this list) that can hold the requested amount.
// @param size: The size of the allocation.
// @return: The address of the allocated memory if the allocation is successful,
// NULL if no group from the group_list can contain such an allocation.
static void *try_allocation(struct list_node * group_list, size_t const size) {
    struct group * group;
    list_for_each_entry(group, group_list, group_list) {
        if (group->free < size) {
            // The group does not have enough space to allocate that much.
            continue;
        }

        // This group is a good candidate, try to allocate in it.
        void * const addr = kmalloc_in_group(group, size);
        if (addr) {
            return addr;
        }
    }
    // Not enough space in all the group.
    return NULL;
}

// The spinlock that must be held while performing any operation in the dynamic
// memory allocator.
DECLARE_SPINLOCK(KMALLOC_LOCK);

// Allocate memory in the first group available in `group_list`.
// @param group_list: A linked list of groups to try to allocate into. The
// particular group that will contain the allocation is the first group (in the
// order of this list) that can hold the requested amount.
// @param size: The size of the allocation.
// @return: The address of the allocated memory.
static void * do_kmalloc(struct list_node * group_list, size_t const size) {
    void * const addr = try_allocation(group_list, size);
    if (addr) {
        return addr;
    } else {
        // Either we do not have a group right now or no group is big enough to
        // contain the allocation. Try to allocate a new one.

        // Unlock the KMALLOC_LOCK while we are allocating a new group. The
        // reason is that create_group will modify the page tables and therefore
        // may execute a TLB shootdown. This could cause a deadlock if remote
        // cpus need to acquire the kmalloc lock while processing their messages
        // (if another message is enqueued alongside the TLB-shootdown message).
        spinlock_unlock(&KMALLOC_LOCK);

        struct group * group;
        uint32_t const num_pages = ceil_x_over_y_u32(size + sizeof(group) +
            HEADER_SIZE, PAGE_SIZE);
        group = create_group(num_pages);

        spinlock_lock(&KMALLOC_LOCK);

        // While we released the group, some space might have been freed in the
        // existing group or another cpu created a new group. Check if we can
        // allocate before actually adding the new group.
        void * const addr = try_allocation(group_list, size);
        if (addr) {
            // The allocation was successful, no need to add the group.

            // Note: Because free_group assumes that KMALLOC_LOCK is not held we
            // need to release, call free_group and re-acquire KMALLOC_LOCK.
            // This is ok here, the allocation has already been done.
            spinlock_unlock(&KMALLOC_LOCK);
            free_group(group);
            spinlock_lock(&KMALLOC_LOCK);

            return addr;
        } else {
            // There is still no space for the allocation, add the new group and
            // allocate in it.

            // Add the group to the group list.
            list_add_tail(group_list, &group->group_list);

            // Since we allocated the group to at least hold this allocation we
            // must be able to find a node in it. Also we already know that all
            // other group cannot hold such an allocation so only check this
            // one.
            void * const addr = kmalloc_in_group(group, size);
            ASSERT(addr);
            return addr;
        }
    }
}

// Free allocated memory.
// @param group_list: A list of group to look into to de-allocate the memory.
// This function will look for the group containing the address to de-allocate.
// @param addr: The address to de-allocated.
static void do_kfree(struct list_node * group_list, void * const addr) {
    struct group * group;
    list_for_each_entry(group, group_list, group_list) {
        if (addr_in_group(group, addr)) {
            kfree_in_group(group, addr);

            // If removing the allocation from the group makes it empty,
            // deallocate the group to free physical frames.
            if (group_is_empty(group)) {
                // Remote the group from the group list before freeing it. Note:
                // This is safe to do so in the loop since we will return
                // anyway.
                list_del(&group->group_list);

                // Unlock the KMALLOC_LOCK while we are allocating a new group.
                // The reason is that free_group will modify the page tables
                // and therefore may execute a TLB shootdown. This could cause a
                // deadlock if remote cpus need to acquire the kmalloc lock
                // while processing their messages (if another message is
                // enqueued alongside the TLB-shootdown message).
                spinlock_unlock(&KMALLOC_LOCK);
                free_group(group);
                spinlock_lock(&KMALLOC_LOCK);
            }
            return;
        }
    }
    PANIC("Unknow pointer to free.");
}

// The global kernel list of groups.
struct list_node GROUP_LIST;

// Keep track if the GROUP_LIST has been initialized. The dynamic memory
// allocator is somewhat lazily initialized, that is there is no explicit init
// function. Instead, the first call to kmalloc will initialize the state.
static bool KMALLOC_INITIALIZED = false;

// This is the public interface for the dynamic memory allocation.
// @param size: The requested amount of memory.
// @return: The virtual address of the allocated memory.
void * kmalloc(size_t const size) {
    spinlock_lock(&KMALLOC_LOCK);
    if (!KMALLOC_INITIALIZED) {
        // Initialize the group list if it wasn't done already.
        list_init(&GROUP_LIST);
        KMALLOC_INITIALIZED = true;
    }
    void * const addr = do_kmalloc(&GROUP_LIST, size);
    spinlock_unlock(&KMALLOC_LOCK);
    return addr;
}

// Public interface to free dynamically allocated memory.
// @param addr: The address of the memory to free. This cannot point in the
// middle of an allocated buffer. It _must_ point to the very first byte of the
// allocated buffer.
void kfree(void * const addr) {
    spinlock_lock(&KMALLOC_LOCK);
    ASSERT(KMALLOC_INITIALIZED);
    do_kfree(&GROUP_LIST, addr);
    spinlock_unlock(&KMALLOC_LOCK);
}

// Compute the number of total bytes currently allocated through kmalloc.
size_t kmalloc_total_allocated(void) {
    spinlock_lock(&KMALLOC_LOCK);

    size_t tot = 0;
    struct group *group;
    list_for_each_entry(group, &GROUP_LIST, group_list) {
        tot += group->size - group->free;
    }

    spinlock_unlock(&KMALLOC_LOCK);
    return tot;
}

#include <kmalloc.test>
