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
#include <sys/ioctl.h>
#include <limits.h>
#include <math.h>
#include <semaphore.h>

#define MAX_BUF 1024

#define SHORT "/u/data/u3/park/pub/pp.au"
#define LONG "/u/data/u3/park/pub/kline-jarrett.au"

char* log_buf;
size_t size;
FILE *in_stream,*out_stream,*logfile;
char entry[100];
char log_filename[50];

typedef struct node_s{
    char* array;
}packet;

typedef struct circularBuf{
    packet* packets;
    int readPosition;
    int writePosition;
    sem_t validItems;
}circBuf;

circBuf audio_buffer;
int arr_size;

int audio_fd;
int udp_client_socket;
struct sockaddr_in udp_server_addr;

int buf_sz, target_buf;
double gama, mu;
int payload_size;

struct timeval start;

void sigPollHandler(int sig){
    int pid = (int)getpid();
    int bytes,seq_num;
    char read_buffer[payload_size+5], write_buffer[500];
    struct sockaddr_in server_addr;
    int addr_len = sizeof(server_addr);
    
    struct timeval timestamp;
    gettimeofday(&timestamp,NULL);
    
    bzero(&server_addr,addr_len);
    memset(read_buffer,'\0',payload_size+5);
    // recvfrom server
    if((bytes = recvfrom(udp_client_socket,read_buffer,payload_size+5,0,(struct sockaddr*) &server_addr, &addr_len))==-1){
        printf("Error in recvfrom server %s",strerror(errno));
        exit(1);
    }
    if(bytes==0){
        return;
    }
    
    // parse buffer and write to audio_buffer
    int i = 0;
    char temp[5];
    memset(temp,'\0',5);
    strncpy(temp,&read_buffer[0],4);
    if (temp!='\0')
        sscanf(temp, "%d", &seq_num);
   
    int limit;
    int ret = sem_getvalue(&audio_buffer.validItems,&limit);
    if (limit > arr_size-1){
        //printf("Buffer is full. limit is %d\n", limit);
        return;
    }
    memset(audio_buffer.packets[audio_buffer.writePosition].array,'\0',payload_size+1);
    strncpy(audio_buffer.packets[audio_buffer.writePosition].array,&read_buffer[4],payload_size+1);
    /***************************************/
    //printf("Write Buffer - read_buffer %s len %d audio_buffer %s\n", &read_buffer[4], strlen(&read_buffer[4]), audio_buffer.packets[audio_buffer.writePosition].array);
    /***************************************/
    audio_buffer.writePosition = (audio_buffer.writePosition+1)%arr_size;
    sem_post(&audio_buffer.validItems);
    ret = sem_getvalue(&audio_buffer.validItems,&limit);
    /***********************************/
    //printf("%d Writing -  writePostiion %d readPosition %d current_size %d\n", pid, audio_buffer.writePosition, audio_buffer.readPosition, limit);
    /*********************************/
    if(fprintf(in_stream,"%lf%s%d\n",(float)(timestamp.tv_sec-start.tv_sec)+(float)(timestamp.tv_usec-start.tv_usec)/1000000.0,";", limit*250)<0){
        printf("Error in fprintf to in_stream %s",strerror(errno));
        exit(1);
    }

    // send feedback to audio server
    memset(write_buffer,'\0',500);
    snprintf(write_buffer, 500, "%s%s%d%s%d%s%lf", "Q", " ", limit*250, " " , target_buf, " ", gama*1000);
    if(sendto(udp_client_socket, write_buffer, strlen(write_buffer), 0, (struct sockaddr*)&udp_server_addr, sizeof(udp_server_addr))==-1){
        printf("Error sending feedback to server %s\n", strerror(errno));
        exit(1);
    }
    /*********************************/
    //printf("%d Feedback sent to client - ========= %s ============\n",pid,write_buffer);
    /**********************************/
}


int main(int argc, char** argv){
    int tcp_client_socket;
    struct sigaction sigPoll, sigAlrm;
    char read_buffer[MAX_BUF], write_buffer[MAX_BUF];
    int udp_client_port, udp_server_port;
    struct sockaddr_in tcp_server_addr;
    struct sockaddr_in udp_client_addr;
    double playback_del;

    // check for format of input arguments
    if (argc<11){
        printf("Format is audiolisten server-ip server-tcp-port client-udp-port payload-size playback_del gamma buf-sz target-buf logfile-c filename");
        exit(1);
    }
    udp_client_port = atoi(argv[3]);
    gama = atof(argv[6]);
    mu = 1/gama;
    buf_sz = atoi(argv[7]);
    target_buf = atoi(argv[8]);
    playback_del = atof(argv[5]);
    payload_size = atoi(argv[4]);
    strcpy(log_filename,argv[9]);

    /******************************/
    //printf("mu = %d\n",(int)mu);
    /******************************/
    
    // initilize instrem for logging
    if ((in_stream = open_memstream(&log_buf,&size))==NULL){
        printf("Error opening strem for open_memstream %s", strerror(errno));
        exit(1);
    }

    //initialize audio buffer
    arr_size = buf_sz/payload_size;
    audio_buffer.packets = (packet*)malloc(arr_size*sizeof(packet));
    int k;
    for(k=0;k<arr_size;k++){
        audio_buffer.packets[k].array = (char*)malloc(sizeof(char)*(payload_size+1));
        memset(audio_buffer.packets[k].array,'\0',payload_size+1);
    }
    audio_buffer.writePosition=0;
    audio_buffer.readPosition=0;
    //initialize semaphore
    if (sem_init(&(audio_buffer.validItems), 0, 0) == -1){
        printf("Error initializing semaphore\n");
        exit(1);
    }

    // open /dev/audio
    if((audio_fd=open("/dev/audio",O_WRONLY))==-1){
        printf("Error opening dev audio file %s", strerror(errno));
        exit(1);
    }

    // set signal handler for SIGPOLL with sigaction
    sigPoll.sa_handler = sigPollHandler;
    if(sigfillset(&sigPoll.sa_mask) < 0){
        printf("Error initializing signal handler");
        exit(1);
    }
    sigPoll.sa_flags = 0;
    if(sigaction(SIGPOLL, &sigPoll, 0) < 0) {
        printf("Error in handler call for SIGPOLL failed");
        exit(1);
    }
    /******************************/
    //printf("created signal handlers\n");
    /*******************************/

    // create tcp client socket
    if ((tcp_client_socket=socket(AF_INET,SOCK_STREAM,0))==-1){
        printf("Client: Error creating client socket");
        exit(1);
    }
    /******************************/
    //printf("tcp socket created\n");
    /*******************************/

    // initialize tcp connection address
    bzero(&tcp_server_addr, sizeof(tcp_server_addr));
    tcp_server_addr.sin_family = AF_INET;
    tcp_server_addr.sin_port = htons(atoi(argv[2]));
    tcp_server_addr.sin_addr.s_addr = inet_addr(argv[1]);

    // connect to server
    if(connect(tcp_client_socket, (struct sockaddr*)&tcp_server_addr, sizeof(tcp_server_addr))==-1){
        printf("Error connecting to server - %s", strerror(errno));
        exit(1);
    }
    /******************************/
    //printf("connect to tcp server on %s:%d\n", inet_ntoa(tcp_server_addr.sin_addr), ntohs(tcp_server_addr.sin_port));
    /*******************************/

    memset(write_buffer, '\0',MAX_BUF);
    memset(read_buffer, '\0', MAX_BUF);
    // create client request 
    if (strcmp(argv[10],"short")==0){
        snprintf(write_buffer, MAX_BUF-1, "%s%s%s", argv[3], " ", SHORT);
    }
    else if(strcmp(argv[10],"long")==0){
        snprintf(write_buffer, MAX_BUF-1,"%s%s%s", argv[3], " ", LONG);
    }
    else {
        snprintf(write_buffer, MAX_BUF-1,"%s%s%s", argv[3], " ", argv[10]);
    }
    /******************************/
    //printf("write_buffer %s\n", write_buffer);
    /*******************************/

    // write request to client socket
    if (write(tcp_client_socket,write_buffer,strlen(write_buffer))==-1){
        printf("Error in sending tcp request to server");
        exit(1);
    }
    /******************************/
    //printf("request sent to tcp server\n");
    /*******************************/

    // read response from server
    if(read(tcp_client_socket,read_buffer,MAX_BUF)==-1){
        printf("Error in reading response from tcp socket");
        exit(1);
    }
    /******************************/
    //printf("response from server %s\n",read_buffer);
    /*******************************/
   
    //parse response from server
    if (strcmp(read_buffer,"KO")==0){
        printf("file is not available with the server");
        exit(1);
    }
    char temp[3], server_port[10];
    int i=0,j=0;
    while(read_buffer[i]!=' '){
        temp[j++] = read_buffer[i++];
    }
    temp[j]='\0';
    /******************************/
    //printf("OK or KO - %s\n",temp);
    /*******************************/

   if (strcmp(temp,"OK")!=0){
        printf("Did not receive appropriate response from server - %s\n", read_buffer);
        exit(1);
    }
    if(read_buffer[i]==' ')
        i++;
    j=0;
    // get udp port from server response
    while(read_buffer[i]!='\0'){
        server_port[j++]=read_buffer[i++];
    }
    server_port[j]='\0';
    udp_server_port = atoi(server_port);
    /******************************/
    //printf("port returned by server - %d\n",udp_server_port);
    /*******************************/

    if(close(tcp_client_socket)==-1){
        printf("Error closing tcp client socket");
        exit(1);
    }
    /******************************/
    //printf("client tcp socket closed\n");
    /*******************************/

    // create client socket for udp transmission
    if ((udp_client_socket = socket(AF_INET,SOCK_DGRAM,0))==-1){
        printf("Error creating client socket\n");
        exit(1);
    }
    /******************************/
    //printf("client udp socket created\n");
    /*******************************/

    // initialize udp server address for specific client
    udp_client_addr.sin_family = AF_INET;
    udp_client_addr.sin_port = htons(udp_client_port);
    udp_client_addr.sin_addr.s_addr = INADDR_ANY; 
    /******************************/
    //printf("udp address initialized %s\n", inet_ntoa(udp_client_addr.sin_addr));
    /*******************************/

    // initialize udp client address for specific client
    udp_server_addr.sin_family = AF_INET;
    udp_server_addr.sin_port = htons(udp_server_port);
    udp_server_addr.sin_addr.s_addr = inet_addr(argv[1]);        

    //bind client socket for udp transmission
    if (bind(udp_client_socket,(struct sockaddr *)&udp_client_addr,sizeof(udp_client_addr)) ==-1){
        printf("Error binding to client address %s\n", strerror(errno));
        exit(1);
    }
    /******************************/
    //printf("udp address bound client\n");
    /*******************************/

    gettimeofday(&start,NULL);

    // register socket to trap interrupt SIGPOLL
    if (fcntl(udp_client_socket, F_SETOWN, getpid())<0){
        printf("Error in fcntl %s", strerror(errno));
        exit(1);
    }
    // make socket aysynchronous and non-blocking
    int flag = fcntl(udp_client_socket, F_GETFL);
    flag |= O_NONBLOCK;
    if(fcntl(udp_client_socket, F_SETFL, flag)==-1){
        printf("fcntl: Error setting non blocking control %s", strerror(errno));
        exit(1);
    }
    int on = 1;
    // set socket to be asynchronous
    if (ioctl(udp_client_socket,FIOASYNC,&on)==-1){
       printf("ioctl: Error setting Asynchronous control %s", strerror(errno));
       exit(1);
    }
    /******************************/
    //printf("made udp non blocking and asynchronous\n");
    /*******************************/

    // induce initial delay of playback_del seconds    
    double mseconds, seconds;
    mseconds = modf(playback_del,&seconds); 
    struct timespec time_s, time_r;
    time_s.tv_sec = (int)seconds;
    time_s.tv_nsec = (int)(mseconds*1000000);
    while(nanosleep(&time_s,&time_r)==-1){
        if(errno==EINTR){
            time_s.tv_sec = time_r.tv_sec;
            time_s.tv_nsec = time_r.tv_nsec;
            /***************************************/
            //printf("Time remaining for sleep %ld\n", time_r.tv_nsec);
            /***************************************/
        }
        else {
            printf("Error in putting code to sleep");
            exit(1);
        }
    }
    /******************************/
    //printf("alarm sounded\n");
    /*******************************/

    // write packets to /dev/audio with delay mu
    int limit;
    while(1) {
        int bytes;
        while(sem_wait(&audio_buffer.validItems)==-1 && errno==EINTR);
        if((bytes=write(audio_fd,audio_buffer.packets[audio_buffer.readPosition].array,strlen(audio_buffer.packets[audio_buffer.readPosition].array)))==-1){
            printf("Error writing to /dev/audio file %s\n", strerror(errno));
            exit(1);
        }
        if(bytes==0){
            break;
        }
        /***************************************/
        //printf("Read Buffer -  %s len - %d bytes - %d\n", audio_buffer.packets[audio_buffer.readPosition].array, strlen(audio_buffer.packets[audio_buffer.readPosition].array), bytes);
        /***************************************/
        audio_buffer.readPosition = (audio_buffer.readPosition+1)%arr_size;
        int ret = sem_getvalue(&audio_buffer.validItems,&limit);
        
        // log lamda and timestamp
        struct timeval timestamp;
        gettimeofday(&timestamp,NULL);
        if(fprintf(in_stream,"%lf%s%d\n",(float)(timestamp.tv_sec-start.tv_sec)+(float)(timestamp.tv_usec-start.tv_usec)/1000000.0,";",limit*250)<0){
            printf("Error in fprintf to in_stream %s",strerror(errno));
            exit(1);
        }

        //printf("%d Reading -  writePostiion %d readPosition %d current_size %d\n", (int)getpid(), audio_buffer.writePosition, audio_buffer.readPosition, limit);
        /************************************/

        // sleep before conusming packets
        struct timespec time_s, time_r;
        time_s.tv_sec = 0;
        time_s.tv_nsec = (int)(mu*1000000);
        while(nanosleep(&time_s,&time_r)==-1){
            if(errno==EINTR){
                time_s.tv_sec = time_r.tv_sec;
                time_s.tv_nsec = time_r.tv_nsec;
                continue;
            }
            /***************************************/
            //printf("Time remaining for sleep %d %ld\n", time_r.tv_sec, time_r.tv_nsec);
            /***************************************/

        }
        /**********************************/
        //printf("Sleep complete\n");
        /**********************************/

    }

    // close fd and socket
    if(close(audio_fd)==-1){
        printf("Error closing /dev/audio");
        exit(1);
    }
    /******************************/
    //printf("/dev/audio closed\n");
    /*******************************/

    if(close(udp_client_socket)==-1){
        printf("Error closing client socket");
        exit(1);
    }
    /******************************/
    //printf("client socket closed\n");
    /*******************************/

    // write logs to logfile
    if (fclose(in_stream)==-1){
        printf("Error closing in_stream %s\n", strerror(errno));
        exit(1);
    }
    char l_filename[50];
    memset(l_filename,'\0',50);
    snprintf(l_filename,50,"%s%c%d",log_filename,'_',(int)getpid());
    if((logfile = fopen(l_filename,"w"))==NULL){
        printf("Error opening logfile %s\n", strerror(errno));
        exit(1);
    }
    if ((out_stream = fmemopen(log_buf,strlen(log_buf),"r"))==NULL){
        printf("error opening out_stream %s\n", strerror(errno));
        exit(1);
    }
    while(fgets(entry,100,out_stream)!=NULL){
        fputs(entry,logfile);
    }
    if(fclose(out_stream)==-1){
        printf("error closing out_stream %s\n", strerror(errno));
        exit(1);
    }
    if(fclose(logfile)==-1){
        printf("Error closing logfile %s", strerror(errno));
        exit(1);
    }
    return 0;
}




