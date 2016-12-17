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
 
// main function
// recieve packets from client
// compute completion time, bps, pps
int main(int argc, char** argv){
    char portNum[10];
    int sd_server;
    int bytes,total_bytes=0,total_packets=0;;
    struct timeval start,end;
   
    // verifying number of arguments
    if (argc<3) {
        printf("Format is 'traffic_rcv port-number payload_size'\n");
        exit(1);
    }
    int MAX_BUF = atoi(argv[2]);
    char read_buffer[MAX_BUF];

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

    // first_request flags if a packet is first packet in a series of exchange to occur
    // this comes in use when the last three packets of three bytes are received
    // when the first 3 byte packet is received, the value of first request will be 1
    // and we will get end time
    // for the next two packets, first_request will be 0 and we continue
    // Each new client request sets first_request to 1.
    int first_request=0;
    while(1) {        
        //set buffers to null values
        memset(read_buffer,'\0',MAX_BUF);
        int flag = 0;
        total_packets=0;
        total_bytes=0;
        //Read request from client socket and check for payload size
        while((bytes=recvfrom(sd_server,read_buffer,MAX_BUF,0,(struct sockaddr *)&client_addr, &addr_len))>0){         
            // for last three packets which are sent
            // if the packet size is 3 bytes, do nothing
            // if it is the first 3 byte packet, get the end time of packet transfer
            if (bytes==3){
                if (first_request==0) continue;
                first_request = 0;
                gettimeofday(&end,NULL);
                break;
            }
            else {
                // If this is the first packet, put the flag to 1.
                // Get start time.
                if (flag==0){
                    flag=1;
                    gettimeofday(&start,NULL);
                }
                // Signify that exchange has started.
                first_request=1;
                // Calculate total packets and total bytes
                total_packets+=1;
                total_bytes=total_bytes+bytes+46; // ethernet header of 46
                memset(read_buffer,'\0',MAX_BUF); // set read buffer to null values so that when payload is of lesser length, it doesnt get corrupted.
            }
        }
        // recvFrom not successful
        if(bytes==-1){ 
            printf("SERVER:Error reading from socket\n");
            exit(1);
        } 

        // print completion time, bits per second, packets per second
        float completion_time = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec))/1000000.0f;
        printf("Completion Time \t %0.6f sec\n", completion_time );
        printf("Bits Per Second \t %0.3f bps\n",(float) ((total_bytes*8)/completion_time));
        printf("Packets Per Second \t %0.3f bps\n",(float) (total_packets/completion_time));
    }

    //Close server socket
    if (close(sd_server)==-1){
        printf("SERVER:Error closing server socket");
        exit(1);
    }
    
    return 0;
}
