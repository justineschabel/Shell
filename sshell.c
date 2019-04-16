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

struct backgroundCommand {
	char* cmd;
	int pid;
	int runInBack;
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

int checkProcessCompletion(struct backgroundCommand *backgroundCommands, int num_processes, char* cmd, int do_background, int status)
{
	if(backgroundCommands[num_processes - 1].pid != -1)
	{
		backgroundCommands[num_processes - 1].cmd = malloc(strlen(cmd));
		memcpy(backgroundCommands[num_processes - 1].cmd, cmd, strlen(cmd));
		if(do_background)
		{
			backgroundCommands[num_processes - 1].runInBack = 1;
			waitpid(backgroundCommands[num_processes - 1].pid, &status, WNOHANG);
		}
		else
		{
			backgroundCommands[num_processes - 1].runInBack = 0;
			waitpid(backgroundCommands[num_processes - 1].pid, &status, 0);
		}
	}
	else
	{
		num_processes--;
	}

	int initialProcesses = num_processes;
	int ret;
	for(int i = 0; i < initialProcesses; i++)
	{
		if(backgroundCommands[i].runInBack == 1)
		{
			ret = waitpid(backgroundCommands[i].pid, &status, WNOHANG);
		}
		else
		{
			ret = waitpid(backgroundCommands[i].pid, &status, 0);
		}
		if(ret != 0)
		{
			fprintf(stderr, "+ completed '%s' [%d]\n", backgroundCommands[i].cmd , WEXITSTATUS(status));
			backgroundCommands[i].cmd = "";
			backgroundCommands[i].pid = 0;
			num_processes--;
		}
	}
	return num_processes;
}

void execute_pipe(struct Command *commands, int num_commands, int* err_codes){
	int *fd = malloc(2 * (num_commands - 1) * sizeof(int));
	int stat = 0;
	for(int i = 0; i < num_commands - 1; i++) //create all pipes first, one less pipe than number of commands
	{
		pipe(fd + i * 2);
	}


	for(int i = 0; i < num_commands; i++)
	{
		int pid = fork();
		if (pid == 0)
		{
			if(i != 0) //Not first command, redirect output
			{
				dup2(fd[2 * (i - 1)], STDIN_FILENO);
			}
			else //First Command might redirect input
			{
				if(strlen(commands[i].input_file) > 0) //input redirection
				{
					stat = redirect_input(commands[i].input_file);
					if(stat == NO_INPUT_FILE)
					{
						fprintf(stderr, "Error: cannot open input file\n");
						return;
					}
				}
			}

			if(i == num_commands - 1)
			{ //Last command might redirect output
				if(strlen(commands[i].output_file) > 0)
				{
					stat = redirect_output(commands[i].output_file);
					if(stat == NO_OUTPUT_FILE)
					{
						fprintf(stderr, "Error: cannot open output file\n");
						return;
					}
				}
			}
			else
			{
				dup2(fd[1 + 2 * i], STDOUT_FILENO);
			}
			for(int i = 0; i < (num_commands - 1) * 2; i++){
				close(fd[i]);
			}
			stat = execvp(commands[i].args[0], commands[i].args);
		}
	} //for
	for(int i = 0; i < (num_commands - 1) * 2; i++){
		close(fd[i]);
	}

	for(int i = 0; i < num_commands; i++) //wait for children to execute and collect error codes
	{
		waitpid(-1, &stat, 0);
		err_codes[i] = WEXITSTATUS(stat);
	}
} //function


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


int parse_args(struct Command *commands, const char* command, int* num_commands, int* do_piping, int* background){ //special characters (< > & |) can have not spaces around them, so need to check for those specifically
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

			if(special_command == '<'){
				i++;
				while(command[i] == ' '){ //eat up all whitespace
					i++;
				}
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
				i++;
				while(command[i] == ' '){ //eat up all whitespace
					i++;
				}
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
				*do_piping = 1;
				commands[struct_index].args[new_args_index] = NULL;
				new_args_index = 0;

				struct_index++;
				commands[struct_index].args = malloc(17 * sizeof(char*));
				commands[struct_index].input_file = "";
				commands[struct_index].output_file = "";
			}
			if(special_command == '&')
			{
				i++;
				while(command[i] == ' '){ //eat up all whitespace
					i++;
				}
				if(i < strlen(command))
				{
					return MISLOC_AMP;
				}
				*background = 1;
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
	*num_commands = struct_index+1;
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
		case MISLOC_AMP:
		fprintf(stderr, "Error: mislocated background sign\n");
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
	struct backgroundCommand *backgroundCommands = malloc(10 * sizeof(struct backgroundCommand)); //prob need to realloc when new process
	int num_commands;
	int num_processes = 0;
	int do_piping = 0;
	while(1)
	{
		commands[0].args = malloc(17 * sizeof(char*));
		commands[0].input_file = "";
		commands[0].output_file = "";
		fprintf(stdout, "sshell$ ");
		int length = getline(&cmd, &cmdSize, stdin);
		if (!isatty(STDIN_FILENO))
		{
			printf("%s", cmd);
			fflush(stdout);
		}
		num_processes++;
		if(length > 1)
		{
			cmd[length-1] = '\0';
		}
		else
		{
			backgroundCommands[num_processes - 1].pid = -1;
			num_processes = checkProcessCompletion(backgroundCommands, num_processes, cmd, 0, status);
			continue;
		}
		do_piping = 0;
		int do_background = 0;
		status = parse_args(commands, cmd, &num_commands, &do_piping, &do_background);
		int error = errorMessage(status);
		if(error)
		{
			num_processes--;
			continue;
		}
		if(do_piping)
		{
			int *err_codes = malloc(num_commands*sizeof(int));
			execute_pipe(commands, num_commands, err_codes);
			backgroundCommands[num_processes - 1].pid = 0;
			num_processes = checkProcessCompletion(backgroundCommands, num_processes, cmd, do_background, status);
			fprintf(stderr, "+ completed '%s' ", cmd); //add to checkProcessCompletion to make it work with background pipes
			for(int i = 0; i < num_commands; i++)
			{
				fprintf(stderr, "[%d]", err_codes[i]);
			}
			fprintf(stderr, "\n");
		}
		else
		{
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
			if(strcmp(commands[0].args[0], "exit") == 0){
				if(num_processes > 1)
				{
					fprintf(stderr, "Error: active jobs still running\n");
					fprintf(stderr, "+ completed '%s' [%d]\n", cmd , 1);
					num_processes--;
					continue;
				}
				our_exit();
			}
			else if (strcmp(commands[0].args[0], "pwd") == 0){
				our_pwd();
				fprintf(stderr, "+ completed '%s' [%d]\n", cmd , 0);
				num_processes--;
			}
			else if(strcmp(commands[0].args[0], "cd") == 0){
				our_cd(commands[0].args[1], &status);
				fprintf(stderr, "+ completed '%s' [%d]\n", cmd , status);
				num_processes--;
			}
			else
			{
				backgroundCommands[num_processes - 1].pid = pid;
				num_processes = checkProcessCompletion(backgroundCommands, num_processes, cmd, do_background, status);
			}
		}
		else
		{
			fprintf(stderr, "u gofed\n");
		}
	}

} //while

return EXIT_SUCCESS;
}
