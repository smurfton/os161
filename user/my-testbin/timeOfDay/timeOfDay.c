#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
int main (void)
{
	time_t curtime = (time_t) 0;
	unsigned tsecs, tmins, thours;
	char ampm;
	int curerr;
	//curtime = time(NULL);
	time(&curtime);
	if (curtime == -1)
	{
		curerr = errno;
		printf("Time error: %s\n", strerror(errno));
		errno = curerr;
		return errno;
	}

	
	tsecs = curtime % 60;
	tmins = (curtime/60) % 60;
	thours = (curtime/360) % 24;
	if (thours == 0)
	{
		thours = 12;
		ampm = 'A';
	}
	else if (thours == 12)
	{
		ampm = 'P';
	}
	else if (thours > 12)
	{
		ampm = 'P';
		thours -= 12;
	}
	else // thours < 12 && thours != 0
	{
		ampm = 'A';
	}

	tmins %= 60;
	tsecs %= 60;

	printf("The time is %2u:%2u:%2u %cM.\n", thours, tmins, tsecs, ampm);
	return 0;
}
