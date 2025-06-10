/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "hash.h"

#include "include/threads/vaddr.h"
#include "include/threads/mmu.h"
#include "include/threads/thread.h"

#define STACK_MAX_SIZE (1 << 20)
#define STACK_BOTTOM (USER_STACK - STACK_MAX_SIZE)

/*해쉬값을 반환해주는 함수*/
unsigned
page_hash(const struct hash_elem *p_, void *aux UNUSED)
{
	const struct page *p = hash_entry(p_, struct page, hash_elem);
	return hash_bytes(&p->va, sizeof p->va);
}

/*해쉬에 추가 할때 순서대로 비교하는 함수*/
bool page_less(const struct hash_elem *a_,
			   const struct hash_elem *b_, void *aux UNUSED)
{
	const struct page *a = hash_entry(a_, struct page, hash_elem);
	const struct page *b = hash_entry(b_, struct page, hash_elem);

	return a->va < b->va;
}

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void vm_init(void)
{
	vm_anon_init();
	vm_file_init();
#ifdef EFILESYS /* For project 4 */
	pagecache_init();
#endif
	register_inspect_intr();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
}

/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type(struct page *page)
{
	int ty = VM_TYPE(page->operations->type);
	switch (ty)
	{
	case VM_UNINIT:
		return VM_TYPE(page->uninit.type);
	default:
		return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim(void);
static bool vm_do_claim_page(struct page *page);
static struct frame *vm_evict_frame(void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage, bool writable,
									vm_initializer *init, void *aux)
{

	ASSERT(VM_TYPE(type) != VM_UNINIT)

	struct supplemental_page_table *spt = &thread_current()->spt;

	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page(spt, upage) == NULL)
	{
		/* TODO: Create the page, fetch the initialier according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page *page = calloc(1, sizeof(struct page));
		// 나중에처리
		if (page == NULL)
			return false;
		bool (*page_initializer)(struct page *page, enum vm_type type, void *kva);
		switch (VM_TYPE(type))
		{
		case VM_ANON:
			page_initializer = anon_initializer;
			break;
		case VM_FILE:
			page_initializer = file_backed_initializer;
			break;
		default:
			goto err;
		}
		/* uninit page 초기화 */
		uninit_new(page, upage, init, type, aux, page_initializer);

		page->writable = writable;
		/* TODO: Insert the page into the spt. */
		if (!spt_insert_page(spt, page))
			goto err;

		return true;
	}
err:
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page(struct supplemental_page_table *spt, void *va)
{
	struct page *page = NULL;
	struct hash *h = &spt->spt_hash;
	struct page tmp_page;
	tmp_page.va = pg_round_down(va);

	struct hash_elem *he = hash_find(h, &tmp_page.hash_elem);
	if (he != NULL)
		page = hash_entry(he, struct page, hash_elem);
	return page;
}

/* Insert PAGE into spt with validation. */
bool spt_insert_page(struct supplemental_page_table *spt UNUSED,
					 struct page *page UNUSED)
{
	/* TODO: Fill this function. */
	struct hash_elem *he = hash_insert(&spt->spt_hash, &page->hash_elem);

	return he == NULL;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page)
{
	vm_dealloc_page(page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim(void)
{
	struct frame *victim = NULL;
	/* TODO: The policy for eviction is up to you. */

	return victim;
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
static struct frame *
vm_evict_frame(void)
{
	struct frame *victim UNUSED = vm_get_victim();
	/* TODO: swap out the victim and return the evicted frame. */

	return NULL;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame(void)
{
	struct frame *frame = NULL;
	/* swap 을 구현하기 전에는 할당 실패시 PANIC */
	frame = calloc(1, sizeof(struct frame));
	if (frame == NULL)
		PANIC("todo");
	void *p = palloc_get_page(PAL_USER);
	if (p == NULL)
		PANIC("todo");

	frame->kva = p;

	ASSERT(frame != NULL);
	ASSERT(frame->page == NULL);
	return frame;
}

/* Growing the stack. */
static void
vm_stack_growth(void *fault_addr)
{
	void *pg = pg_round_down(fault_addr);
	if (pg < STACK_BOTTOM)
		return; // 최대 크기 초과

	if (!vm_alloc_page_with_initializer(VM_ANON, pg, true, NULL, NULL))
		return;
	vm_claim_page(pg);
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp(struct page *page UNUSED)
{
}

/* Return true on success */
/* 첫 번째 fault발생시 page에 물리 프레임할당해서 연결 */
bool vm_try_handle_fault(struct intr_frame *f UNUSED, void *addr UNUSED,
						 bool user UNUSED, bool write UNUSED, bool not_present UNUSED)
{
	struct supplemental_page_table *spt UNUSED = &thread_current()->spt;
	struct page *page = NULL;
	// printf("vm fault handler addr: %p\n", addr);
	// void *va = pg_round_down(addr);
	/* TODO: Validate the fault */
	/* TODO: Your code goes here */
	/* valid address인지 확인 */
	// 유저모드에서 발생한 인터럽트인지 체크
	page = spt_find_page(spt, addr);
	// printf("page->va: %p\n", page->va);

	if (page == NULL)
	{
		if (not_present)
		{
			void *rsp = user ? f->rsp : thread_current()->rsp;
			// printf("user mode interrupt\n");

			if (((USER_STACK - (1 << 20)) <= addr && rsp < addr && addr <= USER_STACK) ||
				((USER_STACK - (1 << 20)) <= addr && addr == rsp - 8 && addr <= USER_STACK))
			{
				// 폴트난 addr에서 가장 가까운 1페이지 주소로 내림
				vm_stack_growth(pg_round_down(addr));

				return true;
			}
		}
		return false;
	}

	if (write && !page->writable)
	{
		return false; // 쓰기 권한이 없는데 쓰기 요청 → 거절
	}

	enum vm_type type = page->operations->type;
	if (VM_TYPE(type) != VM_UNINIT)
		return false;
	// printf("do claim\n");
	return vm_do_claim_page(page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page)
{
	destroy(page);
	free(page);
}

/* Claim the page that allocate on VA. */
bool vm_claim_page(void *va UNUSED)
{
	struct page *page = NULL;
	/* TODO: Fill this function */
	page = spt_find_page(&thread_current()->spt, va);

	return vm_do_claim_page(page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page(struct page *page)
{
	struct frame *frame = vm_get_frame();

	/* Set links */
	frame->page = page;
	page->frame = frame;

	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable);

	return swap_in(page, frame->kva);
}

/* Initialize new supplemental page table */
void supplemental_page_table_init(struct supplemental_page_table *spt UNUSED)
{
	hash_init(spt, page_hash, page_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool supplemental_page_table_copy(struct supplemental_page_table *dst UNUSED,
								  struct supplemental_page_table *src UNUSED)
{
	struct hash_iterator i;
	hash_first(&i, &src->spt_hash);
	while (hash_next(&i))
	{
		// src_page 정보
		struct page *src_page = hash_entry(hash_cur(&i), struct page, hash_elem);
		enum vm_type type = src_page->operations->type;
		void *upage = src_page->va;
		bool writable = src_page->writable;

		/* 1) type이 uninit이면 */
		if (type == VM_UNINIT)
		{ // uninit page 생성 & 초기화
			vm_initializer *init = src_page->uninit.init;
			void *aux = src_page->uninit.aux;
			vm_alloc_page_with_initializer(VM_ANON, upage, writable, init, aux);
			continue;
		}

		/* 2) type이 uninit이 아니면 */
		if (!vm_alloc_page_with_initializer(type, upage, writable, NULL, NULL)) // uninit page 생성 & 초기화
			// init(lazy_load_segment)는 page_fault가 발생할때 호출됨
			// 지금 만드는 페이지는 page_fault가 일어날 때까지 기다리지 않고 바로 내용을 넣어줘야 하므로 필요 없음
			return false;

		// vm_claim_page으로 요청해서 매핑 & 페이지 타입에 맞게 초기화
		if (!vm_claim_page(upage))
			return false;

		// 매핑된 프레임에 내용 로딩
		struct page *dst_page = spt_find_page(dst, upage); 
		memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
	}
	return true;
}

void hash_page_destroy(struct hash_elem *e)
{
}

/* Free the resource hold by the supplemental page table */
void supplemental_page_table_kill(struct supplemental_page_table *spt UNUSED)
{
	/* TODO: Destroy all the supplemental_page_table hold by thread and
	 * TODO: writeback all the modified contents to the storage. */
	hash_clear(&spt->spt_hash, hash_page_destroy);
}
