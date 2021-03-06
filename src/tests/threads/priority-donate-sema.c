/* Low priority thread L acquires a lock, then blocks downing a
   semaphore.  Medium priority thread M then blocks waiting on
   the same semaphore.  Next, high priority thread H attempts to
   acquire the lock, donating its priority to L.

   Next, the main thread ups the semaphore, waking up L.  L
   releases the lock, which wakes up H.  H "up"s the semaphore,
   waking up M.  H terminates, then M, then L, and finally the
   main thread.

   Written by Godmar Back <gback@cs.vt.edu>. */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/synch.h"
#include "threads/thread.h"

struct lock_and_sema 
  {
    struct lock lock;
    struct semaphore sema;
  };

static thread_func l_thread_func;
static thread_func m_thread_func;
static thread_func h_thread_func;

void
test_priority_donate_sema (void) 
{
  struct lock_and_sema ls;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  /* Make sure our priority is the default. */
  ASSERT (thread_get_priority () == PRI_DEFAULT);

  lock_init (&ls.lock);
  sema_init (&ls.sema, 0);
  thread_create ("low", PRI_DEFAULT + 1, l_thread_func, &ls);//goto stage1,line 56
  thread_create ("med", PRI_DEFAULT + 3, m_thread_func, &ls);//stage2:create "med" thread
  thread_create ("high", PRI_DEFAULT + 5, h_thread_func, &ls);//stage4:create "high" thread,goto line 76
  sema_up (&ls.sema);//stage8:-2->-1,"high" unblocked, 'high' is running ,goto line 77
  msg ("Main thread finished.");
}

static void
l_thread_func (void *ls_) 
{
  struct lock_and_sema *ls = ls_;

  lock_acquire (&ls->lock);//stage0,acquire success, L hoding a lock
  msg ("Thread L acquired lock.");
  sema_down (&ls->sema); //stage1:sema 0->-1, then thread which is calling sema_down(which is 'low')blocked ,goto line 43
  msg ("Thread L downed semaphore.");
  lock_release (&ls->lock);
  msg ("Thread L finished.");
}

static void
m_thread_func (void *ls_) 
{
  struct lock_and_sema *ls = ls_;

  sema_down (&ls->sema); //stage:sema -1->-2, then thread "med" blocked ,goto line 44
  msg ("Thread M finished.");
}

static void
h_thread_func (void *ls_) 
{
  struct lock_and_sema *ls = ls_;

  lock_acquire (&ls->lock);//stage7:acquire the lock,falure,"high"blocked,goto line 45
  msg ("Thread H acquired lock.");//stage9:

  sema_up (&ls->sema);//stage10:sema-1->0,M is unblocked ,now readyList:<M>,H is keep running.
  lock_release (&ls->lock);//stage11:release the lock that H old
  msg ("Thread H finished.");
}
