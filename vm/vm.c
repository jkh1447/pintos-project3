/* vm.c: Generic interface for virtual memory objects. */

#include "vm/vm.h"
#include "include/threads/mmu.h"
#include "include/threads/thread.h"
#include "include/threads/vaddr.h"
// #include "include/vm/uninit.h"
#include "threads/malloc.h"
#include "vm/inspect.h"

static unsigned page_hash(const struct hash_elem *e, void *aux);
static bool page_less(const struct hash_elem *a, const struct hash_elem *b,
                      void *aux);
void hash_kill(struct hash_elem *e, void *aux);

/* 각 하위 시스템의 초기화 코드를 호출 -> 가상 메모리 하위 시스템을 초기화 */
void vm_init(void) {
  vm_anon_init();
  vm_file_init();
#ifdef EFILESYS /* For project 4 */
  pagecache_init();
#endif
  register_inspect_intr();
  /*윗줄 수정하지 마세요*/
  /* TODO: Your code goes here. */
}

/* 페이지 유형을 가져옴 */
/* 페이지가 초기화 된 후 페이지 유형을 알고 싶을 때 유용 */
/* 현재 완전히 구현되어 있음 */
enum vm_type page_get_type(struct page *page) {
  int ty = VM_TYPE(page->operations->type);
  switch (ty) {
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

/* initailizer를 사용하여 보류 중인 페이지 개체를 만들기 */
/* 페이지를 만들고 싶다면 직접만들지 말고 */
/* 아래 함수 또는 'vm_alloc_page를 통해 생성' */
bool vm_alloc_page_with_initializer(enum vm_type type, void *upage,
                                    bool writable, vm_initializer *init,
                                    void *aux) {

  ASSERT(VM_TYPE(type) != VM_UNINIT);
  // printf("vm initial\n");
  struct supplemental_page_table *spt = &thread_current()->spt;

  /* upage가 이미 사용 중인지 여부 확인 */
  if (spt_find_page(spt, upage) == NULL) {
    /* TODO: 페이지를 만들고 VM 유형에 따라 이니셜라이저를 가져오기 */

    struct page *page = malloc(sizeof(struct page));
    // if (page == NULL) {
    //   goto err;
    // }

    bool (*page_initializer)(struct page *, enum vm_type, void *kva) = NULL;

    switch (VM_TYPE(type)) {
    case VM_ANON:
      page_initializer = anon_initializer;
      break;

    case VM_FILE:
      page_initializer = file_backed_initializer;
      break;

      // default:
      //   free(page);
      //   goto err;
    }
    /* TODO: 그런 다음 uninit_new를 호출하여 'uninit'페이지 구조를 만들기 */
    uninit_new(page, upage, init, type, aux, page_initializer);

    /* TODO: uninit_new를 호출한 후 필드를 수정해야 함 */
    page->writable = writable;

    /* TODO: 페이지를 SPT에 삽입 */
    if (!spt_insert_page(spt, page)) {
      free(page);
      goto err;
    }
    // printf("here\n");
    return true;
  }
err:
  return false;
}

/* SPT에서 VA를 찾고 페이지를 반환. 오류가 발생하면 NULL을 반환. */
struct page *spt_find_page(struct supplemental_page_table *spt, void *va) {
  /* TODO: Fill this function. */
  struct page *page = NULL;
  struct hash *h = &spt->sp_table_hash;
  struct page tmp_page;
  tmp_page.va = pg_round_down(va);

  struct hash_elem *elem = hash_find(h, &tmp_page.hash_elem);
  if (elem != NULL) {
    page = hash_entry(elem, struct page, hash_elem);
  }
  return page;

  // struct page *page = malloc(sizeof(struct page));
  // page->va = pg_round_down(va);
  // struct hash_elem *e = hash_find(&spt->sp_table_hash, &page->hash_elem);
  // if (e == NULL)
  //   return NULL;
  // return hash_entry(e, struct page, hash_elem);
}

/* 검증을 위해 SPT에 PAGE를 삽입 */
bool spt_insert_page(struct supplemental_page_table *spt, struct page *page) {

  /* TODO: Fill this function. */
  struct hash_elem *elem = hash_insert(&spt->sp_table_hash, &page->hash_elem);

  return elem == NULL;

  // int succ = false;
  // /* TODO: Fill this function. */
  // if (hash_insert(&spt->sp_table_hash, &page->hash_elem)) {
  //   succ = true;
  // }
  // return succ;
}

void spt_remove_page(struct supplemental_page_table *spt, struct page *page) {
  hash_delete(&spt->sp_table_hash, &page->hash_elem);
  vm_dealloc_page(page);
  return true;
}

/* struct frame을 가져오면 evicted(기존 페이지 내보내기) 돼야함 */
static struct frame *vm_get_victim(void) {
  struct frame *victim = NULL;
  /* TODO: 교체(퇴출) 전략을 결정 해야 함 */

  return victim;
}

/* 한 페이지를 퇴출하고 해당 프레임을 반환 */
/* 오류가 발생하면 NULL을 반환 */
static struct frame *vm_evict_frame(void) {
  struct frame *victim = vm_get_victim();
  /* 퇴출자를 교체하고 퇴출된 프레임을 반환 */

  return NULL;
}

/* palloc()하고 get frame */
/* if 사용 가능한 페이지가 없으면 해당 페이지를 삭제 & 반환 */
/* 이 함수는 항상 유효한 주소를 반환 */
/* 즉, 유저 풀 메모리가 가득 차면 이 기능은 프레임을 제거하여 사용 가능한
 * 메모리 공간을 가져옴 */
static struct frame *vm_get_frame(void) {
  struct frame *frame = NULL;

  frame = calloc(1, sizeof(struct frame));
  if (frame == NULL) {
    PANIC("todo");
  }
  void *p = palloc_get_page(PAL_USER);
  if (p == NULL) {
    PANIC("todo");
  }

  frame->kva = p;
  ASSERT(frame != NULL);
  ASSERT(frame->page == NULL);

  return frame;

  // void *kva = palloc_get_page(PAL_USER);
  // if (kva == NULL) {
  //   PANIC("todo");
  // }
  // struct frame *frame = malloc(sizeof(struct frame));
  // ASSERT(frame != NULL);

  // frame->kva = kva;
  // frame->page = NULL;

  // ASSERT(frame->page == NULL);
  // return frame;
}

/* 스택 성장(확장)하기??? */
static void vm_stack_growth(void *addr) {}

/* write_protected page에서 fault 처리 */
static bool vm_handle_wp(struct page *page) {}

/* Return true on success */
bool vm_try_handle_fault(struct intr_frame *f, void *addr, bool user,
                         bool write, bool not_present) {
  struct supplemental_page_table *spt = &thread_current()->spt;
  struct page *page = NULL;
  // printf("vm fault: %p\n", addr);
  /* TODO: fault 검증 */
  /* TODO: Your code goes here */
  if (is_kernel_vaddr(addr)) {
    return false;
  }

  if (not_present) {
    page = spt_find_page(spt, addr);
    if (page == NULL) {
      return false;
    }
    if (write == 1 && page->writable == 0) {
      return false;
    }
    return vm_do_claim_page(page);
  }

  return false;
}

/* Free the page.
 * DO NOT MODIFY THIS FUNCTION. */
void vm_dealloc_page(struct page *page) {
  destroy(page);
  free(page);
}

/* VA에 할당된 페이지를 요청 */
bool vm_claim_page(void *va) {
  struct page *page = NULL;
  /* TODO: Fill this function */
  struct thread *cur = thread_current();
  page = spt_find_page(&cur->spt, va);
  if (page == NULL) {
    return false;
  }
  return vm_do_claim_page(page);
}

/* page를 요청하고 MMU를 설정 */
static bool vm_do_claim_page(struct page *page) {
  struct frame *frame = vm_get_frame();

  /* Set links */
  frame->page = page;
  page->frame = frame;

  /* TODO: page table entry를 삽입하여 페이지의 VA를 프레임의 PA에 매핑 */
  pml4_set_page(thread_current()->pml4, page->va, frame->kva, page->writable);

  // struct thread *cur = thread_current();
  // if (!pml4_set_page(cur->pml4, page->va, frame->kva, page->writable)) {
  //   return false;
  // }

  return swap_in(page, frame->kva);
}

/* 새 SPT 초기화 */
void supplemental_page_table_init(struct supplemental_page_table *spt) {
  hash_init(&spt->sp_table_hash, page_hash, page_less, NULL);
}

/* SPT를 src에서 dst로 복사 */
bool supplemental_page_table_copy(struct supplemental_page_table *dst,
                                  struct supplemental_page_table *src) {
  struct hash_iterator i;
  hash_first(&i, &src->sp_table_hash);
  while (hash_next(&i)) {
    struct page *src_page = hash_entry(hash_cur(&i), struct page, hash_elem);
    void *upage = src_page->va;

    /* Allocate a new page in the destination SPT */
    if (!vm_alloc_page_with_initializer(
            page_get_type(src_page), upage, src_page->writable,
            src_page->uninit.init, src_page->uninit.aux)) {
      return false;
    }

    struct page *dst_page = spt_find_page(dst, upage);
    if (dst_page == NULL) {
      return false;
    }

    if (!vm_do_claim_page(dst_page)) {
      return false;
    }

    memcpy(dst_page->frame->kva, src_page->frame->kva, PGSIZE);
  }
  return true;
}

/* SPT 에서 resource hold 해제 */
void supplemental_page_table_kill(struct supplemental_page_table *spt) {
  /* 모든 SPT를 파기하고 수정된 내용을 저장소에 다시 작성*/
  hash_clear(&spt->sp_table_hash, hash_kill);
}

static unsigned page_hash(const struct hash_elem *e, void *aux) {
  struct page *p = hash_entry(e, struct page, hash_elem);
  return hash_bytes(&p->va, sizeof(p->va));
}

static bool page_less(const struct hash_elem *a, const struct hash_elem *b,
                      void *aux) {
  struct page *p1 = hash_entry(a, struct page, hash_elem);
  struct page *p2 = hash_entry(b, struct page, hash_elem);
  return p1->va < p2->va;
}

void hash_kill(struct hash_elem *e, void *aux) {
  struct page *page = hash_entry(e, struct page, hash_elem);
  destroy(page);
  free(page);
}