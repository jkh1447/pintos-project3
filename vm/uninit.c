/* uninit.c: Implementation of uninitialized page.
 *
 * All of the pages are born as uninit page. When the first page fault occurs,
 * the handler chain calls uninit_initialize (page->operations.swap_in).
 * The uninit_initialize function transmutes the page into the specific page
 * object (anon, file, page_cache), by initializing the page object,and calls
 * initialization callback that passed from vm_alloc_page_with_initializer
 * function.
 * */

#include "vm/vm.h"
#include "vm/uninit.h"

static bool uninit_initialize (struct page *page, void *kva);
static void uninit_destroy (struct page *page);

/* DO NOT MODIFY this struct */
static const struct page_operations uninit_ops = {
	.swap_in = uninit_initialize,
	.swap_out = NULL,
	.destroy = uninit_destroy,
	.type = VM_UNINIT,
};

/* DO NOT MODIFY this function */
void
uninit_new (struct page *page, void *va, vm_initializer *init,
		enum vm_type type, void *aux,
		bool (*initializer)(struct page *, enum vm_type, void *)) {
	ASSERT (page != NULL);
	/* 페이지 구조체의 맴버들을 초기화함. 
	uninit페이지의 operator들을 등록한다.
	가상주소 등록
	물리 프레임은 아직 정해지지 않음(lazy loading)
	uninit page 구조체 맴버들도 초기화함. 
	init 맴버가 페이지 폴트시 실행하는 함수
	page initializer가 해당 페이지를 초기화하는 함수*/
	*page = (struct page) {
		.operations = &uninit_ops,
		.va = va,
		.frame = NULL, /* no frame for now */
		.uninit = (struct uninit_page) {
			.init = init,
			.type = type,
			.aux = aux,
			.page_initializer = initializer,
		}
	};
}

/* Initalize the page on first fault */
/* .swap_in = uninit_initialize 이렇게 지정되어서
	나중에 vm try handle fault를 거쳐 vm_do_claim_page에서 프레임을 할당받고
	swap_in함수로 실행되게 된다.
	실행 이후에는 데이터가 로드되고 페이지타입이 바뀐다. 
	vm_type에 따라 operation->type도 바뀌게 된다. */
static bool
uninit_initialize (struct page *page, void *kva) {
	struct uninit_page *uninit = &page->uninit;
	//printf("uninit_initialize\n");
	/* Fetch first, page_initialize may overwrite the values */

	/* lazy load segment  */
	vm_initializer *init = uninit->init;
	/* lazy load info */
	void *aux = uninit->aux;

	

	// vm type에 맞는 초기화함수를 호출하고, lazy load한다.
	/* TODO: You may need to fix this function. */
	return uninit->page_initializer (page, uninit->type, kva) &&
		(init ? init (page, aux) : true);
}

/* Free the resources hold by uninit_page. Although most of pages are transmuted
 * to other page objects, it is possible to have uninit pages when the process
 * exit, which are never referenced during the execution.
 * PAGE will be freed by the caller. */
static void
uninit_destroy (struct page *page) {
	struct uninit_page *uninit UNUSED = &page->uninit;
	/* TODO: Fill this function.
	 * TODO: If you don't have anything to do, just return. */
	/* 나중에 aux 등이 동적할당 되어있다면 그것을 free 
	page 자체는 caller가 free한다.
	*/

	//free(page->uninit.aux);
	
}
