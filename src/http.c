#include <sys/socket.h>
#include <unistd.h> // close
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>

int http_main(int argc, char **argv) {
    int enable = 1; // int for setsockopt to get pointed to
    int LISTEN_BACKLOG = 5; // backlog for `listen` call

    // create a socket for our server
    int server_sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sockfd == -1) {
        perror("error: failed to create socket");
        exit(EXIT_FAILURE);
    }

    // set server to be at 0.0.0.0:8080
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = htonl(INADDR_ANY); // should be 0.0.0.0 (broadcast/any)
    server_address.sin_port = htons(8080u); // port 8080

    // set socket options
    // REUSEADDR: 
    if (setsockopt(
        server_sockfd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &enable,
        sizeof(enable)
    ) == -1) {
        perror("error: setsockopt(SO_REUSEADDR) failed");
        exit(EXIT_FAILURE);
    }

    // finally bind the socket
    if (bind(
        server_sockfd,
        (struct sockaddr*)&server_address,
        sizeof(server_address)
    ) == -1) {
        perror("error: failed to bind socket");
        exit(EXIT_FAILURE);
    }

    // start listening
    if (listen(server_sockfd, LISTEN_BACKLOG) == -1) {
        perror("error: failed to start listening");
        exit(EXIT_FAILURE);
    }
    fprintf(stdout, "server: listening on port %hu\r\n",  ntohs(server_address.sin_port));

    struct sockaddr_in client_address;
    socklen_t client_len = sizeof(client_address);
    
    #define BUFFER_SIZE 1024
    char buffer[BUFFER_SIZE];

    // accept an incoming connection
    for (;;) {
        int client_sockfd = accept(
            server_sockfd,
            (struct sockaddr*)&client_address,
            &client_len
        );
        if (client_sockfd == -1) {
            perror("error: failed to accept connection");
            exit(EXIT_FAILURE);
        }

        fprintf(stdout, "server: received connection with %X\r\n", ntohl(client_address.sin_addr.s_addr));

        ssize_t bytes_read = recv(client_sockfd, &buffer, BUFFER_SIZE, 0);
        switch (bytes_read) {
        case 0:
            close(client_sockfd);
            continue;
        case -1:
            perror("error: failed to recv data");
            close(client_sockfd);
            close(server_sockfd);
            exit(EXIT_FAILURE);
        default:
            break;
        }
        
        // print received message
        for (ssize_t i = 0; i < bytes_read; ++i)
            fputc(buffer[i], stdout);

        char message[] = "HTTP/1.0 200 OK\r\nContent-Type: text/html; charset=utf=8\r\n\r\n<!DOCTYPE html><html><head><title>test</title></head><body><h1>no, lol</h1></body></html>\r\n";
        
        send(client_sockfd, &message, sizeof(message), 0);

        close(client_sockfd);
    }

    return EXIT_SUCCESS;
}
