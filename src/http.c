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
#include "search.h"
    
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

struct HTTP_Error HTTP_Server_accept(
    struct HTTP_Server *server,
    struct HTTP_Connection *connection
) {
    // create the error to return
    struct HTTP_Error error;
    memset(&error, 0, sizeof(error));

    // accept a connection to the server, returning error if necessary
    connection->address_len = sizeof(connection->address);
    if (accept(
        server->socket,
        (struct sockaddr *)&connection->address,
        &connection->address_len
    ) == -1) {
        error.type = HTTP_EACCEPT;
        error.info.errnum = errno;
        return error;
    }

    // initialize the buffer variables of the connection
    connection->buf_size = _HTTP_CONNECTION_BUF_SIZE;
    connection->buf = malloc(connection->buf_size);
    if (connection->buf == NULL) {
        error.type = HTTP_EALLOC;
        error.info.errnum = errno;
        return error;
    }

    connection->read_index = 0;
    connection->bytes_received = 0;

    return error;
}

struct HTTP_Error HTTP_Request_deinit(struct HTTP_Request *request) {
    // create the error to return
    struct HTTP_Error error;
    memset(&error, 0, sizeof(error));

    free(request->data);
    request->_data_cap = 0;
    request->_data_len = 0;

    return error;
}

struct HTTP_Error read_status_and_headers(
    struct HTTP_Connection *connection,
    struct HTTP_Request *request,
    size_t max_request_size
) {
    // create the error to return
    struct HTTP_Error error;
    memset(&error, 0, sizeof(error));

    const char needle[] = {'\r', '\n', '\r', '\n'}; // needle to search for (where headers end)
    char cut_buf[2*sizeof(needle) - 2]; memset(&cut_buf, 0, sizeof(needle));
    
    // read status-line and headers into request data
    while (request->_data_cap <= max_request_size) {
        size_t header_end = 0;
        
        // search has to return the index after the end of the needle (that way 0 is err)
        // look into Rabin-Karp (https://en.wikipedia.org/wiki/Rabin%E2%80%93Karp_algorithm)
        // use the cut-off buffer if we've just `recv`ed more data
        if (connection->read_index == 0) {
            memcpy(cut_buf+(sizeof(needle)-1), connection->buf, sizeof(needle)-1);
            header_end = search(cut_buf, sizeof(cut_buf), needle, sizeof(needle));

            // exit the loop if we've found the end of the headers
            if (header_end != 0) {
                // add data to request, update read_index

                // make sure we can fit the data into the request's array
                while (request->_data_cap - request->_data_len <= sizeof(needle) - 1) {
                    request->_data_cap *= 2;
                    char *new_data_ptr = reallocarray(request->data, request->_data_cap, sizeof(char));
                    if (new_data_ptr == NULL) {
                        error.type = HTTP_EALLOC;
                        error.info.errnum = errno;
                        return error;
                    }
                    request->data = new_data_ptr;
                }
                memcpy(request->data+request->_data_len, cut_buf + ((header_end - 1) - sizeof(needle)), sizeof(needle));
                connection->read_index = header_end - (sizeof(needle) - 1);
                break;
            }
        }

        // search the entire string for \r\n\r\n
        header_end = search(connection->buf + connection->read_index, connection->bytes_received - connection->read_index, needle, sizeof(needle)); 

        // exit the loop if we've found the end of the headers
        if (header_end != 0) {
            // add data to request, update read_index
            // make sure we can fit the data into the request's array
            while (request->_data_cap - request->_data_len <= sizeof(needle) - 1) {
                request->_data_cap *= 2;
                char *new_data_ptr = reallocarray(request->data, request->_data_cap, sizeof(char));
                if (new_data_ptr == NULL) {
                    error.type = HTTP_EALLOC;
                    error.info.errnum = errno;
                    return error;
                }
                request->data = new_data_ptr;
            }

            memcpy(request->data+request->_data_len, cut_buf + connection->read_index, connection->bytes_received - connection->read_index);
            connection->read_index = header_end;
            break;
        }

        // header has not yet ended
        if (header_end == 0) {
            // dump data into request, put the end in a buffer to check for cut-off needle, recv new data

            // make sure we can fit the data into the request's array
            while (request->_data_cap - request->_data_len <= sizeof(needle) - 1) {
                request->_data_cap *= 2;
                char *new_data_ptr = reallocarray(request->data, request->_data_cap, sizeof(char));
                if (new_data_ptr == NULL) {
                    error.type = HTTP_EALLOC;
                    error.info.errnum = errno;
                    return error;
                }
                request->data = new_data_ptr;
            }

            memcpy(request->data+request->_data_len, cut_buf + connection->read_index, connection->bytes_received - connection->read_index);

            // recieve more data
            connection->bytes_received = recv(connection->socket, connection->buf, connection->buf_size, 0); // consider some flags here
            if (connection->bytes_received == -1) {
                error.type = HTTP_ERECV;
                error.info.errnum = errno;
                return error;
            }

            connection->read_index = 0;
        }
    }
}

struct HTTP_Error request_identify(struct HTTP_Request *request) {

}

struct HTTP_Error HTTP_Connection_recv(
    struct HTTP_Connection *connection,
    struct HTTP_Request *request
) {
    // create the error to return
    struct HTTP_Error error;
    memset(&error, 0, sizeof(error));

    // init the request
    memset(request, 0, sizeof(*request));
    request->_data_cap = 1024; // start off with 1KiB of space
    request->data = calloc(sizeof(char), request->_data_cap);

    const size_t MAX_REQUEST_SIZE = 1024 * 8; // 8KiB at most

    // Status-Line = METHOD URI VERSION "\r\n"
    // Headers = *( TOKEN ":" VALUE "\r\n" ) "\r\n" (technically a header can span multiple lines if it \t tabs)
    // Body = Content-Length( OCTET )
    
    // read until \r\n\r\n
    error = read_status_and_headers(connection, request, MAX_REQUEST_SIZE);
    if (error.type != HTTP_ENOERROR)
        return error;

    // identify METHOD URI Content-Length etc.
    error = request_identify(request);
    if (error.type != HTTP_ENOERROR)
        return error;

    // (We're only rocking HTTP/1.0, so we don't worry about content-type: chunked)
    
    // read body

    return error;
}


