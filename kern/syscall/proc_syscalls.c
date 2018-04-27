#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
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

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"\nSyscall: _exit(%d)\n",exitcode);

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
  proc_destroy(p);
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = curproc->p_pid;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;
//  struct proc *childproc;
  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  

  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

