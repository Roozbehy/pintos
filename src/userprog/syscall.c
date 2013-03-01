#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);


//Handler type
typedef void (* handler) (int *, uint32_t, uint32_t, uint32_t);
//list of handlers
static handler syscall_vec[128];

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  
  syscall_vec[SYS_WRITE] = (handler) sys_write;
  syscall_vec[SYS_EXIT] = (handler) sys_exit;
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /*
  printf ("system call!\n");
  thread_exit ();
  */

  handler h;
  uint32_t *p;
  uint32_t ret;
  p = f->esp;
  h = syscall_vec[*p];

  h (&ret, *(p + 1), *(p + 2), *(p + 3));
  if (ret != 1)
    f->eax = ret;
}

static void
sys_write (int *ret, int fd, const void *buffer, unsigned length)
{
  if (fd == 1)
    putbuf (buffer, length);

  *ret = length;
}

static void
sys_exit (int *ret, int status)
{
  ret = status;
  thread_exit();
}
