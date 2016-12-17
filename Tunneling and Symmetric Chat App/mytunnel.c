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
#include <sys/time.h>
#include <math.h>

#define MAX_BUF 1024
volatile int interrupt_count = 0;

// function to generate a random string of specified length
// used to create a payload of 1000 bytes
char* generateRandomString(int len){
    char characters[] = "abcdefghijklmnopqrstuvwxyz1234567890";
    char* newString = (char*)malloc(len*sizeof(char));
    if (newString==NULL){
        printf("Error creating payload\n");
        exit(1);
    }
    int i;
    for (i=0;i<len-1;i++){
        newString[i] = characters[rand()%(sizeof(characters)-1)];
    }
    newString[i]='\0';
    return newString;
}

// Signal Handler to exit when alarm sounds
void sigalrmHandler(int status){
    interrupt_count ++;
}

int main(int argc, char** argv){
    pid_t pid;
    char secretkeyClient[50];
    int sd_client;
    char read_buffer[MAX_BUF],write_buffer[MAX_BUF];
    char* payload;
    int len,bytes;
    struct timeval start, end;
    struct sigaction sigAction;

    // check if number of arguments are proper
    if (argc<6) {
        printf("Format is 'mytunnel vpn-IP vpn-port server-IP server-port secretkey'\n");
        exit(1);
    }
    strcpy(secretkeyClient,argv[5]);

    // registering signal handler for SIGALRM with sigaction
    sigAction.sa_handler = sigalrmHandler;
    if (sigfillset(&sigAction.sa_mask) < 0){
        printf("Error initializing sigaction");
        exit(1);
    }
    sigAction.sa_flags = 0;
    if (sigaction(SIGALRM, &sigAction, 0) < 0){
        printf("Error in handler call for SIGALRM failed");
        exit(1);
    }

    // create server address
    struct sockaddr_in server_addr, client_addr, server_addr_ret;
    bzero((char*) &server_addr, sizeof(server_addr));
    bzero((char*) &client_addr, sizeof(client_addr));
    bzero((char*) &server_addr_ret, sizeof(server_addr_ret));
    unsigned int addr_len_ret = sizeof(server_addr_ret);

    //Create server socket
    if ((sd_client = socket(AF_INET,SOCK_DGRAM,0))==-1){
        printf("Error creating client socket");
        exit(1);
    }
    // initialize client_address
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(0);
    client_addr.sin_addr.s_addr = INADDR_ANY; //inet_addr("128.10.2.13"); 

    // initialize server_address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    server_addr.sin_addr.s_addr = inet_addr(argv[1]);
    unsigned int addr_len = sizeof(server_addr);

    // bind socket to client address
    if (bind(sd_client, (struct sockaddr *) &client_addr,sizeof(client_addr)) ==-1){
        printf("SERVER:Error binding to server address %s\n", strerror(errno));
        exit(1);
    }
    // create client request with $server-IP$serverPort
    snprintf(write_buffer,MAX_BUF-1,"%s%s%s%s", "$", argv[3], "$", argv[4]);
    
    // send data to ping server socket
    if(sendto(sd_client,write_buffer,strlen(write_buffer),0,(struct sockaddr*) &server_addr, addr_len)==-1){
        printf("Error sending payload to ping server");
        exit(1);
    }

    // start the clock for alarm for 2.55 sec
    // since ularam only takes 1 sec as the max TIMEOUT
    // break it into 3 equal parts and ignore the signal 
    // till the alarm interrupts for the third time.
    ualarm(850000,0);

    // Receive response from server
    // If client process is interrupted by alarm, check if it's 2.55 sec
    // If not, continue to wait for the parent process.
    // Else, exit
    while(recvfrom(sd_client,read_buffer,MAX_BUF,0,(struct sockaddr *)&server_addr_ret, &addr_len_ret)==-1){
        if (errno == EINTR){
            if (interrupt_count<6){
                ualarm(850000,0);
            }
            else {
                printf("no response from server\n");
                exit(1);
            }
        }
        else {
            printf("Error reading response from ping server");
            exit(1);
        }
    }
    printf("%s\n", read_buffer);
    
    // parse read buffer
    // $port$serverip
    int i =0, j=0;
    char port[10], ip[20];
    if (read_buffer[i]=='$')
        i++;
    while(read_buffer[i]!='$')
        port[j++]=read_buffer[i++];
    port[j]='\0';
    j=0;
    if(read_buffer[i]=='$')
        i++;
    while(read_buffer[i]!='\0')
        ip[j++]=read_buffer[i++];
    ip[j]='\0';
   
    // check ip returned by tunnel matches server ip
    if(strcmp(ip,argv[3])!=0){
        printf("Wrong request received by client. Ip address and server address do not match");
        exit(1);
    }
    
    // set up a new sender address with new port returned by tunnel app
    server_addr.sin_port = htons(atoi(port));  

    // clear read and write buffer
    memset(read_buffer,'\0',MAX_BUF);
    memset(write_buffer,'\0',MAX_BUF);

    // create client request with 1000 bytes
    int payload_len = 1001-strlen(secretkeyClient)-2;
    payload = generateRandomString(payload_len);
    snprintf(write_buffer,MAX_BUF-1,"%s%s%s%s","$",secretkeyClient,"$",payload);
    
    // send data to tunnel server socket
    if(sendto(sd_client,write_buffer,strlen(write_buffer),0,(struct sockaddr*) &server_addr, addr_len)==-1){
        printf("Error sending payload to ping server");
        exit(1);
    }
    
    // start the clock for alarm for 2.55 sec
    // since ularam only takes 1 sec as the max TIMEOUT
    // break it into 3 equal parts udbhav and ignore the signal 
    // till the alarm interrupts for the third time.
    ualarm(850000,0);

    // get time of departure for client request
    gettimeofday(&start,NULL);

    // Receive response from server
    // If client process is interrupted by alarm, check if it's 2.55 sec
    // If not, continue to wait for the parent process.
    // Else, exit
    while(recvfrom(sd_client,read_buffer,MAX_BUF,0,(struct sockaddr *)&server_addr_ret, &addr_len_ret)==-1){
        if (errno == EINTR){
            if (interrupt_count<10){
                ualarm(850000,0);
            }
            else {
                printf("no response from server\n");
                exit(1);
            }
        }
        else {
            printf("Error reading response from ping server");
            exit(1);
        }
    }
    // restting ualarm to 0 if value has been received from recvFrom()
    ualarm(0,0);
    
    // verify the server response is "terve"
    if (strcmp(read_buffer,"terve")!=0){
        printf("Wrong response received from server");
        exit(1);
    }

    // get time of arrival of response from ping server
    gettimeofday(&end,NULL);

    // print server ip, port and round trip time
    char* serverIP = malloc(sizeof(char)*INET_ADDRSTRLEN);
    inet_ntop(AF_INET,&(server_addr_ret.sin_addr),serverIP,INET_ADDRSTRLEN);
    printf("Server IP\t Port\t RTT\n");
    printf("%s\t %d\t %0.3f msec\n", serverIP, ntohs(server_addr_ret.sin_port), ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec))/1000.0f);

    //Close server socket
    if (close(sd_client)==-1){
        printf("SERVER:Error closing server socket");
        exit(1);
    }
    return 0;
}
