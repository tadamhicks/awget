#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>
#include "awget.h"

//Global delcarations
const char *DEFAULT_FILENAME = "index.html";
const char *DEFAULT_CHAINFILE = "chaingang.txt";

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) 
{
    struct ss_packet verss;
    size_t line_len = 25;
    int c;
    int arg_counter = 1;
    int port_num;
    char* url = NULL;
    char *chain_file = NULL;
    char *ip_addr = NULL;
    char *line = NULL;
    char *tok = NULL;
    char ssbuff[512] = "";
    FILE *fp = NULL;
    
    // Strip URL
    for (; arg_counter < argc; arg_counter++) {
        if (argv[arg_counter][0] == '-') {
            arg_counter++;
            continue;
        } 
	    else {
            url = argv[arg_counter];
            argc--;
            while (arg_counter < argc) {
                argv[arg_counter] = argv[++arg_counter];
            }
            break;
        }
    }

    int sockfd = 0;
    int rv = 0;
    struct addrinfo hints, *server_sck_addr, *p;
    int rand_stone;
    int numbytes; 
    char sizebuff[10] = ""; 
    long int f_size = 0;
    char *databuff;
    int bytes = 0; 
    const char *fname; 
    char *f_ptr; 
    int bytes_write = 0;
    int file_fd;
    char write_buf[4096] = "";
    int offset = 0;
    int chunk = 4096;
    int count = 0, i = 0;
    
    opterr = 0;
    while ((c = getopt(argc, argv, "c:")) != -1) {
        switch (c) {
        case 'c':
            chain_file = optarg;
            break;
        case '?':
            if (optopt == 'c') {
		fprintf(stderr, "Option -%c requires an argument. If you would prefer to use the deafult then leave blank.\n", optopt);
		return 1;
	    }
	default:
            printf("Using default chainfile\n");
	    abort();
	}
    }
    
    if (chain_file == NULL) {
	    chain_file = DEFAULT_CHAINFILE;
    }

    if (optind < argc) {
        fprintf(stderr,
                "Usage is 'awget <URL> [-c chainfile]'.\n");
        return -1;
    }

    strcpy(verss.url, url);

    if ((fp = fopen(chain_file, "r")) == NULL)
	    error("ERROR OPENING, file may not exist");

    char stones[4];
    for (i=0; i < 3; i++) {
        char c = fgetc(fp);
        stones[i] = c;
        if (c == '\0') {
            break;
        }
    }

    verss.stone_count = atoi(stones);
    if (verss.stone_count == 0) {
        fprintf(stderr, "Invalid chainfile, check number of stones. (atoi)\n");
        exit(1);
    }

    // Read ip and port from chain_file
    while ((getline(&line, &line_len, fp) > 0)) {
        struct int_tuple* cur = (verss.steps + count);
        tok = strtok(line, " ");
        struct sockaddr_in temp;
        inet_pton(AF_INET, tok, &(temp.sin_addr));
        cur->ip_addr = htonl(temp.sin_addr.s_addr);

        tok = strtok(NULL, "\n");
        cur->port_num = htons(atoi(tok));
        count++;
    }
    fclose(fp);
    printf("Request:%s \n", url);

    // Create a Socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	    error("ERROR CREATING SOCKET");

    printf("chainlist is \n");


    // Random stepping stone
    srand(time(NULL));
    rand_stone = rand() % verss.stone_count;

    struct int_tuple* to_stone;
    char to_ip[INET_ADDRSTRLEN];
    int to_port = 0;

    // Printing the chainlist
    for (i = 0; i < verss.stone_count; i++) {
        struct int_tuple* cur = (verss.steps + i);
        char ip[INET_ADDRSTRLEN];
        struct in_addr temp;
        temp.s_addr = ntohl(cur->ip_addr);
        int port = ntohs(cur->port_num);
        inet_ntop(AF_INET, &(temp), ip, INET_ADDRSTRLEN);
        printf("<%s, %d>\n", ip, port);

        if (i == rand_stone) {
            strcpy(to_ip, ip);
            to_port = port;
            to_stone = cur;
        }

    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char sPort[6];
    sprintf(sPort, "%d", to_port);
    if ((rv = getaddrinfo(to_ip, sPort, &hints, &server_sck_addr)) != 0) {
        fprintf(stderr, "ERROR GETTING INFORMATION: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the first we can
    for(p = server_sck_addr; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) < 0) {
            perror("COULD NOT CREATE SOCKET");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
            close(sockfd);
            perror("COULD NOT CONNECT");
            continue;
        }
        break;
    }

    // Send Data to SS
    if ((numbytes = send(sockfd, &verss, sizeof(struct ss_packet), 0)) < 0) 
        error("ERROR SENDING DATA");
    

    printf("waiting for file...\n");

    if (recv(sockfd, sizebuff, 10, 0) < 0) 
        error("ERROR ON RECEIPT, FILE EMPTY");
    

    // Byte order conversion 
    memcpy(&f_size, sizebuff, sizeof(f_size));
    f_size = ntohl(f_size);
    databuff = (char*) malloc(f_size);

    if ((bytes = recv(sockfd, databuff, f_size, MSG_WAITALL)) < 0)
	    error("ERROR RECEIVING FILE");
    
    close(sockfd);
    sleep(1);

    f_ptr = strrchr(url, '/');
    // Strip filename from URL
    if (f_ptr == NULL) {
        fname = (char *) DEFAULT_FILENAME;
    } 
    else {
        f_ptr++;
        fname = (char *) f_ptr;
    }
    printf("Received file: %s\n", fname);

    if ((file_fd = open(fname, O_CREAT | O_WRONLY | O_APPEND, 0666)) < 0)
	    error("COULD NOT OPEN FILE");
    
    // Writing the file data into file
    while (f_size > 0) {
        if (f_size > chunk) {
            memcpy(&write_buf, databuff+offset, chunk);
            f_size -= chunk;
            offset += chunk;
            bytes_write = write(file_fd, &write_buf, chunk);
            if (bytes_write <= 0) 
                error("File Write");
           
        } 
	else {
            memcpy(&write_buf, databuff+offset, f_size);
            bytes_write = write(file_fd, &write_buf, f_size);
            if (bytes_write <= 0) 
                error("COULD NOT WRITE FILE");
            f_size -= f_size;
        }
    }
    close(file_fd);
    printf("\nGoodbye!\n\n");
    close(sockfd);
    return 0;
} 
