#include <memory.h>
#include <kmalloc.h>
#include <debug.h>
#include <kernel_map.h>

void memcpy(void * const to, void const * const from, size_t const len) {
    uint8_t * const __to = (uint8_t*)to;
    uint8_t const * const __from = (uint8_t*)from;
    for (size_t i = 0; i < len; ++i) {
        __to[i] = __from[i];
    }
}

void memset(void * const to, uint8_t const byte, size_t const len) {
    uint8_t * const __to = (uint8_t*)to;
    for (size_t i = 0; i < len; ++i) {
        __to[i] = byte;
    }
}

void memzero(void * const to, size_t const len) {
    memset(to, 0, len);
}

bool memeq(void const * const s1, void const * const s2, size_t const size) {
    uint8_t const * __s1 = (uint8_t const *)s1;
    uint8_t const * __s2 = (uint8_t const *)s2;
    for (size_t i = 0; i < size; ++i) {
        if (*__s1 != *__s2) {
            return false;
        }
        __s1 ++;
        __s2 ++;
    }
    return true;
}

void *memdup(void const * const buf, size_t const len) {
    void * const dup_buf = kmalloc(len);
    if (dup_buf) {
        memcpy(dup_buf, buf, len);
    }
    return dup_buf;
}

// Compute the address in the current address space and mode (PM flat, PM higher
// half, paging) resolving to a physical address.
// @param paddr: The target physical address.
// @return: An address that resolve to `paddr`.
static void *get_adjusted_addr(void * const paddr) {
    if (cpu_paging_enabled()) {
        // TODO: For now only allow addresses under the physical address where
        // the kernel has been mapped, this is because those addresses are
        // mapped to higher half. For any other addresses we would need to map
        // them first.
        ASSERT(paddr < to_phys(KERNEL_END_ADDR));
        return to_virt(paddr);
    } else {
        // Check if we are using the boot GDT (that is higher half mapping) or
        // the GDT from the bootloader.
        if (in_higher_half()) {
            // We are using the boot GDT, and paging is not yet enabled. Use the
            // virtual address resolving to the desired physical address.
            return to_virt(paddr);
        } else {
            // We are not in higher half, the segments from the bootloader are
            // assumed to be flat (TODO) and hence we can use the address as is.
            return paddr;
        }
    }

    __UNREACHABLE__;
    return NULL;
}

void phy_read(void const * const addr, void * const dest, size_t const size) {
    void const * const adj = get_adjusted_addr((void*)addr);
    memcpy(dest, adj, size);
}

void phy_write(void * const addr, void const * const buf, size_t const size) {
    void * const adj = get_adjusted_addr(addr);
    memcpy(adj, buf, size);
}

// Testing.
#include <memory.test>
