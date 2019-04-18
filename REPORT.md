---
title: "Shell"
author: "Rohit and Justine"
date: "4/18/2019"
---

```{r setup, include=FALSE}
knitr::opts_chunk$set(echo = TRUE)
```

##Data Structures Used:
Structs:
	Each of the two structs were linked lists because we want to
	manage memory better.
	Since we don't know the sizes before hand, we did not want to use an
	array and hard code the sizes or use realloc() repeatedly.
	We used a Command struct to store data on each command for running purposes.
	We created a backgroundCommand struct to store data on the commands for
	completion and output purposes.

Linked lists:
	Instead of using formal queues for piping and holding background commands,
	we used linked lists as a way to manage memory better.

##Code design overview
From main, the command line input is parsed and a command object is created.
We also ensure to check for invalid syntax and exclude whitespace while parsing.

Right away, if there is input or output redirection, a separate function reads
in the file name and we set the input or output filename of the command object.
In order to do this, we needed to keep track of where we were in the parsing
and where we needed to return to after extracting the filename. We did this
just by passing by reference the index in the command.

If there is piping we malloc more space for the next command in the pipe.

If the special character is &, we ensure the syntax is valid

Otherwise we store the rest of the command pieces character by character
(in a buffer), and when we run into special characters or white space,
we add the entirety of the buffer to the list of arguments.

After parsing the command line, we check if we are piping or not.
Piping is handled differently than all other commands, because we need to
create and manage the pipes.

For piping we followed the professors example code in class. The first and last
command are special due to potentially dealing with input or output redirection.
Otherwise, we change the file descriptors, close all the pipes created and
execute the command, storing each of the exit statuses in an array.

If no piping needs to occur, the child process handles basic input/output
redirection (adapted from lecture examples) and executes most commands.

There are specific commands the parent process executes, basic ones such as pwd,
exit and cd. We implemented these in small functions that make basic libc calls.
Other than those commands, the parent just waits for the child to complete and
prints out the completion message + error codes.

##Error handling
Error checking typically occurs at the beginning of a function and we used an
enum with a dedicated error function to print out error messages. We followed
the examples presented in discussion. Some specific execution errors are
handled separately.


##TESTING
To test our sshell, we used the limited testing script and the examples from the 
prompt. We also used the compared our outputs to the reference shell.

##Sources
We documented our sources for each function, using comments directly in the 
code. 
The sources were: lecture/discussion slides, piazza, and documentation linked in
the prompt. 