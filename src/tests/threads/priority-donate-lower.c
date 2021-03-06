/* The main thread acquires a lock.  Then it creates a
   higher-priority thread that blocks acquiring the lock, causing
   it to donate their priorities to the main thread.  The main
   thread attempts to lower its priority, which should not take
   effect until the donation is released. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static thread_func acquire_thread_func;

void
test_priority_donate_lower (void) 
{
  struct lock lock;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  lock_init (&lock);
  lock_acquire (&lock);
  thread_create ("acquire", PRI_DEFAULT + 10, acquire_thread_func, &lock);
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 10, thread_get_priority ());

  msg ("Lowering base priority...");
  thread_set_priority (PRI_DEFAULT - 10);   //this one try to lower it's priority to 22 ,but this one is 'been dontated' from 'acquire' thread , so the priority templely cannot change , what we do is save it to it's pre_priority, after the realease_lock called , set this(current) priority to pre_priority ,reschedule the queue
/*
thread_set_priority (priority){
..
if (cur->donate_times > 0 ) {  // means this thread is been donated at least onece (see test priority-donate-multiple) ; 
	
	//save his priority 
	cur->pre_priority = get_priority();
}
..
}
*/ 
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 10, thread_get_priority ());
  lock_release (&lock);  //this one will release the lock holding by 'main', IF this theard is 'been donated 'thread , then we need to swtich back to his previous priority (which is been saved before)
/*
lock_release (&lock){
..
if (cur->donate_times > 0 ){
	lock->holder->been_donated = false ;
	thread_set_priority(lock->holder->previous_priority);  // this will auto reschedule it 
}
..
}
*/
  msg ("acquire must already have finished.");
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT - 10, thread_get_priority ());
}

static void
acquire_thread_func (void *lock_) 
{
  struct lock *lock = lock_;

  lock_acquire (lock);
  msg ("acquire: got the lock");
  lock_release (lock);
  msg ("acquire: done");
}
