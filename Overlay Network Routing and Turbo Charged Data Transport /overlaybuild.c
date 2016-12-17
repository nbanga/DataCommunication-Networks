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

#define MAX_BUF 2048

// signal handler for no confirmation
void sigalrmHandler(int sig){
    printf("Response timeout\n");
    exit(1);
}

int main(int argc, char** argv){
    // check if argc is correct
    if (argc<4){
        printf("Format is 'overlaybuild dest_IP dest_port routerk-IP router(k-1)-IP..router-IP server_port src_port");
        exit(1);
    }

    int sd_server,sd_listen;
    char read_buffer[MAX_BUF], write_buffer[MAX_BUF], data_port[10];
    int i, j, bytes;

    // register signal handler for alarm
    signal(SIGALRM,sigalrmHandler);

    // initialize server and client address
    struct sockaddr_in router_addr, self_addr, global_addr;
    bzero((char*) &router_addr, sizeof(router_addr));
    bzero((char*) &self_addr, sizeof(self_addr));
    bzero((char*) &global_addr, sizeof(global_addr));
    int addr_len = sizeof(self_addr);

    // create server socket
    if((sd_server = socket(AF_INET,SOCK_DGRAM,0))==-1) {
        printf("SERVER: Error creating source socket");
        exit(1);
    }
    if((sd_listen = socket(AF_INET,SOCK_DGRAM,0))==-1) {
        printf("SERVER: Error creating server socket");
        exit(1);
    }
    /**********************/
    //printf("Socket created\n");
    /******************/
    
    // initialize self_address
    self_addr.sin_family = AF_INET;
    self_addr.sin_port = htons(atoi(argv[argc-1]));
    self_addr.sin_addr.s_addr = INADDR_ANY;
    // initialize global_address
    global_addr.sin_family = AF_INET;
    global_addr.sin_port = htons(atoi(argv[argc-2]));
    global_addr.sin_addr.s_addr = INADDR_ANY;

    // bind socket to self address
    if (bind(sd_server, (struct sockaddr *)&self_addr, sizeof(self_addr)) ==-1){
        printf("SERVER:Error binding to server address %s\n", strerror(errno));
        exit(1);
    }
    // bind socket to self address
    if (bind(sd_listen, (struct sockaddr *)&global_addr, sizeof(global_addr)) ==-1){
        printf("SERVER:Error binding to server address %s\n", strerror(errno));
        exit(1);
    }
    /**********************/
    //printf("Socket bound\n");
    /******************/
 
    // initialize router_address
    router_addr.sin_family = AF_INET;
    router_addr.sin_port = htons(atoi(argv[argc-2]));
    router_addr.sin_addr.s_addr = inet_addr(argv[argc-3]);

    // create packet to be sent to next hop
    memset(write_buffer,'\0',MAX_BUF);
    for(i=1;i<argc-2;i++){
       // snprintf(write_buffer, MAX_BUF,"%s%s%s", write_buffer, "$", argv[i]);
       strcat(write_buffer,"$");
       strcat(write_buffer,argv[i]);
    }
    strcat(write_buffer,"$");
    /**********************/
    //printf("Write_buffer %s\n", write_buffer);
    /******************/
 
    // send the packet to next hop
    if(sendto(sd_server,write_buffer,strlen(write_buffer),0,(struct sockaddr*) &router_addr, sizeof(router_addr))==-1){
        printf("Error sending to router\n");
        exit(1);
    }
    /**********************/
    //printf("Packet sent\n");
    /******************/
 

    // set ad alarm for 30 sec to wait for confirmation
    alarm(30);

    // wait for ACK from router
    if((bytes=recvfrom(sd_server, read_buffer, MAX_BUF,0,(struct sockaddr*)&router_addr, &addr_len))==-1){
        printf("Error reading ACK from router\n");
        exit(1);
    }
    /**********************/
    //printf("ACK received %s\n",read_buffer);
    /******************/
 
    // store data port received form ACK
    snprintf(data_port, 10, "%s", &(read_buffer[1]));
    printf("ACK port - %s\n", data_port);

    // wait for confirmation from router
    if((bytes=recvfrom(sd_listen, read_buffer, MAX_BUF,0,(struct sockaddr*)&router_addr, &addr_len))==-1){
        printf("Error reading confirmation from router%s", strerror(errno));
        exit(1);
    }
    /**********************/
    //printf("Confirmation received from %s\n", inet_ntoa(router_addr.sin_addr));
    /******************/
 

    // parse the confirmation request
    i=0,j=0;
    char next_router_ip[20], next_router_port[10];
    memset(next_router_ip,'\0',20);
    memset(next_router_port,'\0',10);
    while (read_buffer[i]=='$'){
        i++;
    }
    while(read_buffer[i]!='$'){
        next_router_ip[j++]=read_buffer[i++];
    }
    if (read_buffer[i]=='$'){
        i++;
    }
    j=0;
    while(read_buffer[i]!='$'){
        next_router_port[j++]=read_buffer[i++];
    }
    /**********************/
    //printf("Next router ip %s, next router port %s\n",next_router_ip, next_router_port);
    /******************/
 
 
    // compare ACK and confirmation data port and IP
    // if they match, disable the alarm and print the data port
    // else fail
    if (inet_addr(next_router_ip)==router_addr.sin_addr.s_addr && strcmp(next_router_port,data_port)==0){
        alarm(0);
        printf("%s\n", data_port);
    }
    else{
        printf("ACK and confirmation ports do not match!\n");
        exit(1);
    }
    return 0;
}
 
