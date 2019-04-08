#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
	size_t cmdSize = 512;
	char *cmd = malloc(cmdSize);
	int status;
	int pid;

	while(1)
	{
		fprintf(stdout, "sshell$ ");
		int len = getline(&cmd, &cmdSize, stdin);
		cmd[len - 1] = '\0'; //replace newline at end
		//printf("%s", cmd);
		char *args[] = { cmd, NULL };
		pid = fork();
		if(pid == 0) //execute command as child
		{
			status = execvp(cmd, args);
			perror("invalid argument name");
			exit(1);
		}
		else if(pid > 0)
		{
			waitpid(-1, &status, 0);
			fprintf(stderr, "+ completed '%s': [%d]\n", args[0], WEXITSTATUS(status));
		}
		else
		{
			fprintf(stderr, "u gofed");
		}
	}

	return EXIT_SUCCESS;
}
