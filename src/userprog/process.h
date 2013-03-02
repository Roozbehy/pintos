#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "threads/synch.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

//duc
//new struct load_info, to pass as aux arg to thread_create
//used in process_execute
struct load_info
{
  //Ptr to cmd line string,
  //only the filename, not incl args
  char *file_name;
  //semaphore to ensure load() execution
  struct semaphore load_sema;
  //determines load() success or not
  bool success;
};

#endif /* userprog/process.h */
