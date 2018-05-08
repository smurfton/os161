/*
 * Copyright (c) 2013
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Process support.
 *
 * There is (intentionally) not much here; you will need to add stuff
 * and maybe change around what's already present.
 *
 * p_lock is intended to be held when manipulating the pointers in the
 * proc structure, not while doing any significant work with the
 * things they point to. Rearrange this (and/or change it to be a
 * regular lock) as needed. //SEA It's a lock now.
 *
 * Unless you're implementing multithreaded user processes, the only
 * process that will have more than one thread is the kernel process.
 */

#include <types.h>
#include <proc.h>
#include <current.h>
#include <addrspace.h>
#include <vnode.h>
#include <vfs.h>
#include <synch.h>
#include <kern/fcntl.h>  
#include <limits.h> 
/*
 * The process for the kernel; this holds all the kernel-only threads.
 */
struct proc *kproc;

/*
 * Mechanism for making the kernel menu thread sleep while processes are running
 */
#ifdef UW
/* count of the number of processes, excluding kproc */
static unsigned int proc_count;
/* provides mutual exclusion for proc_count */
/* it would be better to use a lock here, but we use a semaphore because locks are not implemented in the base kernel */ 
static struct semaphore *proc_count_mutex;
/* used to signal the kernel menu thread when there are no processes */
struct semaphore *no_proc_sem;   
#endif  // UW

/*
 * The most recently assigned process id
 */
static pid_t lastpid;

/*
 * Process Table
 */
static 
int
proctable_insert(struct proc *p) {
	pid_t i;
	struct proc **nproctable;
	KASSERT(proctable != NULL);
	P(proc_table_mutex);
	for (i = lastpid; i < proc_table_size; i++) {
		if (proctable[(unsigned) i] == NULL) {
			proctable[(unsigned) i] = p;
			kprintf("pid: %d pointer: %p\n", (int) i, (void *) p);
			lastpid = i;
			p->p_pid = i;
			V(proc_table_mutex);
			return 0;
		}
	}
	if (i >= proc_table_size && proc_table_size < MAXPROCTABLE) {
		nproctable = (struct proc **) kmalloc(2 * proc_table_size * sizeof(struct proc *));
		if (nproctable == NULL) {
			V(proc_table_mutex);
			panic("Proc_table_insert failed!");
			return 1;
		}
		
		memcpy((void*)nproctable, (void*)proctable, (size_t) proc_table_size * sizeof (struct proc *));
		kfree(proctable);
		proctable = nproctable;
		proc_table_size *= 2;
		proctable[(unsigned) i] = p;
		V(proc_table_mutex);
		p->p_pid = i;
		lastpid = i;
		

		return 0;
	}
	V(proc_table_mutex);
	return 1;

}

struct proc *
proctable_deregister(pid_t pid) {
	struct proc * proc;
	P(proc_table_mutex);
	proc = proctable[pid];
	proctable[pid] = NULL;
	if (lastpid > pid) {
		lastpid = pid;
	}
	V(proc_table_mutex);
	return proc;
}

/*
 * Create a proc structure.
 */
static
struct proc *
proc_create(const char *name)
{
	struct proc *proc;
//	int result;
	proc = kmalloc(sizeof(*proc));
	if (proc == NULL) {
		return NULL;
	}
	proc->p_name = kstrdup(name);
	if (proc->p_name == NULL) {
		kfree(proc);
		return NULL;
	}
	proc->p_lock = lock_create("plock");
	if (proc->p_lock == NULL) {
		kfree(proc->p_name);
		kfree(proc);
		return NULL;
	}
	proc->p_children = kmalloc(32 * sizeof( pid_t)); 
	//TODO make it adjust size
	if(proc->p_children == NULL) {
		lock_destroy(proc->p_lock);
		kfree(proc->p_name);
		kfree(proc);
		return NULL;
	}
	proc->p_ring = cv_create(name);
	if(proc->p_ring == NULL) {
		kfree(proc->p_children);
		lock_destroy(proc->p_lock);
		kfree(proc->p_name);
		kfree(proc);
		return NULL;
	}
	proc->p_childcount = 32; // TODO: make it adjust size
	for(unsigned long i = 0; i < proc->p_childcount; i++) {
		proc->p_children[i] = -1;
	}


	proc->p_exitcode	= -1;
	spinlock_init(&proc->p_threadlock);
	threadarray_init(&proc->p_threads);

	/* VM fields */
	proc->p_addrspace = NULL;

	/* VFS fields */
	proc->p_cwd = NULL;
#ifdef UW
	proc->console = NULL;
#endif // UW

	return proc;
}

/*
 * Destroy a proc structure.
 */
void
proc_destroy(struct proc *proc)
{
	/*
         * note: some parts of the process structure, such as the address space,
         *  are destroyed in sys_exit, before we get here
         *
         * note: depending on where this function is called from, curproc may not
         * be defined because the calling thread may have already detached itself
         * from the process.
	 */
	KASSERT(proc != NULL);
	KASSERT(proc != kproc);

	/*
	 * We don't take p_lock in here because we must have the only
	 * reference to this structure. (Otherwise it would be
	 * incorrect to destroy it.)
	 */

	/* VFS fields */
	if (proc->p_cwd) {
		VOP_DECREF(proc->p_cwd);
		proc->p_cwd = NULL;
	}


#ifndef UW  // in the UW version, space destruction occurs in sys_exit, not here
	if (proc->p_addrspace) {
		/*
		 * In case p is the currently running process (which
		 * it might be in some circumstances, or if this code
		 * gets moved into exit as suggested above), clear
		 * p_addrspace before calling as_destroy. Otherwise if
		 * as_destroy sleeps (which is quite possible) when we
		 * come back we'll be calling as_activate on a
		 * half-destroyed address space. This tends to be
		 * messily fatal.
		 */
		struct addrspace *as;

		as_deactivate();
		as = curproc_setas(NULL);
		as_destroy(as);
	}
#endif // UW

#ifdef UW
	if (proc->console) {
	  vfs_close(proc->console);
	}
#endif // UW
	threadarray_cleanup(&proc->p_threads);
	spinlock_cleanup(&proc->p_threadlock);
	cv_destroy(proc->p_ring);
	lock_destroy(proc->p_lock);
	kfree(proc->p_children);	
	kfree(proc->p_name);
	kfree(proc);

#ifdef UW
	/* decrement the process count */
        /* note: kproc is not included in the process count, but proc_destroy
	   is never called on kproc (see KASSERT above), so we're OK to decrement
	   the proc_count unconditionally here */
	P(proc_count_mutex); 
	KASSERT(proc_count > 0);
	proc_count--;
	/* signal the kernel menu thread if the process count has reached zero */
	if (proc_count == 0) {
	 	V(no_proc_sem);
	}
	V(proc_count_mutex);
#endif // UW
	

}

/*
 * Create the process structure for the kernel.
 */
void
proc_bootstrap(void) {
	proctable = (struct proc **) kmalloc(MINPROCTABLE * sizeof(struct proc *)); //SEA
	if (proctable == NULL) {
		panic("Could not allocate process table");
	}
	proc_table_size = MINPROCTABLE;
	lastpid = (pid_t) 1;
	
	kproc = proc_create("[kernel]");  
	if (kproc == NULL) {
		panic("proc_create for kproc failed\n");
	}
	kproc->p_status = PROC_ORPHAN;	
	//Space is guaranteed before other processes are created.
	proctable[lastpid] = kproc; //SEA
	lastpid++; //SEA
#ifdef UW
	proc_count = 0;
	proc_count_mutex = sem_create("proc_count_mutex",1);
	if (proc_count_mutex == NULL) {
		panic("could not create proc_count_mutex semaphore\n");
	}
	no_proc_sem = sem_create("no_proc_sem",0);
	if (no_proc_sem == NULL) {
		panic("could not create no_proc_sem semaphore\n");
	}
	proc_table_mutex = sem_create("proctable", 1);
	if (proc_table_mutex == NULL) {
		panic("sem_create for proc_table_mutex failed\n");
	}
#endif // UW 
}

/*
 * Create a fresh proc for use by runprogram.
 *
 * It will have no address space and will inherit the current
 * process's (that is, the kernel menu's) current directory.
 */
struct proc *
proc_create_runprogram(const char *name)
{
	struct proc *proc;
	char *console_path;
	unsigned long i;
	proc = proc_create(name);
	if (proc == NULL) {
	//	panic("proc create fails!");
		return NULL;
	}
	if (proctable_insert(proc)) {
		proc_destroy(proc);
		panic("proctable insert failed!\n");
		return NULL;
	}
	for (i = 0; i < curproc->p_childcount && curproc->p_children[i] != -1; i++)
		;
	
	if (i >= curproc->p_childcount) {
		//TODO
		panic("No empty children!"); // no way to deal with this yet //XXX
		return NULL;
	}
	else {
		curproc->p_children[i] = proc->p_pid;
	}
		
//	kprintf("new pid: %u\n", (unsigned) lastpid);
	KASSERT(proc->p_pid >= PID_MIN); 
	KASSERT(proc->p_pid <= PID_MAX);

	proc->pp_pid = curproc->p_pid;
// Proc status
   if(proc->pp_pid == 0) {
		proc->p_status = PROC_ORPHAN;
	}
	else {
		proc->p_status = PROC_OWNED;
	}

#ifdef UW
	/* open the console - this should always succeed */
	console_path = kstrdup("con:");
	if (console_path == NULL) {
	  panic("unable to copy console path name during process creation\n");
	}
	if (vfs_open(console_path,O_WRONLY,0,&(proc->console))) {
	  panic("unable to open the console during process creation\n");
	}
	kfree(console_path);
#endif // UW
	  
	/* VM fields */

	proc->p_addrspace = NULL;

	/* VFS fields */

#ifdef UW
	/* we do not need to acquire the p_lock here, the running thread should
           have the only reference to this process */
        /* also, acquiring the p_lock is problematic because VOP_INCREF may block */
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		proc->p_cwd = curproc->p_cwd;
	}
#else // UW
	lock_acquire(curproc->p_lock);
	if (curproc->p_cwd != NULL) {
		VOP_INCREF(curproc->p_cwd);
		proc->p_cwd = curproc->p_cwd;
	}
	lock_release(curproc->p_lock);
#endif // UW

#ifdef UW
	/* increment the count of processes */
        /* we are assuming that all procs, including those created by fork(),
           are created using a call to proc_create_runprogram  */
	P(proc_count_mutex); 
	proc_count++;
	V(proc_count_mutex);

#endif // UW

	return proc;
}

/*
 * Add a thread to a process. Either the thread or the process might
 * or might not be current.
 */
int
proc_addthread(struct proc *proc, struct thread *t)
{
	int result;

	KASSERT(t->t_proc == NULL);

	spinlock_acquire(&proc->p_threadlock);
	result = threadarray_add(&proc->p_threads, t, NULL);
	spinlock_release(&proc->p_threadlock);
	if (result) {
		return result;
	}
	
	t->t_proc = proc;
	return 0;
}

/*
 * Remove a thread from its process. Either the thread or the process
 * might or might not be current.
 */
void
proc_remthread(struct thread *t)
{
	struct proc *proc;
	unsigned i, num;

	proc = t->t_proc;
	KASSERT(proc != NULL);

	spinlock_acquire(&proc->p_threadlock);
	/* ugh: find the thread in the array */
	num = threadarray_num(&proc->p_threads);
	for (i=0; i<num; i++) {
		if (threadarray_get(&proc->p_threads, i) == t) {
			threadarray_remove(&proc->p_threads, i);
			spinlock_release(&proc->p_threadlock);
			t->t_proc = NULL;
			return;
		}
	}
	/* Did not find it. */
	spinlock_release(&proc->p_threadlock);
	panic("Thread (%p) has escaped from its process (%p)\n", t, proc);
}

/*
 * Fetch the address space of the current process. Caution: it isn't
 * refcounted. If you implement multithreaded processes, make sure to
 * set up a refcount scheme or some other method to make this safe.
 */
struct addrspace *
curproc_getas(void)
{
	struct addrspace *as;
#ifdef UW
        /* Until user processes are created, threads used in testing 
         * (i.e., kernel threads) have no process or address space.
         */
	if (curproc == NULL) {
		return NULL;
	}
#endif
	// Occasionally this happens during a vm fault.
	if (curthread->t_in_interrupt) {
		as = curproc->p_addrspace;
	}
	else {
		lock_acquire(curproc->p_lock);
		as = curproc->p_addrspace;
		lock_release(curproc->p_lock);
	}
	return as;
}

/*
 * Change the address space of the current process, and return the old
 * one.
 */
struct addrspace *
curproc_setas(struct addrspace *newas)
{
	struct addrspace *oldas;
	struct proc *proc = curproc;


	lock_acquire(proc->p_lock);
	oldas = proc->p_addrspace;
	proc->p_addrspace = newas;
	lock_release(proc->p_lock);
	return oldas;
}
