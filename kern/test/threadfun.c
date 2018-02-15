
/*
 * Thread test code.
 */
#include <types.h>
#include <lib.h>
#include <thread.h>
#include <synch.h>
#include <test.h>

#define NTHREADS  10

static struct semaphore *tsem = NULL;

static
void
init_sem(void)
{
	if (tsem==NULL) {
		tsem = sem_create("tsem", 0);
		if (tsem == NULL) {
			panic("threadtest: sem_create failed\n");
		}
	}
}

static
void
threadfuntest(void *junk, unsigned long num)
{
	(void) junk;
	kprintf("%lu", num);
	V(tsem);
}

static
void
threadfuntest2(void *junk, unsigned long num)
{
	(void) junk;
	kprintf(":%lu%lu%lu%lu:", num, num, num, num);
	V(tsem);
}

static
void
runthreadtest4(int testtype, int threadcount)
{
	char name[16];
   int i, result;

   for (i=0; i<threadcount; i++) {
      snprintf(name, sizeof(name), "threadtest%d", i);
      result = thread_fork(name, NULL,
                 testtype ? threadfuntest : threadfuntest2,
                 NULL, i);
      if (result) {
         panic("threadfuntest: thread_fork failed %s)\n",
               strerror(result));
      }
   }

   for (i=0; i<threadcount; i++) {
      P(tsem);
   }
}

//TODO
int
runmanythreadtest(int nargs, char **args) {
	char name[16];
	int  result;
	unsigned i;
	unsigned num;
	(void) nargs;
	kprintf("Starting Run Many Thread Test...\n");
	init_sem();
	if (nargs < 2)	{
		num = NTHREADS;
	}
	else	{
		num = atoi(args[1]);
	}
	for (i=0; i<num; i++) {
		snprintf(name, sizeof(name), "threadtest%u", i);
		result = thread_fork(name, NULL,
				threadfuntest,NULL, i);
		if (result) {
			panic("Many Thread Fun Test: thread_fork failed %s)\n",
					strerror(result));
		}
	}
	
	for (i=0; i<num; i++) {
		P(tsem);
	}
	kprintf("\nRun Many Thread Test Done!\n");
	return 0;
}


int
threadtest4(int nargs, char **args)
{
   (void)nargs;
   (void)args;
   init_sem();
   kprintf("Starting thread test 4...\n");
   if(nargs < 2)
      runthreadtest4(0, NTHREADS);
   else
      runthreadtest4(0, atoi(args[1]));
 
	kprintf("\nThread test 4 done.\n");

   return 0;
}


int
threadtest5(int nargs, char **args)
{
   (void)nargs;
   (void)args;

   init_sem();
   kprintf("Starting thread test 5...\n");
	if(nargs < 2)
	   runthreadtest4(1, NTHREADS);
	else
		runthreadtest4(1, atoi(args[1]));
   kprintf("\nThread test 5 done.\n");

   return 0;
}

