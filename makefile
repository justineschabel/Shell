sshell: sshell.o
	gcc -Wall -Werror -o sshell sshell.o 

sshell.o:
	gcc -Wall -Werror -c -o sshell.o sshell.c

clean:
	rm -f sshell *.o