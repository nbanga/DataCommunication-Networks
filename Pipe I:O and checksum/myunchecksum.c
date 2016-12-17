#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <fcntl.h>
#include <errno.h>

int main(int argc, char* argv[]){
    uint64_t checksum = 0U, stored_checksum = 0U, checksum_le = 0U;
    int fd_source, fd_dest; 
    char buffer='\0';
    int bytes_read;
    char curr_reads[8];

    // maintain a char array to store the last eight bytes that represent checksum.
    memset(curr_reads,'\0',8);

    // check number of input arguments
    if (argc<3){
        printf("Input format - myunchecksum <file1> <file2>");
        exit(1);
    }

    // open file to read.
    if ((fd_source = open(argv[1], O_RDONLY))==-1){
        printf("Error opening file %s", argv[1]);
        exit(1);
    }

    // open file to write
    if ((fd_dest = open(argv[2], O_WRONLY|O_CREAT,S_IRWXU|S_IRWXG|S_IRWXO))==-1){
        printf("Error opening file %s %s", argv[2], strerror(errno));
        exit(1);
    }

    // read the first eight bytes into char array curr_reads
    // assuming that even if the input file to mychecksum was empty, 8-byte checksum would be present.
    if ((bytes_read = read(fd_source,curr_reads,8))==-1){
        printf("Error reading bytes from source file %s", strerror(errno));
        exit(1);
    }

    // read the file byte by byte starting from 9th byte of the file.
    // with each character added to curr_read, pop the first character and add it to checksum.
    // when the eof is reached, the curr_read array has the 8 byte appended checksum and 
    // it has not been included in the current checksum calculation.
    int curr_index=0;
    char curr_char = '\0';
    do {
        if ((bytes_read = read(fd_source,&buffer,1))==-1){        // read byte from file
            printf("Error reading bytes from source file %s", strerror(errno));
            exit(1);
        }
        if(bytes_read==0) break;                                  // break the loop if EOF
        curr_char = curr_reads[curr_index];                       // pop character at the fornt of queue to consider for checksum
        curr_reads[curr_index]=buffer;                            // push latest read byte into the 8-byte curr_array
        curr_index = (curr_index+1)%8;
        if (write(fd_dest, &curr_char, bytes_read)!=bytes_read){  // write popped byte to file1
            printf("Error writing bytes to dest file");
            exit(1);
        }
        checksum = checksum + curr_char;                          // add popped byte to checksum
    }while(1);
    
    // convert curr_reads(appended checksum) to little endian format.
    int i=0;
    while (i<8){
        stored_checksum = stored_checksum | (((uint64_t)curr_reads[(curr_index+i)%8])<<(i*8));
        i++;
    }
    
    // convert computed checksum for file2 to little endian format.
    // required since we are dealing with variables in C
    i=0;
    char checksum_char;
    while (i<8){
        checksum_char = (checksum >> (7-i)*8) &0xFF;
        checksum_le = checksum_le | (((uint64_t)checksum_char)<<(i*8));;
        i++;
    }


    // close read and write file descriptors.
    if(close(fd_source)==-1){
        printf("Error closing source file");
        exit(1);
    }
    if(close(fd_dest)==-1){
        printf("Error closing source file");
        exit(1);
    }

    // check the checksums byte by byte as actual checksum variable is in big endian format and appended checksum is in little endian format.
    int j = 0;
    while(j<8){
        if((checksum>>(j*8)&0xFF) == ((stored_checksum>>(7-j)*8)&0xFF)){
            j++;
        }
        else {
            printf("Checksums do not match.\n%"PRIx64"\n" "%"PRIx64"\n", checksum_le, stored_checksum);
            return 0;
        }
    }
    printf("Checksums match.\n%"PRIx64"\n" "%"PRIx64"\n", checksum_le, stored_checksum);
   
    return 0;       
}
