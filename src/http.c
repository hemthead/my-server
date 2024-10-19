#include <sys/socket.h>
#include <unistd.h> // close
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
    
// normal errno errors are negative for convenience!
// non-errors are 0
// custom errors are positive
enum HTTP_errno {
    // errno errors
    HTTP_ECLOSE = -5,
    HTTP_ELISTEN = -4,
    HTTP_EBIND = -3,
    HTTP_ESETSOCKOPT = -2,
    HTTP_ESOCKET = -1,

    // non-error / Ok
    HTTP_ENOERROR = 0, // default, result when no error occured

    // custom errors
};

// not exposed in api
// the strings for our errors
char *HTTP_ENOERROR_STRING = "No error occured";
char *HTTP_EERRNO_STRING = "System error, see errno";

// get the string representation of an error
char *HTTP_strerror(enum HTTP_errno err) {
    if (err < 0)
        return HTTP_ENOERROR_STRING;

    switch (err) {
        case HTTP_ENOERROR:
            return HTTP_ENOERROR_STRING;
        break;
    }
}

// print an HTTP_errno to stderr (like perror)
void HTTP_perror(const char *s, enum HTTP_errno err) {
    fprintf(stderr, "%s: %s", s, HTTP_strerror(err));
}

struct HTTP_Server {
    int socket_fd;
    struct sockaddr_in address;
    int listen_backlog;
};

const int HTTP_SETSOCKOPT_ENABLE = 1; // int for setsockopt to get pointed to

// create the server's socket and address and set its socket options
enum HTTP_errno HTTP_create_server(struct HTTP_Server *http_server, const uint16_t port) {
    http_server->listen_backlog = SOMAXCONN; // backlog for `listen` call

    // create a socket for the server
    http_server->socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (http_server->socket_fd == -1) {
        return HTTP_ESOCKET;
    }

    // set server to be at 0.0.0.0:<given port>
    http_server->address.sin_family = AF_INET;
    http_server->address.sin_addr.s_addr = htonl(INADDR_ANY); // should be 0.0.0.0 (broadcast/any)
    http_server->address.sin_port = htons(port);

    // set socket options
    // REUSEADDR: 
    if (setsockopt(
        http_server->socket_fd,
        SOL_SOCKET,
        SO_REUSEADDR, // make optional?
        &HTTP_SETSOCKOPT_ENABLE,
        sizeof(HTTP_SETSOCKOPT_ENABLE)
    ) == -1) {
        return HTTP_ESETSOCKOPT;
    }

    return HTTP_ENOERROR;
}

/* Bind an HTTP_Server's socket and start listening. Should be accompanied by
   a call to `HTTP_stop_server` to close the stream and tidy up when done. */
enum HTTP_errno HTTP_start_server(struct HTTP_Server *http_server) {
    // TODO: create socket if it is null (if server is restarting)
    // HTTP_create_socket(http_server) as a general solution might be better?
    
    // finally bind the socket
    if (bind(
        http_server->socket_fd,
        (struct sockaddr*)&http_server->address,
        sizeof(http_server->address) // i'm pretty sure sizeof works here because
                                     // we know the type of http_server->address
    ) == -1) {
        return HTTP_EBIND;
    }

    // start listening
    if (listen(http_server->socket_fd, http_server->listen_backlog) == -1) {
        return HTTP_ELISTEN;
    }

    // success, most outputs go through *http_server
    return HTTP_ENOERROR;
}

/* Close an HTTP_Server's socket. Is a minimal wrapper over `close`. */
enum HTTP_errno HTTP_stop_server(struct HTTP_Server *http_server) {
    // NOTE: check logic in HTTP_destroy_server when modifying this
    if (close(http_server->socket_fd) == -1)
        return HTTP_ECLOSE;
}

/* Deallocate the innards of an HTTP_Server. NOTE: Does not deallocate the 
   server's struct itself, just the insides */
enum HTTP_errno HTTP_destroy_server(struct HTTP_Server *http_server) {
// this will be more important once more logic comes in
    // TODO: call HTTP_stop_server if necessary.
    enum HTTP_errno err = HTTP_stop_server(http_server);

    // potentially cover this case in HTTP_stop_server?
    // TODO: yes
    if (
        err != HTTP_ENOERROR && // if: there was an error, and
        !(err == HTTP_ECLOSE && errno == EBADF) // socket_fd existed
    ) return err; // return that err (otherwise it's expected)
        

    return HTTP_ENOERROR;
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
