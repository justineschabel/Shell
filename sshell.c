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
	fprintf(stdout, "%s\n" , getcwd(NULL, 0)); //ASK: "usage: pwd [-L | -P]" exit [1] ??don't need anymore??
	return;
}


void our_cd(char* path, int* status){ //need status for error number
	if(strcmp(path, "..") == 0){
		char* current_path = getcwd(NULL, 0);
		char* lastSlash = strrchr(current_path, '/'); //finds index of last slash
		free(path);
		path = malloc(lastSlash - current_path + 1);
		memcpy(path, current_path, lastSlash - current_path + 1);

	}
		*status = chdir(path);
		if(*status < 0) //error changing directory
		{
			fprintf(stderr, "Error: no such directory\n");
			*status = 1;
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

int isSpecialChar(char toCheck)
{
	return toCheck == '&' || toCheck == '|' || toCheck == '>' || toCheck == '<';
}

int parse_args(const char* command, char* program_name, char** new_args){ //special characters (< > & |) can have not spaces around them, so need to check for those specifically
	char *buffer = malloc(512);
	int buffer_index = 0;
	int new_args_index = 0;
	int program_size = 0;

	for(int i= 0; i < strlen(command); i++){
		if(isSpecialChar(command[i]))
		{
			while(command[i] == ' '){ //eat up all whitespace
				i++;
			}
			i--; //go back one
			if (new_args_index == 0){
				return -1; //corresponds to invalid command line
			}
			else
			{
				new_args[new_args_index] = malloc(buffer_index);
				memcpy(new_args[new_args_index], buffer, buffer_index); //copy over buffer first
				new_args_index ++;
				char arg[1] = {command[i]};
				new_args[new_args_index] = arg;
				memset(buffer, 0, buffer_index);
				new_args_index ++;
				buffer_index = 0;
			}
		}
		else if(command[i] != ' '){
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
			i--; //go back one
			//fprintf(stdout, "Buffer: %s", buffer);

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
	if(buffer_index > 0) //leftover in buffer
	{
		new_args[new_args_index] = malloc(buffer_index);
		memcpy(new_args[new_args_index], buffer, buffer_index);
		new_args_index ++;
	}
	new_args[new_args_index] = NULL;
	return 0;
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

		status = parse_args(cmd, program_name ,args);
		if(status == -1)
		{
			fprintf(stderr, "Error: invalid command line\n");
			continue;
		}


		pid = fork();
		if(pid == 0) //execute command as child
		{
			if(strcmp(program_name, "exit") == 0){
				exit(0);
			}
			else if (strcmp(program_name, "pwd") == 0){
				exit(0);
			}
			else if(strcmp(program_name, "cd") == 0){
				exit(0);
			}
			status = execvp(program_name, args);
			exit(1);
		}
		else if(pid > 0)
		{
			waitpid(-1, &status, 0);
			if(strcmp(program_name, "exit") == 0){
				our_exit();
			}
			else if (strcmp(program_name, "pwd") == 0){
				our_pwd();
				fprintf(stderr, "+ completed '%s': [%d]\n", cmd , 0);
			}
			else if(strcmp(program_name, "cd") == 0){
				our_cd(args[1], &status);
				fprintf(stderr, "+ completed '%s': [%d]\n", cmd , status);
			}
			else
			{
				fprintf(stderr, "+ completed '%s': [%d]\n", cmd , WEXITSTATUS(status));
			}
		}
		else
		{
			fprintf(stderr, "u gofed");
		}
	}

	return EXIT_SUCCESS;
}
