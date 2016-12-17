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
#include <sys/time.h>

#define MAX_BUF 100

// check if filename is valid
// length is less than 16
// no '/' in the name
int isFilenameValid(char* filename){
    char* temp = filename;
    if (strlen(filename)<17){
        while(*temp!='\0' && *temp!='/')
            temp++;
    }
    if (*temp=='\0')
        return 1;
    return 0;
}

// main function
// request for a file from file server
int main(int argc, char** argv){
    int sd_client,fd,fd_config;
    char write_buffer[MAX_BUF], block_size[MAX_BUF];
    int block_len=0,total_bytes=0,bytes;
    struct sockaddr_in server_addr;
    struct hostent* host; 
    struct timeval start,end;

    // check for appropriate number of arguments
    if (argc<6) {
        printf("Format is 'fileclient hostname portnumber secretkey filename configfile.dat'");
        exit(1);
    }
   
    // open the file specified by client in write mode
    // Create the file before writing to it.
    // Throw an error and exit if the file is already present.
    char filename[MAX_BUF];
    snprintf(filename,MAX_BUF-1,"%s%s","./",argv[4]);
    // verify filename format
    if (isFilenameValid(filename)){
        printf("Filename is not valid \n");
        exit(1);
    }
    // check if file exists
    if(access(filename,F_OK)!=-1){
        printf("File already exists\n");
        exit(1);
    }
    
    // set write_buffer to null values
    memset(write_buffer,'\0',MAX_BUF);
    memset(block_size,'\0',MAX_BUF);

    // open config file
    if ((fd_config=open(argv[5],O_RDONLY))==-1){
        printf("Error opening configfile %s %s", argv[5], strerror(errno));
        exit(1);
    }
    // get number of bytes to be read from config file
    if ((bytes=read(fd_config,block_size,MAX_BUF))==-1){
        printf("Error reading config file %s %s", argv[5], strerror(errno));
        exit(1);
    }
    // close config file
    if(close(fd_config)==-1){
        printf("Error reading config file %s %s", argv[5], strerror(errno));
        exit(1);
    }

    // convert the value read from config file to an int
    int i;
    for(i=0;block_size[i+1]!='\0';i++){
        block_len = block_len*10 + (block_size[i] - '0');
    }

    // declare a read_buffer of appropriate length
    char read_buffer[block_len+1];
    memset(read_buffer,'\0',block_len+1);

    // Create client request
    snprintf(write_buffer, MAX_BUF-1, "%s%s%s%s", "$",argv[3],"$",argv[4]);

    // open a new file for transferring the requested file to
    if((fd=open(filename,O_CREAT|O_EXCL|O_WRONLY,S_IRWXU|S_IRWXG|S_IRWXO))==-1){
        printf("Error opening file %s %s",argv[4],strerror(errno));
        exit(1);
    }

    // get host information from host name
    if ((host = gethostbyname(argv[1]))==NULL){
        printf("Error finding host details");
        exit(1);
    }

    // create client socket
    if ((sd_client=socket(AF_INET,SOCK_STREAM,0))==-1){
        printf("CLIENT:Error creating client socket");
        exit(1);
    }

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

    int flag = 0; // to mark the completion of first read operation.
    int bytes_written;
    // Read response from server while server is sending data
    while ((bytes=read(sd_client,read_buffer,block_len))>0){
        // get time after first read operation
        if (!flag){
            gettimeofday(&start,NULL);
            flag = 1;
        }
        // Write bytes received to the file specified
        if ((bytes_written = write(fd,read_buffer,bytes))!=bytes){
            printf("Error writing to file %s %s", filename, strerror(errno));
            exit(1);
        }
        // compute total_bytes read till now
        total_bytes+=bytes;      
        memset(read_buffer,'\0',block_len+1);
    }
    if (bytes==0){
        // close client socket
        if(close(sd_client)==-1){
          printf("CLIENT:Error closing client socket");
          exit(1);
       }
    }
    // Throw an error if error reading from file
    if(bytes==-1){
        printf("CLIENT:Error reading from socket");
        exit(1);
    }

    // get time of completion of last read call
    gettimeofday(&end,NULL);

    //close the file after writing to it
    if (close(fd)==-1){
        printf("Error closing file %s %s", filename,strerror(errno));
        exit(1);
    }

    // calculate the completion time of the request 
    // and the reliable throughput
    float completion_time = ((end.tv_sec*1000000 + end.tv_usec) - (start.tv_sec*1000000 + start.tv_usec))/1000.0f;
    float reliable_throughput = (8*total_bytes)/(completion_time/1000.0f);
    
    // print total bytes read, completion time and reliable throughput 
    printf("Bytes read %d\n", total_bytes);
    printf("Completion time %0.3f msec\n",completion_time);
    printf("Reliable throughput %0.3f bps\n", reliable_throughput);
    
    return 0;
}
