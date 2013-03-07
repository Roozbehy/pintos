#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <list.h>
#include "filesys/file.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "userprog/process.h"
#include <list.h>
#include "lib/user/syscall.h"

static void
syscall_handler(struct intr_frame *);
static bool
valid_pointer(const void *vaddr);
static int
allocate_fid(void);
typedef int
(*handler)(uint32_t, uint32_t, uint32_t);
#define SYSCALL_NUM 50
static handler syscalls[SYSCALL_NUM];
static struct lock lock;
static struct lock fid_lock;
static struct list files;
void
syscall_assign(void);

//syscalls
static void
sys_exit(int status);

static int
sys_write(int fd, const void *buffer, unsigned length);

static void
sys_halt(void);

static int
sys_wait(tid_t pid);

static bool
sys_create(const char *file, unsigned initial_size);

static bool
sys_remove(const char *file);

static int
sys_open(const char *file);

static pid_t
sys_exec(const char *cmd);

static void
sys_close(int fd);

static unsigned
sys_tell(int fd);

static int
sys_read(int fd, void *buffer, unsigned size);

static void
sys_seek(int fd, unsigned pos);

static int
sys_filesize(int fd);


struct fd_elem
{
  int fd;
  struct file *file;
  struct list_elem elem;
  struct list_elem thread_elem;
};
void
syscall_init(void);

#endif /* userprog/syscall.h */
