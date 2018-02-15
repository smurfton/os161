#include <types.h>
#include <lib.h>
#include <thread.h>
#include <synch.h>
#include <test.h>
#include <kern/errno.h>
volatile int count = 0;
struct semaphore *countsem = NULL;

static
void
unsafethreadcnt(void *argv, unsigned long argc) {
	unsigned long i;
	(void) argv;
	for (i = 0; i < argc; i++)
	{
		count++;
	}
	V(countsem);
}


int 
unsafethreadcounter(int argc, char **argv) {
	char name[16];
	int threads, numcount;
	int i, result;
		
	count = 0;
	if (argc < 3) {
		kprintf("Usage: nsc <threadcount> <numcount>\n");
		return EINVAL;
	}

	threads = atoi(argv[1]);
	numcount = atoi(argv[2]);

	kprintf("Starting unsafe thread counter...\n");

	if (countsem == NULL) {
		countsem = sem_create("UnsafeThreadCnt", 0);
		if (countsem == NULL) {
			return ENOMEM;
		}
	}
		

	for(i = 0; i < threads; i++) {
		snprintf(name, sizeof(name), "unsafethreadcnt");
		result = thread_fork(name, NULL, unsafethreadcnt, NULL, numcount);
		if (result) {
			panic("unsafethreadcounter: thread_fork failed %s)\n", 
					strerror(result));
		}
	}

	for(i = 0; i < threads; i++) {
		P(countsem);
	}
	

	kprintf("Expected Result: %d\n", numcount * threads);
	kprintf("Actual   Result: %d\n", count);

	kprintf("Unsafe Thread Counter Done!\n");

	return 0;
}

struct lock *l = NULL;

static
void 
lockthreadcnt (void *argv, unsigned long argc) {
   unsigned long i;
   (void) argv;
   for (i = 0; i < argc; i++)
   {
		lock_acquire(l);
      count++;
		lock_release(l);
   }
   V(countsem);
}
 

int 
lockthreadcounter (int argc, char **argv) {
   char name[16];
   int threads, numcount;
   int i, result;

   count = 0;
   if (argc < 3) {
      kprintf("Usage: utc <threadcount> <numcount>\n");
      return EINVAL;
   }

   threads = atoi(argv[1]);
   numcount = atoi(argv[2]);

   kprintf("Starting Lock Thread Counter...\n");

   if (countsem == NULL) {
      countsem = sem_create("LockThreadCnt", 0);
      if (countsem == NULL) {
         return ENOMEM;
      }
   }
	
	if (l == NULL) {
		l = lock_create("LockThreadCnt");
		if (l == NULL) {
			sem_destroy(countsem);
			countsem = NULL;
			return ENOMEM;
		}
	}

   for(i = 0; i < threads; i++) {
      snprintf(name, sizeof(name), "lockthreadcnt");
      result = thread_fork(name, NULL, lockthreadcnt, NULL, numcount);
      if (result) {
         panic("unsafethreadcounter: thread_fork failed %s)\n",
               strerror(result));
      }
   }

   for(i = 0; i < threads; i++) {
      P(countsem);
   }


   kprintf("Expected Result: %d\n", numcount * threads);
   kprintf("Actual   Result: %d\n", count);

   kprintf("Lock Thread Counter Done!\n");

   return 0;
}


