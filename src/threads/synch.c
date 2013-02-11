/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
*/

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

#define MAX_DEPTH 8
/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
     decrement it.

   - up or "V": increment the value (and wake up one waiting
     thread, if any). */
void
sema_init (struct semaphore *sema, unsigned value) 
{
  ASSERT (sema != NULL);

  sema->value = value;
  list_init (&sema->waiters); //initialized the semaphores
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. */
/* original code --
void
sema_down (struct semaphore *sema)
{
  enum intr_level old_level;

  ASSERT (sema != NULL);
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  while (sema->value == 0)
    {
      list_push_back (&sema->waiters, &thread_current ()->elem);
      thread_block ();
    }
  sema->value--;
  intr_set_level (old_level);
}
*/
/*yan*/
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;
	ASSERT (sema != NULL);
	ASSERT (!intr_context ());
	old_level = intr_disable(); //disable the interrupts

	while (sema->value==0)
	{
	  //insert current thread into waiter list
	  list_insert_ordered(&sema->waiters,&thread_current()->elem,higher_priority,NULL);
	  thread_block();
	}

	sema->value--;
	intr_set_level (old_level);
}





/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) 
{
  enum intr_level old_level;
  bool success;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (sema->value > 0) 
    {
      sema->value--;
      success = true; 
    }
  else
    success = false;
  intr_set_level (old_level);

  return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
/*original code
void
sema_up (struct semaphore *sema) 
{
  enum intr_level old_level;

  ASSERT (sema != NULL);

  old_level = intr_disable ();
  if (!list_empty (&sema->waiters))
    thread_unblock (list_entry (list_pop_front (&sema->waiters),
                                struct thread, elem));
  sema->value++;
  intr_set_level (old_level);
}
*/

/*our implements*/
void sema_up (struct semaphore *sema){
	enum intr_level old_level;
	ASSERT (sema!=NULL);
	old_level=intr_disable();

	sema->value ++ ;  //DON'T MOVE IT:this one seems must been put here

	//if sema do have waiting threads
	if (!list_empty(&sema->waiters))
	{
	  /*unblock the thread with the highest priority*/
	  /*PRE:before we sema_up,we call the sema_down, which this will always gives you a sorted list of waiters , highest priority first*/

	  //thread with highest priority
	  struct thread *max_t;
	  struct list_elem *e = list_min(&sema->waiters, higher_priority, NULL);
	  max_t = list_entry(e, struct thread, elem);

	  //remove it from the waiter list
	  list_remove(e);
	  thread_unblock(max_t);
	}

	intr_set_level(old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) 
{
  struct semaphore sema[2];
  int i;

  printf ("Testing semaphores...");
  sema_init (&sema[0], 0);
  sema_init (&sema[1], 0);
  thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
  for (i = 0; i < 10; i++) 
    {
      sema_up (&sema[0]);
      sema_down (&sema[1]);
    }
  printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) 
{
  struct semaphore *sema = sema_;
  int i;

  for (i = 0; i < 10; i++) 
    {
      sema_down (&sema[0]);
      sema_up (&sema[1]);
    }
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock)
{
  ASSERT (lock != NULL);

  lock->holder = NULL;
  sema_init (&lock->semaphore, 1);

}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock)
{
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (!lock_held_by_current_thread (lock));

  //implementing donation
  //enum intr_level old_level;
  //old_level = intr_disable();

  //PRE:1.HIGHEST PRIORITY THREADS TAKING THE CPU
  //2.CURRENT->THREAD WHICH HAVE HIGHEST PRIORITY
  //3.Thus the lock holder must be in the ready list

  //NEW
  if (thread_mlfqs)
  { //roozbeh
	//in case mlfqs, no need for donations
    sema_down (&lock->semaphore);
    lock->holder = thread_current ();
  }
  else
  {
	//non- mlfqs case, do donation
	//duc:
    struct thread *cur = thread_current();
    struct thread *lock_holder = lock->holder;

    if (lock_holder != NULL)
	    donate(lock, cur->priority, MAX_DEPTH);

    //stick lock to current thread
    cur->target_lock = lock;
    sema_down(&lock->semaphore);
    lock->holder = thread_current();

    lock->max_priority = cur->priority;
    list_insert_ordered(&cur->lock_list, &lock->elem, higher_priority_lock, NULL);
    cur->target_lock = NULL;
  }
  //OLD
  /*if (lock_holder!=NULL && lock_holder->priority < cur->priority)
  {
    cur->donee_priority = lock_holder->priority;
    //insert into this lock's waiting queue according to it's priority
	//list_insert_ordered(&lock->semaphore.waiters, &cur->elem, higher_priority, NULL);
	//lock_holder->donation_times++; //>do we check donation_time initialized as 0?
	lock_holder->priority=cur->priority;

	//thread_block();
  }
  sema_down (&lock->semaphore);//try to release the lock
  lock->holder = thread_current ();*/

  //intr_set_level(old_level);
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock)
{
  bool success;

  ASSERT (lock != NULL);
  ASSERT (!lock_held_by_current_thread (lock));

  success = sema_try_down (&lock->semaphore);
  if (success)
    lock->holder = thread_current ();
  return success;
}

/* Releases LOCK, which must be owned by the current thread.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) 
{
  ASSERT (lock != NULL);
  ASSERT (lock_held_by_current_thread (lock));

  //enum intr_level old_level;
  //old_level = intr_disable();

  //PRE LOCK HOLDER MUST BE THE LOWER ONE , AND HIGHER ONE IS STILL WAITING FOR THE LOWER ONE
  //THE HIGHER ONE 'S CURRENT PRIORITY IS LOWER ONE'S
  if (thread_mlfqs)
  { //roozbeh
	//no need for checking priority donations
    lock->holder = NULL;
 	sema_up (&lock->semaphore);
  }
   else
  {
    struct thread *cur = thread_current();

  //NEW
  //duc: remove lock from list and remove lock holder from lock
    lock->holder = NULL;
    list_remove(&lock->elem);
  /*if any priority change to the donee happends inside the donation,it will save it and restore it after donation finished */
  /*been_donated_aux==is_any_priority_change_when_this_thread_is_in_the_case_of_donation*/
    if (cur-> been_donated_aux)
    {
	  thread_set_priority(cur->saved_priority);
	  cur->been_donated_aux = false;
    }
    sema_up(&lock->semaphore);

    take_back_donation(cur);
  }
}


  //OLD
  //Unused vars
  //struct thread *lock_holder = lock->holder;
  //struct list_elem *e;
  //duc: fixed some type clash issues
  /*if (cur->donation_times>0 && (!list_empty(&lock->semaphore.waiters)))
  {
    // Undefined behavior if LIST is empty before removal in List_pop_front();
	//ASSERT (&lock->semaphore.waiters!=NULL);
	struct list_elem *e = list_max(&lock->semaphore.waiters,higher_priority,NULL);
	struct thread *doner = list_entry(e, struct thread, elem);
	int original_priority = doner->donee_priority;
	cur->donation_times--;
	//thread_unblock(doner);
	sema_up (&lock->semaphore);
	thread_set_priority(original_priority);
  }

  lock->holder = NULL;
  //sema_up (&lock->semaphore); */

  //intr_set_level(old_level);
//}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) 
{
  ASSERT (lock != NULL);

  return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem 
  {
    struct list_elem elem;              /* List element. */
    struct semaphore semaphore;         /* This semaphore. */
  };

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond)
{
  ASSERT (cond != NULL);

  list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) 
{
  struct semaphore_elem waiter;

  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));
  
  sema_init (&waiter.semaphore, 0);
  list_push_back (&cond->waiters, &waiter.elem);
  lock_release (lock);
  sema_down (&waiter.semaphore);
  lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
/*origianl code
void
cond_signal (struct condition *cond, struct lock *lock UNUSED)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  if (!list_empty (&cond->waiters))
    sema_up (&list_entry (list_pop_front (&cond->waiters),
                          struct semaphore_elem, elem)->semaphore);
}
*/

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
/*yan*/
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) 
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);
  ASSERT (!intr_context ());
  ASSERT (lock_held_by_current_thread (lock));

  //sort the list by it's condition value
  if (!list_empty (&cond->waiters))
  {
    list_sort(&cond->waiters,higher_priority_cond,NULL);
    sema_up (&list_entry (list_pop_front (&cond->waiters),
           struct semaphore_elem, elem)->semaphore);
  }
}

void
cond_broadcast (struct condition *cond, struct lock *lock)
{
  ASSERT (cond != NULL);
  ASSERT (lock != NULL);

  while (!list_empty (&cond->waiters))
    cond_signal (cond, lock);
}

void
donate(struct lock *lock,int new_priority,int times)
{
	ASSERT(lock!=NULL);
  struct thread *lock_holder = lock->holder;
  /*if priority of donee is less then doner's , donation happend*/
  if (lock_holder->priority < new_priority)
  {
	/*PRE:every new created doner's priority(we call this doner is
	 * because dontation happends) MUST BE higher then donee's  priority.*/
    lock->max_priority = new_priority;
    lock_holder->been_donated = true;
    lock_holder->priority=new_priority;
//
//    if (!lock_holder->been_donated)
//      lock_holder->prev_priority = lock_holder->priority;

    /*for the nested case*/
    //recursively call donate
    if (lock_holder->status == THREAD_BLOCKED //lock_holder is not current but in the lock holder list
    		&& lock_holder->target_lock != NULL
    		&& times > 0)
      donate(lock_holder->target_lock, new_priority, times-1);
  }
}


void
take_back_donation(struct thread *cur)
{
	ASSERT (cur==thread_current());
  if (cur->been_donated)
  {
	  /*every lock of this thread is released .*/
    if (list_empty(&cur->lock_list))
    {
    	cur->priority = cur->prev_priority;
    	cur->been_donated = false;

    	thread_yield();
    }
    	/*thread still holding some locks(<case multiple2> line 54:lock_release (&a) */
    else
    {
      bool need_yield;
      struct list_elem *e = list_min(&cur->lock_list, higher_priority_lock, NULL);
      struct lock *max_lock =  list_entry(e, struct lock, elem);
      /*current priority is bigger then max lock priority->yield
       * put back to the ready list and let system decide which one need
       * to run next -- avoid troubles*/
      if (cur->priority > max_lock->max_priority)
        need_yield = true;
      /*current priority less or equal to max lock priority->keep running*/
      else
        	need_yield=false;
      /*set current priority to the same level lock's max priority ->avoid unexpected troubles
       * (originally donee have lower priority ,it's high priority is come from the donner,
       * so why not?)*/
      cur->priority = max_lock->max_priority;
      if (need_yield)
    	  thread_yield();
    }
  }
  /*other case : leave it running */
}

/*METHOD:sort lock list as it's max_priority*/
bool
higher_priority_lock (const struct list_elem *a, const struct list_elem *b,
		void *aux)
{
   const struct lock *l1,*l2;
   l1=list_entry (a, struct lock, elem);
   l2=list_entry (b, struct lock, elem);
   return(l1->max_priority > l2->max_priority);
}

/*yan*/
bool
higher_priority_cond (const struct list_elem *e1, const struct list_elem *e2,
               void *aux)
{
  struct thread *t1,*t2;
  const struct list_elem *se1,*se2;
  se1=list_front(&list_entry(e1,struct semaphore_elem,elem)->semaphore.waiters);
  se2=list_front(&list_entry(e2,struct semaphore_elem,elem)->semaphore.waiters);
  t1 = list_entry(se1, struct thread, elem);
  t2 = list_entry(se2, struct thread, elem);
  return (t1->priority > t2->priority);
}
