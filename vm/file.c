/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "include/threads/mmu.h"
#include "include/userprog/process.h"
#include <string.h>
static bool file_backed_swap_in (struct page *page, void *kva);
static bool file_backed_swap_out (struct page *page);
static void file_backed_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations file_ops = {
	.swap_in = file_backed_swap_in,
	.swap_out = file_backed_swap_out,
	.destroy = file_backed_destroy,
	.type = VM_FILE,
};

/* The initializer of file vm */
void
vm_file_init (void) {
}

/* Initialize the file backed page */
bool
file_backed_initializer (struct page *page, enum vm_type type, void *kva) {
	/* Set up the handler */
	page->operations = &file_ops;

	struct file_page *file_page = &page->file;
	struct load_segment_para *lsp = page->uninit.aux;
	file_page->file = lsp->file;
	file_page->ofs = lsp->ofs;
	file_page->read_bytes = lsp->read_bytes;
	file_page->upage = lsp->upage;
	file_page->writable = lsp->writable;
	file_page->zero_bytes = lsp->zero_bytes;

	return true;
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
	//printf("file swap in\n");

	return lazy_load_segment(page, file_page);
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
	//printf("file swap out\n");

	if(pml4_is_dirty(thread_current()->pml4, page->va)){
		file_seek(file_page->file, file_page->ofs);
		file_write_at(file_page->file, page->frame->kva, file_page->read_bytes, file_page->ofs);
		pml4_set_dirty(thread_current()->pml4, page->va, false);
	}


	page->frame->page = NULL;
	page->frame = NULL;
	pml4_clear_page(thread_current()->pml4, page->va);

	return true;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	
	struct file_page *file_page UNUSED = &page->file;

	if(pml4_is_dirty(thread_current()->pml4, page->va)){
		file_write_at(file_page->file, file_page->upage, file_page->read_bytes, file_page->ofs);
		pml4_set_dirty(thread_current()->pml4, page->va, false);
	}

	if(page->frame){
		list_remove(&page->frame->elem);
		page->frame->page = NULL;
        free(page->frame);
        page->frame = NULL;
		
	}
	
	pml4_clear_page(thread_current()->pml4, page->va);
	hash_delete(&thread_current()->spt.spt_table, &page->elem);
}

/* Do the mmap */
void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {
}

/* Do the munmap */
void
do_munmap (void *addr) {
}
