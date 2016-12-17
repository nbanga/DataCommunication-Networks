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

// function to check if secretkey of client is a valid key or not
int isValidSecretkey(char* secretkeyServer, char* secretkeyClient){
    // checks for the length of the keys
    if (strlen(secretkeyClient)<10 || strlen(secretkeyClient)>20 || strcmp(secretkeyServer,secretkeyClient)!=0){
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

int main(int argc, char** argv){
    char secretkeyClient[50],portNum[10];
    int sd_server;
    char command[MAX_BUF];
    char read_buffer[MAX_BUF], write_buffer[MAX_BUF];
    int len,bytes,numRequests=0;

    // verifying number of arguments
    if (argc<3) {
        printf("Format is 'mypingd portnumber secretkey'");
        exit(1);
    }

    // create server address
    struct sockaddr_in server_addr, client_addr;
    bzero((char*) &server_addr, sizeof(server_addr));
    bzero((char*) &client_addr, sizeof(client_addr));
    int addr_len= sizeof(client_addr);

    //Create server socket
    if ((sd_server = socket(AF_INET,SOCK_DGRAM,0))==-1){
        printf("SERVER:Error creating server socket\n");
        exit(1);
    }

    // initialize server_address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = inet_addr("128.10.3.13"); 

    // bind socket to server address
    if (bind(sd_server, (struct sockaddr *) &server_addr,sizeof(server_addr)) ==-1){
        printf("SERVER:Error binding to server address %s\n", strerror(errno));
        exit(1);
    }

    while(1) {        
        //set buffers to null values
        memset(read_buffer,'\0',MAX_BUF);
        memset(command,'\0',MAX_BUF);
        memset(secretkeyClient,'\0',50);
        memset(write_buffer,'\0',MAX_BUF);

        //Read request from client socket and check for payload size
        if ((bytes=recvfrom(sd_server,read_buffer,MAX_BUF,0,(struct sockaddr *)&client_addr, &addr_len))==-1){
            printf("SERVER:Error reading from socket\n");
            exit(1);
        }
        else if (bytes!=1000){
            continue;
        }

        //Parse client's request
        len = strlen(read_buffer);
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
        // check if secret key of client meets validity constraints
        if (!isValidSecretkey(argv[2],secretkeyClient)){
            continue;
        }
        // write payload value to be sent to client in response
        snprintf(write_buffer,MAX_BUF-1, "%s","terve");

        // send packet back to client
        if(sendto(sd_server,write_buffer,strlen(write_buffer),0,(struct sockaddr*) &client_addr, addr_len)==-1){
            printf("Error sending payload to client: %s", strerror(errno));
            exit(1);
        }
    }

    //Close server socket
    if (close(sd_server)==-1){
        printf("SERVER:Error closing server socket");
        exit(1);
    }
    return 0;
}
