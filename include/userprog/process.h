#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/* lazy load segment의 aux인자를 위한 구조체 */
struct lazy_load_info
{
	struct file *file;		// 이 페이지를 위해 읽어올 파일 객체 포인터
	off_t ofs;				// 파일 내에서 읽기를 시작할 위치 (offset)
	uint8_t *upage;			// 이 데이터를 매핑할 사용자 가상 주소 (user page)
	size_t page_read_bytes; // 파일에서 읽어 실제 메모리에 채울 바이트 수
	size_t page_zero_bytes; // 나머지를 0으로 채워야 할 바이트 수 (페이지의 끝까지)
	bool writable;			// 해당 페이지가 사용자 프로그램에 의해 쓰기 가능한지 여부
};

tid_t process_create_initd (const char *file_name);
tid_t process_fork (const char *name, struct intr_frame *if_);
int process_exec (void *f_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (struct thread *next);

#endif /* userprog/process.h */
