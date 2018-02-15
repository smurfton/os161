#include <stdio.h>
#include <unistd.h>

int main (void)
{
	pid_t curpid = getpid();
	printf("This pid is: %u\n", (unsigned) curpid);
	return 0;
}
