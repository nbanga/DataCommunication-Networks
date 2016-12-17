// simple shell example using fork() and execlp()

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
//declare variables for storing process ID(pid) of child process, buffer for input, 
//status of child process and length of buffer.
pid_t k;
char buf[100];
int status;
int len;

//infinite loop for server to listen to incoming requests
  while(1) {

	// print prompt
  	fprintf(stdout,"[%d]$ ",getpid());

	// read command from stdin
	fgets(buf, 100, stdin);
	len = strlen(buf);
	if(len == 1) 				// only return key pressed
	  continue;
        buf[len-1] = '\0';

        // fork the parent process to spawn a child process.
        // the child shares its memory segment with the parent.
        // Both parent and child continue execution from the next line.
  	k = fork();
        
        // Check return value of fork() call to determine if this is the parent or child.  
  	if (k==0) {
  	// child code
        // Call to execlp to override child process with a new process 
        // that would execute the request from the client. 
        // On successful completion, execlp() will override child process 
        // and the function call will not return to child process .
        // If call fails, -1 is returned.
        // Status of the execlp() function is verified because if the call is unsuccessful
        // no new process overrides the child process and we need to terminate the child process.
          printf("INSIDE CHILD");
    	  if(execlp(buf,buf,NULL) == -1)	// if execution failed, terminate child
	  	exit(1);
  	}
  	else {
  	// parent code 
        // If parent code in process, it waits for the specific child process to complete
        // execution. If execlp() was successful, it will return a status to parent process as well.
        // Else if the call was unsuccessful, child process will exit and return a status 
        // Parent process waits for this status before resuming itself.
	   waitpid(k, &status, 0);
  	}
  }
}
