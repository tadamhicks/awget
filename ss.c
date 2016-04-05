#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include "awget.h"

// Global declarations
void *ss_func(void *file_desc);
const char *DEFAULT_FILE = "index.html";
const int VERSION = 1;
socklen_t cli_len; 
char my_ip[15] = ""; 
int my_ip_addr;
int port_num;

void error(char *msg)
{
    perror(msg);
    exit(1);
}

void pthread_error(char *msg)
{
    perror(msg);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) 
{
    int c; 
    opterr = 0;
    if (argc == 1) {
        fprintf(stderr, "No port, no start!.\n");
        return 1;
    } else if (argc != 3) {
        fprintf(stderr, "Usage: -p <port number>\n");
        return 1;
    }

    // Invalid usage handling
    while ((c = getopt(argc, argv, "p:")) != -1) {
        switch (c) {
        case 'p':
            port_num = atoi(optarg);
	    break;
        case '?':
            if (optopt == 'p') {
                fprintf(
                        stderr,
                        "Option -%c requires an argument (-p <port number>)\n",
                        optopt);
            }
            return 1;
        default:
            abort();
        }
    }

    /*
     * Get ready by initializing vars,
     * using addrinfo for getting hostname instead of sockaddr_in structs for handling addresses 
     *
     */  
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_in serv_addr, cli_addr; 
    struct hostent *host_entry;
    int sockfd, newsockfd;
    char sZhostName[50] = "";

    /*
     * Getting host name below,
     * start by zeroing structure,
     * use memset instead of bzero,
     * use addrinfo
     */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    
    int gh = gethostname(sZhostName, sizeof(sZhostName));
    host_entry = gethostbyname(sZhostName);
    strcpy(my_ip, inet_ntoa(**((struct in_addr **)host_entry->h_addr_list)));
    struct sockaddr_in temp;
    
    inet_pton(AF_INET, my_ip, &(temp.sin_addr));
    my_ip_addr = temp.sin_addr.s_addr;

    /*
     * Creating socket for listening,
     * start by zeroing structure
     */ 
    memset(&serv_addr, 0, sizeof serv_addr);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(port_num);

    // Socket creation
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) 
	error("ERROR OPENING SOCKET.");
    
    if (bind(sockfd, (struct sockaddr *) &serv_addr,
	sizeof(serv_addr)) < 0)
		error("ERROR ON BINDING");
     
    // Initialize and listen, 10 threads 
    pthread_t ss;
    listen(sockfd, 10);
    printf("Listening on ip %s and port %d\n", my_ip, port_num);

    while (1) {
        cli_len=sizeof(cli_addr);
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &cli_len);
        if (newsockfd < 0) error("ERROR ON ACCEPT");

        if ((pthread_create(&ss, NULL, ss_func, (void *) &newsockfd)) != 0) 
            error("ERROR INITIALIZING PTHREAD");
    }
}

/*
 * This is where we operate threads,
 * if this is the last stone we wget,
 * if not we send on and wait
 */
void *ss_func(void *file_desc) {
    int *filedesc = (int *) file_desc;
    int file_descriptor = *filedesc;
    int siz;
    int packet_size = sizeof(struct ss_packet);
    char ss_buffer[15000];
    siz += recv(file_descriptor, ss_buffer, packet_size, MSG_WAITALL);
    
    if ( siz < 0) 
        pthread_error("ERROR RECEIVING");

    struct ss_packet *ssp = (struct ss_packet*) ss_buffer;
    int len = strlen(ssp->url);
    char *ss_url = ssp->url;
    ssp->stone_count--;
    
    // If this is the last stone then get the file
    if (ssp->stone_count == 0) { 
        struct stat st;
        long int file_size;
        char file_buffer[4000];
        
	    // Get the File
        printf("Request: %s\n", ss_url);
        printf("chainlist is empty.\n");
        
        sprintf(file_buffer,"wget -q %s",ss_url);
        system(file_buffer);

        const char *filename;
        char *file_ptr;
        file_ptr = strrchr(ss_url, '/');

        if (file_ptr == NULL) {
            filename = DEFAULT_FILE;
        } 
        else {
            file_ptr++;
            filename = (char *) file_ptr;
        }
        printf("Issuing wget for file %s \n", filename);

        // Finding size of downloaded file
        if (stat(filename, &st) == 0) {
            file_size = st.st_size;
        } 
        else {
            printf("File Size Stat\n");
            close(file_descriptor);
            pthread_exit(NULL);
        }

        char *filebuff;
        filebuff = (char *) malloc(file_size);
        int open_fd = open(filename, O_RDONLY, 0666);
        
        if ((read(open_fd, filebuff, file_size) <= 0)) 
            pthread_error("File Read:");
        close(open_fd);

        // Remove the file
        char rem_file[128] = "";
        char size[10] = "";
        long int siz = htonl(file_size);
        sprintf(rem_file,"rm %s", filename);
        system(rem_file);
        memcpy(size, &siz, sizeof(siz));

        if ((send(file_descriptor, size, 10, 0)) < 0) 
            pthread_error("File Size Send");
        
        // Sending File Data
        long int bytes = 0;
        if ((bytes = send(file_descriptor, filebuff, file_size, 0)) < 0) 
            pthread_error("Data Send");
        
        printf("Relaying File\n");
        printf("Still listening for more fetches.\n");
        close(file_descriptor);
        pthread_exit(NULL);
    }

    // Not the last stone 
    else {
	    printf( "Request: %s\n", ssp->url);
        int count = 0, stone_counter = 0;
        // Check for self in chainlist 
        for (stone_counter = 0; stone_counter < ssp->stone_count+1; stone_counter++) { 
            int the_ip;
            int the_port;
            the_ip = ntohl(ssp->steps[stone_counter].ip_addr);
            the_port = ntohs(ssp->steps[stone_counter].port_num);
            if ((the_ip == my_ip_addr && the_port != port_num) || (the_ip != my_ip_addr)) {
                ssp->steps[count].ip_addr = ssp->steps[stone_counter].ip_addr;
                ssp->steps[count++].port_num = ssp->steps[stone_counter].port_num;
            }
        }

        //Clear out last element by setting them to 0
        ssp->steps[stone_counter].ip_addr = 0;
        ssp->steps[stone_counter].port_num = 0;
        printf("chainlist is \n");

        // picking a random ss from the list
        int rand_ss;
        char to_ip[INET_ADDRSTRLEN];
        int to_port;
        int to_dest;
        int loop;
        srand(time(NULL));
        rand_ss = rand() % ssp->stone_count;

        // For loop to print the chainlist
        for (loop = 0; loop < ssp->stone_count; loop++) {
            struct int_tuple* cur = (ssp->steps + loop);
            char ip[INET_ADDRSTRLEN];
            struct in_addr temp;
            temp.s_addr = ntohl(cur->ip_addr);
            int port = ntohs(cur->port_num);
            inet_ntop(AF_INET, &(temp), ip, INET_ADDRSTRLEN);
            printf("<%s, %d>\n", ip, port);

            // If we've landed on the random one, we use it
            if (loop == rand_ss) {
                strcpy(to_ip, ip);
                to_port = port;
                to_dest = loop;
            }
        }

        printf("next SS is <%s, %d>\n", to_ip, to_port);

        struct addrinfo hints, *server_sck_addr, *p;
        int rv;
        int cli_sock;

        // Zeroing structure
        memset(&hints, 0, sizeof hints);
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        char sPort[6];
        sprintf(sPort, "%d", to_port);
        if ((rv = getaddrinfo(to_ip, sPort, &hints, &server_sck_addr)) != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
            pthread_exit(NULL);
        }

        for(p = server_sck_addr; p != NULL; p = p->ai_next) {
            if ((cli_sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0) {
                perror("SOCKET ERROR AT CLIENT");
                continue;
            }

            if (connect(cli_sock, p->ai_addr, p->ai_addrlen) < 0) {
                close(cli_sock);
                perror("ERROR CONNECTING TO CLIENT");
                continue;
            }
            break;
        }
        printf("waiting for file...\n");
        
        ssize_t bytes_size;
        size_t size_packet = sizeof(struct ss_packet);
        bytes_size += send(cli_sock, &(*ssp), sizeof(struct ss_packet), MSG_WAITALL);

        if (bytes_size < 0) 
            pthread_error("NOTHING TO SEND");

        char sizebuff[10] = "";
        int frecv_bytes;

        if ((frecv_bytes = recv(cli_sock, sizebuff, 10, 0)) < 0) 
            pthread_error("FILE IS EMPTY UPON RECEIPT");
        
        if ((send(file_descriptor, sizebuff, frecv_bytes, 0)) < 0) 
            pthread_error("FILE IS EMPTY UPON TRANSMISSION");

        long int filesize = ntohl(filesize);
        char *databuff = (char*) malloc(filesize);

        if (recv(cli_sock, databuff, filesize, MSG_WAITALL) < 0)
            pthread_error("DATA COULD NOT BE RECEIVED");

        close(cli_sock);
        printf("Relaying File\n");
        printf("Still listening for more fetches.\n");

        if ((send(file_descriptor, databuff, filesize, 0)) < 0) 
            perror("Data Send:");
        close(file_descriptor);
    }
    pthread_exit(NULL);
}
