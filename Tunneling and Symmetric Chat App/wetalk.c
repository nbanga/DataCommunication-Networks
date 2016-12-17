/* Chat client using select and getchar()
 * Setup SIGALRM and SIGPOLL handlers and set socket to be able to trap these errors
 * Stage 1: User sends a request to peer. 
 * Blokcing on select() to multiplex between listening on both STDIN and socket.
 * Stage 2: Made socket Asynchronous and Non blocking.
 * Reading user input with getchar after changing termios settings to disable line discipline
 * Once chat terminated, reset termios
 * Rest socket to be blocking and synchronous for user to be able to block on select again for next request
 * 'q' to quit, 'c' to accept, 'n' to decline, 'e' to exit current session, '$ip$port' to send request to peer
 */

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
#include <termios.h>

#define MAX_BUF 1024

// global buffer to store incomplete input entered by user during chat session
char buffer[MAX_BUF];
volatile int my_socket;
volatile int chatTerminated = 0;

// signal handler for SIGALRM
void sigalarmHandler(int sig){
    // nothing to be done
}

// signal handler for SIGPOLL
void sigpollHandler(int sig) {
    char read_buffer[MAX_BUF];
    struct sockaddr_in peer_addr;
    int peer_len = sizeof(peer_addr);
    bzero(&peer_addr,sizeof(peer_addr));
    // get packets till the receive buffer of kernel is not empty
    // if no more packets to read, recvfrom returns EWOULDBLOCK error when socket is non blocking
    while(1){
        memset(read_buffer,'\0',MAX_BUF);
        int bytes = recvfrom(my_socket,read_buffer,MAX_BUF,0,(struct sockaddr*) &peer_addr, &peer_len);
        // if EWOULDBLOCK, print buffer and return
        if(bytes==-1){
            if (errno==EWOULDBLOCK){
                printf("\n> %s",buffer);
                fflush(stdout);
                return;
            }
        }
        // else if 'E' is received, set chatTerminated to true
        // ow print read_buffer
        else if(bytes>0){
            if (strcmp(read_buffer,"E")==0){
                chatTerminated = 1;
                return;
            }
            else {
                // print messgae without D
                printf("\n| %.*s",(int)strlen(read_buffer),&(read_buffer[1]));
            }
        }
    }   
}

// main function that has chat functionality
int main(int argc, char** argv){
    char read_buffer[MAX_BUF], write_buffer[MAX_BUF], in_buffer[MAX_BUF];
    int bytes, response, i, len;
    fd_set read_fd_set;
    int numfd;
    struct sigaction sigPoll, sigAlarm;
    char peer_ip[20], peer_port[10];
    struct termios old_termios, new_termios;
    
    // initialize connection addresses for both peers
    struct sockaddr_in my_addr, peer_addr;
    bzero((char*) &my_addr, sizeof(my_addr));
    bzero((char*) &peer_addr, sizeof(peer_addr));
    int my_len = sizeof(my_addr), peer_len = sizeof(peer_addr);
     
    // check for format of input arguments
    if (argc<2){
        printf("Format is wetalk my_port_num");
        exit(1);
    }

    // create socket
    if((my_socket=socket(AF_INET, SOCK_DGRAM, 0))==-1){
        printf("SERVER: Error creating server socket");
        exit(1);
    }

    // initialize server address
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(atoi(argv[1]));
    my_addr.sin_addr.s_addr = inet_addr("128.10.2.13");

    //bind socket to server address
    if (bind(my_socket, (struct sockaddr*) &my_addr, sizeof(my_addr))==-1){
        printf("SERVER: Error binding to server address %s\n", strerror(errno));
        exit(1);
    }

    // print welcome message for user
    printf("Welcome to WeTalk!\n");
    printf("Please enter the ip and port number of peer '$hostname$port'\n");
    printf("Enter 'q' if you wish to exit WeTalk\n");
    printf("Enter 'c' to accept incoming request, 'n' to decline\n");

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
    
    // registering signal handler for SIGPOLL with sigaction
    sigPoll.sa_handler = sigpollHandler;
    if(sigfillset(&sigPoll.sa_mask) < 0){
        printf("Error initializing signal handler");
        exit(1);
    }
    sigPoll.sa_flags = 0;
    if(sigaction(SIGPOLL, &sigPoll, 0) < 0) {
        printf("Error in handler call for SIGPOLL failed");
        exit(1);
    }

    // register socket to trap interrupt SIGPOLL
    if (fcntl(my_socket, F_SETOWN, getpid())<0){
        printf("Error in fcntl %s", strerror(errno));
        exit(1);
    }
        
    // set read_fd_set for select
    FD_ZERO(&read_fd_set);
    FD_SET(my_socket, &read_fd_set);
    FD_SET(fileno(stdin),&read_fd_set); // read_set for STDIN

    // set buffer flushing to NULL to flush buffer immediately
    setbuf(stdout,NULL);

    while(1){
        // Stage 1: establish a connection
        // Stage 2: Chat

        // Stage 1
        // Wait on STDIN and socket to receive a peer address or a request to chat
        while(1){
            memset(read_buffer, '\0', MAX_BUF);
            memset(write_buffer, '\0', MAX_BUF);
            memset(in_buffer, '\0', MAX_BUF);
            FD_ZERO(&read_fd_set);
            FD_SET(my_socket, &read_fd_set);
            FD_SET(fileno(stdin), &read_fd_set);
            printf("? ");
            if ((response = select(my_socket+1, &read_fd_set, NULL, NULL, NULL))==-1){
                printf("Error in select(): %s %d", strerror(errno), errno);
                exit(1);
            }
            // if input form STDIN, send a request to the peer
            // Wait for a response for 7 sec
            // Parse peer response if received
            if(FD_ISSET(0,&read_fd_set)){
                fgets(read_buffer, MAX_BUF, stdin); 
                strtok(read_buffer,"\n");

                // Parse input
                // If first character is '$', input is a peer address
                int i=0,j=0;
                if (read_buffer[i]=='$'){
                    i++;
                    while(read_buffer[i]!='$'){
                        peer_ip[j++] = read_buffer[i++];
                    }
                    peer_ip[j]='\0';
                    if(read_buffer[i]=='$')
                        i++;
                    j=0;
                    while(read_buffer[i]!='\0'){
                        peer_port[j++]=read_buffer[i++];
                    }
                    peer_port[j]='\0';  
                    snprintf(write_buffer, MAX_BUF-1,"%s", "wannatalk");
                    
                    // initialize peer address
                    peer_addr.sin_family = AF_INET;
                    peer_addr.sin_port = htons(atoi(peer_port));
                    peer_addr.sin_addr.s_addr = inet_addr(peer_ip);

                    // send 'wannatalk' to peer address
                    if(sendto(my_socket, write_buffer, strlen(write_buffer), 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr))==-1){
                        printf("Error sending 'wannatalk' to peer at %s:%d\n", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
                        continue;
                    }

                    // put on alarm of 7 seconds
                    // timeout if no response received from peer
                    alarm(7);
                    if((bytes = recvfrom(my_socket,in_buffer,MAX_BUF,0,(struct sockaddr*)&peer_addr, &peer_len))==-1){
                        // If interrupted by alarm
                        if (errno == EINTR){
                            printf("No response from peer. Try again or press 'q' to quit.\n");
                            continue;
                        }                        
                        else {
                            printf("Error reading response from peer %s.", strerror(errno));
                            exit(1);
                        }
                    }
                    // disable the alarm if response is received form the client
                    alarm(0);

                    // If response if OK, continue to Stage 2.
                    if (strcmp(in_buffer,"OK")==0){
                        break;
                    }
                    // If response is KO, give a choice for new request
                    else if (strcmp(in_buffer,"KO")==0){
                        printf("| doesn't want to chat\n");
                        bzero(&peer_addr,sizeof(peer_addr));
                        continue;
                    }
                }
                // case when you receive a request and post a reply
                // if reply is 'c', send OK to requesting peer and continue to stage 2
                else if(strcmp(read_buffer,"c")==0){
                    snprintf(write_buffer, MAX_BUF-1, "%s", "OK");
                    if(sendto(my_socket, write_buffer, strlen(write_buffer), 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr))==-1){
                        printf("Error sending OK to peer. Please check host address\n");
                        continue;;
                    }
                    break;
                }
                // else if reply is 'n', send KO to requesting peer and continue to Stage 1 again.
                else if(strcmp(read_buffer,"n")==0){
                    snprintf(write_buffer, MAX_BUF-1, "%s", "KO");
                    if(sendto(my_socket, write_buffer, strlen(write_buffer), 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr))==-1){
                        printf("Error sending KO to peer, Please check host address\n");
                    }
                    bzero(&peer_addr, sizeof(peer_addr));
                    continue;
                }
                // if user wants to quit the application
                else if (strcmp(read_buffer,"q")==0){
                    printf("Exiting the chat client...\n");
                    close(my_socket);
                    exit(1);
                }  
            }
            // if request received on socket in select
            if (FD_ISSET(my_socket, &read_fd_set)){
                // peer_addr is set with values from recv_from
                if((bytes = recvfrom(my_socket,read_buffer,MAX_BUF,0,(struct sockaddr*)&peer_addr, &peer_len))==-1){
                    printf("Error reading data from recvform at socket\n");
                    exit(1);
                }
                // If incoming request, ask user for a response
                if (!strcmp(read_buffer,"wannatalk")){
                   printf("\n| chat request from %s %d\n", inet_ntoa(peer_addr.sin_addr), ntohs(peer_addr.sin_port));
                   continue;
                }
                // if incoming response is OK, peer accepted your request to chat.
                // continue to stage 2
                else if (!strcmp(read_buffer,"OK")){
                    printf("| Peer has accepted your request. Happy chat!\n");
                    break;
                }
                // if incoming request is KO, peer rejected your request to chat.
                // continue to stage 1 again
                else if(!strcmp(read_buffer,"KO")){
                    printf("| doesn't want to chat\n");
                    bzero(&peer_addr,sizeof(peer_addr));
                    continue;
                }
            }
        }

        // Stage 2
        // Chat connection is established. Need to make socket non blocking and asynchronous for it to be able to trap SIGPOLL
        // Make socket non-blocking so that recvfrom returns if no more data to be read
        int flag = fcntl(my_socket, F_GETFL);
        flag |= O_NONBLOCK;
        if(fcntl(my_socket, F_SETFL, flag)==-1){
            printf("fcntl: Error setting non blocking control %s", strerror(errno));
            exit(1);
        }
        int on = 1;
        // set socket to be asynchronous
        if (ioctl(my_socket,FIOASYNC,&on)==-1){
           printf("ioctl: Error setting Asynchronous control %s", strerror(errno));
           exit(1);
        }

        printf("\n");
        // Chat client
        // Block on stdin until SIGPOLL trapped
        // Go to handler, print the data received from peer
        // Continue current buffer where left off
        // Use global flag chatTerminated to signify exit form current chat
        while(1){
          printf("> ");
          fflush(stdout);
          memset(buffer, '\0', MAX_BUF);
          int i=0; char c, send_buf[51];
          
          // change termios settings to disable line discipline
          // read each character as it comes
          tcgetattr(fileno(stdin),&old_termios);
          new_termios = old_termios;
          new_termios.c_lflag &= (~ICANON);
          new_termios.c_cc[VTIME]=0;
          new_termios.c_cc[VMIN]=1;
          tcsetattr(fileno(stdin),TCSANOW,&new_termios);

          // read characters till new line or carriage return
          // On returning from SIGPOLL, signal character is being read by getchar()
          // Ignore the character
          do {
              c = getchar();
              if(c=='\n' || c=='\r')
                  break;
              if((int)c==-1)
                  continue;
              // truncate at 50
              if (i<49){
                  buffer[i++] = c;
              }
          }while(!chatTerminated);

          // reset terminal attributes to old settings
          tcsetattr(fileno(stdin),TCSANOW,&old_termios);

          // if chat was terminated by the peer, reset the peer_addr so that no more messages can be sent to the peer
          // exit from Stage 2
          if(chatTerminated){
              printf("\n| chat terminated\n\n");
              bzero(&peer_addr,sizeof(peer_addr));
              break;
          }

          // If you want to terminate chat, pree 'e'
          // If e pressed, send "E" else send "D" + value entered by user
          if(!strcmp(buffer,"e")){
              snprintf(send_buf,51,"%s", "E");
          }
          else{
              snprintf(send_buf,51,"%s%s", "D",buffer);
          }

          // send the packet to peer
          if(sendto(my_socket, send_buf, strlen(send_buf), 0, (struct sockaddr*)&peer_addr, sizeof(peer_addr))==-1){
              printf("Error sending message to peer %s\n", strerror(errno));
              exit(1);
          }  

          // if you have terminated chat, clear peer address and exit Stage 2
          if(!strcmp(send_buf,"E")){
              printf("\n");
              bzero(&peer_addr,sizeof(peer_addr));
              break;
          }
       }

       // reset socket to SYNCHRONOUS and BLOCKING for Stage 1
       // if not, select() call gets interrupted on recvfrom
       on = 0;
       if (ioctl(my_socket,FIOASYNC,&on)==-1){
           printf("ioctl; Error resetting socket to synchronous mode %s", strerror(errno));
           exit(1);
       }
       if(fcntl(my_socket, F_SETFL, flag & ~O_NONBLOCK)==-1){
          printf("fcntl: Error setting non blocking control %s\n", strerror(errno));
          exit(1);
       }
       // go back to stage 1
    }
    return 0;
}
