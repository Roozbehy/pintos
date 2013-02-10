/* The main thread acquires a lock.  Then it creates two
   higher-priority threads that block acquiring the lock, causing
   them to donate their priorities to the main thread.  When the
   main thread releases the lock, the other threads should
   acquire it in priority order.

   Based on a test originally submitted for Stanford's CS 140 in
   winter 1999 by Matt Franklin <startled@leland.stanford.edu>,
   Greg Hutchins <gmh@leland.stanford.edu>, Yu Ping Hu
   <yph@cs.stanford.edu>.  Modified by arens. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static thread_func acquire1_thread_func;
static thread_func acquire2_thread_func;

void
test_priority_donate_one (void) 
{
  struct lock lock;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  lock_init (&lock);
  lock_acquire (&lock); //'main' get the lock 
  thread_create ("acquire1", PRI_DEFAULT + 1, acquire1_thread_func, &lock);
/* 'acquire1' try to acquire a lock but faild , and the priority of this one is higher then the lock_holder(this test is 'main') , it will dontate his priority to lock_holder('main') , and the lock_holder('main')'s donate_times will increase 1 (intial is 0 <- let's put initialization in thread.c) , any threads' donate_time which is >0 will be indicated as a donee (implementation of donate_time see priority-donate-multiple);
*/

/*code 
lock_acquire(&lock){
 if (lock_holder!=NULL&&lock_holder->priority < cur->priority) {
	cur->donee_priority = lock_holder->priority // store lock_holder's priority to doner's donee_priority
	lock->holding_queue.add_sorted(cur);  //add cur to this lock's holding queue,holding queue need to be sorted and the head is the highest priority one 
	lock_holder->donate_times++; //increase the donation times
	lock_holder->priority = cur->priority; //donate priority manualy
	thread_block(cur);  //block current thread
	}
}
*/


  msg ("This thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 1, thread_get_priority ());
  thread_create ("acquire2", PRI_DEFAULT + 2, acquire2_thread_func, &lock);
  msg ("This thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 2, thread_get_priority ());
  lock_release (&lock);
/*
lock_holder('main') release it's lock , lock_holder will switch back to original priority (donee_priority of the highest priority in this lock's holding queue), and decrement it's donate_times by 1 (in this test , it decrease from 1 to 0, which means lock_holder is not the donee any more) 
*/
/*
lock_release(&lock){
	..
	if (cur->donate_times > 1){
	doner = pop_out(lock->holding_list)//this will return the highest_priority_thread in the holding list (becasue we insert as sorted ),and remove this from the holding list
	original_priority = doner->donee_priority; 
	cur->donate_times--; //decrease the donate times 
	thread_set_priority(original_priority); //resume current (donee) to origial priority
	thread_unblock(doner);	//doner thread unblocked , adding doner to the ready list and reschedule etc.
	}
	..
}
*/

  msg ("acquire2, acquire1 must already have finished, in that order.");
  msg ("This should be the last line before finishing this test.");
}

static void
acquire1_thread_func (void *lock_) 
{
  struct lock *lock = lock_;

  lock_acquire (lock);
  msg ("acquire1: got the lock");
  lock_release (lock);
  msg ("acquire1: done");
}

static void
acquire2_thread_func (void *lock_) 
{
  struct lock *lock = lock_;

  lock_acquire (lock);
  msg ("acquire2: got the lock");
  lock_release (lock);
  msg ("acquire2: done");
}
/*
Acceptable output:
  (priority-donate-one) begin
  (priority-donate-one) This thread should have priority 32.  Actual priority: 32.
  (priority-donate-one) This thread should have priority 33.  Actual priority: 33.
  (priority-donate-one) acquire2: got the lock
  (priority-donate-one) acquire2: done
  (priority-donate-one) acquire1: got the lock
  (priority-donate-one) acquire1: done
  (priority-donate-one) acquire2, acquire1 must already have finished, in that order.
  (priority-donate-one) This should be the last line before finishing this test.
  (priority-donate-one) end

Actual output:
  (priority-donate-one) begin
  (priority-donate-one) This thread should have priority 32.  Actual priority: 31.
  (priority-donate-one) This thread should have priority 33.  Actual priority: 31.
  (priority-donate-one) acquire2, acquire1 must already have finished, in that order.
  (priority-donate-one) This should be the last line before finishing this test.
  (priority-donate-one) end
*/
