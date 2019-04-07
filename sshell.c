#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
	char *cmd = "/bin/date";
	char *args[] = { cmd, "-u", NULL };
	int status;
	int pid;


	pid = fork();
	if(pid == 0) //execute date command as child
	{
		status = execv(cmd, args);

	}
	else
	{
		waitpid(-1, &status, 0);
		fprintf(stderr, "+ completed '%s %s': [%d]\n", args[0], args[1], WEXITSTATUS(status));
	}

	return EXIT_SUCCESS;
}
