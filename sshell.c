#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#define COMMANDLINE_MAX 512

struct Command {
	char** args;
	char* input_file;
	char* output_file;
};

enum Errors {
	NO_ERR,
	INV_CMDLINE,
	CMD_NOT_FND,
	NO_DIR,
	CANT_OPN_INPUT_FILE,
	NO_INPUT_FILE,
	CANT_OPN_OUTPUT_FILE,
	NO_OUTPUT_FILE,
	MISLOC_INPUT_REDIR,
	MISLOC_OUTPUT_REDIR,
	MISLOC_AMP,
	JOB_RUNNING
};

/**
purpose:
	use file input for command
inputs:
	@filename: command line file name to be used as input
**/
int redirect_input(char* filename){
	int fd = open(filename, O_RDONLY);
	printf("%d\n", fd);
	if(fd == -1)
	{
		return NO_INPUT_FILE;
	}
	close(STDIN_FILENO);
	dup2(fd,STDIN_FILENO);
	close(fd);
	return 0;
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

int parse_args(struct Command *commands, const char* command){ //special characters (< > & |) can have not spaces around them, so need to check for those specifically
	char *buffer = malloc(COMMANDLINE_MAX);
	int buffer_index = 0;
	int new_args_index = 0;
	int struct_index = 0;

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
				if(strlen(filename) == 0) //no file
				{
					return NO_INPUT_FILE;
				}
				fprintf(stdout, "%s\n", filename);
				commands[struct_index].input_file = malloc(strlen(filename));
				memcpy(commands[struct_index].input_file, filename, strlen(filename));
			}
			if(special_command == '>'){
				char* filename = malloc(COMMANDLINE_MAX);
				i = get_filename(filename, command, i);
				if(strlen(filename) == 0) //no file
				{
					return NO_OUTPUT_FILE;
				}
				fprintf(stdout, "%s\n", filename);
				commands[struct_index].output_file = malloc(strlen(filename));
				memcpy(commands[struct_index].output_file, filename, strlen(filename));
			}
			if (new_args_index == 0){
				return INV_CMDLINE; //corresponds to invalid command line
			}
			else
			{
				commands[struct_index].args[new_args_index] = malloc(buffer_index);
				memcpy(commands[struct_index].args[new_args_index], buffer, buffer_index); //copy over buffer first
				new_args_index ++;

				char arg[1] = {command[i]}; //Adds special char to args, need to call function
				commands[struct_index].args[new_args_index] = arg;
				memset(buffer, 0, buffer_index);
				new_args_index ++;
				buffer_index = 0;
			}
		}
		else if(command[i] != ' '){
			buffer[buffer_index] = command[i];
			buffer_index++;
		}
		else{ //whitespace
			while(command[i] == ' '){ //eat up all whitespace
				i++;
			}
			i--; //go back one
			//fprintf(stdout, "Buffer: %s", buffer);
			commands[struct_index].args[new_args_index] = malloc(buffer_index);
			memcpy(commands[struct_index].args[new_args_index], buffer, buffer_index);
			memset(buffer, 0, buffer_index);
			new_args_index ++;
			buffer_index = 0;
		}
	}
	if(buffer_index > 0) //leftover in buffer
	{
		commands[struct_index].args[new_args_index] = malloc(buffer_index);
		memcpy(commands[struct_index].args[new_args_index], buffer, buffer_index);
		new_args_index ++;
	}
	commands[struct_index].args[new_args_index] = NULL;
	return NO_ERR;
}


int main(int argc, char *argv[])
{
	size_t cmdSize = COMMANDLINE_MAX;
	char *cmd = malloc(cmdSize);
	int status;
	int pid;
	struct Command *commands = malloc(sizeof(struct Command));
	commands[0].args = malloc(17*sizeof(char*));
	while(1)
	{
		fprintf(stdout, "sshell$ ");
		int length = getline(&cmd, &cmdSize, stdin);
		cmd[length-1] = '\0';
		status = parse_args(commands, cmd);
		if(status == -1)
		{
			fprintf(stderr, "Error: invalid command line\n");
			continue;
		}


		pid = fork();
		if(pid == 0) //execute command as child
		{
			if(strlen(commands[0].input_file)) //input redirection
			{
				int stat = redirect_input(commands[0].input_file);
				if(stat == NO_INPUT_FILE)
				{
					fprintf(stderr, "Error: cannot open input file\n");
					continue;
				}
			}
			if(strcmp(commands[0].args[0], "exit") == 0){
				exit(0);
			}
			else if (strcmp(commands[0].args[0], "pwd") == 0){
				exit(0);
			}
			else if(strcmp(commands[0].args[0], "cd") == 0){
				exit(0);
			}
			status = execvp(commands[0].args[0], commands[0].args);
			exit(1);
		}
		else if(pid > 0)
		{
			waitpid(-1, &status, 0);
			if(strcmp(commands[0].args[0], "exit") == 0){
				our_exit();
			}
			else if (strcmp(commands[0].args[0], "pwd") == 0){
				our_pwd();
				fprintf(stderr, "+ completed '%s': [%d]\n", cmd , 0);
			}
			else if(strcmp(commands[0].args[0], "cd") == 0){
				our_cd(commands[0].args[1], &status);
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
