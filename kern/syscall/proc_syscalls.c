#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <synch.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>

/** SEA **/ //SEA
int 
sys_fork(struct trapframe *tf, int *retval) {
	struct proc * proc;
	struct trapframe *temp_tf;
	int result;
	pid_t childpid;

//	DEBUG(DB_SYSCALL, "Syscall: sys_fork()\n");	

	
	temp_tf = (struct trapframe *) kmalloc(sizeof (struct trapframe));
	if(temp_tf == NULL) {
		return ENOMEM;
	}
	temp_tf = memcpy(temp_tf, tf, sizeof(struct trapframe));

	// create proc
	proc = proc_create_runprogram(curproc->p_name);	
	if (proc == NULL) {
		kfree(temp_tf);
		return ENOMEM;
	}
	childpid = proc->p_pid;
	result = as_copy(curproc->p_addrspace, &(proc->p_addrspace));
	if (result) {
		proc_destroy(proc);
		kfree(temp_tf);
		return result;
	}
	//XXX PID	
	result = thread_fork(curproc->p_name,
			proc,
			enter_forked_process,
			(void *)temp_tf,//need to do something to this
			(unsigned long) 1);
	
	if(result) {
		proc_destroy(proc);
		return result;
	}
	*retval = proc->p_pid;
	return 0;
}

static 
void 
ring(pid_t parent) {
	KASSERT(proctable != NULL);
	P(proc_table_mutex);
	struct proc * pp = proctable[parent];
	KASSERT(pp != NULL);
	V(proc_table_mutex);
	KASSERT(pp->p_lock != NULL);
	KASSERT(pp->p_ring != NULL);
	lock_acquire(pp->p_lock);
	cv_signal(pp->p_ring, pp->p_lock);
}

static
void
migrate(pid_t child) {
	if ( child == -1 ) {
		return;
	}
	P(proc_table_mutex);
	struct proc * pc = proctable[child];
	V(proc_table_mutex);
	if(!lock_do_i_hold(pc->p_lock)) {
		lock_acquire(pc->p_lock);
	}
	pc->pp_pid = 0;
	pc->p_status = PROC_ORPHAN;
	lock_release(pc->p_lock);
}



  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */
//p uw-testbin/onefork
void 
sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
	bool badppid = false;
  enum proc_status old_status;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  //(void)exitcode;
  DEBUG(DB_SYSCALL,"\nSyscall: _exit(%d)\n",exitcode);
  lock_acquire(p->p_lock);
 
  p->p_exitcode = _MKWAIT_EXIT(exitcode);

  old_status = p->p_status;
  p->p_status = PROC_ZOMBIE;

  if (p->pp_pid < 2 && old_status == PROC_OWNED) {
	  old_status = PROC_ORPHAN;
	  badppid = true;
  }

  for (unsigned long i = 0; i < p->p_childcount; i++) {
	  migrate(p->p_children[i]);
	  p->p_children[i] = -1;
  }
  lock_release(p->p_lock);//XXX
  
  if (badppid) { // should be no more DEADBEEF if owned.
	  DEBUG(DB_SYSCALL, "\n_exit: Bad pp_pid: %p\n", (void *) p->pp_pid);
  }
  
  if (old_status == PROC_OWNED) {
	  DEBUG(DB_SYSCALL, "\n_exit: PROC_OWNED: %d:%s\n",(int)p->p_pid, p->p_name);
	  ring(p->pp_pid);
  }
  else if (old_status == PROC_ORPHAN) {
	  DEBUG(DB_SYSCALL, "\n_exit: PROC_ORPHAN: %d:%s\n", (int)p->p_pid, p->p_name);
	  proctable_deregister(p->p_pid);
  }
  else { //PROC_ZOMBIE
	  panic("Exiting process %d:%s already PROC_ZOMBIE!\n", (int)p->p_pid, p->p_name);
  }
  //old ↓
  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);
  
  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  if (old_status == PROC_ORPHAN) {
	  proc_destroy(p);
  }
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  *retval = curproc->p_pid;
  return(0);
}


static 
struct proc *
proc_getzombie(struct proc * p, int *haschild) 
{
	unsigned long i;
	struct proc * child;
	KASSERT(haschild != NULL);
	*haschild = 0;
	for (i = 0; i < p->p_childcount; i++) {
		if (p->p_children[i] != 0) {
			*haschild = 1;
			P(proc_table_mutex);
			child = proctable[p->p_children[i]];
			V(proc_table_mutex);

			lock_acquire(child->p_lock);
			if(child->p_status == PROC_ZOMBIE) {
				lock_release(child->p_lock); // unnecessary
				p->p_children[i] = 0; // clear entry.
				return child;
			}
			lock_release(child->p_lock);
		}
	}
	return NULL;
}

static 
int 
proc_exorcise(pid_t pid) 
{
	int exitcode;
	struct proc *child = proctable_deregister(pid);
	KASSERT(child != NULL);
	KASSERT(child->p_status == PROC_ZOMBIE);
	exitcode = child->p_exitcode; // format happens in _exit()
	proc_destroy(child);
	return exitcode;
}

/* handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;
  struct proc *child;
//  struct proc *childproc;
  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  KASSERT(proc_table_mutex != NULL);

  curthread->t_in_interrupt = false; // this is literally required to sleep.
	
  if (options != 0) {
    return(EINVAL);
  }
  if (pid == WAIT_ANY || pid == WAIT_MYPGRP) {
	  int haschild = 0;

	  lock_acquire(curproc->p_lock);
	  child = proc_getzombie(curproc, &haschild);
	  if (!haschild) {
		  return ECHILD;
	  }
	  else if (child != NULL) {
		  KASSERT(child->pp_pid == curproc->p_pid);
		  exitstatus = proc_exorcise(child->p_pid);
	  }
	  else {
		  cv_wait(curproc->p_ring, curproc->p_lock);
		  child = proc_getzombie(curproc, &haschild);
		  exitstatus = proc_exorcise(child->p_pid);
	  }
  }
  else {
	  unsigned long i = 0;
	  for (i = 0; i < curproc->p_childcount && curproc->p_children[i] != pid; i++)
		  ;
	  if (i == curproc->p_childcount) {
		  //NO SUCH CHILD
		  return ECHILD;
	  }
	  else {
		  curproc->p_children[i] = -1;
	  }
	  P(proc_table_mutex);
	  if (proctable[pid] == NULL) {
		  V(proc_table_mutex);
		  return ESRCH; // tbh this is a bad sign already--the proc struct got corrupted.
	  }

	  child = proctable[pid];
	  V(proc_table_mutex);
	  lock_acquire(child->p_lock);
	 
	  while (child->p_status != PROC_ZOMBIE) {
		  lock_release(child->p_lock);
		  cv_wait(curproc->p_ring, curproc->p_lock);
		  KASSERT(child != NULL);
		  lock_acquire(child->p_lock);
	  }
	  exitstatus = proc_exorcise(child->p_pid);
	  lock_release(child->p_lock);
  }

  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

