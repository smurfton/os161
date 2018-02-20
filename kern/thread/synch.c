/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
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
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, int initial_count)
{
        struct semaphore *sem;

        KASSERT(initial_count >= 0);

        sem = kmalloc(sizeof(struct semaphore));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
	sem->sem_count = initial_count;

	return sem;
}

void
sem_destroy(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
	kfree(sem->sem_name);
	kfree(sem);
}

void 
P(struct semaphore *sem)
{
	KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
	KASSERT(curthread->t_in_interrupt == false);

	spinlock_acquire(&sem->sem_lock);
	while (sem->sem_count == 0) {
		/*
		 * Bridge to the wchan lock, so if someone else comes
		 * along in V right this instant the wakeup can't go
		 * through on the wchan until we've finished going to
		 * sleep. Note that wchan_sleep unlocks the wchan.
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_lock(sem->sem_wchan);
		spinlock_release(&sem->sem_lock);
		wchan_sleep(sem->sem_wchan);

		spinlock_acquire(&sem->sem_lock);
	}
	KASSERT(sem->sem_count > 0);
	sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
	KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

	sem->sem_count++;
	KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.



struct lock *
lock_create(const char *name)
{ 

	struct lock *lock;

	lock = kmalloc(sizeof(struct lock));
	if (lock == NULL) {
		return NULL;
	}
	
	lock->lk_name = kstrdup(name);
	if (lock->lk_name == NULL) {
		kfree(lock);
		return NULL;
	}
	lock->lk_wchan = wchan_create(lock->lk_name);
	if (lock->lk_wchan == NULL) {
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}
	lock->lk_holder = NULL;
	spinlock_init(&lock->lk_lock);
	return lock;
}


void
lock_destroy(struct lock *lock)
{
	KASSERT(lock != NULL);
	KASSERT(lock->lk_holder == NULL);
	
	spinlock_cleanup(&lock->lk_lock);//SEA
	wchan_destroy(lock->lk_wchan);
	kfree(lock->lk_name);
	kfree(lock);
	
	
}

void
lock_acquire(struct lock *lock) 
{//SEA
	(void) lock;
	return;
	/*
	KASSERT(lock != NULL && lock->lk_name != NULL);
	KASSERT(lock->lk_held != NULL);
	P(lock->lk_held);

	KASSERT(lock->lk_holder == NULL);

	lock->lk_holder = curthread;
	return;
	*/
}

void
lock_release(struct lock *lock)
{
	(void) lock;
	return;
	/*
	KASSERT(lock != NULL && lock->lk_name != NULL);
	KASSERT(lock->lk_holder == curthread);
	
	lock->lk_holder = NULL;
	V(lock->lk_held);
	*/
}

bool
lock_do_i_hold(struct lock *lock)
{
	(void) lock;
	return 1;
	/*
	return (lock != NULL) && (lock->lk_holder == curthread);
	*/
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
	struct cv *cv;

	cv = kmalloc(sizeof(struct cv));
	if (cv == NULL) {
		return NULL;
	}

	cv->cv_name = kstrdup(name);
	if (cv->cv_name==NULL) {
		kfree(cv);
		return NULL;
	}
	cv->cv_sem = sem_create(name, 0);
	if (cv->cv_sem==NULL) {
		kfree(cv);
		return NULL;
	}
	cv->cv_asleep = 0;
   
	cv->cv_lk = lock_create(name);
	if (cv->cv_lk == NULL) {
		kfree(cv);
		return NULL;
	}

	return cv;
}

void
cv_destroy(struct cv *cv)
{
	KASSERT(cv != NULL);
	KASSERT(cv->cv_sem != NULL);
	lock_acquire(cv->cv_lk);
	if(cv->cv_asleep != 0)
	{
		kprintf("\nError: cv_asleep nonzero on %s.\nCurrent value: %d\n", cv->cv_name, cv->cv_asleep);
		KASSERT(false);
	}
	lock_release(cv->cv_lk);
	// add stuff here as needed
	lock_destroy(cv->cv_lk);
	sem_destroy(cv->cv_sem);
	kfree(cv->cv_name);
	kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	KASSERT(cv != NULL && lock != NULL);
	KASSERT(cv->cv_name != NULL && cv->cv_sem != NULL);
	KASSERT(cv->cv_lk != NULL);
	KASSERT(lock_do_i_hold(lock));
	
	// THIS IS INCREDIBLY NOISY
	// DEBUG(DB_SYNCPROB, "\nWait on cv: %s with lock: %s\n", cv->cv_name, cv->cv_sem->sem_name);
	lock_acquire(cv->cv_lk);
	++(cv->cv_asleep);
	lock_release(cv->cv_lk);

	lock_release(lock);

	P(cv->cv_sem);

	lock_acquire(cv->cv_lk);
	--(cv->cv_asleep);
	lock_release(cv->cv_lk);

	lock_acquire(lock);

	return;
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	KASSERT(cv != NULL && lock != NULL);
	KASSERT(cv->cv_name != NULL && cv->cv_sem != NULL);
	KASSERT(lock_do_i_hold(lock));

	DEBUG(DB_SYNCPROB, "\nSignal on CV: %s\n", cv->cv_name);
	
	lock_acquire(cv->cv_lk);	
	if(cv->cv_asleep != 0)
		V(cv->cv_sem);
	lock_release(cv->cv_lk);

	return;
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	int i;
	KASSERT(cv != NULL && lock != NULL);
	KASSERT(cv->cv_name != NULL && cv->cv_sem != NULL);
	KASSERT(cv->cv_lk != NULL);
	KASSERT(lock_do_i_hold(lock));

	//SO NOISY
	//DEBUG(DB_SYNCPROB, "\nBroadcast on CV: %s\n", cv->cv_name);
	lock_acquire(cv->cv_lk);
	for(i = 0; i < cv->cv_asleep; i++)
	{
		V(cv->cv_sem);
	}
	lock_release(cv->cv_lk);

	return;
}
