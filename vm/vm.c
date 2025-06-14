/* vm.c: Generic interface for virtual memory objects. */

#include "threads/malloc.h"
#include "vm/vm.h"
#include "vm/inspect.h"
#include "include/threads/vaddr.h"
#include "include/threads/mmu.h"
#include "include/threads/thread.h"

/* frame table */
struct list frame_table;
struct list_elem *clock_hand;

struct lock frame_lock;

void frame_table_init(void);

/* Initializes the virtual memory subsystem by invoking each subsystem's
 * intialize codes. */
void
vm_init (void) {
	vm_anon_init ();
	vm_file_init ();
#ifdef EFILESYS  /* For project 4 */
	pagecache_init ();
#endif
	register_inspect_intr ();
	/* DO NOT MODIFY UPPER LINES. */
	/* TODO: Your code goes here. */
	frame_table_init();
	lock_init(&frame_lock);
}

/* frame table */
void frame_table_init(){
	list_init(&frame_table);
}


/* Get the type of the page. This function is useful if you want to know the
 * type of the page after it will be initialized.
 * This function is fully implemented now. */
enum vm_type
page_get_type (struct page *page) {
	int ty = VM_TYPE (page->operations->type);
	switch (ty) {
		case VM_UNINIT:
			return VM_TYPE (page->uninit.type);
		default:
			return ty;
	}
}

/* Helpers */
static struct frame *vm_get_victim (void);
static bool vm_do_claim_page (struct page *page);
static struct frame *vm_evict_frame (void);

/* Create the pending page object with initializer. If you want to create a
 * page, do not create it directly and make it through this function or
 * `vm_alloc_page`. */
// userprog을 load할 때 호출하게 됨. 무조건 VM_UNINIT으로 초기화
// 보충 페이지를 할당함.
bool
vm_alloc_page_with_initializer (enum vm_type type, void *upage, bool writable,
		vm_initializer *init, void *aux) {

	// vm_type이 VM_UNINIT이면 안된다.
	ASSERT (VM_TYPE(type) != VM_UNINIT)
	// printf("vm_alloc_page_with_initializer\n");
	// printf("type: %d\n", type);
	// printf("upage: %p\n", upage);
	// printf("writable: %d\n", writable);
	struct supplemental_page_table *spt = &thread_current ()->spt;
			
	/* Check wheter the upage is already occupied or not. */
	if (spt_find_page (spt, upage) == NULL) {
		/* TODO: Create the page, fetch the initializer according to the VM type,
		 * TODO: and then create "uninit" page struct by calling uninit_new. You
		 * TODO: should modify the field after calling the uninit_new. */
		struct page* page = calloc(1, sizeof(struct page));
		// 나중에처리
		if(page == NULL) return false;
		bool (*page_initializer) (struct page *page, enum vm_type type, void *kva);
		switch(VM_TYPE(type)){
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
		
		//printf("writable: %d\n", writable);
		page->writable = writable;
		
		/* TODO: Insert the page into the spt. */
		if(!spt_insert_page(spt, page))
			goto err;
		
		return true;
	}

	
err:
	//printf("err\n");
	return false;
}

/* Find VA from spt and return page. On error, return NULL. */
struct page *
spt_find_page (struct supplemental_page_table *spt UNUSED, void *va UNUSED) {
	struct page *page = NULL;
	struct hash *h = &spt->spt_table;
	struct page tmp_page;
	tmp_page.va = pg_round_down(va);

	struct hash_elem *he = hash_find(h, &tmp_page.elem);
	if(he != NULL)
		page = hash_entry(he, struct page, elem);
	return page;
}

/* Insert PAGE into spt with validation. */
bool
spt_insert_page (struct supplemental_page_table *spt UNUSED,
		struct page *page UNUSED) {

	/* TODO: Fill this function. */
	struct hash_elem *he = hash_insert(&spt->spt_table, &page->elem);

	return he == NULL;
}

void
spt_remove_page (struct supplemental_page_table *spt, struct page *page) {
	hash_delete(&spt->spt_table, &page->elem);
	vm_dealloc_page (page);
	return true;
}

/* Get the struct frame, that will be evicted. */
static struct frame *
vm_get_victim (void) {
	// lock_acquire(&frame_lock);
	// struct frame *victim = NULL;
	// //printf("vm_get_victim\n");
	// /* clock algorithm */
	// if(clock_hand == NULL) clock_hand = list_begin(&frame_table);
	// // printf("get victim here\n");
	// // printf("clock hand: %p\n", clock_hand);	
	// // printf("frame table end: %p\n", list_end(&frame_table));
	// // printf("frame table begin: %p\n", list_begin(&frame_table));
	// struct thread *curr = thread_current();
	// while(true){
	// 	if(clock_hand == list_end(&frame_table)){
	// 		clock_hand = list_begin(&frame_table);
	// 	}
	// 	// printf("clock hand: %p\n", clock_hand);	
	// 	struct frame *f = list_entry(clock_hand, struct frame, elem);

	// 	if(pml4_is_accessed(curr->pml4, f->page->va)){
	// 		pml4_set_accessed(curr->pml4, f->page->va, false);
	// 		clock_hand = list_next(clock_hand);
	// 	}
	// 	else{
	// 		victim = list_entry(clock_hand, struct frame, elem);
	// 		clock_hand = list_next(clock_hand);
	// 		//printf("here\n");
	// 		lock_release(&frame_lock);
	// 		return victim;
	// 	}
		
	// }
	// lock_release(&frame_lock);
	// return NULL;

	struct frame *victim = NULL;
    /* TODO: The policy for eviction is up to you. */
    struct thread *curr = thread_current();

    // Second Chance 방식으로 결정
    struct list_elem *e = list_begin(&frame_table);
    for (e; e != list_end(&frame_table); e = list_next(e)) {
        victim = list_entry(e, struct frame, elem);
        if (pml4_is_accessed(curr->pml4, victim->page->va))
            pml4_set_accessed(curr->pml4, victim->page->va, false);  // pml4가 최근에 사용됐다면 기회를 한번 더 준다.
        else
            return victim;
    }

    return list_entry(list_begin(&frame_table), struct frame, elem);
}

/* Evict one page and return the corresponding frame.
 * Return NULL on error.*/
// 희생자 프레임을 골라서 해당 프레임을 swap out 하고
// frame을 free할 필요 없는것 같다. 다시 쓴다.
// pml4에서의 연결만 해제해준다.
static struct frame *
vm_evict_frame (void) {
	struct frame *victim UNUSED = vm_get_victim ();
	if(!victim) return NULL;
	
	
	struct page *page = victim->page;
	/* swap out */
	swap_out(page);
	// 매핑 해제
	pml4_clear_page(thread_current()->pml4, page->va);
	
	page->frame = NULL;
	victim->page = NULL;
	
	//victim->page->frame = NULL;

	return victim;
}

/* palloc() and get frame. If there is no available page, evict the page
 * and return it. This always return valid address. That is, if the user pool
 * memory is full, this function evicts the frame to get the available memory
 * space.*/
static struct frame *
vm_get_frame (void) {
	
	struct frame *frame = NULL;
	/* swap 을 구현하기 전에는 할당 실패시 PANIC */
	frame = calloc(1, sizeof(struct frame));
	if(frame == NULL) PANIC("frame allocate fail");
	

	void *p = palloc_get_page(PAL_USER);
	if(p == NULL) {
		struct frame *f = vm_evict_frame();
		// printf("return evicted frame\n");
		return f;
	}

	frame->kva = p;

	list_push_back(&frame_table, &frame->elem);

	ASSERT (frame != NULL);
	ASSERT (frame->page == NULL);
	return frame;
}



static void
vm_stack_growth (void *addr UNUSED) {

	// printf("stack growth addr: %p\n", addr);
	
	//printf("stack growth\n");
	vm_alloc_page_with_initializer(VM_ANON | VM_MARKER_0 , addr, true, NULL, NULL);
	//printf("after vm alloc: %d\n", success);
	vm_claim_page(addr);

	
}

/* Handle the fault on write_protected page */
static bool
vm_handle_wp (struct page *page UNUSED) {
}

/* Return true on success */
/* 첫 번째 fault발생시 page에 물리 프레임할당해서 연결 */
bool
vm_try_handle_fault (struct intr_frame *f UNUSED, void *addr UNUSED,
		bool user UNUSED, bool write UNUSED, bool not_present UNUSED) {
	struct supplemental_page_table *spt UNUSED = &thread_current ()->spt;
	struct page *page = NULL;
	// printf("vm fault handler addr: %p\n", addr);

	
	/* valid address인지 확인 */
	page = spt_find_page(spt, addr);
	// printf("page->va: %p\n", page->va);
	// printf("page->type: %d\n", VM_TYPE(page->uninit.type));
	if(page == NULL) {
		if(not_present){
			void* rsp = user ? f->rsp : thread_current()->rsp;
			// printf("user mode interrupt\n");
			
			if(((USER_STACK - (1 << 20)) <= addr && rsp <= addr && addr <= USER_STACK) ||
				((USER_STACK - (1 << 20)) <= addr && addr >= rsp - 8 && addr <= USER_STACK)){
				// 폴트난 addr에서 가장 가까운 1페이지 주소로 내림
				vm_stack_growth(pg_round_down(addr));

				return true;
			}
		}
		return false;
	}
	
	if(write && !page->writable) return false;
	
	enum vm_type type = page->operations->type;
	//printf("do claim\n");
	//if(VM_TYPE(type) != VM_UNINIT) return false;
	return vm_do_claim_page (page);
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void
vm_dealloc_page (struct page *page) {
	destroy (page);
	free (page);
}

/* Claim the page that allocate on VA. */
/* supplemental table에서 va에 해당하는 페이지를 찾아서
	물리 페이지를 할당한다. */
bool
vm_claim_page (void *va UNUSED) {
	struct page *page = NULL;
	page = spt_find_page(&thread_current()->spt, va);

	return vm_do_claim_page (page);
}

/* Claim the PAGE and set up the mmu. */
static bool
vm_do_claim_page (struct page *page) {
	struct frame *frame = vm_get_frame ();
	//printf("get frame done\n");
	/* Set links */
	frame->page = page; 
	page->frame = frame;
	/* TODO: Insert page table entry to map page's VA to frame's PA. */
	// printf("pml4_set_page\n");
	// printf("page->writable: %d\n", page->writable);
	// printf("page->va: %p\n", page->va);
	// printf("kva: %p\n", page->frame->kva);
	if(!pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable))
		PANIC("set page fail");
	
	//printf("do claim page type: %d\n", page->operations->type);
	
	return swap_in (page, frame->kva);
}

uint64_t hash_func(const struct hash_elem *e, void *aux){
	struct page *page = hash_entry(e, struct page, elem);
	return hash_bytes(&page->va, sizeof(page->va));
}

bool hash_less(const struct hash_elem *a, const struct hash_elem *b, void *aux){
	struct page *p1 = hash_entry(a, struct page, elem);
	struct page *p2 = hash_entry(b, struct page, elem);

	return p1->va < p2->va;
}

/* Initialize new supplemental page table */
void
supplemental_page_table_init (struct supplemental_page_table *spt UNUSED) {
	
	hash_init(&spt->spt_table, hash_func, hash_less, NULL);
}

/* Copy supplemental page table from src to dst */
bool
supplemental_page_table_copy (struct supplemental_page_table *dst UNUSED,
		struct supplemental_page_table *src UNUSED) {

	struct hash_iterator i;
	hash_first(&i, &src->spt_table);
	while(hash_next(&i)){
		struct page* page = hash_entry(hash_cur(&i), struct page, elem);
		// 첫 폴트가 아직 안났을 경우
		if(page->operations->type == VM_UNINIT){
			if(!vm_alloc_page_with_initializer(page->uninit.type, page->va, page->writable,
			page->uninit.init, page->uninit.aux))
			{
				printf("vm_alloc_page_with_initializer failed\n");
				return false;
			}
				
		}
		else{
			/* 첫 폴트가 난 페이지는 operation->type이 anon이거나 file-backed이다. */
			struct page* newpage = calloc(1, sizeof(struct page));
			if(!newpage){
				printf("page allocation fali\n");
				return false;
			}
			newpage->operations = page->operations;
			newpage->va = page->va;
			newpage->writable = page->writable;
			
			switch(VM_TYPE(page->operations->type)){
				case VM_ANON:
					newpage->anon = page->anon;
					break;
				case VM_FILE:
					newpage->file = page->file;
					break;
				default:
					return false;
			}

			newpage->frame = vm_get_frame();
			newpage->frame->page = newpage;
			memcpy(newpage->frame->kva, page->frame->kva, PGSIZE);
			if(!spt_insert_page(&thread_current()->spt, newpage))
				return false;
			if(!pml4_set_page(thread_current()->pml4, newpage->va, newpage->frame->kva, newpage->writable))
				return false;
		}

	}
	return true;
}

void hash_kill(struct hash_elem *e, void *aux){
	struct page *page = hash_entry(e, struct page, elem);
    destroy(page);
	free(page);
}

/* Free the resource hold by the supplemental page table */
void
supplemental_page_table_kill (struct supplemental_page_table *spt UNUSED) {
	hash_clear(&spt->spt_table, hash_kill);
}
