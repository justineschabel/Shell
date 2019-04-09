#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

/**
arguments:
	*potentially move command and args into a struct 
	@command - The entire command line 
	@program_mane - exclusively the program name/first argument
	@new_args - an array of every argument name including program_name
purpose:
	parse and store the entire command line
**/

void parse_args(const char* command, char* program_name, char** new_args){
	char *buffer = malloc(512);
	int buffer_index = 0;
	int new_args_index = 0;

	for(int i= 0; i < strlen(command); i++){
		if(command[i] != ' '){ 
			buffer[buffer_index] = command[i];
			buffer_index++;
		}
		else{ //whitespace 
			while(command[i] == ' '){ //eat up all whitespace
				i++;
			}
			if(new_args_index == 0){ //first characters are the program name 
				memcpy(program_name, buffer, buffer_index);
			}

			new_args[new_args_index] = malloc(buffer_index);
			memcpy(new_args[new_args_index], buffer, buffer_index);
			memset(buffer, 0, buffer_index);
			buffer_index = 0;
			new_args_index ++;
		}
	}
}


int main(int argc, char *argv[])
{
	size_t cmdSize = 512;
	char *cmd = malloc(cmdSize);
	int status;
	int pid;

	while(1)
	{
		fprintf(stdout, "sshell$ ");
		getline(&cmd, &cmdSize, stdin);

		char **args = malloc(17*sizeof(char*));
		char *program_name = malloc(cmdSize);

		parse_args(cmd, program_name ,args); 

		pid = fork();
		if(pid == 0) //execute command as child
		{
			status = execvp(program_name, args);
			exit(1);
		}
		else if(pid > 0)
		{
			waitpid(-1, &status, 0);
			fprintf(stderr, "+ completed '%s': [%d]\n", cmd , WEXITSTATUS(status));
		}
		else
		{
			fprintf(stderr, "u gofed");
		}
	}

	return EXIT_SUCCESS;
}
