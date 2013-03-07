#include "userprog/syscall.h"

//find a file by its fd from the file_list of a process
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

//Check validity of a pointer, address etc
static bool
valid_pointer(const void *vaddr)
{
  return is_user_vaddr(vaddr)
      && pagedir_get_page(thread_current()->pagedir, vaddr) != NULL;
}

void
syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
  syscall_assign();
  list_init(&files);
  lock_init(&lock);
  lock_init(&fid_lock);
}

//Can add more syscalls later
//For now we have 13
void
syscall_assign(void)
{
  syscalls[SYS_EXIT] = (handler) sys_exit;
  syscalls[SYS_WRITE] = (handler) sys_write;
  syscalls[SYS_HALT] = (handler) sys_halt;
  syscalls[SYS_WAIT] = (handler) sys_wait;
  syscalls[SYS_CREATE] = (handler) sys_create;
  syscalls[SYS_REMOVE] = (handler) sys_remove;
  syscalls[SYS_OPEN] = (handler) sys_open;
  syscalls[SYS_EXEC] = (handler) sys_exec;
  syscalls[SYS_CLOSE] = (handler) sys_close;
  syscalls[SYS_TELL] = (handler) sys_tell;
  syscalls[SYS_READ] = (handler) sys_read;
  syscalls[SYS_SEEK] = (handler) sys_seek;
  syscalls[SYS_FILESIZE] = (handler) sys_filesize;
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
  int ret;
  syscall_no = f->esp;

  //check for validity in all args and see if syscall cmd is in range
  if (!valid_pointer(syscall_no) || *syscall_no > SYS_INUMBER
      || *syscall_no < SYS_HALT || !valid_pointer(syscall_no + 1)
      || !valid_pointer(syscall_no + 2) || !valid_pointer(syscall_no + 3))
    sys_exit(-1);

  hndlr = syscalls[*syscall_no];

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
  //need lock here
  lock_acquire(&lock);
  struct file* f;
  if (valid_pointer(buffer + length))
    {
      switch (fd)
        {
      case STDOUT_FILENO:
        putbuf(buffer, length);
        lock_release(&lock);
        return result;
      case STDIN_FILENO:
        lock_release(&lock);
        return result;
      default:
        f = find_file(fd);
        if (f != NULL)
          result = file_write(f, buffer, length);
        lock_release(&lock);
        return result;
        }
    }
  else
    {
      lock_release(&lock);
      sys_exit(-1);
    }
}

static void
sys_halt(void)
{
  //not sure, is that it?
  shutdown_power_off();
}

static int
sys_wait(tid_t pid)
{
  //stuff will be done in process_wait. Do that!
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
  if (!valid_pointer(file))
    sys_exit(-1);
  struct file *f = filesys_open(file);
  if (f == NULL)
    return -1;
  size_t size = sizeof(struct fd_elem);
  struct fd_elem *elm = (struct fd_elem *) malloc(size);
  if (elm == NULL)
    {
      file_close(f);
      return -1;
    }
  elm->file = f;
  elm->fd = allocate_fid();
  list_push_back(&files, &elm->elem);
  list_push_back(&thread_current()->opened_files, &elm->thread_elem);
  return elm->fd;
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
  int i;
  lock_acquire(&lock);

  struct file* f;
  if (valid_pointer(buffer + size))
    {
      switch (fd)
        {
      case STDOUT_FILENO:
        lock_release(&lock);
        return result;
      case STDIN_FILENO:
        for (i = 0; i < size; i++)
          *(uint8_t *) (buffer + i) = input_getc();
        result = size;
        lock_release(&lock);
        return result;
      default:
        f = find_file(fd);
        if (f != NULL)
          result = file_read(f, buffer, size);
        lock_release(&lock);
        return result;
        }
    }
  else
    {
      lock_release(&lock);
      sys_exit(-1);
    }

  /*if (valid_pointer(buffer + size))
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
   return result;*/
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

