#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char* argv[]){
    uint64_t checksum = 0U;
    int fd_source, fd_dest; 
    char buffer='\0';
    char checksum_char[8];
    int bytes_read;

    // check number of input arguments
    if (argc<3){
        printf("Input format - mychecksum <file1> <file2>");
        exit(1);
    }
    // open file to read.
    if ((fd_source = open(argv[1], O_RDONLY))==-1){
        printf("Error opening file %s", argv[1]);
        exit(1);
    }

    // open file to write.
    if ((fd_dest = open(argv[2], O_WRONLY|O_CREAT, S_IRWXU|S_IRWXG|S_IRWXO))==-1){
        printf("Error opening file %s %s", argv[2], strerror(errno));
        exit(1);
    }

    // read file1 byte by byte.
    // write to file2 byte by byte.
    // Calculate checksum by adding byte to checksum value.
    do {
        if ((bytes_read = read(fd_source,&buffer,1))==-1){                           // read byte from file1
            printf("Error reading bytes from source file %s", strerror(errno));
            exit(1);
        }
        if (bytes_read==0) break;                                                    // break the loop if EOF
        if (write(fd_dest, &buffer, bytes_read)!=bytes_read){                        // write byte to file2
            printf("Error writing bytes to dest file");
            exit(1);
        }
        checksum = checksum + buffer;                                                // add byte to checksum
    }while(1);

    // convert 64-bit checksum integer to 8-byte char array with MSB at index 0.
    // and append to file2 in big endian format
    int i=0;
    for(i=0;i<8;i++){
        checksum_char[i]= (checksum >> (7-i)*8) & 0xFF;                              // convert checksum to 8-byte char array
        if (write(fd_dest, &(checksum_char[i]), 1)!=1){                              // append byte to file with MSB first
           printf("Error writing bytes to dest file");
           exit(1);
        }
    }

    // close read and write file descriptors
    if(close(fd_source)==-1){
        printf("Error closing source file");
        exit(1);
    }
    if(close(fd_dest)==-1){
        printf("Error closing source file");
        exit(1);
    }

    return 0;       
}
