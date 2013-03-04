#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <list.h>
#include "filesys/file.h"

struct fd_elem {
	int fd;
	struct file *file;
	struct list_elem elem;
	struct list_elem thread_elem;
};
void syscall_init(void);

#endif /* userprog/syscall.h */
