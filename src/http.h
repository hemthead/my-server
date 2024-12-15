#ifndef _HTTP_SERVER_H
#define _HTTP_SERVER_H 1

#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* The following constants represent the error types that may be encountered.
   Negative errors are system errors that already use `errno`.
   The value `0` represents a non-error / the null error.
   Positive errors are custom errors that may be encountered. */
enum HTTP_ErrType {
    // errno errors, check `info.errnum`
    HTTP_ECLOSE = -5, /* `close` encountered a problem. */
    HTTP_ELISTEN = -4, /* `listen` encountered a problem. */
    HTTP_EACCEPT = -3, /* `accept` encountered a problem. */
    HTTP_EALLOC = -2, /* An allocator function encountered a problem. */
    HTTP_ERECV = -1,

    // non-error / Ok
    HTTP_ENOERROR = 0, /* default, result when no error occured. */

    HTTP_ENOSOCKET, /* No properly bindable socket was found. */

    HTTP_EGETADDRINFO, /* `getaddrinfo` error occured, check `info.gai_errcode`. */
};

union HTTP_ErrInfo {
    int errnum; /* the value of `errno` when an error occured. */
    int gai_errcode; /* a `getaddrinfo` error return value. */
};

struct HTTP_Error {
    union HTTP_ErrInfo info;
    enum HTTP_ErrType type;
};

/* Gets the string representation of an HTTP_errno. */
const char *HTTP_strerror(struct HTTP_Error err);

/* Prints an HTTP_errno to stderr (like perror). */
void HTTP_perror(const char *s, struct HTTP_Error err);

/* This struct represents an HTTP server. */
struct HTTP_Server {
    int socket; /* The file descriptor of the server's socket. */
    struct sockaddr_in address; /* The internet address that the server is 
                                   bound to. */
};

/* Create a new HTTP_Server that listens on the given port/service.
   An uninitialized HTTP_Server is intended to be passed as `http_server`. `port`
   is a decimal port number as a string (is passed to `getaddrinfo` so service
   name works as well). Returns any errors encountered, as an HTTP_errno.
   Create's the HTTP_Server's socket and address and sets its socket options.
   Should be accompanied by a call to `HTTP_destroy_server` to tidy up when done. */
struct HTTP_Error HTTP_Server_init(
    struct HTTP_Server *server,
    const char *restrict port
);

/* Shut of and deallocate the innards of an HTTP_Server. NOTES: Does not deallocate the 
   server struct itself, just the insides; does not handle EINTR automatically. */
struct HTTP_Error HTTP_Server_deinit(struct HTTP_Server *http_server);

// this allows the user to control the buf size by defining it prior to including this file
#ifndef _HTTP_CONNECTION_BUF_SIZE
#define _HTTP_CONNECTION_BUF_SIZE 1024
#endif // _HTTP_CONNECTION_BUF_SIZE
/* Represents an HTTP connection to a server client. */
struct HTTP_Connection {
    int socket; /* The file descriptor of the connection to the client. */

    /* The below are used as in `accept`. */
    struct sockaddr_storage address; /* The address of the connecting socket. */
    socklen_t address_len; /* The length of the above address. */

    /* Buffer variables to control reading from the stream. */
    char *buf; /* A buffer to read request data from. */
    size_t buf_size; /* The length of the buffer. */
    size_t read_index; /* The current index of the buffer to read from. */
    size_t bytes_received; /* The number of bytes last read from the stream. */
};

/* Accept an `HTTP_Connection` from an `HTTP_Server`. */
struct HTTP_Error HTTP_Server_accept(
    struct HTTP_Server *server,
    struct HTTP_Connection *connection
);

/* An HTTP Request message. */
struct HTTP_Request {
    char *data; /* The data in the message. */
    size_t _data_cap; /* The size of the allocated `data` array. */
    size_t _data_len;

    size_t header_index; /* The index where the header starts. */
    size_t body_index; /* The index where the body starts, invalid if `content_length == 0`. */
    size_t content_length; /* The length of the body content, as specified by
                              the `Content-Length` header's value. */
};

/* Recieve an `HTTP_Request` from an `HTTP_Connection`. */
struct HTTP_Error HTTP_Connection_recv(
    struct HTTP_Connection *connection,
    struct HTTP_Request *request
);

#endif /* _HTTP_SERVER_H */
