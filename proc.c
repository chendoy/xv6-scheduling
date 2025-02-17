#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"
#include <float.h>

long long get_min_acc(struct proc *p, int flag);
void switch_proc(struct proc *p, struct cpu *c);
double cfs_ratio (struct proc *curr_proc);
int sched_type = 0;

struct {
  struct spinlock lock;
  struct proc proc[NPROC];
} ptable;

static struct proc *initproc;

int nextpid = 1;
extern void forkret(void);
extern void trapret(void);

static void wakeup1(void *chan);

void
pinit(void)
{
  initlock(&ptable.lock, "ptable");
}

// Must be called with interrupts disabled
int
cpuid() {
  return mycpu()-cpus;
}

// Must be called with interrupts disabled to avoid the caller being
// rescheduled between reading lapicid and running through the loop.
struct cpu*
mycpu(void)
{
  int apicid, i;
  
  if(readeflags()&FL_IF)
    panic("mycpu called with interrupts enabled\n");
  
  apicid = lapicid();
  // APIC IDs are not guaranteed to be contiguous. Maybe we should have
  // a reverse map, or reserve a register to store &cpus[i].
  for (i = 0; i < ncpu; ++i) {
    if (cpus[i].apicid == apicid)
      return &cpus[i];
  }
  panic("unknown apicid\n");
}

// Disable interrupts so that we are not rescheduled
// while reading proc from the cpu structure
struct proc*
myproc(void) {
  struct cpu *c;
  struct proc *p;
  pushcli();
  c = mycpu();
  p = c->proc;
  popcli();
  return p;
}

//PAGEBREAK: 32
// Look in the process table for an UNUSED proc.
// If found, change state to EMBRYO and initialize
// state required to run in the kernel.
// Otherwise return 0.
static struct proc*
allocproc(void)
{
  struct proc *p;
  char *sp;

  acquire(&ptable.lock);

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == UNUSED)
      goto found;

  release(&ptable.lock);
  return 0;

found:
  p->state = EMBRYO;
  p->pid = nextpid++;

  release(&ptable.lock);

  // Allocate kernel stack.
  if((p->kstack = kalloc()) == 0){
    p->state = UNUSED;
    return 0;
  }
  sp = p->kstack + KSTACKSIZE;

  // Leave room for trap frame.
  sp -= sizeof *p->tf;
  p->tf = (struct trapframe*)sp;

  // Set up new context to start executing at forkret,
  // which returns to trapret.
  sp -= 4;
  *(uint*)sp = (uint)trapret;

  sp -= sizeof *p->context;
  p->context = (struct context*)sp;
  memset(p->context, 0, sizeof *p->context);
  p->context->eip = (uint)forkret;

  // assignment 1 task 4
  p->accumulator = get_min_acc(p, 0);
  p->ps_priority = 5;
  p->retime = 0;
  p->rtime = 1;
  p-> stime = 0;

  return p;
}

//PAGEBREAK: 32
// Set up first user process.
void
userinit(void)
{
  struct proc *p;
  extern char _binary_initcode_start[], _binary_initcode_size[];

  p = allocproc();
  
  initproc = p;
  if((p->pgdir = setupkvm()) == 0)
    panic("userinit: out of memory?");
  inituvm(p->pgdir, _binary_initcode_start, (int)_binary_initcode_size);
  p->sz = PGSIZE;
  memset(p->tf, 0, sizeof(*p->tf));
  p->tf->cs = (SEG_UCODE << 3) | DPL_USER;
  p->tf->ds = (SEG_UDATA << 3) | DPL_USER;
  p->tf->es = p->tf->ds;
  p->tf->ss = p->tf->ds;
  p->tf->eflags = FL_IF;
  p->tf->esp = PGSIZE;
  p->tf->eip = 0;  // beginning of initcode.S

  safestrcpy(p->name, "initcode", sizeof(p->name));
  p->cwd = namei("/");

  // this assignment to p->state lets other cores
  // run this process. the acquire forces the above
  // writes to be visible, and the lock is also needed
  // because the assignment might not be atomic.
  acquire(&ptable.lock);

  p->state = RUNNABLE;
  p->decay_factor = 1;
  
  release(&ptable.lock);
}

// Grow current process's memory by n bytes.
// Return 0 on success, -1 on failure.
int
growproc(int n)
{
  uint sz;
  struct proc *curproc = myproc();

  sz = curproc->sz;
  if(n > 0){
    if((sz = allocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  } else if(n < 0){
    if((sz = deallocuvm(curproc->pgdir, sz, sz + n)) == 0)
      return -1;
  }
  curproc->sz = sz;
  switchuvm(curproc);
  return 0;
}

// Create a new process copying p as the parent.
// Sets up stack to return as if from system call.
// Caller must set state of returned proc to RUNNABLE.
int
fork(void)
{
  int i, pid;
  struct proc *np;
  struct proc *curproc = myproc();

  // Allocate process.
  if((np = allocproc()) == 0){
    return -1;
  }

  // Copy process state from proc.
  if((np->pgdir = copyuvm(curproc->pgdir, curproc->sz)) == 0){
    kfree(np->kstack);
    np->kstack = 0;
    np->state = UNUSED;
    return -1;
  }
  np->sz = curproc->sz;
  np->parent = curproc;
  *np->tf = *curproc->tf;

  // Clear %eax so that fork returns 0 in the child.
  np->tf->eax = 0;

  for(i = 0; i < NOFILE; i++)
    if(curproc->ofile[i])
      np->ofile[i] = filedup(curproc->ofile[i]);
  np->cwd = idup(curproc->cwd);

  safestrcpy(np->name, curproc->name, sizeof(curproc->name));

  pid = np->pid;

  acquire(&ptable.lock);

  np->state = RUNNABLE;

  //assignment 1 task 4
  np->decay_factor = curproc->decay_factor;

  release(&ptable.lock);

  return pid;
}

// Exit the current process.  Does not return.
// An exited process remains in the zombie state
// until its parent calls wait() to find out it exited.
void
exit(int status)
{
  struct proc *curproc = myproc();
  struct proc *p;
  int fd;

  curproc->status = status; //assignment 1 task 3

  if(curproc == initproc)
    panic("init exiting");

  // Close all open files.
  for(fd = 0; fd < NOFILE; fd++){
    if(curproc->ofile[fd]){
      fileclose(curproc->ofile[fd]);
      curproc->ofile[fd] = 0;
    }
  }

  begin_op();
  iput(curproc->cwd);
  end_op();
  curproc->cwd = 0;

  acquire(&ptable.lock);

  // Parent might be sleeping in wait().
  wakeup1(curproc->parent);

  // Pass abandoned children to init.
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->parent == curproc){
      p->parent = initproc;
      if(p->state == ZOMBIE)
        wakeup1(initproc);
    }
  }

  // Jump into the scheduler, never to return.
  curproc->state = ZOMBIE;
  sched();
  panic("zombie exit");
}

// Wait for a child process to exit and return its pid.
// Return -1 if this process has no children.
int
wait(int *status)
{
  struct proc *p;
  int havekids, pid;
  struct proc *curproc = myproc();
  
  acquire(&ptable.lock);
  for(;;){
    // Scan through table looking for exited children.
    havekids = 0;
    for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
      if(p->parent != curproc)
        continue;
      havekids = 1;
      if(p->state == ZOMBIE){
        // Found one.
        if(status != null) 
          *status = p->status; //collecing child's exit code (assignment 1 task 3)
        pid = p->pid;
        kfree(p->kstack);
        p->kstack = 0;
        freevm(p->pgdir);
        p->pid = 0;
        p->parent = 0;
        p->name[0] = 0;
        p->killed = 0;
        p->state = UNUSED;
        release(&ptable.lock);
        return pid;
      }
    }

    // No point waiting if we don't have any children.
    if(!havekids || curproc->killed){
      release(&ptable.lock);
      return -1;
    }

    // Wait for children to exit.  (See wakeup1 call in proc_exit.)
    sleep(curproc, &ptable.lock);  //DOC: wait-sleep
  }
}

//PAGEBREAK: 42
// Per-CPU process scheduler.
// Each CPU calls scheduler() after setting itself up.
// Scheduler never returns.  It loops, doing:
//  - choose a process to run
//  - swtch to start running that process
//  - eventually that process transfers control
//      via swtch back to the scheduler.
void
scheduler(void)
{
  struct proc *p = 0;
  struct cpu *c = mycpu();
  c->proc = 0;
  
  for(;;){
    // Enable interrupts on this processor.
    sti();
    // Loop over process table looking for process to run.
    acquire(&ptable.lock);
    if (sched_type == DEFAULT) //round_robin scheduling
    {
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
          if (sched_type != DEFAULT)
            break;
          else{
            if(p->state != RUNNABLE)
              continue;
            switch_proc(p,c);
          }   
      }
       release(&ptable.lock);
    }
    else  {
      if(sched_type == PS) { //priority scheduler
        for (p = ptable.proc; p< &ptable.proc[NPROC]; p++){
          if (sched_type != PS)
            break;
          else {
            if(p->state == RUNNABLE && p->accumulator == get_min_acc(p,1)) {
            switch_proc(p,c);
            }
          }
        }
        release(&ptable.lock);  
      }
      else //completely fair scheduler
      {
        double min_ratio = DBL_MAX;
        struct proc* min_ratio_proc = null;
        int type_changed = false;
        for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
          if (sched_type != CFS){
            type_changed = true;
            break;
          }
          else{
            if(p->state == RUNNABLE){
               double p_cfs_ratio = cfs_ratio(p);
               if (p_cfs_ratio < min_ratio) {
                 min_ratio = p_cfs_ratio;
                 min_ratio_proc = p;
               }
            }
          }
        }
        if (type_changed == false){ //validate scheduling type not changed
            if(min_ratio_proc != null){ //validate we found a runnable process
                switch_proc(min_ratio_proc,c);
            }   
        }
        release(&ptable.lock);
      }
    } 
  }
}

void
switch_proc(struct proc *p, struct cpu *c) {
    // Switch to chosen process.  It is the process's job
    // to release ptable.lock and then reacquire it
    // before jumping back to us.
    c->proc = p;
    switchuvm(p);
    p->state = RUNNING;

    swtch(&(c->scheduler), p->context);
    switchkvm();

    // Process is done running for now.
    // It should have changed its p->state before coming back.
    c->proc = 0;
    
}


// Enter scheduler.  Must hold only ptable.lock
// and have changed proc->state. Saves and restores
// intena because intena is a property of this
// kernel thread, not this CPU. It should
// be proc->intena and proc->ncli, but that would
// break in the few places where a lock is held but
// there's no process.
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&ptable.lock))
    panic("sched ptable.lock");
  if(mycpu()->ncli != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(readeflags()&FL_IF)
    panic("sched interruptible");
  intena = mycpu()->intena;
  swtch(&p->context, mycpu()->scheduler);
  mycpu()->intena = intena;
}

// Give up the CPU for one scheduling round.
void
yield(void)
{
  struct proc *curproc = myproc();
  acquire(&ptable.lock);  //DOC: yieldlock
  curproc->state = RUNNABLE;

  curproc->accumulator += curproc->ps_priority;

  sched();
  release(&ptable.lock);
}

// A fork child's very first scheduling by scheduler()
// will swtch here.  "Return" to user space.
void
forkret(void)
{
  static int first = 1;
  // Still holding ptable.lock from scheduler.
  release(&ptable.lock);

  if (first) {
    // Some initialization functions must be run in the context
    // of a regular process (e.g., they call sleep), and thus cannot
    // be run from main().
    first = 0;
    iinit(ROOTDEV);
    initlog(ROOTDEV);
  }

  // Return to "caller", actually trapret (see allocproc).
}

// Atomically release lock and sleep on chan.
// Reacquires lock when awakened.
void
sleep(void *chan, struct spinlock *lk)
{
  struct proc *p = myproc();
  
  if(p == 0)
    panic("sleep");

  if(lk == 0)
    panic("sleep without lk");

  // Must acquire ptable.lock in order to
  // change p->state and then call sched.
  // Once we hold ptable.lock, we can be
  // guaranteed that we won't miss any wakeup
  // (wakeup runs with ptable.lock locked),
  // so it's okay to release lk.
  if(lk != &ptable.lock){  //DOC: sleeplock0
    acquire(&ptable.lock);  //DOC: sleeplock1
    release(lk);
  }
  // Go to sleep.
  p->chan = chan;
  p->state = SLEEPING;

  sched();

  // Tidy up.
  p->chan = 0;

  // Reacquire original lock.
  if(lk != &ptable.lock){  //DOC: sleeplock2
    release(&ptable.lock);
    acquire(lk);
  }
}

//PAGEBREAK!
// Wake up all processes sleeping on chan.
// The ptable lock must be held.
static void
wakeup1(void *chan)
{
  struct proc *p;

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++)
    if(p->state == SLEEPING && p->chan == chan) {

      p->state = RUNNABLE;
      p->accumulator = get_min_acc(p,0);
    }

}

// Wake up all processes sleeping on chan.
void
wakeup(void *chan)
{
  acquire(&ptable.lock);
  wakeup1(chan);
  release(&ptable.lock);
}

// Kill the process with the given pid.
// Process won't exit until it returns
// to user space (see trap in trap.c).
int
kill(int pid)
{
  struct proc *p;

  acquire(&ptable.lock);
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->pid == pid){
      p->killed = 1;
      // Wake process from sleep if necessary.
      if(p->state == SLEEPING)
        p->state = RUNNABLE;
      release(&ptable.lock);
      return 0;
    }
  }
  release(&ptable.lock);
  return -1;
}

//PAGEBREAK: 36
// Print a process listing to console.  For debugging.
// Runs when user types ^P on console.
// No lock to avoid wedging a stuck machine further.
void
procdump(void)
{
  static char *states[] = {
  [UNUSED]    "unused",
  [EMBRYO]    "embryo",
  [SLEEPING]  "sleep ",
  [RUNNABLE]  "runble",
  [RUNNING]   "run   ",
  [ZOMBIE]    "zombie"
  };
  int i;
  struct proc *p;
  char *state;
  uint pc[10];

  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if(p->state == UNUSED)
      continue;
    if(p->state >= 0 && p->state < NELEM(states) && states[p->state])
      state = states[p->state];
    else
      state = "???";
    cprintf("%d %s %s", p->pid, state, p->name);
    if(p->state == SLEEPING){
      getcallerpcs((uint*)p->context->ebp+2, pc);
      for(i=0; i<10 && pc[i] != 0; i++)
        cprintf(" %p", pc[i]);
    }
    cprintf("\n");
  }
}

int
set_ps_priority(int priority) 
{
  struct proc *curproc = myproc();
  if(priority < 1 || priority > 10) {
    cprintf("set_ps_priority failed, cannot change priority to %d\n", priority);
    return -1;
  }
  curproc->ps_priority = priority;
  return 0;
}

int
policy(int policy_id)
{
  char *policy_str = 0;
  if(policy_id < 0 || policy_id > 3) {
    cprintf("Error replacing policy, no such a policy number (%d)\n", policy_id);
    return -1;
  }

  switch(policy_id) {
      case 0:
          policy_str = "Default Policy";
      break;
      case 1:
          policy_str = "Priority Policy";
      break;
      case 2:
          policy_str = "CFS Policy";
      break;
  }

  sched_type = policy_id;
  cprintf("Policy has been successfully changed to %s\n",policy_str);
  return 0;
}

int
set_cfs_priority(int priority)
{
  if(priority < 1 || priority > 3) {
    cprintf("Error changing CFS priority, illegal CFS priority (%d)\n", priority);
    return -1;
  }
  switch (priority) {
    case 1:
      myproc()->decay_factor = 0.75;
    break;
    case 2:
    myproc()->decay_factor = 1;
    break;
    case 3:
    myproc()->decay_factor = 1.25;
    break;
  }
  return 0;
}

long long
get_min_acc(struct proc *curr_proc, int flag) // flag = 1 -> find actual minimum (no zero)
{
  struct proc *p;
  long long min_acc = curr_proc->accumulator;
  int not_single_proc = false;
  for(p = ptable.proc; p < &ptable.proc[NPROC]; p++){
    if((p->state == RUNNABLE || p->state == RUNNING)) {
      if(p != curr_proc)
        not_single_proc = true;
      if(p->accumulator < min_acc)
        min_acc = p->accumulator;
    }
      if(not_single_proc == false && !flag)
        min_acc = 0;
  }
  return min_acc;
}

double
cfs_ratio (struct proc *curr_proc)
{
  double ratio;
  int numerator = curr_proc->rtime * curr_proc->decay_factor;
  int denominator = curr_proc->rtime + (curr_proc->stime + curr_proc->retime);
  ratio = denominator;
  if(denominator != 0)
    ratio = numerator / ratio;
  return ratio;
}

// lock on ptable here?
void
update_cfs_stats(void) {
  int i;
  struct proc *p = 0;
  for(i = 0; i < NPROC; i++) {
    p = &ptable.proc[i];
    switch (p->state) {
      case RUNNING:
        p->rtime++;
      break;
      case RUNNABLE: 
        p->retime++;
      break;
      case SLEEPING:
        p->stime++;
      break;
      default:
      break;
    }
  } 
}

int 
proc_info(struct perf *performance) {

  if(performance == null) 
    return -1;

  int pid = myproc()->pid;
  int ps_priority = performance->ps_priority;
  int stime = performance->stime;
  int retime = performance->retime;
  int rtime = performance->rtime;

  cprintf("%d          %d          %d       %d        %d\n", pid, ps_priority, stime, retime, rtime);
  return 0;
}

int
query_perf(int flag) {

    switch (flag) {
      case 0:
        return myproc()->ps_priority;
      break;
      case 1:
        return myproc()->stime;
      break;
      case 2: 
        return myproc()->retime;
      break;
      case 3: 
        return myproc()->rtime;
      break;
    }
    return -1;
}