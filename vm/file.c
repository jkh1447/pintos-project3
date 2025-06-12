/* file.c: Implementation of memory backed file object (mmaped object). */

#include "vm/vm.h"
#include "include/threads/vaddr.h"
#include "include/userprog/process.h"

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
}

/* Swap in the page by read contents from the file. */
static bool
file_backed_swap_in (struct page *page, void *kva) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Swap out the page by writeback contents to the file. */
static bool
file_backed_swap_out (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Destory the file backed page. PAGE will be freed by the caller. */
static void
file_backed_destroy (struct page *page) {
	struct file_page *file_page UNUSED = &page->file;
}

/* Do the mmap */
bool
lazy_file_segment(struct page *page, void *aux)
{
	/* TODO: Load the segment from the file */
	/* TODO: This called when the first page fault occurs on address VA. */
	/* TODO: VA is available when calling this function. */
	struct lazy_load_info *info = (struct lazy_load_info *)aux;

	struct file *file = info->file;
	off_t offset = info->ofs;
	size_t page_read_bytes = info->page_read_bytes;
	size_t page_zero_bytes = info->page_zero_bytes;
	uint8_t *kva = page->frame->kva;

	// 파일에서 읽어오기
	if (file_read_at(file, kva, page_read_bytes, offset) != (int)page_read_bytes)
		return false;

	// 남은 영역 0으로 채우기
	memset(kva + page_read_bytes, 0, page_zero_bytes);

	// 페이지의 writable 플래그 설정
	page->writable = info->writable;

	// 페이지 매핑
	return true;
}

void *
do_mmap (void *addr, size_t length, int writable,
		struct file *file, off_t offset) {

	off_t file_size = file_length(file);
	uint32_t read_bytes = file_size - offset;
	uint32_t zero_bytes = ((length + PGSIZE - 1) / PGSIZE * PGSIZE) - read_bytes;

	void *new_page = addr;

	while (read_bytes > 0 || zero_bytes > 0)
	{
		/* Do calculate how to fill this page.
		 * We will read PAGE_READ_BYTES bytes from FILE
		 * and zero the final PAGE_ZERO_BYTES bytes. */
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;

		/* TODO: Set up aux to pass information to the lazy_load_segment. */
		void *aux = NULL;
		struct lazy_load_info *lli = malloc(sizeof(struct lazy_load_info));
		if (lli == NULL)
			return false;
		lli->file = file;
		lli->ofs = offset;
		lli->upage = addr;
		lli->page_read_bytes = page_read_bytes;
		lli->page_zero_bytes = page_zero_bytes;
		lli->writable = writable;
		aux = lli;

		if (!vm_alloc_page_with_initializer(VM_FILE, addr,
											writable, lazy_file_segment, aux))
			return false;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		addr += PGSIZE;

		/* file_read를 하지 않기 때문에 수동으로 ofs을 이동시켜줘야 한다. */
		offset += page_read_bytes;
	}
	return new_page;
}
	

/* Do the munmap */
void
do_munmap (void *addr) {
}
