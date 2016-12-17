#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <errno.h>

#define MAX_BUF 100
#define MAX_LEN 1024
#define COMMAND "cal"

int main(int argc, char** argv){
    int sd_client;
    char read_buffer[MAX_LEN], write_buffer[MAX_BUF];
    int len;
    struct sockaddr_in server_addr;
    struct hostent* host; 

    if (argc<4) {
        printf("Format is 'cmdclient hostname portnumber secretkey'");
        exit(1);
    }

    if ((host = gethostbyname(argv[1]))==NULL){
        printf("Error finding host details");
        exit(1);
    }

    // set write_buffer to null values
    memset(write_buffer,'\0',MAX_BUF);
    memset(read_buffer,'\0',MAX_LEN);

    // create client socket
    if ((sd_client=socket(AF_INET,SOCK_STREAM,0))==-1){
        printf("CLIENT:Error creating client socket");
        exit(1);
    }

    // Create client request
    snprintf(write_buffer, MAX_BUF-1, "%s%s%s%s", "$",argv[3],"$",COMMAND);

    // create server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    memcpy(&server_addr.sin_addr, host->h_addr_list[0], host->h_length);

    // connect to server
    if(connect(sd_client,(struct sockaddr*)&server_addr, sizeof(server_addr))==-1) {
        printf("Error while connecting to host %s on port %s\n %s", argv[1], argv[2], strerror(errno));
        exit(1);
    }

    //Write request to client socket
    if (write(sd_client,write_buffer,strlen(write_buffer))==-1){
        printf("CLIENT:Error while writing to client socket");
        exit(1);
    }

    // Read response from server
    if (read(sd_client,read_buffer,MAX_LEN)==-1){
        printf("CLIENT:Error reading from socket");
        exit(1);
    }
    else {
        printf("Response to request by client \n%s\n", read_buffer);
    }
    
    // close client socket
    if(close(sd_client)==-1){
        printf("CLIENT:Error closing client socket");
        exit(1);
    }
    
    return 0;
}
