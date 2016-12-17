#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define SERVER_FIFO "cmdfifo"
#define MAX_BUF 100

// Every child exit is caught and redirected to this function to be handled gracefully.
// Will prevent child process from becoming a zombie as waitpid executed for all exited processes at the time.
void sigchldHandler(int status){
    while (waitpid(-1,status,WNOHANG)>0);
    return;
}

int main(void){
    pid_t pid;
    int fd_server, fd_client;
    char client_pid[20],*command[MAX_BUF],clientFIFO[20];
    char read_buffer[MAX_BUF];
    int len;

    //Create server fifo
    if (mkfifo(SERVER_FIFO,0666)==-1){
        printf("SERVER:Error creating server fifo");
        exit(1);
    }
    
    //Open server fifo
    if ((fd_server=open(SERVER_FIFO,O_RDONLY))==-1){
        printf("SERVER:Error creating server fifo file descriptor");
        exit(1);
    }

    // register SIGCHLD handler with parent process.
    // Enables parent to process any child exits and prevents zombies.
    signal(SIGCHLD,sigchldHandler);

    while(1) {
        // set buffers to null values
        memset(read_buffer,'\0',MAX_BUF);

        //Read request from server fifo
        if (read(fd_server,read_buffer,MAX_BUF)==-1){
            printf("SERVER:Error reading from server fifo file descriptor");
            exit(1);
        }

        //Parse client's request
        len = strlen(read_buffer);
        if (len<=1)
            continue;
        int i=0,j=0,k=0,l=0;
        if(read_buffer[i]=='$')
            i++;
        while(read_buffer[i]!='$'){
            client_pid[j++]=read_buffer[i++];
        }
        client_pid[j]='\0';
        if(read_buffer[i]=='$')
            i++;
        char temp[len];
        while(read_buffer[i]!='\0'){
            while(read_buffer[i]!=' ' && read_buffer[i]!='\0'){
                temp[k++]=read_buffer[i++];
            }
            temp[k]='\0';
            command[l] = (char*)malloc(strlen(temp)*sizeof(char));
            strcpy(command[l],temp);
            while(read_buffer[i]==' '){
                i++;
            }
            k=0;l++;
        }
        command[l]=NULL;

        // Fork the child process 
        pid = fork();

        // if child process: 
        if (pid==0) {
           // open child fifo 
           sprintf(clientFIFO,"cfifo%s",client_pid);
           if ((fd_client = open(clientFIFO,O_WRONLY))==-1){
               printf("SERVER:Could not open client fifo in write mode");
               exit(1);
            }
            // redirect output of execlp to client fifo
            if (dup2(fd_client,1)<0){
                printf("SERVER: dup2() errored");
                exit(1);
            }
            if (close(fd_client)==-1){
                printf("Error closing client fd");
                exit(1);
            }
            // execute client's request
            if(execvp(command[0],command)==-1){
                exit(1);
            }
        }
        // if fork() was unsuccessful
        else if (pid==-1) {
            printf("SERVER: Error in fork() call");
        }
        // if parent process
        else {
            continue;
        }
    }

    //Close server FD
    if (close(fd_server)==-1){
        printf("SERVER:Error closing server fifo file descriptor");
        exit(1);
    }
    
    // exit without calling unlink() on server fifo to prevent file from getting deleted.
    return 0;
}
