#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define MAX_BUF 1024 
#define DIRECTORY "./filedeposit/"

// Every child exit is caught and redirected to this function to be handled gracefully.
// Will prevent child process from becoming a zombie as waitpid executed for all exited processes at the time.
void sigchldHandler(int status){
    while (waitpid(-1,status,WNOHANG)>0);
    return;
}

// function to check if secretkey of client is a valid key or not
int isValidSecretkey(char* secretkeyServer, char* secretkeyClient){
    // checks if the server and client keys are same
    // checks for the length of the keys
    if (strcmp(secretkeyServer, secretkeyClient)!=0 || strlen(secretkeyClient)<10 || strlen(secretkeyClient)>20){
       return 0;
    }
    
    // checks if the key is an alpha-numeric string
    char* temp = secretkeyClient;
    while(*temp!='\0'){
        if ((*temp>='a' && *temp<='z') || (*temp>='A' && *temp<='Z') || (*temp>='0' && *temp<='9'))
            temp++;
        else {
            return 0;
        }
    }
    return 1;            
}

// main function
// tranfers a requested file to client
int main(int argc, char** argv){
    pid_t pid;
    char secretkeyClient[50],portNum[10],block_size[MAX_BUF];
    int fd_config,fd;
    int block_len;
    int sd_server, sd_client;
    char filename[MAX_BUF], filepath[MAX_BUF];
    char read_buffer[MAX_BUF];
    int len,bytes;

    // check if arguments are proper
    if (argc<4) {
        printf("Format is 'fileserver portnumber secretkey configfile'\n");
        exit(1);
    }

    memset(block_size,'\0',MAX_BUF);
    // get number of bytes to be read from config file
    // open config file 
    if ((fd_config=open(argv[3],O_RDONLY))==-1){
        printf("Error opening configfile %s %s\n", argv[3], strerror(errno));
        exit(1);
    }
    // read the data from config file one byte at a time
    int q = 0;
    while((bytes=read(fd_config,&(block_size[q++]),1))>0);
    if (bytes==-1){
        printf("Error reading config file %s %s\n", argv[3], strerror(errno));
        exit(1);
    }
    // close the config file
    if(close(fd_config)==-1){
        printf("Error reading config file %s %s\n", argv[3], strerror(errno));
        exit(1);
    }
    
    // convert the value read from config file to an int
    int p;
    for(p=0;block_size[p+1]!='\0';p++){
        block_len = block_len*10 + (block_size[p] - '0');
    }

    // declare a write_buffer of appropriate length
    // used to write data to client file
    char write_buffer[block_len+1];

    // initilaize server address
    struct sockaddr_in server_addr, client_addr;
    bzero((char*) &server_addr, sizeof(server_addr));
    bzero((char*) &client_addr, sizeof(client_addr));
    int addr_len = sizeof(client_addr);

    //Create server socket
    if ((sd_server = socket(AF_INET,SOCK_STREAM,0))==-1){
        printf("SERVER:Error creating server socket\n");
        exit(1);
    }
    
    // initialize server_address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = inet_addr("128.10.2.13"); 

    // bind socket to server address
    if (bind(sd_server, (struct sockaddr *) &server_addr,sizeof(server_addr)) ==-1){
        printf("SERVER:Error binding to server address %s\n", strerror(errno));
        exit(1);
    }

    // listen on server side socket
    if (listen(sd_server, 5)==-1){
        printf("Error in listen call\n");
        exit(1);
    }

    // register SIGCHLD handler with parent process.
    // Enables parent to process any child exits and prevents zombies.
    signal(SIGCHLD,sigchldHandler);

    while(1) {
        // set buffers to null values
        memset(read_buffer,'\0',MAX_BUF);
        memset(filename,'\0',MAX_BUF);
        memset(filepath,'\0',MAX_BUF);
        memset(secretkeyClient,'\0',50);
        memset(write_buffer,'\0',block_len+1);

        // accept client request and forms full associations
        if ((sd_client=accept(sd_server, (struct sockaddr *) &client_addr, &addr_len))==-1){
            printf("Failed to accept connection\n");
            exit(1);
        }

        //Read request from client socket
        if (read(sd_client,read_buffer,MAX_BUF)==-1){
            printf("SERVER:Error reading from server fifo file descriptor\n");
            exit(1);
        }

        //Parse client's request
        len = strlen(read_buffer);
        if (len<=1)
            continue;
        int i=0,j=0,k=0;
        if(read_buffer[i]=='$')
            i++;
        // get secret key from client request
        while(read_buffer[i]!='$'){
            secretkeyClient[j++]=read_buffer[i++];
        }
        if(read_buffer[i]=='$')
            i++;
        // get filename from client request
        while(read_buffer[i]!='\0'){
            filename[k++]=read_buffer[i++];
        }

        // verify constraints on secret key received from client
        if (!isValidSecretkey(argv[2],secretkeyClient)){
            //continue;
        }

        // construct file path for transfering
        // ./fledeposit/<filename>
        snprintf(filepath, MAX_BUF-1,"%s%s",DIRECTORY,filename);
        
        // check if the requested file exists
        if (access(filepath,F_OK)==-1){
            printf("File does not exist\n");
            continue;
        }
        
        // open the requested file for read
        if((fd=open(filepath,O_RDONLY))==-1){
            printf("Error opening the file %s %s\n",filepath,strerror(errno));
            exit(1);
        }  

        // Fork the child process 
        pid = fork();

        // if child process: 
        if (pid==0) {
            int bytes;
            // read till EOF
            while((bytes=read(fd,write_buffer,block_len))>0){
                // Write read bytes to client, which writes it back to a new file
                if(write(sd_client,write_buffer,bytes)!=bytes){
                    printf("Child - Error writing to file %s\n", strerror(errno));
                    exit(1);
                }
            }
            // error in read process
            if(bytes==-1){
                printf("Child - Error reading from file %s %s\n", filepath, strerror(errno));
                exit(1);
            }
            // close client socket
            if(close(sd_client)==-1){
                printf("Child - Error closing client sd\n");
                exit(1);
            }
            // close file descriptor of file to be transferred
            if(close(fd)==-1){
                printf("Child - Error closing file descriptor\n");
                exit(1);
            }
        }
        // if fork() was unsuccessful
        else if (pid==-1) {
            printf("SERVER: Error in fork() call\n");
        }
        // if parent process
        else {
            // sd_client is not needed in parent process after the fork
            if (close(sd_client)==-1){
                printf("Parent - Error closing client socket\n");
                exit(1);
            }  
            // file descriptor is not needed in parent process after fork
            if (close(fd)==-1){
                printf("Parent - Error closing file descriptor\n");
                exit(1);
            }
        }
    }

    //Close server socket
    if (close(sd_server)==-1){
        printf("SERVER:Error closing server socket\n");
        exit(1);
    }
    
    return 0;
}
