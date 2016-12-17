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
#define tunnel_ip "128.10.19.13"
#define max_clients 10

// client Info struct to hold status information of each client request
typedef struct clientInfo{
    int fd_client; // socket descriptor
    unsigned int source_port;
    unsigned int dest_port;
    unsigned int source_ip;
    unsigned int dest_ip;
    char buffer[MAX_BUF];
    int active; // if client is active or dead
}clientInfo;

/* main function to handle client requests for tunneling
* Provides logic for tunneling to server and returning the response back to client
* Uses select to perform non-blocking listen on all regisiterd socket descriptors.
* Assigns ports dynamically by polling OS for an unused port
*/
int main(int argc, char** argv){
    int sd_server,sd_client, max_socket;
    char read_buffer[MAX_BUF], write_buffer[MAX_BUF];
    int bytes, response, i, len; 
    fd_set read_fd_set;
    clientInfo client[max_clients];

    // check if argc is correct
    if (argc<2){
        printf("Format is 'tunneld port_num");
        exit(1);
    }

    // initialize server and client address
    struct sockaddr_in server_addr, client_addr;
    bzero((char*) &server_addr, sizeof(server_addr));
    bzero((char*) &client_addr, sizeof(client_addr));
    int addr_len = sizeof(client_addr);

    // create server socket
    if((sd_server = socket(AF_INET,SOCK_DGRAM,0))==-1){
        printf("SERVER: Error creating server socket");
        exit(1);
    }
    
    // initialize server_address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = INADDR_ANY; //inet_addr(tunnel_ip);

    // bind socket to server address
    if (bind(sd_server, (struct sockaddr *) &server_addr,sizeof(server_addr)) ==-1){
        printf("SERVER:Error binding to server address %s\n", strerror(errno));
        exit(1);
    }
   
    // set client sockets to inactive
    for (i=0;i<10;i++){
        client[i].active = 0;
        client[i].fd_client = -1;
    }

    while(1){
        // clear the socket set
        FD_ZERO(&read_fd_set);
        // add master socket to set
        FD_SET(sd_server, &read_fd_set);
        max_socket = sd_server;
        // add child sockets to set
        for (i=0;i<max_clients;i++){
            // get client socket descriptor 
            sd_client = client[i].fd_client;
            // if sockect is valid, add to read_fd_set
            if (client[i].active && sd_client>=0){
                FD_SET(sd_client, &read_fd_set);
            }
            // update max_socket if required
            if (sd_client>max_socket){
                max_socket = sd_client;
            }
        }

        // wait on select for actvity on one of the active ports
        if((response = select(max_socket+1, &read_fd_set, NULL, NULL, NULL))==-1){
            printf("Error in select(): %s", strerror(errno));
            exit(1);
        }
   
        // If there is a new client request for establishing link with tunnel
        if(FD_ISSET(sd_server, &read_fd_set)){
            memset(read_buffer,'\0',MAX_BUF);
            memset(write_buffer,'\0',MAX_BUF);
            // receive the request from client and populate client_addr
            if ((bytes=recvfrom(sd_server, read_buffer, MAX_BUF,0,(struct sockaddr *)&client_addr, &addr_len))==-1){
                printf("Error reading bytes from client");
                exit(1);
            }
            /***********************/
            printf("Request received from client\n");
            /************************/

            // Parse client's request
            // Format - $serverip$port
            i=0;
            int j=0;
            char ip[20], port[10];
            if(read_buffer[i]=='$'){
                i++;
            }
            while(read_buffer[i]!='$'){
                ip[j++]=read_buffer[i++];
            }
            ip[j]='\0';
            if (read_buffer[i]=='$'){
                i++;
            }
            j=0;
            while(read_buffer[i]!='\0'){
                port[j++]=read_buffer[i++];
            }
            port[j]='\0';

            // create a temporary socket to assign to client
            int temp_socket;
            if ((temp_socket = socket(AF_INET, SOCK_DGRAM,0))==-1){
                printf("SERVER: Error creating ");
                exit(1);
            }
  
            // initialize temporary address to get an unused port from OS
            struct sockaddr_in temp_addr;
            bzero((char*) &temp_addr, sizeof(temp_addr));
            temp_addr.sin_family = AF_INET;
            temp_addr.sin_port = htons(0);
            temp_addr.sin_addr.s_addr = server_addr.sin_addr.s_addr; //inet_addr(tunnel_ip);

            // bind to the tunnel server address with port 0.
            // This assigns an unused port to the socket
            if (bind(temp_socket, (struct sockaddr*) &temp_addr, sizeof(temp_addr))==-1){
                printf("bind unsuccessful %s", strerror(errno));
                exit(1);
            }
  
            // get the assigned port to send back to client
            len = sizeof(temp_addr);
            getsockname(temp_socket, (struct sockaddr *) &temp_addr, &len);
            snprintf(write_buffer,MAX_BUF-1,"%s%d%s%s", "$", ntohs(temp_addr.sin_port),"$",ip);
          
            // iterate over the client array to find an empty spot to register the new request client
            for(i=0;i<max_clients;i++){
                if (client[i].active==0){
                    client[i].fd_client = temp_socket;
                    client[i].dest_ip = inet_addr(ip);
                    client[i].dest_port = htons(atoi(port));
                    client[i].source_port = client_addr.sin_port;
                    client[i].source_ip = client_addr.sin_addr.s_addr;
                    client[i].active = 1;
                    break;
                }
            }
            // Error if max number of clients being serviced
            if(i==max_clients){
                printf("Max client limit reached");
            }
            // send the ACK to client with new port and client's requested server IP 
            if (sendto(sd_server,write_buffer, strlen(write_buffer),0,(struct sockaddr*) &client_addr, addr_len)==-1){
                printf("Error sending payload to client");
                exit(1);
            }
        }
      
        // check for all client sockets 
        // cater to all sockets which have pending requests
        for(i=0;i<max_clients;i++){
            // if client is an active
            if (client[i].active){
                int sock = client[i].fd_client;
                // if client socket is set in &read_fd_set
                // receive data from the socket and populate client_addr (can be either of server or client)
                if(FD_ISSET(sock,&read_fd_set)){
                    memset(client[i].buffer,'\0',MAX_BUF);
                    if((bytes=recvfrom(sock,client[i].buffer,MAX_BUF,0,(struct sockaddr*)&client_addr, &addr_len))==-1){
                        printf("Error receiving data from client");
                        exit(1);
                    }
                    // if >0 bytes received, the connection is still alive
                    if (bytes>0){
                        // if client_add matches the client's ip and port,
                        // payload has to be forwarded to the server
                        if (client_addr.sin_addr.s_addr==client[i].source_ip){
                            struct sockaddr_in addr;
                            addr.sin_family = AF_INET;
                            addr.sin_port = client[i].dest_port;
                            addr.sin_addr.s_addr = client[i].dest_ip;                            
                            if (sendto(sock,client[i].buffer,strlen(client[i].buffer),0,(struct sockaddr*) &addr, sizeof(addr))==-1){
                                printf("Error sending to server from client\n");
                                exit(1);
                            }
                        }
                        // else if client_addr matches the server's ip and port
                        // response from server to client's request
                        // Tunnel needs to send it back to the client
                        else if (client_addr.sin_addr.s_addr==client[i].dest_ip){
                            struct sockaddr_in addr;
                            addr.sin_family = AF_INET;
                            addr.sin_port = client[i].source_port;
                            addr.sin_addr.s_addr = client[i].source_ip;
                            if (sendto(sock,client[i].buffer,strlen(client[i].buffer),0,(struct sockaddr*) &addr, sizeof(addr))==-1){
                                printf("Error sending to server from client\n");
                                exit(1);
                            }
                        }
                        // else wrong address
                        else {
                            printf("Wrong source/dest IP address. Not registered with tunnel app.");
                            exit(1);
                        }
                    }
                    // if bytes = 0, client shutdown its connection
                    // remove client from the ist of active clients, can be overwritten
                    // Remocve its socket descriptor from fd_set that select blocks on
                    else if (bytes==0){
                        FD_CLR(sock,&read_fd_set);
                        if(close(sock)==-1){
                            printf("Erro closing socket for client");
                            exit(1);
                        }
                        client[i].active = 0;
                        client[i].fd_client = -1;
                    }
                }
            }
        }      
    }
    return 0;
}
