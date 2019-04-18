#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#define COMMANDLINE_MAX 512

/*
Purpose:
	organize commands/store information
Elements:
	@args: a character array that stores the command and its arguments
	@input_file: if theres input redirection this is set to the file name
	@output_file: if theres output redirection this is set to the file name
Source:
	None
*/
struct Command {
	char** args;
	char* input_file;
	char* output_file;

	struct Command *next;
};

/*
Purpose:
	We used a separate struct for the background command because we need to keep
	an array of the commands running in the background, and the order the
	commands were entered and we want to store more information than we would
	for a normal command (such as pid)
Source:
	None
*/
struct backgroundCommand {
	char* cmd;
	int pid;
	int pipe; //holds number of operations in whole command
	int* err_codes; //for pipe commands
	int errStat; //status for when a command fails initially
	int runInBack;

	struct backgroundCommand *next;
};

/*
Purpose:
	Enumerating errors allows for readability and organization
Source:
	Coding tips from lecture
*/
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
Purpose:
	use file input for command
Inputs:
	@filename: command line file name to be used as input
Source:
	Adapted from lecture slides
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
Purpose:
	send output of command to another file
Inputs:
	@filename: command line file name to be used as output
Source:
	Adapted from lecture slides
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

/*
Purpose:
	Append a background command to the linked list of commmands
Inputs:
	@node: node to add to the linked list
	@head: the beginning of the linked list
Source:
	None
*/
void append(struct backgroundCommand *node, struct backgroundCommand *head)
{
	struct backgroundCommand *temp = head;
	if(head == NULL)
	{
		head = node;
	}
	while(temp->next != NULL)
	{
		temp = temp->next;
	}
	temp->next = node;
	node->next = NULL;
}

/*
Purpose:
	Second function for adding to background job linked list,
	sets the user entered command and if it needs to be run in the background
Inputs:
	@curr: current command
	@cmd: user entered command
	@do_background: 1 if there is &
Source:
	None
*/
void setCurrentCommand(struct backgroundCommand *curr, char* cmd, int do_background)
{
	curr->cmd = malloc(strlen(cmd));
	memcpy(curr->cmd, cmd, strlen(cmd));
	if(do_background)
	{
		curr->runInBack = 1;
	}
	else
	{
		curr->runInBack = 0;
	}
	curr->next = NULL;
}

/*
Purpose:
	Print out command after completion
Inputs:
	@command: current completed command to print
	@status: exit status of command
Source:
	None
*/
void printCommandCompletion(struct backgroundCommand *command, char* cmd, int status)
{
	if(command->pipe)
	{
		//add to checkProcessCompletion to make it work with background pipes
		fprintf(stderr, "+ completed '%s' ", cmd);
		for(int i = 0; i < command->pipe; i++)
		{
			fprintf(stderr, "[%d]", command->err_codes[i]);
		}
		fprintf(stderr, "\n");
	}
	else
	{
		if(command->errStat) //needed to get proper error code from process
		{
			fprintf(stderr, "+ completed '%s' [%d]\n", command->cmd, command->errStat);
		}
		else
		{
			fprintf(stderr, "+ completed '%s' [%d]\n", command->cmd, WEXITSTATUS(status));
		}
	}
}

/*
Purpose:
	If a current process or a process in the background completes,
	we need to rearrange the linked	list
Inputs:
	@background_commmands: array of commands that need to be ran/are running
	@num_processes: length of background_commands array
	@cmd: the user entered command
	@do_background: if last executed command was to be done in the background
Source:
	None
*/
int checkProcessCompletion(struct backgroundCommand *backgroundCommands, int num_processes, char* cmd, int do_background)
{
	int ret;
	int status = 0;
	struct backgroundCommand *temp = backgroundCommands;
	struct backgroundCommand *prev = NULL;
	while(temp != NULL)
	{
		if(temp->cmd == NULL) //empty command, logically remove from linked list
		{
			prev = temp;
			temp = temp->next;
			continue;
		}
		if(temp->runInBack == 1)
		{
			ret = waitpid(temp->pid, &status, WNOHANG);
		}
		else
		{
			ret = waitpid(temp->pid, &status, 0);
		}
		if(ret != 0) // ret is nonzero if a command is finished
		{
			printCommandCompletion(temp, cmd, status);
			if(temp->next == NULL) //last command in linked list
			{
				if(prev != NULL) //not first command in linked list
				{
						prev->next = NULL;
				}
			}
			else
			{
				if(prev != NULL) //neither first nor last command
				{
					prev->next = temp->next;
				}
			}
			free(temp);
			num_processes--;
		}
		prev = temp;
		temp = temp->next;
	}
	return num_processes;
}

/*
Purpose:
	if there are any pipes, this function handles them one at a time
Inputs:
	@commands: array of commands, each one separated by a pipe
	@num_commands: length of commands array
	@error_codes: keeps track of each commands error code (so main can print
	them out after all are done)
Source:
	Piazza Post 121 + in comments Professor mentioned you could essentially
	do a loop through all the commands
*/
void execute_pipe(struct Command *commands, int num_commands, int* err_codes)
{
	int *fd = malloc(2 * (num_commands - 1) * sizeof(int));
	int stat = 0;
	struct Command *head = commands;

	//create all pipes first, one less pipe than number of commands
	for(int i = 0; i < num_commands - 1; i++)
	{
		pipe(fd + i * 2);
	}

	//loop through all commands, connect them by channeling their I/O, and execute
	for(int i = 0; i < num_commands; i++)
	{
		int pid = fork();
		if (pid == 0)
		{
			//Not the first command, redirect input
			if(i != 0)
			{
				dup2(fd[2 * (i - 1)], STDIN_FILENO);
			}
			else
			{
				//First Command might redirect input
				if(strlen(commands->input_file) > 0)
				{
					stat = redirect_input(commands->input_file);
					if(stat == NO_INPUT_FILE)
					{
						fprintf(stderr, "Error: cannot open input file\n");
						return;
					}
				}
			}

			if(i == num_commands - 1)
			{
				//Last command might redirect output
				if(strlen(commands->output_file) > 0)
				{
					stat = redirect_output(commands->output_file);
					if(stat == NO_OUTPUT_FILE)
					{
						fprintf(stderr, "Error: cannot open output file\n");
						return;
					}
				}
			}

			//if its not the last command, redirect output
			else
			{
				dup2(fd[1 + 2 * i], STDOUT_FILENO);
			}

			//close all file descriptors
			for(int i = 0; i < (num_commands - 1) * 2; i++){
				close(fd[i]);
			}

			//execute the command: creates entirely new process and should never return
			stat = execvp(commands->args[0], commands->args);
			if(stat == -1)
			{
				fprintf(stderr, "Error: command not found\n");
				exit(1);
			}
		}
		commands = commands->next;
	}

	//close all file descriptors
	for(int i = 0; i < (num_commands - 1) * 2; i++){
		close(fd[i]);
	}

	//wait for children to execute and collect error codes
	for(int i = 0; i < num_commands; i++)
	{
		waitpid(-1, &stat, 0);
		err_codes[i] = WEXITSTATUS(stat);
	}
	commands = head;
}

/*
Purpose:
	if theres a valid special character we need to call specialized functions
Inputs:
	@toCheck: char to check
Outputs:
	1 if special character, 0 otherwise
Source:
	None
*/
int isSpecialChar(char toCheck)
{
	return toCheck == '&' || toCheck == '|' || toCheck == '>' || toCheck == '<';
}

/*
Purpose:
	after reading < or > we need to parse the filename starting from where we
	left off in the command line
Inputs:
	@filename: char* where filename is stored, so
	we can access the filename when we return
	@command: We need to parse the file name out of the original command line
	@starting index: We need to start from right after the < or >
Outputs:
	@command_index: We need to finish parsing from the index immediately
	after the filename
Source:
	None
*/
int get_filename(char* filename, const char* command, int starting_index)
{
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


void our_exit()
{
	fprintf(stderr, "Bye...\n");
	exit(0);
}


void our_pwd()
{
	fprintf(stdout, "%s\n" , getcwd(NULL, 0));
}

/*
Purpose:
	execute cd command
Inputs:
	@path: path entered in command line
	@status: blank pointer, used to return exit status of cd
Source:
	None (provided documentaton from the prompt)
*/
void our_cd(char* path, int* status)
{
	//special case, where the path wasnt explicitly given
	if(strcmp(path, "..") == 0)
	{
		char* current_path = getcwd(NULL, 0);
		char* lastSlash = strrchr(current_path, '/');
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
}

/*
Purpose:
	Parse the command line
Inputs:
	@commands: empty array of command objects that the main function can access
	after parsing
	@command: first command
	@num_commands: return number of commands when piping
	@do_piping: Let main know there is piping so it can be handled properly
	@background: Let main know this command needs to be a background process
Source:
	None
*/
int parse_args(struct Command *commands, const char* command, int* num_commands, int* do_piping, int* background)
{
	char *buffer = malloc(COMMANDLINE_MAX);
	int buffer_index = 0;
	int new_args_index = 0;
	int struct_index = 0;
	struct Command *head = commands;

	for(int i= 0; i < strlen(command); i++){
		if(isSpecialChar(command[i]))
		{
			if (new_args_index == 0 && struct_index == 0){
				return INV_CMDLINE;
			}
			if(buffer_index > 0)
			{
				commands->args[new_args_index] = malloc(buffer_index);
				memcpy(commands->args[new_args_index], buffer, buffer_index); //copy over buffer first
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
				commands->input_file = malloc(strlen(filename));
				memcpy(commands->input_file, filename, strlen(filename));
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
				commands->output_file = malloc(strlen(filename));
				memcpy(commands->output_file, filename, strlen(filename));
			}
			if(special_command == '|') //copy buffer into last part of command, setup new command
			{
				*do_piping = 1;
				commands->args[new_args_index] = NULL;
				new_args_index = 0;

				struct_index++;
				struct Command *next_command = malloc(sizeof(struct Command));
				commands->next = next_command;
				commands = commands->next;
				commands->args = malloc(17 * sizeof(char*));
				commands->input_file = "";
				commands->output_file = "";
				commands->next = NULL;
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
		} //end special character check

		// otherwise add character to buffer
		else if(command[i] != ' '){
			buffer[buffer_index] = command[i];
			buffer_index++;
		}

		//eat up whitespace
		else{
			while(command[i] == ' '){
				i++;
			}
			//go back one since for loop increments
			i--;

			//Left over characters in the buffer that are no longer needed
			if(buffer_index > 0)
			{
				commands->args[new_args_index] = malloc(buffer_index);
				memcpy(commands->args[new_args_index], buffer, buffer_index);
				memset(buffer, 0, buffer_index);
				new_args_index ++;
				buffer_index = 0;
			}
		}
	} //end for loop through the command line input

	//if the buffer has anything in it, there was a command, add the commands
	if(buffer_index > 0)
	{
		commands->args[new_args_index] = malloc(buffer_index);
		memcpy(commands->args[new_args_index], buffer, buffer_index);
	}

	new_args_index ++;
	commands->args[new_args_index] = NULL;
	commands = head;
	//a scruct_index > 0 means theres more than one command i.e. piping. Check for syntax errors.
	if(struct_index > 0)
	{
		int i = 0;
		while(head != NULL)
		{
			if(i > 0 && strlen(head->input_file) > 0)
			{
				return MISLOC_INPUT_REDIR;
			}
			if(i < struct_index && strlen(head->output_file) > 0)
			{
				return MISLOC_OUTPUT_REDIR;
			}
			head = head->next;
			i++;
		}
	}
	*num_commands = struct_index + 1;
	return NO_ERR;
}

/*
Purpose:
	Handle all error output
Input:
	@err: Error Number (calculated using enum Errors)
Source:
	adapted from slides
*/
int errorMessage(int err)
{
	//assume error
	int noErr = 1;
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
	//assume at most 10 pipes? prob need to realloc in parser every time we see pipe CHANGED TO LINKED LIST
	struct Command *commands = malloc(sizeof(struct Command));
	//probably need to realloc when new process
	struct backgroundCommand *backgroundCommands = malloc(sizeof(struct backgroundCommand));
	int num_commands;
	int num_processes = 0;
	int do_background = 0;

	int do_piping = 0;

	//Until the user exits, continue accepting input and throw errors if its invalid
	while(1)
	{
		commands->args = malloc(17 * sizeof(char*));
		commands->input_file = "";
		commands->output_file = "";
		commands->next = NULL;
		fprintf(stdout, "sshell$ ");
		int length = getline(&cmd, &cmdSize, stdin);
		//reference from tester
		if (!isatty(STDIN_FILENO))
		{
			printf("%s", cmd);
			fflush(stdout);
		}
		if(length > 1)
		{
			cmd[length-1] = '\0';
		}
		else
		{
			num_processes = checkProcessCompletion(backgroundCommands, num_processes, cmd, 0);
			continue;
		}
		do_piping = 0;
		do_background = 0;
		status = parse_args(commands, cmd, &num_commands, &do_piping, &do_background);
		int error = errorMessage(status);
		if(error)
		{
			continue;
		}
		num_processes++;
		if(do_piping)
		{
			int *err_codes = malloc(num_commands*sizeof(int));
			execute_pipe(commands, num_commands, err_codes);
			struct backgroundCommand *nextNode = malloc(sizeof(struct backgroundCommand));
			nextNode->pid = 0;
			nextNode->pipe = num_commands;
			nextNode->err_codes = malloc(num_commands * sizeof(int));
			for(int i = 0; i < num_commands; i++)
			{
				nextNode->err_codes[i] = err_codes[i];
			}
			append(nextNode, backgroundCommands);
			setCurrentCommand(nextNode, cmd, do_background);
			num_processes = checkProcessCompletion(backgroundCommands, num_processes, cmd, do_background);
		}
		else
		{
			pid = fork();
			if(pid == 0) //execute command as child
			{
				if(strlen(commands->input_file) > 0) //input redirection
				{
					int stat = redirect_input(commands->input_file);
					if(stat == CANT_OPN_INPUT_FILE)
					{
						fprintf(stderr, "Error: cannot open input file\n");
						continue;
					}
				}
				if(strlen(commands->output_file) > 0)
				{
					int stat = redirect_output(commands->output_file);
					if(stat == CANT_OPN_OUTPUT_FILE)
					{
						fprintf(stderr, "Error: cannot open output file\n");
						continue;
					}
				}
				if(strcmp(commands->args[0], "exit") == 0)
				{ //don't want child to execute these commands
					exit(0);
				}
				else if (strcmp(commands->args[0], "pwd") == 0)
				{
					exit(0);
				}
				else if(strcmp(commands->args[0], "cd") == 0){
					exit(0);
				}
				status = execvp(commands->args[0], commands->args);
				if(status == -1)
				{
					fprintf(stderr, "Error: command not found\n");
				}
				exit(1);
			}
			else if(pid > 0)
			{
				if(strcmp(commands->args[0], "exit") == 0)
				{
					if(num_processes > 1)
					{
						fprintf(stderr, "Error: active jobs still running\n");
						fprintf(stderr, "+ completed '%s' [%d]\n", cmd , 1);
						num_processes--;
						continue;
					}
					our_exit();

				}
				else if (strcmp(commands->args[0], "pwd") == 0)
				{
					our_pwd();
					fprintf(stderr, "+ completed '%s' [%d]\n", cmd , 0);
					num_processes--;
				}
				else if(strcmp(commands->args[0], "cd") == 0)
				{
					our_cd(commands->args[1], &status);
					fprintf(stderr, "+ completed '%s' [%d]\n", cmd , status);
					num_processes--;
				}
				else
				{
					if(!do_background)
					{
						waitpid(pid, &status, 0);
					}
					struct backgroundCommand *nextNode = malloc(sizeof(struct backgroundCommand));
					nextNode->pid = pid;
					nextNode->pipe = 0;
					if(WEXITSTATUS(status) > 0)
					{
						nextNode->errStat = WEXITSTATUS(status);
					}
					append(nextNode, backgroundCommands);
					setCurrentCommand(nextNode, cmd, do_background);
					num_processes = checkProcessCompletion(backgroundCommands, num_processes, cmd, do_background);
				}
			}
		}
	} //while
	return EXIT_SUCCESS;
}
