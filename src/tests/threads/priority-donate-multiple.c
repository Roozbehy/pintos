/* The main thread acquires locks A and B, then it creates two
   higher-priority threads.  Each of these threads blocks
   acquiring one of the locks and thus donate their priority to
   the main thread.  The main thread releases the locks in turn
   and relinquishes its donated priorities.
   
   Based on a test originally submitted for Stanford's CS 140 in
   winter 1999 by Matt Franklin <startled@leland.stanford.edu>,
   Greg Hutchins <gmh@leland.stanford.edu>, Yu Ping Hu
   <yph@cs.stanford.edu>.  Modified by arens. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

static thread_func a_thread_func;
static thread_func b_thread_func;

void
test_priority_donate_multiple (void) 
{
  struct lock a, b;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  lock_init (&a);
  lock_init (&b);

  lock_acquire (&a);
  lock_acquire (&b);

  thread_create ("a", PRI_DEFAULT + 1, a_thread_func, &a); 
/*after this thread is created , 'a' is tring to aquire lock_a , but lock_a is holding by 'main',
so priority_donation happends , inside the donation procedure , 'main'thread's previous priority 
shold be stored in 'a's donee_priority(which is the previouse priority of 'main'),then after the 
lock_a is released , we just put 'main'(current running thread)'s priority back to it's previous 
stored before donation happends (i.e to a_lock_holder's(thread'a') donee_priority )
*/

/*
solutions:
1.implements a new field:
int donee_priority ;  
into thread.h

2.

lock_acquire(&lock){
 if (lock_holder->priority < cur->priority) {
	cur->donee_priority = lock_holder->priority // store lock_holder's priority to doner's donee_priority
	lock->holding_queue.add_sorted(cur);  //add cur to this lock's holding queue,holding queue need to be sorted and the head is the highest priority one 
	lock_holder->donate_times++; //increase the donation times
	lock_holder->priority = cur->priority; //donate priority manualy
	thread_block(cur);  //block current thread
	}
}

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
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 1, thread_get_priority ());

  thread_create ("b", PRI_DEFAULT + 2, b_thread_func, &b);
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 2, thread_get_priority ());

  lock_release (&b);
  msg ("Thread b should have just finished.");
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT + 1, thread_get_priority ());

  lock_release (&a);
  msg ("Thread a should have just finished.");
  msg ("Main thread should have priority %d.  Actual priority: %d.",
       PRI_DEFAULT, thread_get_priority ());
}

static void
a_thread_func (void *lock_) 
{
  struct lock *lock = lock_;

  lock_acquire (lock);
  msg ("Thread a acquired lock a.");
  lock_release (lock);
  msg ("Thread a finished.");
}

static void
b_thread_func (void *lock_) 
{
  struct lock *lock = lock_;

  lock_acquire (lock);
  msg ("Thread b acquired lock b.");
  lock_release (lock);
  msg ("Thread b finished.");
}
