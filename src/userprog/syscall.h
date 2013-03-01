#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);

static void sys_write(int *ret, int fd, const void *buffer, unsigned length);
static void sys_exit(int *ret, int status);


#endif /* userprog/syscall.h */
