#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

//bunch of syscalls
//set it here cuz all the includes are here, not in header files
static int sys_write(int fd, const void *buffer, unsigned length);
static int sys_exit(int status);
static int sys_halt(void);
static int sys_exec(const char *cmd);
static int sys_create(const char *file, unsigned initial_size);
static int sys_remove(const char *file);
static int sys_open(const char *file);
static int sys_close(int fd);
static int sys_read(int fd, void *buffer, unsigned size);
static int sys_wait(tid_t pid);
static int sys_filesize(int fd);
static int sys_tell(int fd);
static int sys_seek(int fd, unsigned pos);



//Handler type
//takes 3 args(which are the actual args when syscall is evoked)
//returns the syscall number
typedef int (* handler) (uint32_t, uint32_t, uint32_t);
//list of handlers,
//set arbitrarily to 128 :|
static handler syscall_vec[128];

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  
  syscall_vec[SYS_WRITE] = (handler) sys_write;
  syscall_vec[SYS_EXIT] = (handler) sys_exit;
}

static void
syscall_handler (struct intr_frame *f)
{
  /*
  printf ("system call!\n");
  thread_exit ();
  */

  handler h;
  int *syscall_number;
  syscall_number = f->esp;
  
  h = syscall_vec[*syscall_number];

  f->eax = h (*(syscall_number + 1),
              *(syscall_number + 2),
              *(syscall_number + 3));
}

static int
sys_write (int fd, const void *buffer, unsigned length)
{

}

static int
sys_exit (int status)
{

}






