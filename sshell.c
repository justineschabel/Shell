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
	if(fd == -1)
	{
		return CANT_OPN_INPUT_FILE;
	}
	dup2(fd,STDIN_FILENO);
	close(fd);
	return 0;
}

/**
purpose:
	use file output for command
inputs:
	@filename: command line file name to be used as output
**/
int redirect_output(char* filename){
	int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
	if(fd == -1)
	{
		return CANT_OPN_OUTPUT_FILE;
	}
	dup2(fd,STDOUT_FILENO);
	close(fd);
	return 0;
}


int isSpecialChar(char toCheck)
{
	return toCheck == '&' || toCheck == '|' || toCheck == '>' || toCheck == '<';
}


int get_filename(char* filename, const char* command, int starting_index){
	int filename_index = 0;
	int command_index = starting_index;
	while(command[command_index] != ' ' && command[command_index] != '\0' && isSpecialChar(command[command_index]) == 0) //last condition
	{
		filename[filename_index] = command[command_index];
		filename_index++;
		command_index++;
	}
	return command_index;
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


int parse_args(struct Command *commands, const char* command){ //special characters (< > & |) can have not spaces around them, so need to check for those specifically
	char *buffer = malloc(COMMANDLINE_MAX);
	int buffer_index = 0;
	int new_args_index = 0;
	int struct_index = 0;

	for(int i= 0; i < strlen(command); i++){
		if(isSpecialChar(command[i]))
		{
			if (new_args_index == 0 && struct_index == 0){
				return INV_CMDLINE;
			}
			if(buffer_index > 0)
			{
				commands[struct_index].args[new_args_index] = malloc(buffer_index);
				memcpy(commands[struct_index].args[new_args_index], buffer, buffer_index); //copy over buffer first
				memset(buffer, 0, buffer_index);
				new_args_index ++;
				buffer_index = 0;
			}
			char special_command = command[i];
			i++;
			while(command[i] == ' '){ //eat up all whitespace
				i++;
			}
			if(special_command == '<'){
				char* filename = malloc(COMMANDLINE_MAX);
				i = get_filename(filename, command, i);
				i--; //go back one
				if(strlen(filename) == 0) //no file
				{
					return NO_INPUT_FILE;
				}
				commands[struct_index].input_file = malloc(strlen(filename));
				memcpy(commands[struct_index].input_file, filename, strlen(filename));
			}
			if(special_command == '>'){
				char* filename = malloc(COMMANDLINE_MAX);
				i = get_filename(filename, command, i);
				i--; //go back one
				if(strlen(filename) == 0) //no file
				{
					return NO_OUTPUT_FILE;
				}
				commands[struct_index].output_file = malloc(strlen(filename));
				memcpy(commands[struct_index].output_file, filename, strlen(filename));
			}
			if(special_command == '|') //copy buffer into last part of command, setup new command
			{
				commands[struct_index].args[new_args_index] = NULL;
				new_args_index = 0;

				struct_index++;
				commands[struct_index].args = malloc(17 * sizeof(char*));
				commands[struct_index].input_file = "";
				commands[struct_index].output_file = "";
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
			i--; //go back one since for loop increments
			if(buffer_index > 0)
			{
				commands[struct_index].args[new_args_index] = malloc(buffer_index);
				memcpy(commands[struct_index].args[new_args_index], buffer, buffer_index);
				memset(buffer, 0, buffer_index);
				new_args_index ++;
				buffer_index = 0;
			}
		}
	}
	if(buffer_index > 0) //leftover in buffer
	{
		commands[struct_index].args[new_args_index] = malloc(buffer_index);
		memcpy(commands[struct_index].args[new_args_index], buffer, buffer_index);
	}
	new_args_index ++;
	commands[struct_index].args[new_args_index] = NULL;
	if(struct_index > 0) //piping
	{
		for(int i = 0; i <= struct_index; i++)
		{
			if(i > 0 && strlen(commands[i].input_file) > 0)
			{
				return MISLOC_INPUT_REDIR;
			}
			if(i < struct_index && strlen(commands[i].output_file) > 0)
			{
				return MISLOC_OUTPUT_REDIR;
			}
		}
	}
	return NO_ERR;
}

int errorMessage(int err)
{
	int noErr = 1; //assume error
	switch (err) {
		case INV_CMDLINE:
			fprintf(stderr, "Error: invalid command line\n");
			break;
		case NO_INPUT_FILE:
			fprintf(stderr, "Error: no input file\n");
			break;
		case NO_OUTPUT_FILE:
			fprintf(stderr, "Error: no output file\n");
			break;
		case MISLOC_INPUT_REDIR:
			fprintf(stderr, "Error: mislocated input redirection\n");
			break;
		case MISLOC_OUTPUT_REDIR:
			fprintf(stderr, "Error: mislocated output redirection\n");
			break;
		default:
			noErr = 0;
	}
	return noErr;
}


int main(int argc, char *argv[])
{
	size_t cmdSize = COMMANDLINE_MAX;
	char *cmd = malloc(cmdSize);
	int status;
	int pid;
	struct Command *commands = malloc(10 * sizeof(struct Command)); //assume at most 10 pipes? prob need to realloc in parser every time we see pipe
	while(1)
	{
		commands[0].args = malloc(17 * sizeof(char*));
		commands[0].input_file = "";
		commands[0].output_file = "";
		fprintf(stdout, "sshell$ ");
		int length = getline(&cmd, &cmdSize, stdin);
		if (!isatty(STDIN_FILENO)) {
        printf("%s", cmd);
        fflush(stdout);
    }
		cmd[length-1] = '\0';
		status = parse_args(commands, cmd);
		int error = errorMessage(status);
		if(error)
		{
			continue;
		}
		pid = fork();
		if(pid == 0) //execute command as child
		{
			if(strlen(commands[0].input_file) > 0) //input redirection
			{
				int stat = redirect_input(commands[0].input_file);
				if(stat == NO_INPUT_FILE)
				{
					fprintf(stderr, "Error: cannot open input file\n");
					continue;
				}
			}
			if(strlen(commands[0].output_file) > 0)
			{
				int stat = redirect_output(commands[0].output_file);
				if(stat == NO_OUTPUT_FILE)
				{
					fprintf(stderr, "Error: cannot open output file\n");
					continue;
				}
			}
			if(strcmp(commands[0].args[0], "exit") == 0){ //don't want child to execute these commands
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
				fprintf(stderr, "+ completed '%s' [%d]\n", cmd , 0);
			}
			else if(strcmp(commands[0].args[0], "cd") == 0){
				our_cd(commands[0].args[1], &status);
				fprintf(stderr, "+ completed '%s' [%d]\n", cmd , status);
			}
			else
			{
				fprintf(stderr, "+ completed '%s' [%d]\n", cmd , WEXITSTATUS(status));
			}
		}
		else
		{
			fprintf(stderr, "u gofed\n");
		}
	}

	return EXIT_SUCCESS;
}
