sshell: sshell.o
	gcc -g -Wall -Werror -o sshell sshell.o 

sshell.o:
	gcc -g -Wall -Werror -c -o sshell.o sshell.c

clean:
	rm -f sshell *.o