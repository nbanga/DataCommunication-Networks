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
#include <time.h>
#include <ifaddrs.h>

#define MAX_BUF 2048

fd_set read_fd_set;
struct sockaddr_in server_addr, client_addr;

// client Info struct to hold status information of each client request
typedef struct clientInfo{
    int fd_client; 
    unsigned int source_port;
    unsigned int dest_port;
    unsigned int source_ip;
    unsigned int dest_ip;
    unsigned int data_port;
    unsigned int data_ip;
    struct clientInfo* next;
}clientInfo;

clientInfo* routing_table=NULL;

// delta_list structure for maintaining temp table
typedef struct delta_node{
    clientInfo* cInfo;
    struct timeval timestamp;
    struct delta_node* next;
}delta_node;

typedef struct delta_list{
    delta_node* head;
    delta_node* tail;
    int size;
}delta_list;

// initialize temp_table for router entries
delta_list* temp_table = NULL;

// queue operations
void enqueue(clientInfo* info){
    //printf("Inside enqueue\n");
    delta_node* newNode = (delta_node*)malloc(1*sizeof(delta_node));
    newNode->cInfo = info;
    newNode->next = NULL;
    gettimeofday(&(newNode->timestamp),NULL);
    if(temp_table->head==NULL){
        temp_table->head=newNode;
        temp_table->tail=newNode;
    }
    else{
        temp_table->tail->next=newNode;
        temp_table->tail = newNode;
    }
    temp_table->size++;
    //printf("Size of temp_table %d\n", temp_table->size);
}

void dequeue(){
    if(temp_table->size==0){
        return;
    }
    delta_node* node = temp_table->head;
    if(temp_table->head==temp_table->tail){
        temp_table->tail=NULL;
    }
    temp_table->head=temp_table->head->next;
    temp_table->size--;
    free(node);
}

// print details to stdout
void print_details(clientInfo* info, char* message){
    time_t rawtime;
    struct tm* timeinfo;
    rawtime = time(NULL);
    timeinfo = localtime(&rawtime);
    char buf[20];
    struct in_addr source_addr,dest_addr,serv_addr;
    source_addr.s_addr = info->source_ip;
    dest_addr.s_addr = info->dest_ip;
    serv_addr.s_addr = info->data_ip;
    strftime(buf, 20, "%H:%M:%S %p", timeinfo);
    printf("%s\t\t",buf);
    printf("%s\t\t", inet_ntoa(serv_addr));
    printf("%s\t\t", message);
    printf("%s:%d -> ", inet_ntoa(source_addr), ntohs(info->source_port));
    printf("%s:%d\n", inet_ntoa(dest_addr), ntohs(info->dest_port));
} 

// Signal handler for SIGALRM
void sigalarmHandler(int sig){
    /************************/
    //printf("Inside alarm handler. Exiting..\n");
    /************************/
    FD_CLR(temp_table->head->cInfo->fd_client,&read_fd_set);
    clientInfo* info = temp_table->head->cInfo;
    print_details(info,"Entry timeout from Temp Table");
    dequeue();
    /**************************/
    //printf("Dequeue done!\n");
    /***************************/
}
   
// main function
int main(int argc, char** argv){
    // check if argc is correct
    if (argc<2){
        printf("Format is 'overlayrouter server-port");
        exit(1);
    }

    int sd_server, max_socket;
    char read_buffer[MAX_BUF], write_buffer[MAX_BUF];
    int bytes, response, i, j, len; 
    clientInfo* temp;
    delta_node* temp_d;
    struct sigaction sigAlarm;

    // to get all IPs present on system
    struct ifaddrs *ifap,*ifa;
    struct sockaddr_in* sa;
    char* addr_buffer; // = (char*)malloc(MAX_BUF*sizeof(char));
    getifaddrs(&ifap);
    /*int count=0;
    for(ifa = ifap;ifa;ifa=ifa->ifa_next){
        if (ifa->ifa_addr->sa_family==AF_INET){
            count++;
        }
    }
    i = 0;
    addr_buffer = (char**)malloc(count*sizeof(char*));
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr->sa_family==AF_INET){
            sa = (struct sockaddr_in*) ifa->ifa_addr;
            printf("%s\n",inet_ntoa(sa->sin_addr));
            addr_buffer[i++] = inet_ntoa(sa->sin_addr);
            printf("%s\n", addr_buffer[i-1]);
        }
    }              
    for(i=0;i<count;i++){
        printf("%s\n", addr_buffer[i]);
    }*/

    // initialize delta_list temp_table
    temp_table = (delta_list*)malloc(1*sizeof(delta_list));
    temp_table->head=NULL;
    temp_table->tail=NULL;
    temp_table->size=0;

    // initialize server and client address
    bzero((char*) &server_addr, sizeof(server_addr));
    bzero((char*) &client_addr, sizeof(client_addr));
    int addr_len = sizeof(client_addr);
    
    // create server socket
    if((sd_server = socket(AF_INET,SOCK_DGRAM,0))==-1){
        printf("SERVER: Error creating server socket");
        exit(1);
    }
    /*********************/
    //printf("Server socket created\n");
    /*********************/

   
    // initialize server_address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = INADDR_ANY; //inet_addr(argv[2]);

    // bind socket to server address
    if (bind(sd_server, (struct sockaddr *) &server_addr,sizeof(server_addr)) ==-1){
        printf("SERVER:Error binding to server address %s\n", strerror(errno));
        exit(1);
    }
     
    len = sizeof(server_addr);
    getsockname(sd_server, (struct sockaddr *) &server_addr, &len);
                    
    /*********************/
    //printf("Server socket bound %s\n", inet_ntoa(server_addr.sin_addr));
    /*********************/

    // registering signal handler for SIGALRM with sigaction
    sigAlarm.sa_handler = sigalarmHandler;
    if(sigfillset(&sigAlarm.sa_mask) < 0){
        printf("Error initializing signal handler");
        exit(1);
    }
    sigAlarm.sa_flags = 0;
    if(sigaction(SIGALRM, &sigAlarm, 0) < 0) {
        printf("Error in handler call for SIGPOLL failed");
        exit(1);
    }
    /*********************/
    //printf("SigAlrm initialized\n");
    /**********************/
    

    while(1){
        // clear the socket set
        FD_ZERO(&read_fd_set);
        //printf("FD_ZERO done\n");
        // add master socket to set
        FD_SET(sd_server, &read_fd_set);
        //printf("FD_SET done\n");
        // add sd_server as max_socket
        max_socket = sd_server;
        for(temp=routing_table;temp!=NULL;temp=temp->next){
            FD_SET(temp->fd_client, &read_fd_set);
            //printf("FD_SET for routing table\n");
            if(temp->fd_client>max_socket){
                max_socket = temp->fd_client;
            }
        }
        for(temp_d=temp_table->head;temp_d!=NULL;temp_d=temp_d->next){
            FD_SET(temp_d->cInfo->fd_client, &read_fd_set);
            //printf("FD_SET for temp table\n");
            if(temp_d->cInfo->fd_client>max_socket){
                max_socket = temp_d->cInfo->fd_client;
            }
        }
        /*********************/
        //printf("FD_SET initialized\n");
        /*********************/

        // wait on select for actvity on one of the active ports
        //printf("Waiting for select!\n");
        response = select(max_socket+1, &read_fd_set, NULL, NULL, NULL);
        //printf("In select!!!!!\n");
       
        if(response>0){
            // fd_isset for temp_table - UDP ACKs
            for(temp_d=temp_table->head;temp_d!=NULL;temp_d=temp_d->next){
                if(FD_ISSET(temp_d->cInfo->fd_client,&read_fd_set)){
                    memset(read_buffer,'\0',MAX_BUF);
                    // receive payload from receiver
                    if((bytes=recvfrom(temp_d->cInfo->fd_client,read_buffer,MAX_BUF,0,(struct sockaddr*)&client_addr, &addr_len))==-1){
                        printf("Error receiving data from client");
                        exit(1);
                    }
                    /*********************/
                    //printf("UDP ACK %s received from %s:%d\n", read_buffer, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                    /*********************/

                    // parse UDP ACK
                    i=0,j=0;
                    char d_port[10];                    
                    if(read_buffer[i]=='$'){
                        i++;
                    }
                    while(read_buffer[i]!='\0'){
                        d_port[j++] = read_buffer[i++];
                    }
                    d_port[j]='\0';               

                    // update the entry with returned UDP_PORT
                    temp_d->cInfo->dest_port = htons(atoi(d_port));
                    /*********************/
                    //printf("Update UDP port %s\n", d_port);
                    /*********************/
                    FD_CLR(temp_d->cInfo->fd_client, &read_fd_set);
                }
            }

            // If there is a new client request for establishing link with tunnel
            if(FD_ISSET(sd_server, &read_fd_set)){
                /*********************/
                //printf("Incoming request on server socket\n");
                /*********************/

               memset(read_buffer,'\0',MAX_BUF);
               
                // receive the request from client and populate client_addr
                if ((bytes=recvfrom(sd_server, read_buffer, MAX_BUF,0,(struct sockaddr *)&client_addr, &addr_len))==-1){
                    printf("Error reading bytes from client");
                    exit(1);
                }
                /*********************/
                //printf("Server socket request %s\n", read_buffer);
                /*********************/

 
                // Parse client's request
                // format 1: $dstIP$dstPort$routerk-IP$router(k-1)-IP$..$router1-IP$
                // format 2: $$routerk-IP$data-port-k$
                int format=1;
                if (read_buffer[1]=='$'){
                    format=2;
                }
                
                // if a new request for routing
                if (format==1){
                    /*********************/
                    //printf("New request\n");
                    /*********************/

                   // Parse payload for currentIP
                    j=0;
                    char current_router_ip[20];
                    memset(current_router_ip,'\0',20);
                    i=strlen(read_buffer)-1;
                    if (read_buffer[i]=='$'){
                        i--;
                    }
                    for(;read_buffer[i]!='$';i--);
                    int pointer = i;
                    for(i=i+1;read_buffer[i]!='$';i++){
                        current_router_ip[j++]=read_buffer[i];
                    }
                    read_buffer[pointer+1]='\0'; //truncate read_buffer
                    
                    // check if request has come to correct router
                    /*int addr_buffer_index=-1;
                    for(i=0;i<count;i++){
                        printf("Checking ip address %s\n", addr_buffer[i]);
                        if (strcmp(addr_buffer[i],current_router_ip)==0){
                            addr_buffer_index=i;
                            break;
                        }
                    }
                    if (i==count){
                        continue;
                    }*/
                    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
                        if (ifa->ifa_addr->sa_family==AF_INET){
                            sa = (struct sockaddr_in*) ifa->ifa_addr;
                            //printf("%s\n",inet_ntoa(sa->sin_addr));
                            addr_buffer = inet_ntoa(sa->sin_addr);
                            //printf("%s\n", addr_buffer);
                            if(strcmp(addr_buffer,current_router_ip)==0){
                                break;
                            }
                        }
                    }
                    if(ifa==NULL){
                        continue;
                    }
                    /*********************/
                    //printf("Request ip matches last ip of payload %s\n", addr_buffer);
                    /*********************/

                    // create a data port for this request
                    int temp_socket;
                    if ((temp_socket = socket(AF_INET, SOCK_DGRAM,0))==-1){
                        printf("SERVER: Error creating temporary socket for format 1");
                        exit(1);
                    }
          
                    // initialize temporary address to get an unused port from OS
                    struct sockaddr_in temp_addr;
                    bzero((char*) &temp_addr, sizeof(temp_addr));
                    temp_addr.sin_family = AF_INET;
                    temp_addr.sin_port = htons(0);
                    //temp_addr.sin_addr.s_addr = server_addr.sin_addr.s_addr; 
                    temp_addr.sin_addr.s_addr = inet_addr(addr_buffer);

                    // bind to the tunnel server address with port 0.
                    // This assigns an unused port to the socket
                    if (bind(temp_socket, (struct sockaddr*) &temp_addr, sizeof(temp_addr))==-1){
                        printf("bind unsuccessful %s", strerror(errno));
                        exit(1);
                    }
          
                    // get the assigned port to send back to client
                    len = sizeof(temp_addr);
                    getsockname(temp_socket, (struct sockaddr *) &temp_addr, &len);
                    /*********************/
                    //printf("temp socket created and bound with port and ip %s: %d\n", inet_ntoa(temp_addr.sin_addr),ntohs(temp_addr.sin_port));
                    /*********************/


                    // send back an acknowledgement to the sender with new data port
                    memset(write_buffer,'\0',MAX_BUF);
                    snprintf(write_buffer,MAX_BUF-1,"%s%d", "$", ntohs(temp_addr.sin_port));
                    if (sendto(sd_server,write_buffer, strlen(write_buffer),0,(struct sockaddr*) &client_addr, addr_len)==-1){
                        printf("Error sending payload to client");
                        exit(1);
                    }
                    /*********************/
                    //printf("UDP ACK sent to client %s %s\n", inet_ntoa(client_addr.sin_addr), write_buffer);
                    /*********************/


                    // check if last router by counting number of $ signs in the message
                    int count_dollar=0;
                    /**********************/
                    //printf("Read_buffer to count dollar signs %s\n", read_buffer);
                    /**********************/
                    for(i=0;read_buffer[i]!='\0';i++){
                        if (read_buffer[i]=='$'){
                            count_dollar++;
                        }
                    }

                    // if last request
                    if (count_dollar==3){
                        /*********************/
                        //printf("Last router\n");
                        /*********************/

                       // extract dest_ip and dest_port
                        i=0;
                        j=0;
                        char dest_ip[20], dest_port[10];
                        if(read_buffer[i]=='$'){
                            i++;
                        }
                        while(read_buffer[i]!='$'){
                            dest_ip[j++]=read_buffer[i++];
                        }
                        dest_ip[j]='\0';
                        if (read_buffer[i]=='$'){
                            i++;
                        }
                        j=0;
                        while(read_buffer[i]!='$'){
                            dest_port[j++]=read_buffer[i++];
                        }
                        dest_port[j]='\0';
                        /*********************/
                        //printf("Dest ip %s dest port %s\n", dest_ip, dest_port);
                        /*********************/


                        // add the source->dest mapping in routing table
                        clientInfo* newEntry = (clientInfo*)malloc(1*sizeof(clientInfo));
                        newEntry->fd_client = temp_socket;
                        newEntry->dest_ip = inet_addr(dest_ip);
                        newEntry->dest_port = htons(atoi(dest_port));
                        newEntry->source_port = client_addr.sin_port;
                        newEntry->source_ip = client_addr.sin_addr.s_addr;
                        newEntry->data_port = temp_addr.sin_port;
                        newEntry->data_ip = temp_addr.sin_addr.s_addr;
                        newEntry->next = routing_table;
                        routing_table = newEntry;
                        /*********************************/
                        //printf("Entry added to routing table\n");
                        /**********************************/
                        
                        print_details(newEntry, "Entry added to Routing Table");
                        
                        // send a confirmation to sender
                        memset(write_buffer,'\0',MAX_BUF);
                        snprintf(write_buffer,MAX_BUF-1,"%s%s%s%d%s", "$$", current_router_ip,"$",ntohs(temp_addr.sin_port), "$");
                        // change client_addr to point to server_port
                        client_addr.sin_port = server_addr.sin_port;
                        // send the confirmation to sender with new port and client's requested server IP 
                        if (sendto(sd_server,write_buffer, strlen(write_buffer),0,(struct sockaddr*) &client_addr, addr_len)==-1){
                            printf("Error sending payload to client");
                            exit(1);
                        }
                        /*********************/
                        //printf("Confirmation sent to %s:%d %s\n", inet_ntoa(client_addr.sin_addr),ntohs(client_addr.sin_port),write_buffer);
                        /*********************/

                    }
                    // if an intermediate request
                    else {
                        // get IP of next router from payload
                        j=0;
                        char next_router_ip[20];
                        memset(next_router_ip,'\0',20);
                        i=strlen(read_buffer)-1;
                        if (read_buffer[i]=='$'){
                            i--;
                        }
                        for(;read_buffer[i]!='$';i--);
                        int pointer = i;                    
                        for(i=i+1;read_buffer[i]!='$';i++){
                            next_router_ip[j++]=read_buffer[i];
                        }
                        /*********************/
                        //printf("Intermediate request with next hop as %s\n", next_router_ip);
                        /*********************/


                        // create next router's address
                        struct sockaddr_in next_router_addr;
                        bzero((char*) &next_router_addr, sizeof(next_router_addr));
                        next_router_addr.sin_family = AF_INET;
                        next_router_addr.sin_port = server_addr.sin_port;
                        next_router_addr.sin_addr.s_addr = inet_addr(next_router_ip);

                        // send request to next router 
                        if (sendto(temp_socket,read_buffer,strlen(read_buffer),0,(struct sockaddr*) &next_router_addr, sizeof(next_router_addr))==-1){
                            printf("Error sending payload to next router %s\n", strerror(errno));
                            exit(1);
                        } 
                        /*********************/
                        //printf("Request forwarded to next hop %s\n", read_buffer);
                        /*********************/


                        // add the source->dest mapping in delta list
                        // note time for this entry
                        clientInfo* newEntry = (clientInfo*)malloc(1*sizeof(clientInfo));
                        if (newEntry==NULL){
                            printf("malloc unsuccessful while adding to temp_table\n");
                        }
                        newEntry->fd_client = temp_socket;
                        newEntry->dest_ip = inet_addr(next_router_ip);
                        newEntry->dest_port = 0;
                        newEntry->source_port = client_addr.sin_port;
                        newEntry->source_ip = client_addr.sin_addr.s_addr;
                        newEntry->data_port = temp_addr.sin_port;
                        newEntry->data_ip = temp_addr.sin_addr.s_addr;
                        newEntry->next = NULL;
                         
                        print_details(newEntry, "Entry added to Temporary Table");
                        
                        // push new entry to delta list
                        enqueue(newEntry);
                        if(temp_table->size==1){
                            //printf("Alarm set\n");
                            alarm(30);
                            //alarm(5);
                        }
                        /*********************/
                        /*len=0;
                        for(temp_d=temp_table->head;temp_d!=NULL;temp_d=temp_d->next) len++;
                        printf("Entry added to temp table. size of temp_table %d\n", len);*/
                        /*********************/
                    }
                }// format=1
                else if (format==2){
                    // get router IP and port from payload
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
                    /*********************/
                    //printf("Confirmation from %s:%s\n", next_router_ip, next_router_port);
                    /*********************/


                    // confirm the entry
                    // move entry with this IP and port as destination to Routing Table
                    delta_node* prev=NULL;
                    for (temp_d=temp_table->head;temp_d!=NULL;temp_d=temp_d->next){
                        if (temp_d->cInfo->dest_ip==inet_addr(next_router_ip) && temp_d->cInfo->dest_port==htons(atoi(next_router_port))){
                            break;
                        }
                        prev=temp_d;
                    }
                    if (temp_d!=NULL){
                        if(prev==NULL){
                            //printf("Entry in temp table.Alarm 0\n");
                            alarm(0);
                        }
                        /*********************/
                        //printf("Entry present in temp_table\n");
                        /*********************/
                    }
                    /*************************/
                    /*len=0;
                    clientInfo* t;
                    for(t=temp_table;t!=NULL;t=t->next) len++;
                    printf("len of temp_table %d\n", len);*/
                    /**************************/
                        
                    // add src->dest to routing table
                    clientInfo* newEntry = (clientInfo*)malloc(1*sizeof(clientInfo));
                    if (newEntry==NULL){
                        printf("process ran out of memory\n");
                    }
                    else {
                        //printf("newEntry malloc successful\n");
                    }
                    temp = temp_d->cInfo;
                    
                    newEntry->fd_client = temp->fd_client;
                    newEntry->dest_ip = temp->source_ip;
                    newEntry->dest_port = temp->source_port;
                    newEntry->source_port = temp->dest_port;
                    newEntry->source_ip = temp->dest_ip;
                    newEntry->data_port = temp->data_port;
                    newEntry->data_ip = temp->data_ip;
                    newEntry->next = routing_table;
                    routing_table = newEntry;
                    /*********************/
                    //printf("Reverse Entry added to routing table\n", read_buffer);
                    /*********************/

                    // add dest->src to routing table
                    clientInfo* newEntry_reverse = (clientInfo*)malloc(1*sizeof(clientInfo));
                    if (newEntry==NULL){
                        printf("process ran out of memory\n");
                    }
                    else {
                        //printf("newEntry_Reverse malloc successful\n");
                    }
                    newEntry_reverse->fd_client = temp->fd_client;
                    newEntry_reverse->dest_ip = temp->dest_ip;
                    newEntry_reverse->dest_port = temp->dest_port;
                    newEntry_reverse->source_port = temp->source_port;
                    newEntry_reverse->source_ip = temp->source_ip;
                    newEntry_reverse->data_port = temp->data_port;
                    newEntry_reverse->data_ip = temp->data_ip;
                    newEntry_reverse->next = routing_table;
                    routing_table = newEntry_reverse;
                    /*********************/
                    /*printf("Entry added to routing table\n", read_buffer);
                    len=0;
                    clientInfo* t;
                    for(t=routing_table;t!=NULL;t=t->next){
                        len++;
                    }
                    printf("length of routing table %d\n", len);*/
                    /*********************/
  
                    print_details(newEntry_reverse, "Entry moved to Routing Table");
                    print_details(newEntry, "Entry moved to Routing Table");
                    
                    // send confirmation to previous hop
                    // create address for previous hop
                    struct sockaddr_in prev_router_addr;
                    bzero((char*) &prev_router_addr, sizeof(prev_router_addr));
                    prev_router_addr.sin_family = AF_INET;
                    prev_router_addr.sin_port = server_addr.sin_port;
                    prev_router_addr.sin_addr.s_addr = temp->source_ip;
                    
                    // create confirmation payload
                    struct in_addr current_addr;
                    current_addr.s_addr = temp->data_ip;
                    memset(write_buffer,'\0',MAX_BUF);
                    snprintf(write_buffer,MAX_BUF-1,"%s%s%s%d%s", "$$", inet_ntoa(current_addr),"$",ntohs(temp->data_port), "$");
                    // send the confirmation to sender with new port and current server IP 
                    if(sendto(sd_server,write_buffer, strlen(write_buffer),0,(struct sockaddr*) &prev_router_addr, sizeof(prev_router_addr))==-1){
                        printf("Error sending payload to previous router");
                        exit(1);
                    }
                    /*********************/
                    //printf("Confirmation sent to prev hop %s at %s:%d\n", write_buffer, inet_ntoa(prev_router_addr.sin_addr), ntohs(prev_router_addr.sin_port));
                    /*********************/
                    FD_CLR(temp->fd_client,&read_fd_set);
                    
                    // remove entry from temp table
                    // remove timer
                    if (prev==NULL){
                        if (temp_d->next!=NULL){
                            struct timeval time_of_entry = temp_d->next->timestamp;
                            struct timeval current;
                            gettimeofday(&current,NULL);
                            unsigned int seconds = 30.0 - ((float)(current.tv_sec-time_of_entry.tv_sec)+(float)(current.tv_usec-time_of_entry.tv_usec)/1000000.0);
                            alarm(seconds);
                            //printf("alarm placed for %d seconds\n", seconds);
                        }
                        dequeue();
                    }
                    else{
                        prev->next=temp_d->next;
                        if(temp_d->next==NULL){
                            temp_table->tail=prev;
                        }
                        temp_table->size--;
                        free(temp_d);
                    }
                    
                }// format=2
            }//fd_isset server

            // fd_isset for routing_table - data
            for(temp=routing_table;temp!=NULL;temp=temp->next){
                if(FD_ISSET(temp->fd_client,&read_fd_set)){
                   /****************************/
                   //printf("Incoming request to routing_table\n");
                   /**************************/
                   memset(read_buffer,'\0',MAX_BUF);
                    // receive payload from receiver
                    if((bytes=recvfrom(temp->fd_client,read_buffer,MAX_BUF,0,(struct sockaddr*)&client_addr, &addr_len))==-1){
                        printf("Error receiving data from client");
                        exit(1);
                    }
                    /*********************/
                    //printf("Payload received form client %s", read_buffer);
                    /*********************/


                    // construct address of client to send packet to
                    struct sockaddr_in addr;
                    if(client_addr.sin_addr.s_addr==temp->source_ip && client_addr.sin_port == temp->source_port){
                        addr.sin_family = AF_INET;
                        addr.sin_port = temp->dest_port;
                        addr.sin_addr.s_addr = temp->dest_ip;
                    }
                    else if(client_addr.sin_addr.s_addr==temp->dest_ip && client_addr.sin_port == temp->dest_port){
                        addr.sin_family=AF_INET;
                        addr.sin_port = temp->source_port;
                        addr.sin_addr.s_addr = temp->source_ip;
                    }
                    else{
                        continue;
                    }

                    // forward data to the next hop
                    if(sendto(temp->fd_client,read_buffer,strlen(read_buffer),0,(struct sockaddr*) &addr, sizeof(addr))==-1){
                        printf("Error sending to server from client\n");
                        exit(1);
                    }
                    /*********************/
                    //printf("Payload forwarded to next hop %s:%d\n", inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
                    /*********************/

                    FD_CLR(temp->fd_client,&read_fd_set);

                }
            }                              
        } // response>0
        else if(response==-1 && errno==EINTR){
            // handle timer event
            // set select timer to new value as per next entry's timeout
            /*******************************/
            //printf("select call interrupted\n");
            /*******************************/
            if(temp_table->size>0){
                struct timeval time_of_entry = temp_table->head->timestamp;
                struct timeval current;
                gettimeofday(&current,NULL);
                unsigned int seconds = 30.0 - ((float)(current.tv_sec-time_of_entry.tv_sec)+(float)(current.tv_usec-time_of_entry.tv_usec)/1000000.0);
                alarm(seconds);
            }
            continue;
        }
        else {
            printf("Error in select() command %s\n", strerror(errno));
            exit(1);
        }
    }//while(1)
    
    freeifaddrs(ifap);
    return 0;
} 
