#ifndef VM_ANON_H
#define VM_ANON_H
#include "vm/vm.h"
struct page;
enum vm_type;

struct anon_page {
    /* swap space location */
	size_t bit_idx;

    /* disk location */
    uint32_t disk_location;
};

void vm_anon_init (void);
bool anon_initializer (struct page *page, enum vm_type type, void *kva);

#endif
