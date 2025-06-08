/* uninit.c: Implementation of uninitialized page. */

/* 모든 페이지는 uninit 페이지로 생성됨 */
/* 첫 번째 페이지 오류가 발생하면 핸들러 체인은 */
/* uninit_initialize(page -> operations.swap_in)를 호출 */
/* uninit_initialize 함수는 페이지 객체를 초기화하여 */
/* 페이지를 특정 페이지 객체(anon, file, page_cache)로 변환 */
/* vm_alloc_page_with_initizer 함수에서 전달된 초기화 콜백을 호출 */

#include "vm/vm.h"

#include "vm/uninit.h"

static bool uninit_initialize(struct page *page, void *kva);
static void uninit_destroy(struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
    .swap_in = uninit_initialize,
    .swap_out = NULL,
    .destroy = uninit_destroy,
    .type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void uninit_new(struct page *page, void *va, vm_initializer *init,
                enum vm_type type, void *aux,
                bool (*initializer)(struct page *, enum vm_type, void *)) {
  ASSERT(page != NULL);

  *page = (struct page){.operations = &uninit_ops,
                        .va = va,
                        .frame = NULL, /* no frame for now */
                        .uninit = (struct uninit_page){
                            .init = init,
                            .type = type,
                            .aux = aux,
                            .page_initializer = initializer,
                        }};
}

/* Initalize the page on first fault */
static bool uninit_initialize(struct page *page, void *kva) {
  struct uninit_page *uninit = &page->uninit;

  /* Fetch first, page_initialize가 값을 텊어쓸 수 있음 */
  vm_initializer *init = uninit->init;
  void *aux = uninit->aux;

  /* TODO: You may need to fix this function. */
  return uninit->page_initializer(page, uninit->type, kva) &&
         (init ? init(page, aux) : true);
}

/* uninit_page로 보유한 리소스를 확보 */
/* 대부분의 페이지는 다른 페이지 객체로 변환 */
/* 프로세스가 종료될 때 실행 중에 참조되지 않는 uninit 페이지가 있을 수 있음 */
/* 호출자가 page를 해제 */
static void uninit_destroy(struct page *page) {
  /* 깃북: 객체지향 언어의 클래스 상속 개념을 도입 <- 찾아보기*/
  /* 클래스나 상속이 존재 X -> 함수 포인터를 이용 */
  /* 실행 중 적절한 함수로 바로 호출할 수 있음 */
  /* destroy(page)를 호출하면, 페이지 타입에 따라 올바른 destroy 루틴이 실행 */

  struct uninit_page *uninit UNUSED = &page->uninit;
  /* TODO: Fill this function.
   * TODO: If you don't have anything to do, just return. */
  return;
}
