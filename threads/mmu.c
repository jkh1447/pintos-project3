#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "threads/init.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/mmu.h"
#include "intrinsic.h"

static uint64_t *
pgdir_walk (uint64_t *pdp, const uint64_t va, int create) {
	// pdp : page directory 테이블의 시작 주소
	// 가상주소에서 page directory 테이블의 엔트리 인덱스를 가상주소에서 추출
	int idx = PDX (va);
	if (pdp) {
		// pte: page directory 테이블의 엔트리, page table의 시작 주소 
		uint64_t *pte = (uint64_t *) pdp[idx];
		if (!((uint64_t) pte & PTE_P)) {
			if (create) {
				uint64_t *new_page = palloc_get_page (PAL_ZERO);
				if (new_page)
					pdp[idx] = vtop (new_page) | PTE_U | PTE_W | PTE_P;
				else
					return NULL;
			} else
				return NULL;
		}
		// PTX(vaddr): page table에서 몇 번째 엔트리인지 계산
		// PTE_ADDR: page table의 실제 물리 주소
		// pte의 커널 가상 주소를 반환
		return (uint64_t *) ptov (PTE_ADDR (pdp[idx]) + 8 * PTX (va));
	}
	return NULL;
}

static uint64_t *
pdpe_walk (uint64_t *pdpe, const uint64_t va, int create) {
	uint64_t *pte = NULL;
	// page directory pointer테이블의 엔트리 인덱스를 가상주소에서 추출
	int idx = PDPE (va);
	int allocated = 0;
	if (pdpe) {
		// page directory 테이블 주소를 pde에 저장
		uint64_t *pde = (uint64_t *) pdpe[idx];
		if (!((uint64_t) pde & PTE_P)) {
			if (create) {
				uint64_t *new_page = palloc_get_page (PAL_ZERO);
				if (new_page) {
					pdpe[idx] = vtop (new_page) | PTE_U | PTE_W | PTE_P;
					allocated = 1;
				} else
					return NULL;
			} else
				return NULL;
		}
		pte = pgdir_walk (ptov (PTE_ADDR (pdpe[idx])), va, create);
	}
	if (pte == NULL && allocated) {
		palloc_free_page ((void *) ptov (PTE_ADDR (pdpe[idx])));
		pdpe[idx] = 0;
	}
	return pte;
}

/* Returns the address of the page table entry for virtual
 * address VADDR in page map level 4, pml4.
 * If PML4E does not have a page table for VADDR, behavior depends
 * on CREATE.  If CREATE is true, then a new page table is
 * created and a pointer into it is returned.  Otherwise, a null
 * pointer is returned. */
//  va라는 가상 주소에 대응하는 최종 페이지 테이블 엔트리(PTE)의 주소를 리턴합니다.
uint64_t *
pml4e_walk (uint64_t *pml4e, const uint64_t va, int create) {
	uint64_t *pte = NULL;
	// 가상주소 va에서 pml4인덱스(상위 9비트) 추출
	int idx = PML4 (va);
	int allocated = 0;
	if (pml4e) {
		/* 가상 주소에서 추출한 pml4테이블의 인덱스에 접근해서, 
			다음 테이블인 PDP테이블의 시작 주소를 pdpe에 넣음
			pml4e[idx]는 가상주소가 아니고 물리주소임
			그래서 이후 ptov (PTE_ADDR (pml4e[idx]) 이 부분처럼 가상주소로 변환해서
			사용한다.
		*/ 
		uint64_t *pdpe = (uint64_t *) pml4e[idx];
		// 해당 주소에 present bit가 0이라면
		// 다음 pdp테이블 없으면 만듬
		if (!((uint64_t) pdpe & PTE_P)) {
			// create가 true면 새 페이지 생성
			if (create) {
				uint64_t *new_page = palloc_get_page (PAL_ZERO);
				if (new_page) {
					pml4e[idx] = vtop (new_page) | PTE_U | PTE_W | PTE_P;
					allocated = 1;
				} else
					return NULL;
			} else
				return NULL;
		}
		// 다음단계인 pdpe_walk를 호출하면서 내려감
		pte = pdpe_walk (ptov (PTE_ADDR (pml4e[idx])), va, create);
	}
	if (pte == NULL && allocated) {
		palloc_free_page ((void *) ptov (PTE_ADDR (pml4e[idx])));
		pml4e[idx] = 0;
	}
	return pte;
}

/* Creates a new page map level 4 (pml4) has mappings for kernel
 * virtual addresses, but none for user virtual addresses.
 * Returns the new page directory, or a null pointer if memory
 * allocation fails. */
// 새 페이지 테이블을 생성해서 반환
uint64_t *
pml4_create (void) {
	uint64_t *pml4 = palloc_get_page (0);
	if (pml4)
		memcpy (pml4, base_pml4, PGSIZE);
	return pml4;
}

static bool
pt_for_each (uint64_t *pt, pte_for_each_func *func, void *aux,
		unsigned pml4_index, unsigned pdp_index, unsigned pdx_index) {
	
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		//printf("pt for each\n");
		uint64_t *pte = &pt[i];
		if (((uint64_t) *pte) & PTE_P) {
			void *va = (void *) (((uint64_t) pml4_index << PML4SHIFT) |
								 ((uint64_t) pdp_index << PDPESHIFT) |
								 ((uint64_t) pdx_index << PDXSHIFT) |
								 ((uint64_t) i << PTXSHIFT));
			if (!func (pte, va, aux))
				return false;
		}
	}
	return true;
}

static bool
pgdir_for_each (uint64_t *pdp, pte_for_each_func *func, void *aux,
		unsigned pml4_index, unsigned pdp_index) {
	//printf("pgdir for each\n");
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pte = ptov((uint64_t *) pdp[i]);
		if (((uint64_t) pte) & PTE_P)
			if (!pt_for_each ((uint64_t *) PTE_ADDR (pte), func, aux,
					pml4_index, pdp_index, i))
				return false;
	}
	return true;
}

static bool
pdp_for_each (uint64_t *pdp,
		pte_for_each_func *func, void *aux, unsigned pml4_index) {
	//printf("pdp for each\n");
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pde = ptov((uint64_t *) pdp[i]);
		if (((uint64_t) pde) & PTE_P)
			if (!pgdir_for_each ((uint64_t *) PTE_ADDR (pde), func,
					 aux, pml4_index, i))
				return false;
	}
	return true;
}

/* Apply FUNC to each available pte entries including kernel's. */
// pml4 테이블에 있는 모든 유효한 항복에 대해 func호출함.
// func가 false 를 반환하면 순회를 중단후 false 반환.
// 모든 항목에 대해 성공적으로 호출되면 true를 반환
bool
pml4_for_each (uint64_t *pml4, pte_for_each_func *func, void *aux) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pdpe = ptov((uint64_t *) pml4[i]);
		if (((uint64_t) pdpe) & PTE_P)
			if (!pdp_for_each ((uint64_t *) PTE_ADDR (pdpe), func, aux, i))
				return false;
	}
	return true;
}

static void
pt_destroy (uint64_t *pt) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pte = ptov((uint64_t *) pt[i]);
		if (((uint64_t) pte) & PTE_P)
			palloc_free_page ((void *) PTE_ADDR (pte));
	}
	palloc_free_page ((void *) pt);
}

static void
pgdir_destroy (uint64_t *pdp) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pte = ptov((uint64_t *) pdp[i]);
		if (((uint64_t) pte) & PTE_P)
			pt_destroy (PTE_ADDR (pte));
	}
	palloc_free_page ((void *) pdp);
}

static void
pdpe_destroy (uint64_t *pdpe) {
	for (unsigned i = 0; i < PGSIZE / sizeof(uint64_t *); i++) {
		uint64_t *pde = ptov((uint64_t *) pdpe[i]);
		if (((uint64_t) pde) & PTE_P)
			pgdir_destroy ((void *) PTE_ADDR (pde));
	}
	palloc_free_page ((void *) pdpe);
}

/* Destroys pml4e, freeing all the pages it references. */
// 해당 페이지 테이블에 할당된 모든 메모리를 정리하고 해제합니다.
void
pml4_destroy (uint64_t *pml4) {
	if (pml4 == NULL)
		return;
	ASSERT (pml4 != base_pml4);

	/* if PML4 (vaddr) >= 1, it's kernel space by define. */

	uint64_t *pdpe = ptov ((uint64_t *) pml4[0]);
	if (((uint64_t) pdpe) & PTE_P)
		pdpe_destroy ((void *) PTE_ADDR (pdpe));

	palloc_free_page ((void *) pml4);
}

/* Loads page directory PD into the CPU's page directory base
 * register. */
/* 	이 페이지 테이블을 CPU가 사용하는 활성 페이지 테이블로 설정합니다.
	CPU는 현재 활성화된 페이지 테이블을 기반으로 가상 주소 → 물리 주소를 변환합니다.
	내부적으로는 CR3 레지스터에 물리 주소를 넣어서 페이지 테이블을 바꿉니다.
*/
void
pml4_activate (uint64_t *pml4) {
	lcr3 (vtop (pml4 ? pml4 : base_pml4));
}

/* Looks up the physical address that corresponds to user virtual
 * address UADDR in pml4.  Returns the kernel virtual address
 * corresponding to that physical address, or a null pointer if
 * UADDR is unmapped. */
/*
	주어진 가상주소 uaddr가 어떤 물리 페이지랑 연결되어있는지 확인하고, 
	있다면 커널가상주소 kpage반환
*/
void *
pml4_get_page (uint64_t *pml4, const void *uaddr) {
	ASSERT (is_user_vaddr (uaddr));
	
	// 테이블 자체가 없으면 그냥 바로 NULL 리턴하게 됨.
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) uaddr, 0);
	//printf("pml4_get_page pte: %p\n", uaddr);

	if (pte && (*pte & PTE_P))
		return ptov (PTE_ADDR (*pte)) + pg_ofs (uaddr);
	return NULL;
}

/* Adds a mapping in page map level 4 PML4 from user virtual page
 * UPAGE to the physical frame identified by kernel virtual address KPAGE.
 * UPAGE must not already be mapped. KPAGE should probably be a page obtained
 * from the user pool with palloc_get_page().
 * If WRITABLE is true, the new page is read/write;
 * otherwise it is read-only.
 * Returns true if successful, false if memory allocation
 * failed. */
/*
	가상주소 upage에 물리메모리 kpage를 연결
	upage는 이미 연결되어있으면 안되고, kpage는 palloc_get_page()함수로 할당된 커널 주소이여야 함.
*/
bool
pml4_set_page (uint64_t *pml4, void *upage, void *kpage, bool rw) {
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (pg_ofs (kpage) == 0);
	ASSERT (is_user_vaddr (upage));
	ASSERT (pml4 != base_pml4);

	//printf("pml4_set_page, upage: %p, rw: %d\n", upage, rw);
	// 가상주소에 해당하는 페이지테이블엔트리
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) upage, 1);
	// kpage는 물리주소를 간접적으로 표현?, pte값 설정해줌
	if (pte)
	// vtop함수는 그냥 kpage - KERN_BASE이다. 
	// 그리고 하위 3비트를 or 연산으로 설정해준 뒤 페이지 테이블 엔트리를 설정한다.
	*pte = vtop (kpage) | PTE_P | (rw ? PTE_W : 0) | PTE_U;
	
	//printf("pml4_set_page: %p\n", upage);
	return pte != NULL;
}

/* Marks user virtual page UPAGE "not present" in page
 * directory PD.  Later accesses to the page will fault.  Other
 * bits in the page table entry are preserved.
 * UPAGE need not be mapped. */

 /*
	주어진 upage 가상주소에 매핑된 페이지를 없음상태로 표시
	주소는 남겨둠. valid bit을 0으로?
 */
void
pml4_clear_page (uint64_t *pml4, void *upage) {
	uint64_t *pte;
	ASSERT (pg_ofs (upage) == 0);
	ASSERT (is_user_vaddr (upage));

	pte = pml4e_walk (pml4, (uint64_t) upage, false);

	if (pte != NULL && (*pte & PTE_P) != 0) {
		*pte &= ~PTE_P;
		if (rcr3 () == vtop (pml4))
			invlpg ((uint64_t) upage);
	}
}

/* Returns true if the PTE for virtual page VPAGE in PML4 is dirty,
 * that is, if the page has been modified since the PTE was
 * installed.
 * Returns false if PML4 contains no PTE for VPAGE. */
// 수정되었는지 여부
bool
pml4_is_dirty (uint64_t *pml4, const void *vpage) {
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) vpage, false);
	return pte != NULL && (*pte & PTE_D) != 0;
}

/* Set the dirty bit to DIRTY in the PTE for virtual page VPAGE
 * in PML4. */
// 이 페이지가 수정되었다고 직접 설정함
void
pml4_set_dirty (uint64_t *pml4, const void *vpage, bool dirty) {
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) vpage, false);
	if (pte) {
		if (dirty)
			*pte |= PTE_D;
		else
			*pte &= ~(uint32_t) PTE_D;

		if (rcr3 () == vtop (pml4))
			invlpg ((uint64_t) vpage);
	}
}

/* Returns true if the PTE for virtual page VPAGE in PML4 has been
 * accessed recently, that is, between the time the PTE was
 * installed and the last time it was cleared.  Returns false if
 * PML4 contains no PTE for VPAGE. */
// 최근에 접근되었는지 여부
bool
pml4_is_accessed (uint64_t *pml4, const void *vpage) {
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) vpage, false);
	return pte != NULL && (*pte & PTE_A) != 0;
}

/* Sets the accessed bit to ACCESSED in the PTE for virtual page
   VPAGE in PD. */
// 접근 비트를 직접 설정함.
void
pml4_set_accessed (uint64_t *pml4, const void *vpage, bool accessed) {
	uint64_t *pte = pml4e_walk (pml4, (uint64_t) vpage, false);
	if (pte) {
		if (accessed)
			*pte |= PTE_A;
		else
			*pte &= ~(uint32_t) PTE_A;

		if (rcr3 () == vtop (pml4))
			invlpg ((uint64_t) vpage);
	}
}
