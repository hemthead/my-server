#include <sys/socket.h>
#include <unistd.h> // close
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>

#include "http.h"
    
// not exposed in api
// the strings for our errors
const char *HTTP_ENOERROR_STRING = "No error occured";
const char *HTTP_ENOSOCKET_STRING = "No applicable socket was bound";
const char *HTTP_EUNKNOWN_STRING = "Unknown error type";

const char *HTTP_strerror(struct HTTP_Error err) {
    if (err.type < 0) {
        return strerror(err.info.errnum);
    }

    switch (err.type) {
        case HTTP_ENOERROR:
            return HTTP_ENOERROR_STRING;
        case HTTP_ENOSOCKET:
            return HTTP_ENOERROR_STRING;

        case HTTP_EGETADDRINFO:
            return gai_strerror(err.info.gai_errcode);

        default:
            return HTTP_EUNKNOWN_STRING;
    }
}

void HTTP_perror(const char *s, struct HTTP_Error err) {
    fprintf(stderr, "%s: %s", s, HTTP_strerror(err));
}

const int HTTP_SETSOCKOPT_ENABLE = 1; // int for setsockopt to get pointed to

struct HTTP_Error HTTP_Server_init(
    struct HTTP_Server *server,
    const char *restrict port
) {
    // initialize the return `HTTP_Error` to no-error
    struct HTTP_Error error;
    memset(&error, 0, sizeof(error));

    // declare addrinfo structures to find an address that works for our server
    struct addrinfo addr_hints, *potential_addresses; 

    // set the hints to our use-case
    memset(&addr_hints, 0, sizeof(addr_hints));
    addr_hints.ai_family = AF_UNSPEC; // IPv4 or IPv6
    addr_hints.ai_socktype = SOCK_STREAM; // TCP socket, please
    addr_hints.ai_flags = AI_PASSIVE; // We want to be able to bind this socket to
                                   // accept connections
    
    // find a socket to use and listen on, return any getaddrinfo errors
    error.info.gai_errcode = getaddrinfo(NULL, port, &addr_hints, &potential_addresses);
    if (error.info.gai_errcode != 0) {
        error.type = HTTP_EGETADDRINFO;
        return error;
    }

    // set the socket_fd to -1 so we can tell if no applicable socket is found
    server->socket = -1;
    // search through the returned addresses to find one that works
    for (struct addrinfo *rp = potential_addresses; rp != NULL; rp = rp->ai_next) {
        int socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        
        // go to the next address if we can't use this one
        if (socket_fd == -1)
            continue;

        // try to set socket options or go to the next address
        if (setsockopt(
            socket_fd,
            SOL_SOCKET,
            SO_REUSEADDR, // reuse old sockets
            &HTTP_SETSOCKOPT_ENABLE,
            sizeof(HTTP_SETSOCKOPT_ENABLE)
        ) == -1)
            continue;

        // try to bind the socket 
        if (bind(socket_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
            // yay, we found a good one! set the socket and break out
            server->socket = socket_fd;
            break;
        }
    }
    freeaddrinfo(potential_addresses); // free up the list, we no longer need it

    // return error if we couldn't find a good socket
    if (server->socket == -1) {
        error.type = HTTP_ENOSOCKET;
        return error;
    }

    // start listening on the socket, return any listen errors
    if (listen(server->socket, 10) == -1) {
        error.type = HTTP_ELISTEN;
        error.info.errnum = errno;
        return error;
    }

    // no error
    return error;
}

struct HTTP_Error HTTP_Server_deinit(struct HTTP_Server *server) {
    // initialize the return `HTTP_Error` to no-error
    struct HTTP_Error error;
    memset(&error, 0, sizeof(error));

    // try to close the socket
    // return an error if `close` failed and the file descriptor was good
    if (close(server->socket) == -1 && errno != EBADF) {
        error.type = HTTP_ECLOSE;
        error.info.errnum = errno;
        return error;
    }

    return error;
}
/*
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
 */
