#include <stdio.h>
#include <unistd.h>
// pid_t fork(void);

int 
main (void) {
	pid_t pid = fork();
	printf("Forked PID: %u\n", (unsigned) pid);
	return 0;
}
