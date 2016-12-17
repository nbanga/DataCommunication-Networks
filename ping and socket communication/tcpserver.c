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

// function to check if client's request is an acceptable command
int isValidCommand(char acceptedCommands[][5], int len, char* command){
    int i=0;
    for(;i<4;i++){
        if(strcmp(command,acceptedCommands[i])==0)
            return 1;
    }
    return 0;
}

int main(int argc, char** argv){
    pid_t pid;
    char secretkeyClient[50],portNum[10];
    int sd_server, sd_client;
    char command[MAX_BUF];
    char read_buffer[MAX_BUF];
    char acceptedCommands[4][5] = {"ls", "date", "host", "cal"};
    int len;

    // check if arguments are proper
    if (argc<3) {
        printf("Format is 'cmdserver portnumber secretkey'");
        exit(1);
    }

    // initilaize server address
    struct sockaddr_in server_addr, client_addr;
    bzero((char*) &server_addr, sizeof(server_addr));
    bzero((char*) &client_addr, sizeof(client_addr));
    int addr_len = sizeof(client_addr);

    //Create server socket
    if ((sd_server = socket(AF_INET,SOCK_STREAM,0))==-1){
        printf("SERVER:Error creating server socket");
        exit(1);
    }
    
    // initialize server_address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = INADDR_ANY; 

    // bind socket to server address
    if (bind(sd_server, (struct sockaddr *) &server_addr,sizeof(server_addr)) ==-1){
        printf("SERVER:Error binding to server address %s\n", strerror(errno));
        exit(1);
    }

    // listen on server side socket
    if (listen(sd_server, 5)==-1){
        printf("Error in listen call");
        exit(1);
    }

    // register SIGCHLD handler with parent process.
    // Enables parent to process any child exits and prevents zombies.
    signal(SIGCHLD,sigchldHandler);

    while(1) {
        // set buffers to null values
        memset(read_buffer,'\0',MAX_BUF);
        memset(command,'\0',MAX_BUF);
        memset(secretkeyClient,'\0',50);

        // accept client request and forms full associations
        if ((sd_client=accept(sd_server, (struct sockaddr *) &client_addr, &addr_len))==-1){
            printf("Failed to accept connection");
            exit(1);
        }

        //Read request from client socket
        if (read(sd_client,read_buffer,MAX_BUF)==-1){
            printf("SERVER:Error reading from server fifo file descriptor");
            exit(1);
        }

        //Parse client's request
        len = strlen(read_buffer);
        if (len<=1)
            continue;
        int i=0,j=0,k=0;
        if(read_buffer[i]=='$')
            i++;
        while(read_buffer[i]!='$'){
            secretkeyClient[j++]=read_buffer[i++];
        }
        if(read_buffer[i]=='$')
            i++;
        while(read_buffer[i]!='\0'){
            command[k++]=read_buffer[i++];
        }

        // verify constraints on secret key received from client
        if (!isValidSecretkey(argv[2],secretkeyClient)){
            continue;
        }
        
        // verify command received from client
        if (!isValidCommand(acceptedCommands,4,command)){
            continue;
        }

        // Fork the child process 
        pid = fork();

        // if child process: 
        if (pid==0) {
            // redirect output of execlp to client socket 
            if (dup2(sd_client,1)<0){
                printf("SERVER: dup2() errored");
                exit(1);
            }
            if (close(sd_client)==-1){
                printf("Error closing client fd");
                exit(1);
            }
            // execute client's request
            if(execlp(command,command,NULL)==-1){
                printf("Error executing execlp %s\n", strerror(errno));
                exit(1);
            }
        }
        // if fork() was unsuccessful
        else if (pid==-1) {
            printf("SERVER: Error in fork() call");
        }
        // if parent process
        else {
            // sd_client is not needed in parent process after the fork
            if (close(sd_client)==-1){
                printf("Error closing client socket");
                exit(1);
            }  
        }
    }

    //Close server socket
    if (close(sd_server)==-1){
        printf("SERVER:Error closing server socket");
        exit(1);
    }
    
    return 0;
}
