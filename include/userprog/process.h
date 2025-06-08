#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_create_initd(const char *file_name);
tid_t process_fork(const char *name, struct intr_frame *if_);
int process_exec(void *f_name);
int process_wait(tid_t);
void process_exit(void);
void process_activate(struct thread *next);

struct file_info {
  struct file *file;
  off_t offset;           // 읽어야 할 파일 오프셋
  size_t page_read_bytes; // 가상 페이지에 쓰여져 있는 데이터 크기
  uint32_t zero_bytes;
};

#endif /* userprog/process.h */
