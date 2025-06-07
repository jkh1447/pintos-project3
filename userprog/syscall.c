#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "lib/user/syscall.h"
#include "userprog/process.h"
#include "include/lib/string.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "include/vm/vm.h"
// #include "filesys/inode.h"
// #include "threads/malloc.h"
// /* An open file. */
// struct file {
// 	struct inode *inode;        /* File's inode. */
// 	off_t pos;                  /* Current position. */
// 	bool deny_write;            /* Has file_deny_write() been called? */
// };

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void halt(void);
void exit(int status);
pid_t fork (const char *thread_name);
int exec (const char *file);
int wait (pid_t pid);
bool create(const char *file, unsigned initial_size);
bool remove (const char *file);
int open(const char *file);
int filesize(int fd);
int read(int fd, void *buffer, unsigned size);
int write(int fd, const void *buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close (int fd);
void *mmap (void *addr, size_t length, int writable, int fd, off_t offset);
void munmap (void *addr);
bool isValidAddress(const void *ptr);
bool isValidString(const char *str);

struct lock filesys_lock;

/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);

	lock_init(&filesys_lock);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.

	thread_current()->rsp = f->rsp;

	// 시스템 콜 번호
	uint64_t syscall_num = f->R.rax;

	switch(syscall_num){
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(f->R.rdi);
			break;
		case SYS_FORK:
			// f->R.rax = fork((char *)f->R.rdi);
			f->R.rax = process_fork((char *)f->R.rdi, f); 
			break;
		case SYS_EXEC:
			f->R.rax = exec((char *)f->R.rdi);
			break;
		case SYS_WAIT:
			f->R.rax = wait((pid_t)f->R.rdi);
			break;
		case SYS_CREATE:
			f->R.rax = create((char *)f->R.rdi, (unsigned)f->R.rsi);
			break;
		case SYS_REMOVE:
			f->R.rax = remove ((char *)f->R.rdi);
			break;
		case SYS_OPEN:
			f->R.rax = open((char *)f->R.rdi);
			break;
		case SYS_FILESIZE:
			f->R.rax = filesize((int)f->R.rdi);
			break;
		case SYS_READ:
			f->R.rax = read((int)f->R.rdi, (void *)f->R.rsi, (unsigned)f->R.rdx);
			break;
		case SYS_WRITE:
			f->R.rax = write((int)f->R.rdi, (void *)f->R.rsi, (unsigned)f->R.rdx);
			break;
		case SYS_SEEK:
			seek((int)f->R.rdi, (unsigned)f->R.rsi);
			break;
		case SYS_TELL:
			f->R.rax = tell((int)f->R.rdi);
			break;
		case SYS_CLOSE:
			close((int)f->R.rdi);
			break;
		case SYS_MMAP:
			f->R.rax = mmap((void *)f->R.rdi, (size_t)f->R.rsi, (int)f->R.rdx, (int)f->R.r10, (off_t)f->R.r8);
			break;
		case SYS_MUNMAP:
			munmap((void *)f->R.rdi);
			break;
		default:
			thread_exit();
	}
	

}

void halt(void){
	power_off();
}

void exit(int status){
	struct thread* curr = thread_current();
	//부모프로세스에서 자식프로세스의 종료 정보를 저장
	if(curr->user_prog != NULL) {
		file_allow_write(curr->user_prog);
		file_close(curr->user_prog);
	}

	struct mmap_table **mmap_table = thread_current()->mmap_table->mmap_table;
	for(int i=0; i<FD_MAX; i++){
		if(mmap_table[i] == NULL) continue;
		//printf("i: %d\n", i);
		struct mmap_entry *e = mmap_table[i];
		munmap(e->addr);
	}


	struct file **fd_entries = curr->fd_table->fd_entries;
	for(int i=0; i<FD_MAX; i++){
		if(fd_entries[i] == NULL) continue;
		file_close(fd_entries[i]);
	}
	free(curr->fd_table);

	struct list *child_list = &curr->child_list;
	while (!list_empty(child_list)) {
		struct list_elem *e = list_pop_front(child_list);
		struct child_status *ch = list_entry(e, struct child_status, elem);
		free(ch); // 부모가 종료되므로, 자식 상태 정보 해제
	}

	struct child_status *ch_st = curr->child_status;
	if(ch_st != NULL){

		//printf("exit tid: %d\n", ch_st->tid);
		ch_st->exit_status = status;
		ch_st->has_exited = true;
		printf("%s: exit(%d)\n", curr->name, status);
		sema_up(&ch_st->sema_wait);

		if(!list_empty(&ch_st->sema_fork.waiters)){
			sema_up(&ch_st->sema_fork);
		}
		if(!list_empty(&ch_st->sema_wait.waiters)){
			sema_up(&ch_st->sema_wait);
		}
	}
	else{
		
		printf("%s: exit(%d)\n", curr->name, status);
		
	}

	

	
	thread_exit();
}

pid_t fork (const char *thread_name){
	tid_t child_tid = process_fork(thread_name, &thread_current()->tf);
	
	return child_tid;
}

int exec(const char *file_name){
	//printf("exec file name: %s, addr: %p\n", file_name, file_name);
	if(!isValidString(file_name)) exit(-1);
	char *fn_copy = palloc_get_page(PAL_ZERO);
	if(fn_copy == NULL) return -1;
	strlcpy(fn_copy, file_name, PGSIZE);
	//printf("fn_copy: %s\n", fn_copy);
	//printf("curr magic: 0x%x\n", thread_current()->magic);

	int tid = process_exec(fn_copy);
	//palloc_free_page(fn_copy);
	if(tid == -1) exit(-1);
	return tid;
}


int wait (pid_t pid){
	return process_wait(pid);
}


// all done. process wait 구현해야 완전 통과
bool create(const char *file, unsigned initial_size) {
	if(file == NULL) exit(-1);
	if(!isValidAddress(file)) exit(-1);
	//lock_acquire(&filesys_lock);
	bool success = filesys_create(file, initial_size);
	return success;
}

bool remove (const char *file){
	
	bool result = filesys_remove(file);
	return result;
}

// open done.
int open(const char *file){
	if(!isValidString(file)) exit(-1);
	if(file == NULL) return -1;
	struct file* f = filesys_open(file);
	

	//printf("f address: %p\n", f);
	if(f == NULL) return -1;
	

	struct thread* curr = thread_current();
	// 0, 1, 2는 예약된 fd
	for(int fd = 3; fd < FD_MAX; fd++){
		if(curr->fd_table->fd_entries[fd] == NULL){
			curr->fd_table->fd_entries[fd] = f;
			return fd;
		}
	}
	// fd table 꽉 찼을 때
	file_close(f);
	return -1;
}

int filesize(int fd){
	int size = 0;
	struct thread* curr = thread_current();
	struct file* f = curr->fd_table->fd_entries[fd];
	size = file_length(f);
	return size;
}

// read done. rox빼고
int read(int fd, void *buffer, unsigned size){
	if(size == 0) return 0;
	if(fd < 0) exit(-1);
	if(fd == 1) exit(-1);
	if(fd >= FD_MAX) exit(-1);
	
	check_valid_buffer(buffer, size);
	//printf("read fd: %d\n", fd);
	/* 커널모드에서는 writable 권한이 무시된다. */
	/* pml4 entry로 권한을 확인하는 방법은 오류가 있다.
	   일단 *pte는 실제 물리주소이고, 이 물리 주소에 아직 접근하지 않았다.
	   그래서 페이지폴트는 일어나지 않고, 물리페이지에 매핑도 안된 상태이다.
	   그 때 writable비트를 확인하게 되면 무조건 0이 나오게된다.
	   하지만 spt를 이용하면, 매핑되기 전에 확인이 가능하다.
	*/
	// uint64_t *pte = pml4e_walk(thread_current()->pml4, (uint64_t) buffer, 0);;
	// printf("1 physical addr: %p\n", *pte);
	 // printf("buffer addr: %p\n", buffer);
	// printf("pte: %p\n", pte);

	
	// if(pte != NULL && is_writable(pte) == 0) {
		
	// 	printf("buffer not writable\n");
	// 	exit(-1);
			
		
	// }


	if(fd == 0){
		char c;
		int i=0;
		for(; i<size; i++){
			c = input_getc();
			((char *)buffer)[i] = c;
			if(c == '\n') break;
		}
		return i+1;
	}
	else if(fd >= 3){
		struct thread* curr = thread_current();
		struct file* f = curr->fd_table->fd_entries[fd];
		//printf("f addr: %p\n", f);
		if(f == NULL) return -1;
		//printf("inode pointer: %p\n", f->inode);
		struct page *page = spt_find_page(&curr->spt, buffer);
		if(page && !page->writable){
			//printf("2 physical addr: %p\n", page->frame->kva);
			exit(-1);
		}
		//file_seek(f, 0);
		lock_acquire(&f->inode->inode_lock);
		int result = file_read(f, buffer, size);
		lock_release(&f->inode->inode_lock);
		//printf("file_read returned %d\n", result);
		return result;
	}
}

// write done
int write(int fd, const void *buffer, unsigned size){
	//printf("write\n");
	if(fd == 0) exit(-1);
	if(fd < 0) exit(-1);
	if(fd >= FD_MAX) exit(-1);
	
	// 표준 출력
	check_valid_buffer(buffer, size);
	if(fd == 1 || fd == 2){
		putbuf(buffer, (size_t)size);
		return (int)size;
	}
	else{
		struct thread* curr = thread_current();
		struct file* f = curr->fd_table->fd_entries[fd];
		if(f == NULL) exit(-1);
		return file_write(f, buffer, size);
	}
	return 0;
}

void seek(int fd, unsigned position){
	struct file *file = thread_current()->fd_table->fd_entries[fd];
	file_seek(file, (off_t)position);
}

unsigned tell(int fd){
	struct file* file = thread_current()->fd_table->fd_entries[fd];
	off_t offset = file_tell(file);
	return offset;
}

void close(int fd){
	if(fd < 0) exit(-1);
	if(fd == 0 || fd == 1 || fd == 2) exit(-1);
	if(fd >= FD_MAX) exit(-1);
	struct thread *curr = thread_current();
	struct file *file = curr->fd_table->fd_entries[fd];
	curr->fd_table->fd_entries[fd] = NULL;
	file_close(file);
}

bool isValidAddress(const void *ptr){
	if(is_user_vaddr(ptr)){
		if(pml4_get_page(thread_current()->pml4, ptr) != NULL)
			return true;
	}
	return false;
}

bool isValidString(const char *str){
	while(isValidAddress(str)){
		if(*str == '\0') return true;
		str++;
	}
	return false;
}

void check_valid_buffer(const void *buffer, unsigned size) {
    uint8_t *ptr = (uint8_t *)buffer;
    struct thread *curr = thread_current();

    for (unsigned i = 0; i < size; i++) {
        // 유저 주소 범위 체크
        if (!is_user_vaddr(ptr + i));
            //exit(-1);

        // 페이지 매핑 존재 여부 체크
        if (pml4_get_page(curr->pml4, ptr + i) == NULL);
            //exit(-1);
    }
}

void *
mmap (void *addr, size_t length, int writable, int fd, off_t offset) {

	// if file has a length of zero bytes
	// addr must be page-aligned.
	// if length is zero, then fail
	if(length <= 0) return NULL;
	/* 64비트 주소가 표현할 수 있는 주소 공간 크기를 벗어나는 길이 */
	if(length > (1ULL << 48)) return NULL;
	if((uintptr_t)addr < 0) return NULL;
	if((uintptr_t)addr < 0x400000) return NULL;
	if((uintptr_t)addr >= KERN_BASE) return NULL;
	if(fd == 0 || fd == 1 || fd >= FD_MAX) return NULL;
	if(addr != pg_round_down(addr)) return NULL;
	if(length < offset) return NULL;
	if(filesize(fd) == 0) return NULL;
	//printf("here\n");
	void *t_addr;
	t_addr = addr;

	off_t real_ofs = offset;
	// printf("addr: %p\n", addr);
	// printf("length: %u\n", (uintptr_t)addr + length);
	// range of pages does not overlap any existing mapped page
	while(t_addr < addr + length){
		//printf("검사\n");
		if(t_addr > KERN_BASE) return NULL;
		struct page* page = spt_find_page(&thread_current()->spt, t_addr);
		if(page) return NULL;

		t_addr += PGSIZE;
		
	}
	
	
	void *adrs = addr;
	struct thread *curr = thread_current();
	// struct file *file = curr->fd_table->fd_entries[fd];
	struct file *file = file_reopen(curr->fd_table->fd_entries[fd]);
	//printf("file: %p\n", file);
	uint32_t read_bytes = length;
	uint32_t zero_bytes = ((length + PGSIZE - 1) / PGSIZE * PGSIZE) - length;

	int pagesize = 0;

	while(read_bytes > 0 || zero_bytes > 0){
		pagesize++;
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		size_t page_zero_bytes = PGSIZE - page_read_bytes;
		// printf("mmap read_bytes: %d\n", read_bytes);
		// printf("mmap page_read_bytes: %d\n", page_read_bytes);
		// printf("mmap page_zero_bytes: %d\n", page_zero_bytes);
		void *aux = NULL;
		struct load_segment_para *lsp = calloc(1, sizeof(struct load_segment_para));
		lsp->file = file;
		lsp->ofs = offset;
		lsp->upage = adrs;
		lsp->read_bytes = page_read_bytes;
		lsp->zero_bytes = page_zero_bytes;
		lsp->writable = writable;
		aux = lsp;

		if (!vm_alloc_page_with_initializer (VM_FILE, adrs,
					writable, lazy_load_segment_mmap, aux))
			return NULL;

		/* Advance. */
		read_bytes -= page_read_bytes;
		zero_bytes -= page_zero_bytes;
		adrs += PGSIZE;
		/* file_read를 하지 않기 때문에 수동으로 ofs을 이동시켜줘야 한다. */
		offset += page_read_bytes;  
	}

	/* insert to mmap table */
	struct mmap_entry *e = calloc(1, sizeof(struct mmap_entry));
	e->addr = addr;
	e->file = file;
	e->length = length;
	e->pagesize = pagesize;
	e->ofs = real_ofs;
	thread_current()->mmap_table->mmap_table[fd] = e;


	

	return addr;
}	

void
munmap (void *addr) {
	//printf("munmap\n");
	struct mmap_entry *e = NULL;
	struct mmap_entry **mmap_table = thread_current()->mmap_table->mmap_table;
	for(int i = 3; i<FD_MAX; i++){
		if(mmap_table[i] != NULL && mmap_table[i]->addr == addr){
			e = mmap_table[i];
			mmap_table[i] = NULL;
			break;
		}
	}
	// printf("addr: %p\n", addr);
	// printf("addr file: %p\n", e->file);
	// printf("length: %d\n", e->length);
	if(e == NULL) return;
	//printf("%s\n", addr);
	
	file_seek(e->file, e->ofs);
	// 수정된 경우
	void *tmpaddr = addr;
	size_t read_bytes = e->length; 
	while(read_bytes > 0){
		size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
		if(pml4_is_dirty(thread_current()->pml4, tmpaddr)){
			size_t result = file_write(e->file, tmpaddr, page_read_bytes);
			//printf("dirty write byte: %d\n", result);
		}
		struct page *page = spt_find_page(&thread_current()->spt, tmpaddr);
		if(!page) return;
		spt_remove_page(&thread_current()->spt, page);
		
		read_bytes -= page_read_bytes;
		tmpaddr += PGSIZE;
	}
	

	
	//file_seek(e->file, 0);

}