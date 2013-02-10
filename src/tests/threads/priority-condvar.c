/* Tests that cond_signal() wakes up the highest-priority thread
   waiting in cond_wait(). */

#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

static thread_func priority_condvar_thread;
static struct lock lock;
static struct condition condition;

void
test_priority_condvar (void) 
{
  int i;
  
  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  lock_init (&lock);
  cond_init (&condition);

  thread_set_priority (PRI_MIN); //set current thread's priority as 0

  for (i = 0; i < 10; i++) 
    {
      int priority = PRI_DEFAULT - (i + 7) % 10 - 1;
      char name[16];
      snprintf (name, sizeof name, "priority %d", priority);  //priority 23 ; priority
      thread_create (name, priority, priority_condvar_thread, NULL);
    }

  for (i = 0; i < 10; i++) 
    {
      lock_acquire (&lock);
      msg ("Signaling...");
      cond_signal (&condition, &lock);
      msg ("Signaled!");
      lock_release (&lock);
    }
}

static void
priority_condvar_thread (void *aux UNUSED) 
{
  msg ("Thread %s starting.", thread_name ());
  lock_acquire (&lock);
  cond_wait (&condition, &lock);
  msg ("Thread %s woke up.", thread_name ());
  lock_release (&lock);
}

//
//Acceptable output:
//  (priority-condvar) begin
//  (priority-condvar) Thread priority 23 starting.
//  (priority-condvar) Thread priority 22 starting.
//  (priority-condvar) Thread priority 21 starting.
//  (priority-condvar) Thread priority 30 starting.
//  (priority-condvar) Thread priority 29 starting.
//  (priority-condvar) Thread priority 28 starting.
//  (priority-condvar) Thread priority 27 starting.
//  (priority-condvar) Thread priority 26 starting.
//  (priority-condvar) Thread priority 25 starting.
//  (priority-condvar) Thread priority 24 starting.
//  (priority-condvar) Signaling...
//  (priority-condvar) Thread priority 30 woke up.
//  (priority-condvar) Signaling...
//  (priority-condvar) Thread priority 29 woke up.
//  (priority-condvar) Signaling...
//  (priority-condvar) Thread priority 28 woke up.
//  (priority-condvar) Signaling...
//  (priority-condvar) Thread priority 27 woke up.
//  (priority-condvar) Signaling...
//  (priority-condvar) Thread priority 26 woke up.
//  (priority-condvar) Signaling...
//  (priority-condvar) Thread priority 25 woke up.
//  (priority-condvar) Signaling...
//  (priority-condvar) Thread priority 24 woke up.
//  (priority-condvar) Signaling...
//  (priority-condvar) Thread priority 23 woke up.
//  (priority-condvar) Signaling...
//  (priority-condvar) Thread priority 22 woke up.
//  (priority-condvar) Signaling...
//  (priority-condvar) Thread priority 21 woke up.
//  (priority-condvar) end

