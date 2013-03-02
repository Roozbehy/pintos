#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

//other needed stuff
#include "threads/synch.h"
#include "filesys/filesys.h"
#include "userprog/pagedir.h"
#include "threads/vaddr.h"

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

//TODO STUFF------------------------
static bool valid_usr_ptr (const void *vaddr);
static struct file* find_file (int fd);
static struct fd_elem* find_fd_elem (int fd);

//-----------------------------------

//Handler type
//takes 3 args(which are the actual args when syscall is evoked)
//returns the syscall number
typedef int (* handler) (uint32_t, uint32_t, uint32_t);
//list of handlers,
//set arbitrarily to 128 :|
static handler syscall_vec[128];

//File list and its lock
static struct list file_list;
static struct lock file_lock;

//Validity of user pointer
static bool
valid_usr_ptr (const void *vaddr)
{
  return ((is_user_vaddr (vaddr)) 
       && (pagedir_get_page(thread_current()->pagedir, vaddr) != NULL));
}

//find file by its fd
static struct file*
find_file (int fd)
{
  struct fd_elem *ret;
  ret = find_fd_elem (fd);
  if (!ret)
    return NULL;
  return ret->file;
}

//aux func for the above
static struct fd_elem*
find_fd_elem (int fd)
{
  struct fd_elem *ret;
  struct list_elem *e;
  for (e = list_begin(&file_list); e != list_end(&file_list);
       e = list_next(e))
  {
    ret = list_entry(e, struct fd_elem, elem);
    if (ret->fd == fd)
      return ret;
  }
  return NULL;
}

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  
  syscall_vec[SYS_WRITE] = (handler) sys_write;
  syscall_vec[SYS_EXIT] = (handler) sys_exit;
  syscall_vec[SYS_HALT] = (handler) sys_halt;
  syscall_vec[SYS_WAIT] = (handler) sys_wait;

  list_init(&file_list);
  lock_init(&file_lock);
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
  
  //TODO Need validity check here

  h = syscall_vec[*syscall_number];

  //TODO Need validity check here

  f->eax = h (*(syscall_number + 1),
              *(syscall_number + 2),
              *(syscall_number + 3));
}

static int
sys_write (int fd, const void *buffer, unsigned length)
{
  struct file *f;
  int ret;
  ret = -1;

  lock_acquire(&file_lock);

  if ((!valid_usr_ptr(buffer)) || (!valid_usr_ptr(buffer + length)))
  {
    lock_release (&file_lock);
    sys_exit (-1);
    goto done;
  }
  
  if (fd == STDOUT_FILENO)
    putbuf (buffer, length);
  else
  if (fd == STDIN_FILENO)
    goto done;
  else
  {
    f = find_file (fd);
    if (!f)
      goto done;
    ret = file_write (f, buffer, length);
  }

done:
  lock_release (&file_lock);
  return ret;
}

static int
sys_exit (int status)
{
  thread_current()->ret = status;
  thread_exit();
  return status;
}

static int
sys_halt (void)
{
  shutdown_power_off();
  return 0;
}

static int
sys_wait(tid_t pid)
{
  return process_wait(pid);
}


