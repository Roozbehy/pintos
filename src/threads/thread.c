#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/timer.h" //Roozbeh
#include "threads/Fixed_Point_Arithmetic.h" //Roozbeh

#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

//Alarm clock
static struct list sleep_list;

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
static struct list all_list;

/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;
//roozbeh
int64_t load_avg;

static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);
//'ASSERT' is program modification -- just ignore all of them 
  
  //init the sleep list, for Alarm clock
  list_init(&sleep_list);
  
  lock_init (&tid_lock);//MUTEX
  list_init (&ready_list);
  list_init (&all_list);//all_list:  list of all processes stored.

  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();//intial_thread is the subthread 
  init_thread (initial_thread, "main", PRI_DEFAULT);//defualt priority is 31(max is 61,min is 0)
  initial_thread->status = THREAD_RUNNING;//make the thread status as 'running'
  initial_thread->tid = allocate_tid ();//and give him a thread id
}// after initialization , the thread whill be scheduled into CPU processing , by schedule() 

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();
  if (thread_mlfqs) //Roozbeh
  {
  	if (t != idle_thread)
  	  t->recent_cpu = FloatPlusInt(t->recent_cpu, 1);

  	if ((timer_ticks()) % TIMER_FREQ == 0)
  	{
  	  load_avg_update();
  	  thread_foreach(recent_cpu_update, NULL);

  	}

  	if (timer_ticks() % 4 == 0)
  	{
  	  thread_foreach(priority_update, NULL);
  	  intr_yield_on_return();
  	}
  }

  /* Update statistics. */
  if (t == idle_thread)
    idle_ticks++;
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;


  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();

}

/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  init_thread (t, name, priority);
  tid = t->tid = allocate_tid ();

  /*yan*/
  //t->donation_times=0; //init donation times;
  /*==yan*/

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  /*yan*/
  t->been_donated_aux = false;
  intr_set_level (old_level);

  /* Add to run queue. */
///  msg("xxxx ");
  thread_unblock (t);

  if (thread_mlfqs)
  { //Roozbeh
    t->recent_cpu = thread_current()->recent_cpu;
  	t->nice = thread_current()->nice;
  	priority_update(t,NULL);
  }
  return tid;
}

/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */

//FAQ:why block method doesnt have inputs?
//-Ans:the only possible for thread to be blocked is this thread is currently running ,
//thus , iff thread been blocked , this thread is currently running
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */

//YAN: the function of this method actually is put threat 't' into the readylist
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;
  ASSERT (is_thread (t));

  //old_level = intr_disable ();

  ASSERT (t->status == THREAD_BLOCKED);
  list_insert_ordered(&ready_list, &t->elem, higher_priority, NULL);
  t->status = THREAD_READY;

  if (!list_empty(&ready_list))
  {
    struct thread *cur = thread_current();
    struct list_elem *e = list_min(&ready_list, higher_priority, NULL);
    int max_priority = list_entry(e, struct thread, elem)->priority;
    if ((cur->priority < max_priority)
        &&(cur != idle_thread))
    {
      if (intr_context())
        intr_yield_on_return();
      else
        thread_yield();
    }
  }

  //intr_set_level (old_level);
}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  if(thread_mlfqs)
  {
  	list_sort(&ready_list, higher_priority, NULL);
  }

  old_level = intr_disable ();
  if (cur != idle_thread) 
    list_insert_ordered(&ready_list, &cur->elem, higher_priority, NULL);
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  struct thread *cur = thread_current();
  cur->prev_priority = new_priority;

  /*deal with case priority-donate-lower*/
  /*if the running thread is been donated,and if you want to reduce it's
   * priority , it will update into saved_priority , after the donation
   * finished , thread's original priority will be set to the saved_priority
   */
  if (cur->been_donated )
  {
	cur->saved_priority = new_priority; //save the priority change inside the donation procedure
	cur->been_donated_aux = true;
  }
  else
    cur->priority = new_priority;

  if (!list_empty(&ready_list))
  {
    struct list_elem *e = list_max(&ready_list, higher_priority, NULL);
    int max_priority = list_entry(e, struct thread, elem)->priority;
    if (max_priority > cur->priority)
      thread_yield();
  }


}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void //roozbeh
thread_set_nice (int new_nice)
{
  thread_current()->nice = new_nice;
  priority_update(thread_current(),NULL);
  struct thread *t = list_min(&ready_list, higher_priority, NULL);
  if ((thread_current()->priority < t->priority)
	&& !(list_empty(&ready_list)))
	thread_yield();
}

struct list*
thread_get_readylist (){
  return &ready_list;
}

/* Returns the current thread's nice value. */
int //roozbeh
thread_get_nice (void) 
{
  return thread_current()->nice;
}

/* Returns 100 times the system load average. */
int //roozbeh
thread_get_load_avg (void) 
{
  int result = FloatMultInt(load_avg,100);
  result = FloatToIntNearest(result);
  return result;
}

/* Returns 100 times the current thread's recent_cpu value. */
int  //roozbeh
thread_get_recent_cpu (void) 
{
  int recentCPU = FloatMultInt(thread_current()->recent_cpu,100);
  recentCPU = FloatToIntNearest(recentCPU);
  return recentCPU;
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  enum intr_level old_level;

  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;

  //duc:
  t->prev_priority = priority;
  t->been_donated = false;
  list_init(&t->lock_list);

  t->magic = THREAD_MAGIC;

  old_level = intr_disable ();
  list_push_back (&all_list, &t->allelem);
  intr_set_level (old_level);

  if (thread_mlfqs)
  { //Roozbeh
  	load_avg = 0;
  	initial_thread->nice = 0;
  	initial_thread->recent_cpu = 0;
  }
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();//cur points to the thread which is currently using CPU
  struct thread *next = next_thread_to_run ();//MOST IMPORTANT PART:next points the next thread need to run , and then run it .
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);

//Alarm clock methods. 1 for adding a thread to sleep_list,
//the other check the sleep_list to see if any threads needs to be woken up
void
thread_addToSleep(int64_t ticks)
{
  struct thread *cur = thread_current();
  enum intr_level old_level;
  
  old_level = intr_disable();
  
  if (cur != idle_thread)
  {
    cur->sleep_time = timer_ticks() + ticks;
    //thread_blocked() doesnt work
    cur->status = THREAD_BLOCKED;
    list_insert_ordered(&sleep_list, &cur->elem, less_sleep_time, NULL);
  }
  
  schedule();
  intr_set_level(old_level);
}

void
thread_checkSleep()
{
  struct list_elem *e,*f;
  struct thread *t;

  for (e  = list_begin(&sleep_list);
       e != list_end(&sleep_list);)
  {
    t = list_entry(e, struct thread, elem);
    if (t->sleep_time <=timer_ticks())
    {
      f = list_remove(e);
      t = list_entry(e, struct thread, elem);
      thread_unblock(t);
      e = f;
    }
    else
      break;
  }
}

//Compare sleep time of 2 threads,
//will be used to create sleep_list
bool
less_sleep_time(const struct list_elem *e1, const struct list_elem *e2, 
               void *aux)
{
  struct thread *t1,*t2;
  t1 = list_entry(e1, struct thread, elem);
  t2 = list_entry(e2, struct thread, elem);
  return (t1->sleep_time < t2->sleep_time);
}

/*our implementation */
//similar to above, but for priority comparison
bool 
higher_priority (const struct list_elem *e1, const struct list_elem *e2, 
               void *aux)
{
  struct thread *t1,*t2;
  t1 = list_entry(e1, struct thread, elem);
  t2 = list_entry(e2, struct thread, elem);
  return (t1->priority > t2->priority);
}

bool
lower_priority (const struct list_elem *e1, const struct list_elem *e2,
               void *aux)
{
  struct thread *t1,*t2;
  t1 = list_entry(e1, struct thread, elem);
  t2 = list_entry(e2, struct thread, elem);
  return (t1->priority < t2->priority);
}


void
priority_update(struct thread *t, void *aux UNUSED)
{ //Roozbeh
  int priority = PRI_MAX - (FloatToIntZero(t->recent_cpu) / 4)
			- (t->nice * 2);
  if (priority > PRI_MAX)
    priority = PRI_MAX;
  if (priority < PRI_MIN)
	priority = PRI_MIN;
  t->priority = priority;
}


void //Roozbeh //Checked
load_avg_update()
{
  int ready_threads = (int) list_size(&ready_list) + 1;
  if (thread_current() == idle_thread)
	ready_threads--;
  int tmp = FloatMultInt(load_avg,59);
  tmp = FloatDivInt(tmp,60);
  int tmp2 = IntToFloat(ready_threads);
  tmp2 = FloatDivInt(tmp2,60);
  int result = FloatPlusFloat(tmp,tmp2);
  load_avg = result;
}

void //Roozbeh //int64
recent_cpu_update(struct thread* t, void *aux UNUSED) {
  int64_t top = FloatMultInt(load_avg,2);
  int64_t bottom = FloatPlusInt(top,1);
  int64_t coefficient = FloatDivFloat(top,bottom);
  int64_t product = FloatMultFloat(coefficient,t->recent_cpu);
  int64_t result = FloatPlusInt(product,t->nice);
  t->recent_cpu = result;
}

