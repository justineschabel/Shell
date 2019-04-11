#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#define COMMANDLINE_MAX 512
/**
purpose: 
	use file input for command
inputs:
	@filename: command line file name to be used as input 
**/
void redirect_input(char* filename){
	int fd = open(filename, O_RDONLY);
	close(STDIN_FILENO);
	dup2(fd,STDIN_FILENO);
	close(fd); 
}

int get_filename(char* filename, const char* command, int starting_index){
	int filename_index = 0;
	while(command[starting_index] != ' ' && command[starting_index] != '\0'){
		filename[filename_index] = command[starting_index];
		filename_index++;
		starting_index++;
	}
	return starting_index;
}


void our_exit(){
	fprintf(stderr, "Bye...\n");
	exit(0);
}


void our_pwd(){ 
	fprintf(stdout, "%s\n" , getcwd(NULL, 0)); 
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



int isSpecialChar(char toCheck)
{
	return toCheck == '&' || toCheck == '|' || toCheck == '>' || toCheck == '<';
}

int parse_args(const char* command, char* program_name, char** new_args){ //special characters (< > & |) can have not spaces around them, so need to check for those specifically
	char *buffer = malloc(COMMANDLINE_MAX);
	int buffer_index = 0;
	int new_args_index = 0;
	int program_size = 0;

	for(int i= 0; i < strlen(command); i++){
		if(isSpecialChar(command[i]))
		{
			char special_command = command[i];
			i++;
			while(command[i] == ' '){ //eat up all whitespace
				i++;
			}
			//i--; //go back one

			if(special_command == '<'){
				char* filename = malloc(COMMANDLINE_MAX);
				i = get_filename(filename, command, i);
				fprintf(stdout, "%s\n", filename);
				redirect_input(filename);
			}

			if (new_args_index == 0){
				return -1; //corresponds to invalid command line
			}
			else
			{
				new_args[new_args_index] = malloc(buffer_index);
				memcpy(new_args[new_args_index], buffer, buffer_index); //copy over buffer first
				new_args_index ++;

				char arg[1] = {command[i]}; //Adds special char to args, need to call function
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
	size_t cmdSize = COMMANDLINE_MAX;
	char *cmd = malloc(cmdSize);
	int status;
	int pid;

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
