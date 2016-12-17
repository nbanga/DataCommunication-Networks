#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>

#define SERVER_FIFO "cmdfifo"
#define MAX_BUF 100
#define MAX_LEN 1500
#define COMMAND "ls -l -a"

int main(void){
    pid_t pid;
    int fd_server, fd_client;
    char clientFIFO[20];
    char read_buffer[MAX_LEN], write_buffer[MAX_BUF];
    int len;

    // getpid()
    pid = getpid();
    // set write_buffer to null values
    memset(write_buffer,'\0',MAX_BUF);
    memset(read_buffer,'\0',MAX_LEN);

    // create client fifo
    sprintf(clientFIFO,"cfifo%ld",(long)pid);
    if (mkfifo(clientFIFO,0666)==-1){
        printf("CLIENT:Error creating client FIFO for %ld", (long)pid);
        exit(1);
    }

    // Create client request
    snprintf(write_buffer, MAX_BUF-1, "%s%ld%s%s", "$",(long)pid,"$",COMMAND);

    //Write request to server's fifo
    if ((fd_server = open(SERVER_FIFO,O_WRONLY))==-1){
       printf("CLIENT:Could not open server fifo in write mode ");
       exit(1);
    }
    if (write(fd_server,write_buffer,strlen(write_buffer))==-1){
        printf("CLIENT:Error while writing to server fifo");
        exit(1);
    }
    if (close(fd_server)==-1){
        printf("CLIENT:Error closing server fifo");
        exit(1);
    
    }

    // Read response from client's fifo
    if ((fd_client = open(clientFIFO,O_RDONLY))==-1){
       printf("CLIENT:Could not open client fifo in read mode");
       exit(1);
    }
    if (read(fd_client,read_buffer,MAX_LEN)==-1){
        printf("CLIENT:Error reading into cfifo%ld fifo file", (long)pid);
        exit(1);
    }
    else {
        printf("Response to request of client %ld:\n%s",(long)pid, read_buffer);
    }
    if(close(fd_client)==-1){
        printf("CLIENT:Error closing client file descriptor");
        exit(1);
    }
    
    // exit without unlinking cfifo to prevent file from deletion.
    return 0;
}
