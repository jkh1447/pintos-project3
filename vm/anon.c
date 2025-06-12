/* anon.c: Implementation of page for non-disk image (a.k.a. anonymous page). */

#include "vm/vm.h"
#include "devices/disk.h"
#include "lib/kernel/bitmap.h"
#include "include/threads/vaddr.h"
#include "include/devices/disk.h"
//swap slot의 개수: 20160개의 섹터(512byte), 1페이지 크기인 4096byte로 나눔.

/* bitmap */
struct bitmap* b;


/* DO NOT MODIFY BELOW LINE */
static struct disk *swap_disk;
static bool anon_swap_in (struct page *page, void *kva);
static bool anon_swap_out (struct page *page);
static void anon_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations anon_ops = {
	.swap_in = anon_swap_in,
	.swap_out = anon_swap_out,
	.destroy = anon_destroy,
	.type = VM_ANON,
};

/* Initialize the data for anonymous pages */
void
vm_anon_init (void) {
	/* TODO: Set up the swap_disk. */
	swap_disk = disk_get(1, 1);
	if(!swap_disk) PANIC("swap disk get fail.");
	// printf("hd1:1 sector size: %d\n", disk_size(swap_disk));
	/* bitmap init */
	b = bitmap_create(disk_size(swap_disk));
}

/* Initialize the file mapping */
bool
anon_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	// struct uninit_page *uninit = &page->uninit;
    // memset(uninit, 0, sizeof(struct uninit_page));
	page->operations = &anon_ops;

	struct anon_page *anon_page = &page->anon;
	// anon_page->bit_idx = BITMAP_ERROR;
	/* anon_page 멤버 변수들 초기화 */
	return true;	
}

/* Swap in the page by read contents from the swap disk. */
static bool
anon_swap_in (struct page *page, void *kva) {
	//printf("swap in\n");
	struct anon_page *anon_page = &page->anon;
	/* 페이지에 대한 시작 섹터 */
	disk_sector_t sec_num = anon_page->disk_location;
	
	void *t_kva = page->frame->kva;
	//printf("swap in target kva: %p\n", t_kva);
	/* 할당한 프레임(kva)에 디스크에서 불러와서 쓰기. */
	for(int i=0; i<(PGSIZE / DISK_SECTOR_SIZE); i++){
		disk_read(swap_disk, sec_num+i, t_kva);
		t_kva += DISK_SECTOR_SIZE;
	}
	/* free swap slot */
	bitmap_set(b, anon_page->bit_idx, false);
	
	//printf("here\n");
	/* set is_swapped false */
	page->is_swapped = false;

	/* kva를 연결해야 하나? */
	return true;
}

/* Swap out the page by writing contents to the swap disk. */
static bool
anon_swap_out (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	//printf("swap out\n");
	/* find free swap slot */
	size_t free_slot = bitmap_scan_and_flip(b, 0, 1, false);
	if(free_slot == BITMAP_ERROR) PANIC("there is no more free slot");
	
	/* 페이지의 시작 섹터 */
	disk_sector_t sec_num = free_slot * (PGSIZE / DISK_SECTOR_SIZE);
	
	void *kva = page->frame->kva;
	/* swap slot에 저장 */
	for(int i=0; i<(PGSIZE / DISK_SECTOR_SIZE); i++){
		disk_write(swap_disk, sec_num + i, page->va + DISK_SECTOR_SIZE * i);
		kva += DISK_SECTOR_SIZE;
	}
	
	/* save the location in disk. */
	anon_page->disk_location = sec_num;
	/* save the location in the swap space. */
	anon_page->bit_idx = free_slot;
	
	pml4_clear_page(thread_current()->pml4, page->va);
	return true;
}

/* Destroy the anonymous page. PAGE will be freed by the caller. */
static void
anon_destroy (struct page *page) {
	struct anon_page *anon_page = &page->anon;
	if(page->frame)
		palloc_free_page(page->frame->kva);
	pml4_clear_page(thread_current()->pml4, page->va);
	free(page->frame);
}
