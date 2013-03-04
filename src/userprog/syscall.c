#include "userprog/syscall.h"
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
static handler syscall_vec[SYSCALL_NUM];
static struct lock lock;
static struct lock fid_lock;
static struct list files;

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


static struct file *
find_file(int fd)
{
  struct fd_elem *ret;
  struct list_elem *e;
  for (e = list_begin(&files); e != list_end(&files); e = list_next(e))
    {
      ret = list_entry (e, struct fd_elem, elem);
      if (ret->fd == fd)
        return ret->file;
    }
  return NULL;
}
void
syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
  syscall_vec[SYS_EXIT] = (handler) sys_exit;
  syscall_vec[SYS_WRITE] = (handler) sys_write;
  syscall_vec[SYS_HALT] = (handler) sys_halt;
  syscall_vec[SYS_WAIT] = (handler) sys_wait;
  syscall_vec[SYS_CREATE] = (handler) sys_create;
  syscall_vec[SYS_REMOVE] = (handler) sys_remove;
  syscall_vec[SYS_OPEN] = (handler) sys_open;
  syscall_vec[SYS_EXEC] = (handler) sys_exec;
  syscall_vec[SYS_CLOSE] = (handler) sys_close;
  syscall_vec[SYS_TELL] = (handler) sys_tell;
  syscall_vec[SYS_READ] = (handler) sys_read;
  syscall_vec[SYS_SEEK] = (handler) sys_seek;
  syscall_vec[SYS_FILESIZE] = (handler) sys_filesize;
  list_init(&files);
  lock_init(&lock);
  lock_init(&fid_lock);
}

//duc
//taken from allocate_tid, does the same thing
//fid start from 2 instead. 0 and 1 taken
static int
allocate_fid(void)
{
  int fid;
  static int next_fid = 2;

  lock_acquire(&fid_lock);
  fid = next_fid++;
  lock_release(&fid_lock);

  return fid;
}

//handle syscall cmd
static void
syscall_handler(struct intr_frame *f)
{
  handler hndlr;
  int *syscall_no;
  syscall_no = f->esp;

  //check for validity in all args and see if syscall cmd is in range
  if (!valid_pointer(syscall_no)
      || *syscall_no > SYS_INUMBER
      || *syscall_no < SYS_HALT
      || !valid_pointer(syscall_no + 1)
      || !valid_pointer(syscall_no + 2)
      || !valid_pointer(syscall_no + 3))
    sys_exit(-1);

  //if all fine, do the shit
  hndlr = syscall_vec[*syscall_no];
  f->eax = hndlr(*(syscall_no + 1), *(syscall_no + 2), *(syscall_no + 3));
}

static void
sys_exit(int status)
{
  //not sure
  thread_current()->ret = status;
  thread_exit();
}

static int
sys_write(int fd, const void *buffer, unsigned length)
{
  int result = -1;
  //need lock for the test
  lock_acquire(&lock);

  if (valid_pointer(buffer) && valid_pointer(buffer + length))
    {
      if (fd == STDOUT_FILENO)
        putbuf(buffer, length);
      else if (fd == STDIN_FILENO)
        goto done;
      else
        {
          struct file* f = find_file(fd);
          if (f == NULL)
            goto done;
          result = file_write(f, buffer, length);
        }
    }
  else
    {
      lock_release(&lock);
      sys_exit(-1);
    }

  done: lock_release(&lock);
  return result;
}

static void
sys_halt(void)
{
  //not sure
  shutdown_power_off();
}

static int
sys_wait(tid_t pid)
{
  return process_wait(pid);
}

static bool
sys_create(const char *file, unsigned initial_size)
{
  if (!valid_pointer(file))
    sys_exit(-1);
  return filesys_create(file, initial_size);
}

static bool
sys_remove(const char *file)
{
  if (!valid_pointer(file))
    sys_exit(-1);
  return filesys_remove(file);
}

static int
sys_open(const char *file)
{
  struct file *f;
  struct fd_elem *e;

  if (!valid_pointer(file))
    sys_exit(-1);
  f = filesys_open(file);
  if (f == NULL)
    return -1;
  e = (struct fd_elem *) malloc(sizeof(struct fd_elem));
  if (e == NULL)
    {
      file_close(f);
      return -1;
    }
  e->file = f;
  e->fd = allocate_fid();
  list_push_back(&files, &e->elem);
  list_push_back(&thread_current()->opened_files, &e->thread_elem);
  return e->fd;
}

static pid_t
sys_exec(const char *cmd)
{
  pid_t result;
  if (!valid_pointer(cmd))
    return -1;
  lock_acquire(&lock);
  result = process_execute(cmd);
  lock_release(&lock);
  return result;
}

static void
sys_close(int fd)
{
  struct fd_elem *e; // find_fd_elem_by_fd_in_process (fd);
  struct list_elem *l;
  struct thread *t = thread_current();
  for (l = list_begin(&t->opened_files); l != list_end(&t->opened_files); l =
      list_next(l))
    {
      e = list_entry (l, struct fd_elem, thread_elem);
      if (e->fd == fd)
        {
          file_close(e->file);
          list_remove(&e->elem);
          list_remove(&e->thread_elem);
          free(e);
          break;
        }
    }
}

static unsigned
sys_tell(int fd)
{
  struct file *f;
  f = find_file(fd);
  if (f == NULL)
    return -1;
  return (unsigned) file_tell(f); //do we need casting?
}

static int
sys_read(int fd, void *buffer, unsigned size)
{
  int result = -1;
  lock_acquire(&lock);

  if (valid_pointer(buffer) && valid_pointer(buffer + size))
    {
      if (fd == STDOUT_FILENO)
        goto done;
      else if (fd == STDIN_FILENO)
        {
          int i;
          for (i = 0; i < size; i++)
            *(uint8_t *) (buffer + i) = input_getc();
          result = size;
          goto done;
        }
      else
        {
          struct file* f = find_file(fd);
          if (f == NULL)
            goto done;
          result = file_read(f, buffer, size);
        }
    }
  else
    {
      lock_release(&lock);
      sys_exit(-1);
    }

  done: lock_release(&lock);
  return result;
}

static void
sys_seek(int fd, unsigned pos)
{
  struct file *f = find_file(fd);
  if (f == NULL)
    {
      lock_release(&lock);
      sys_exit(-1);
    }
  file_seek(f, pos);
}

static int
sys_filesize(int fd)
{
  struct file *f = find_file(fd);
  if (f == NULL)
    return -1;
  return (int) file_length(f);
}

static bool
valid_pointer(const void *vaddr)
{
  return is_user_vaddr(vaddr)
      && pagedir_get_page(thread_current()->pagedir, vaddr) != NULL;
}

