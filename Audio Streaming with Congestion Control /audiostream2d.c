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

#define MAX_BUF 1024

char* log_buf;
size_t size;
FILE* in_stream,*out_stream,*logfile;
char entry[100];
char log_filename[50];

int mode;
volatile double packet_spacing;
int sock_fd;

typedef struct packet_s{
    char* array;
}packet;

int audiobuf;
packet* stash;

int payload_size; 
struct timeval start;

// sigchld handler
void sigchldHandler(int sig){
    while(waitpid(-1,sig,WNOHANG)>0);
    return;
}

// sigpoll handler
void sigPollHandler(int sig){
    //int pid = (int)getpid();
    char read_buffer[MAX_BUF];
    int bytes;
    struct sockaddr_in client_addr;
    int addr_len = sizeof(client_addr);
    
    struct timeval timestamp;
    gettimeofday(&timestamp,NULL);
      
    // set buffers to null
    bzero(&client_addr,addr_len);
    memset(read_buffer,'\0',MAX_BUF);

    while(1){
        // recv feedback from udp port for the client
        bytes = recvfrom(sock_fd,read_buffer,MAX_BUF,0,(struct sockaddr*) &client_addr, &addr_len);
        if(bytes==-1){
            if(errno==EWOULDBLOCK){
                /***********************/
                //printf("In here================\n");
                return;
            }
            else{
                printf("Error in recvfrom client %s", strerror(errno));
                exit(1);
            }
        }
        if(bytes==0){
            return;
        }
        /*******************************************/
        //printf("Feedback %s\n", read_buffer);
        /********************************************/

        // if negative ack from client
        if (read_buffer[0]=='M'){
            /************************/
            //printf("Retransmission resuest %s\n",read_buffer);
            int seq_num,index,seq_stash;
            char temp[5];
            memset(temp,'\0',5);
            sscanf(&(read_buffer[1]),"%d",&seq_num);
            index = seq_num%audiobuf;
            strncpy(temp,stash[index].array,4);
            /***********************/
            //printf("Retransmit stash - %d %s %s\n", seq_num, stash[index].array, temp);
            /***********************/
            sscanf(temp,"%d",&seq_stash); 
            /***********************/
            //printf("seq_num - %d seq_stash - %d\n", seq_num, seq_stash);
            if(seq_num==seq_stash){
                if(sendto(sock_fd,stash[index].array,payload_size+5,0,(struct sockaddr*)&client_addr, sizeof(client_addr))==-1){
                    printf("Error retransmitting packet to client %s",strerror(errno));
                    exit(1);
                }
                /************************************/
                //printf("Retransmitted %d\n",seq_num);
                /************************************/
            }
        }      
        // congestion control    
        else if(read_buffer[0]=='Q'){
           // get lambda for the particular client
            int q,q_star;
            double gama,lambda;
            lambda = 1000/packet_spacing;
            //Parse client's request
            char temp[MAX_BUF];
            memset(temp,'\0',MAX_BUF);
            int i=0,j=0;
            // get q, q_star and gaama from client request
            if (read_buffer[i]=='Q')
                i++;
            if (read_buffer[i]==' ')
                i++;
            while(read_buffer[i]!=' '){
                temp[j++]=read_buffer[i++];
            }
            sscanf(temp, "%d", &q);
            if(read_buffer[i]==' ')
                i++;
            j=0;
            memset(temp,'\0',MAX_BUF);
            while(read_buffer[i]!=' '){
                temp[j++]=read_buffer[i++];
            }
            sscanf(temp, "%d", &q_star);
            if(read_buffer[i]==' ')
                i++;
            j=0;
            memset(temp,'\0',MAX_BUF);
            while(read_buffer[i]!='\0'){
                temp[j++]=read_buffer[i++];
            }
            sscanf(temp, "%lf", &gama);
            
            // calculate lambda based on method specified by user
            if (mode==0){
                // method A : lambda = lambda+/-a
                if (q>q_star) lambda=lambda-3.0;
                else if (q<q_star) lambda=lambda+3.0;
                /****************************************/
                //printf("%d Variables q - %d q_star - %d gama - %lf lambda - %lf", pid, q, q_star,gama,lambda);
                /****************************************/
            }
            else if(mode==1){
                // method B: lambda = lambda*delta
                if (q>q_star) lambda=0.75*lambda;
                else if (q<q_star)lambda=lambda+1.0;
                /****************************************/
                //printf("%d Variables q - %d q_star - %d gama - %lf\n", pid, q, q_star,gama);
                /****************************************/
            }
            else if(mode==2){
                // method C: lambda = lambda + e*(q_star-q)
                if(q!=q_star) lambda = lambda+0.0001*(q_star-q);
                /****************************************/
                //printf("%d Variables q - %d q_star - %d gama - %lf\n",pid, q, q_star,gama);
                /****************************************/
            }
            else if(mode==3){
                // method D: lambda = lambda + e*(q_star-q) - b*(lambda-gamma)
                if (q!=q_star) lambda = lambda+0.001*(q_star-q)-1.0*(lambda-gama);
                /****************************************/
                //printf("%d Variables q - %d q_star - %d gama - %lf\n", pid, q, q_star,gama);
                /****************************************/
            }

            // if lambda>0, update packet spacing of client
            // else set it to infinity i.e do not send any packet till next feedback is received
            if (lambda>0)
                packet_spacing = 1000/lambda;    
            else 
                packet_spacing = packet_spacing;
            
            // log lamda and timestamp
            if(fprintf(in_stream,"%lf\t%lf\n",lambda,(float)(timestamp.tv_sec-start.tv_sec)+(float)(timestamp.tv_usec-start.tv_usec)/1000000.0)<0){
                printf("Error in fprintf to in_stream %s",strerror(errno));
                exit(1);
            }
            //printf("Lambda - %lf packet_spacing - %lf\n", lambda, packet_spacing);
        }
    }
}

// main method that sends audio file to the client
// sets up a tcp connection with client
// forks a child to create a udp connection 
// and transfer the audio file
// interrupt raised for incoming feedback from child process
int main(int argc, char** argv){
    int sd_server, sd_client, udp_port;
    char read_buffer[MAX_BUF], write_buffer[MAX_BUF];
    struct sockaddr_in server_addr, client_addr;

    // check for format of input arguments
    if (argc<8){
        printf("Format is audiostreamd tcp-port udp-port payload-size packet-spacing mode logfile-s audiobuf");
        exit(1);
    }
    udp_port = atoi(argv[2]);
    payload_size = atoi(argv[3]);
    packet_spacing = atof(argv[4]);
    mode = atoi(argv[5]);
    strcpy(log_filename,argv[6]);
    audiobuf = atoi(argv[7]);

   // initialize connection addresses for both peers
    bzero((char*) &server_addr, sizeof(server_addr));
    bzero((char*) &client_addr, sizeof(client_addr));
    int server_len = sizeof(server_addr), client_len = sizeof(client_addr);
  
    //Create server socket
    if ((sd_server = socket(AF_INET,SOCK_STREAM,0))==-1){
        printf("SERVER:Error creating server socket");
        exit(1);
    }
    /******************************/
    //printf("server tcp socket created\n");
    /********************************/
    
    // initialize server_address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = INADDR_ANY; 

    // bind socket to server address
    if (bind(sd_server, (struct sockaddr *) &server_addr,sizeof(server_addr)) ==-1){
        printf("SERVER:Error binding to server address %s\n", strerror(errno));
        exit(1);
    }
    /******************************/
    //printf("server tcp socket bound\n");
    /********************************/
 
    // listen on server side socket
    if (listen(sd_server, 5)==-1){
        printf("Error in listen call");
        exit(1);
    }
    /******************************/
    //printf("server tcp socket listening\n");
    /********************************/
 
    // register SIGCHLD handler with parent process
    // Enables parent to process any child exits and prevent zombies
    signal(SIGCHLD,sigchldHandler);

    // main logic of transferring audio files
    while(1) {
        memset(read_buffer,'\0',MAX_BUF);
        memset(write_buffer,'\0', MAX_BUF);
        bzero(&client_addr, sizeof(client_addr));

        // accept client request and forms full associations
        if ((sd_client=accept(sd_server, (struct sockaddr *) &client_addr, &client_len))==-1){
            printf("Failed to accept connection");
            exit(1);
        }
        /******************************/
        //printf("server tcp socket connection accepted %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        /********************************/
 
        //Read request from client socket
        if (read(sd_client,read_buffer,MAX_BUF)==-1){
            printf("Error reading from server socket");
            exit(1);
        }
        /******************************/
        //printf("Client request %s\n", read_buffer);
        /********************************/
        
        //Parse client's request
        char client_port[10], filename[MAX_BUF];
        memset(filename,'\0', MAX_BUF);
        memset(client_port,'\0', 10);
        int i=0,j=0;
        // get udp port and filename from client request
        while(read_buffer[i]!=' '){
            client_port[j++]=read_buffer[i++];
        }
        client_port[j]='\0';
        if(read_buffer[i]==' ')
            i++;
        j=0;
        // get filename from client request
        while(read_buffer[i]!='\0'){
            filename[j++]=read_buffer[i++];
        }
        filename[j]='\0';
        /******************************/
        //printf("Client request %s %s\n", client_port, filename);
        /********************************/
  
        // check if the requested file exists
        // if it doesn't, send KO to the client and continue to accept()
        if (access(filename,F_OK)==-1){
            snprintf(write_buffer,MAX_BUF,"%s","KO");
            if(write(sd_client,write_buffer,strlen(write_buffer))==-1){
                printf("Error writing to file %s\n", strerror(errno));
                exit(1);
            }
            if (close(sd_client)==-1){
                printf("Parent - Error closing tcp client socket\n");
                exit(1);
            }   
            /******************************/
            //printf("Server response %s\nClient socket closed\n", write_buffer);
            /********************************/
            continue;
        }
        
        // if file exists
        // send OK with the udp_port to client for audio transmission
        snprintf(write_buffer,MAX_BUF,"%s%s%d", "OK", " ", udp_port);
        if(write(sd_client,write_buffer,strlen(write_buffer))==-1){
             printf("Error writing to file %s\n", strerror(errno));
             exit(1);
        }
        /******************************/
        //printf("Server response %s\n", write_buffer);
        /********************************/

        // Fork the child process 
        pid_t pid = fork();
       
        // if child process
        if (pid==0) {
            int pid = (int)getpid();
            int seq_num = 0, fd;
            struct sockaddr_in udp_addr, client_udp_addr;
            
            // initialize stash for keeping last audiobuf number of packets in memory
            stash = (packet*)malloc(audiobuf*sizeof(packet));
            int k=0;
            for(;k<audiobuf;k++){
                stash[k].array = (char*)malloc((payload_size+5)*sizeof(char));
                memset(stash[k].array,'\0',payload_size+5);
            }    
    
            if ((in_stream = open_memstream(&log_buf,&size))==NULL){
                printf("Error opening strem for open_memstream %s", strerror(errno));
                exit(1);
            }
            bzero(&(udp_addr),sizeof(udp_addr));
            bzero(&(client_udp_addr),sizeof(client_udp_addr));

            // initialize udp server address for specific client
            udp_addr.sin_family = AF_INET;
            udp_addr.sin_port = htons(udp_port);
            udp_addr.sin_addr.s_addr = server_addr.sin_addr.s_addr; 
            /******************************/
            //printf("%d Child: server udp address created %d\n", pid, udp_port);
            /********************************/

            // initialize udp client address for specific client
            client_udp_addr.sin_family = AF_INET;
            client_udp_addr.sin_port = htons(atoi(client_port));
            client_udp_addr.sin_addr.s_addr = client_addr.sin_addr.s_addr;   
            /******************************/
            //printf("%d Child: client udp address created %s %s\n", pid, inet_ntoa(client_udp_addr.sin_addr), client_port);
            /********************************/

            // create server socket for udp transmission
            if ((sock_fd = socket(AF_INET,SOCK_DGRAM,0))==-1){
                printf("Error creating client socket\n");
                exit(1);
            }
            /******************************/
            //printf("Child: udp socket created \n");
            /********************************/

           //bind server socket for udp transmission
            if (bind(sock_fd,(struct sockaddr *)&udp_addr,sizeof(udp_addr)) ==-1){
                printf("SERVER:Error binding to server address %s\n", strerror(errno));
                exit(1);
            }
             /******************************/
            //printf("Child: udp socket bound \n");
            /********************************/
 
           // set signal handler for SIGPOLL with sigaction
            struct sigaction sigPoll;
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
            //printf("Child: signal handler registered\n");
            /********************************/

           // register socket to trap interrupt SIGPOLL
           if (fcntl(sock_fd, F_SETOWN, getpid())<0){
               printf("Error in fcntl %s", strerror(errno));
               exit(1);
           }
           // make socket aysynchronous and non-blocking
           int flag = fcntl(sock_fd, F_GETFL);
           flag |= O_NONBLOCK;
           if(fcntl(sock_fd, F_SETFL, flag)==-1){
               printf("fcntl: Error setting non blocking control %s", strerror(errno));
               exit(1);
           }
           int on = 1;
           // set socket to be asynchronous
           if (ioctl(sock_fd,FIOASYNC,&on)==-1){
              printf("ioctl: Error setting Asynchronous control %s", strerror(errno));
              exit(1);
           }
           /*****************************/
           //printf("Child: socket made async and non blocking \n");
           /******************************/
           //printf("%d Child: fd - %d packet_spacig - %lf \n",pid, sock_fd, packet_spacing);
           /********************************/
      
           //initialize buffer of payload_size+5
           //4 bytes for sequence number
           //1 byte for null character
           char audio_buffer[payload_size+5], buffer[payload_size+1];

           // open the requested file for read
            if((fd=open(filename,O_RDONLY))==-1){
                printf("Error opening the file %s %s\n",filename,strerror(errno));
                exit(1);
            }
             /******************************/
            //printf("Child: opened file for read \n");
            /********************************/
        
           int bytes;  
           // read till EOF
           gettimeofday(&start,NULL);
           memset(audio_buffer,'\0',sizeof(audio_buffer));
           memset(buffer,'\0',sizeof(buffer));
           while((bytes=read(fd,buffer,payload_size))>0){
                // create the payload
                snprintf(audio_buffer,payload_size+5,"%04d%s",seq_num,buffer); 
                /******************************/
                //printf("Child: audio_buffer %s \n", audio_buffer);
                /********************************/

                seq_num = (seq_num+1)%10000;
                /******************************/
                //printf("%d Child: seq_number %d\n", pid,seq_num);
                /********************************/

                // send bytes to client on udp port
                if(sendto(sock_fd, audio_buffer, strlen(audio_buffer), 0, (struct sockaddr*)&client_udp_addr, sizeof(client_udp_addr))==-1){
                    printf("Child - Error writing to socket %s\n", strerror(errno));
                    exit(1);
                }
                /******************************/
                //printf("Child: packet sent to client %s\n",audio_buffer);
                /********************************/
                snprintf(stash[(seq_num-1)%audiobuf].array,payload_size+5,"%s",audio_buffer);
                /******************************/
                //printf("Child: packet stashed %d at %d\n",seq_num,seq_num%audiobuf);
                /********************************/

                // sleep for packet_spacing amount of time
                double mseconds=packet_spacing, seconds=0.0;
                if (packet_spacing/1000>=1.0){
                    mseconds = modf(packet_spacing/1000,&seconds); 
                }
                struct timespec time_s, time_r;
                time_s.tv_sec = (int)seconds;
                time_s.tv_nsec = (int)(mseconds*1000000);
                /**********************************/
                //printf("Time for sleep - seconds %lu msec %lu\n",time_s.tv_sec, time_s.tv_nsec);
                /***************************************/
                while(nanosleep(&time_s,&time_r)==-1){
                    if(errno==EINTR){
                        time_s.tv_sec = time_r.tv_sec;
                        time_s.tv_nsec = time_r.tv_nsec;
                    }
                    else{
                        printf("Error invoking sleep %s\n", strerror(errno));
                        exit(1);
                    }
                    /******************************/
                    //printf("%d Child: sleep complete \n", pid);
                    /********************************/
                }
            }
            // send "end to signify end of file
            char b[5];
            int byte;
            for(i=0;i<3;i++){
                snprintf(b,5,"%04d",seq_num);
                b[4]='\0';
                if((byte=sendto(sock_fd, b, strlen(b), 0, (struct sockaddr*)&client_udp_addr, sizeof(client_udp_addr)))==-1){
                    printf("Child - Error writing to socket %s\n", strerror(errno));
                    exit(1);
                }
                seq_num = (seq_num+1)%10000;
                /**************************************/
                //printf("Sent ending packet %s\n",b);
                /**************************************/
            }
            //printf("%d\n", byte);
            // error in read process
            if(bytes==-1){
                printf("Child - Error reading from file %s %s\n", filename, strerror(errno));
                exit(1);
            }
            // close file descriptor of file to be transferred
            if(close(fd)==-1){
                printf("Child - Error closing file descriptor\n");
                exit(1);
            }
            // close client socket
            if(close(sock_fd)==-1){
                printf("Child - Error closing client sd\n");
                exit(1);
            }

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
            printf("%d completed execution\n",(int)getpid());
        }
      
        // if fork() was unsuccessful
        else if (pid==-1) {
            printf("SERVER: Error in fork() call\n");
        }
    
        // if parent process
        else {
            udp_port=udp_port+1;
            
            // sd_client is not needed in parent process after the fork
            if (close(sd_client)==-1){
                printf("Parent - Error closing tcp client socket\n");
                exit(1);
            }  
            /******************************/
            //printf("Parent - tcp socket closed \n");
            /********************************/
        }
    }
    //Close server socket
    if (close(sd_server)==-1){
        printf("SERVER:Error closing server socket\n");
        exit(1);
    }
    return 0;
}





