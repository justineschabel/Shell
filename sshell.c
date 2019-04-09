#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>


void our_exit(){
	fprintf(stderr, "Bye...\n");
	exit(0);
}


void our_pwd(){
	char *buffer = malloc(512); //ASK how big the path can be 
	fprintf(stdout, "%s\n" , getcwd(buffer, 512)); //ASK: "usage: pwd [-L | -P]" exit [1]
	return;
}


void our_cd(char* path){
	fprintf(stdout, "Path: %s", path );
	char *directory_cmd = malloc(1);
	directory_cmd[0] = '.';
	if(strcmp(path, directory_cmd)==0){
		fprintf(stdout, "up one directory");
	}
	else{
		// char *buffer = malloc(512); //ASK how big the path can be 
		// char* current_path = getcwd(buffer, 512);

		//chdir();
	}
	return;
}


/**
arguments:
	*potentially move command and args into a struct 
	@command - The entire command line 
	@program_name - exclusively the program name/first argument
	@new_args - an array of every argument name including program_name
purpose:
	parse and store the entire command line
**/

// void parse_args(const char* command, char* program_name, char** new_args){
// 	char *buffer = malloc(512);
// 	int buffer_index = 0;
// 	int new_args_index = 0;
// 	int program_size = 0;

// 	for(int i= 0; i < strlen(command); i++){
// 		fprintf(stdout,"command: %s\n", command);
// 		if(command[i] != ' '){ 
// 			buffer[buffer_index] = command[i];
// 			buffer_index++;

// 			if(new_args_index == 0){ //first characters are the program name 
// 				program_name[i] = command[i];
// 				program_size++;
// 			}
// 		}
// 		else{ //whitespace 
// 			while(command[i] == ' '){ //eat up all whitespace
// 				i++;
// 				fprintf(stdout, "i in while: %d \n", i);
// 			}
// 			//memcpy(new_args[0], program_name, program_size);
// 			fprintf(stdout, "i after while: %d, %c \n", i, command[i]);
// 			new_args[new_args_index] = malloc(buffer_index);

// 			fprintf(stdout, "Buffer: %s\n", buffer);

// 			memcpy(new_args[new_args_index], buffer, buffer_index);
// 			memset(buffer, 0, buffer_index);
// 			buffer_index = 0;
// 			new_args_index ++;
// 			fprintf(stdout, "\n %d \n", i);
// 		}
// 	}
// 	new_args[new_args_index] = NULL;
// }

void parse_args(const char* command, char* program_name, char** new_args){
	char *buffer = malloc(512);
	int buffer_index = 0;
	int new_args_index = 0;
	int program_size = 0;

	for(int i= 0; i < strlen(command); i++){
		if(command[i] != ' '){ 
			buffer[buffer_index] = command[i];
			buffer_index++;

			if(new_args_index == 0){ //first characters are the program name 
				program_name[i] = command[i];
				program_size++;
			}
		}
		else{ //whitespace 
			while(command[i] == ' '){ //eat up all whitespace
				i++;
			}

			fprintf(stdout, "Buffer: %s", buffer);

			if (new_args_index == 0){
				new_args[0] = malloc(program_size);
				memcpy(new_args[0], program_name, program_size);
			}
			else{
				new_args[new_args_index] = malloc(buffer_index);
				memcpy(new_args[new_args_index], buffer, buffer_index);
			}
			memset(buffer, 0, buffer_index);
			new_args_index ++;
			buffer_index = 0;
		}
	}
	new_args[new_args_index] = NULL;
}


int main(int argc, char *argv[])
{
	size_t cmdSize = 512;
	char *cmd = malloc(cmdSize);
	int status;
	int pid;

	//site skeleton code
	while(1)
	{
		fprintf(stdout, "sshell$ ");
		int length = getline(&cmd, &cmdSize, stdin);
		cmd[length-1] = '\0';
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
			if(strcmp(program_name, "exit") == 0){
				our_exit();
			}
			else if (strcmp(program_name, "pwd") == 0){
				our_pwd();
			}
			else if(strcmp(program_name, "cd") == 0){
				our_cd(args[1]);
			}
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
