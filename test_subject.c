#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main(void) {
	pid_t selfpid = getpid();
	printf("PID: %d\n", selfpid);
	while(1) { ; }

	return 0;
}
