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

// function to generate a random string of specified length
// used to create a payload of payload_size bytes
char* generateRandomString(int len){
    char characters[] = "abcdefghijklmnopqrstuvwxyz1234567890";
    char* newString = (char*)malloc(len*sizeof(char));
    if (newString==NULL){
        printf("Error creating payload\n");
        exit(1);
    }
    // Start the payload with B, followed by specified payload_len bytes.
    int i;
    newString[0]='B';
    for (i=1;i<len-1;i++){
        newString[i] = characters[rand()%(sizeof(characters)-1)];
    }
    newString[i]='\0';
    return newString;
}

// main function
// used to create client address, server address
// send bytes to packet and calculate the bps, pps, and completion time.
int main(int argc, char** argv){
    int sd_client;
    char* payload;
    int bytes,payload_len,packet_count,packet_spacing;
    struct timeval start,end;
    int total_bits=0,total_packets=0;

    // check if number of arguments are proper
    if (argc<6) {
        printf("Format is 'traffic_snd  IP-address port-number payload-size packet-count packet-spacing '\n");
        exit(1);
    }
    // retrive int values from arguments
    payload_len = atoi(argv[3]);
    packet_count = atoi(argv[4]);
    packet_spacing = atoi(argv[5]);

    // create server address
    struct sockaddr_in server_addr, client_addr, server_addr_ret;
    bzero((char*) &server_addr, sizeof(server_addr));
    bzero((char*) &client_addr, sizeof(client_addr));
    bzero((char*) &server_addr_ret, sizeof(server_addr_ret));
    unsigned int addr_len_ret = sizeof(server_addr_ret);


    //Create server socket
    if ((sd_client = socket(AF_INET,SOCK_DGRAM,0))==-1){
        printf("Error creating client socket\n");
        exit(1);
    }
    
    // initialize client_address
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(0);
    client_addr.sin_addr.s_addr = INADDR_ANY; 

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

    // get time of start of transmission
    gettimeofday(&start,NULL);

    // send data to ping server socket
    int i;
    for(i=0;i<packet_count;i++){
        // create client request with payload_size bytes
        payload = generateRandomString(payload_len+1);
        // send bytes to server
        if((bytes=sendto(sd_client,payload,payload_len,0,(struct sockaddr*) &server_addr, addr_len))==-1){
            printf("Error sending payload to ping server\n");
            exit(1);
        }
        // calculate bits sent in this cycle
        total_bits += (bytes+46)*8; 
        // increment packet assuming all bytes are sent as 1 payload
        total_packets = total_packets + 1;
        // sleep for packet_spacing microseconds
        if (i<packet_count-1){
            // if not last packet
            if (usleep(packet_spacing)==-1){
                printf("Error invoking sleep %s\n", strerror(errno));
                exit(1);
            }
        }
    }

    // get time of end of transmission
    gettimeofday(&end,NULL);

    // send 3 payloads of 3 bytes without pause to signal end of transmission
    int j;
    char* sigend_payload = "end";  // since 1 char is 1 byte, therefore a char string of length 3
    for(j=0;j<3;j++){
        if((bytes=sendto(sd_client,sigend_payload,3,0,(struct sockaddr*) &server_addr, addr_len))==-1){
            printf("Error sending payload to ping server\n");
            exit(1);
        }
        else if (bytes!=3){
            printf("Error in sending 3 bytes of data. Sent %d bytes\n", bytes);
            exit(1);
        }
    }

    // calculate completion time
    float completion_time = ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec))/1000000.0f;
    // print completion time, bits per scond, packets per second
    printf("Completion Time \t %0.6f sec\n", completion_time );
    printf("Bits Per Second \t %0.3f bps\n",(float) (total_bits/completion_time));
    printf("Packets Per Second \t %0.3f bps\n",(float) (total_packets/completion_time));

    //Close server socket
    if (close(sd_client)==-1){
        printf("SERVER:Error closing server socket");
        exit(1);
    }
    
    return 0;
}
