/*
 * server.c
 *
 * Compile: gcc -Wall -I. server.c pfd.c -o server
 * Run: ./server 8080
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>

#include <sys/socket.h>
#include <arpa/inet.h>

#include <pfd.h>

#define BUFFER_SIZE (8 * 1024)
#define LISTEN_BACKLOG 10
#define POLL_TIMEOUT (100 * 1000)

/* function declaration */
int init_server(int port, struct sockaddr_in *addr);
int handle_new_connection(int server_fd);
int handle_client_data(int index);

void handle_signal(int sig);


/* global variables */
char buffer[BUFFER_SIZE];


int
init_server(int port, struct sockaddr_in *addr)
{
    int fd, optval = 1;

    if (addr == NULL) {
        exit(-1);
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "socket: %s\n", strerror(errno));
        exit(-1);
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) != 0) {
        fprintf(stderr, "setsockopt: %s\n", strerror(errno));
        close(fd);
        exit(-1);
    }

    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr*)addr, sizeof(*addr)) != 0) {
        fprintf(stderr, "bind: %s\n", strerror(errno));
        close(fd);
        exit(-1);
    }

    if (listen(fd, LISTEN_BACKLOG) != 0) {
        fprintf(stderr, "listen: %s\n", strerror(errno));
        close(fd);
        exit(-1);
    }

    return fd;
}

int
handle_new_connection(int server_fd)
{
    int client_fd;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char ip_addr[INET6_ADDRSTRLEN];

    client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_fd < 0) {
        fprintf(stderr, "accept: %s\n", strerror(errno));
        return -1;
    }

    fprintf(stdout, "\n");
    fprintf(stdout, "New connection (ip-address: %s, port: %d)\n"
        , (get_address(client_addr, ip_addr, sizeof(ip_addr)) == NULL) ? "unknown" : ip_addr
        , get_port(client_addr)
    );

    if (add_fd_into_pfd(client_fd, client_addr) != 0) {
        fprintf(stderr, "Unable to add new connection\n");
        fprintf(stderr, "Connection Rejected\n");
        close(client_fd);
        return -1;
    }

    // fprintf(stderr, "Connection Accepted\n");

    return 0;
}

int
handle_client_data(int index)
{
    int client_fd, recv_bytes, send_bytes;
    char ip_addr[INET6_ADDRSTRLEN];
    char *response = "HTTP/1.0 200 OK\r\n"
        "Server: your_dad\r\n"
        "Content-type: text/plain\r\n"
        "Content-length: 18\r\n"
        "\r\n"
        "Shut the fuck up\r\n";

    if (index < 0 || index >= pfd_count) {
        return -1;
    }

    client_fd = pfd[index].fd;

    fprintf(stdout, "\n");
    recv_bytes = recv(client_fd, buffer, BUFFER_SIZE, 0);
    if (recv_bytes < 0) {
        fprintf(stderr, "recv: %s\n", strerror(errno));
        return -1;
    } else if (recv_bytes == 0) {
        fprintf(stdout, "Client disconnected -> %s:%d\n"
            , (get_client_address(index, ip_addr, sizeof(ip_addr)) == NULL) ? "unknown" : ip_addr
            , get_client_port(index)
        );
        delete_fd_from_pfd(index);
        return 0;
    }
    fprintf(stdout, "From -> %s:%d\n"
        , (get_client_address(index, ip_addr, sizeof(ip_addr)) == NULL) ? "unknown" : ip_addr
        , get_client_port(index)
    );
    fprintf(stdout, "Received: %d bytes\n", recv_bytes);
    // fprintf(stdout, "%.*s\n", recv_bytes, buffer);
    print_byte_string(stdout, buffer, recv_bytes);

    fprintf(stdout, "\n");
    send_bytes = send(client_fd, response, strlen(response), MSG_NOSIGNAL);
    if (send_bytes < 0) {
        fprintf(stderr, "send: %s\n", strerror(errno));
    } else if (send_bytes == 0) {
        fprintf(stdout, "Sent: %d bytes\n", send_bytes);
    } else {
        if (send_bytes < strlen(response)) {
            fprintf(stdout, "Sent: %d bytes (partial)\n", send_bytes);
            // fprintf(stdout, "Response sent partially\n");
        } else {
            fprintf(stdout, "Sent: %d bytes\n", send_bytes);
            // fprintf(stdout, "Response sent successfully\n");
        }
    }

    fprintf(stdout, "Disconnecting client\n");
    /*
    fprintf(stdout, "Disconnecting client -> %s:%d\n"
        , (get_client_address(index, ip_addr, sizeof(ip_addr)) == NULL) ? "unknown" : ip_addr
        , get_client_port(index)
    );
    */
    delete_fd_from_pfd(index);

    return 0;
}

void
handle_signal(int sig)
{
    fprintf(stdout, "\n");
    fprintf(stdout, "sigint (ctrl+c) occurred. Terminating the program\n");
    destroy_pollfd();
    exit(0);
}

int
main(int argc, char *argv[])
{
    int server_fd, server_port;
    struct sockaddr_storage server_addr;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return -1;
    }

    if (sscanf(argv[1], "%d", &server_port) != 1 || server_port <= 0) {
        fprintf(stderr, "Invalid port\n");
        return -1;
    }

    signal(SIGINT, handle_signal);

    init_pollfd();
    server_fd = init_server(server_port, (struct sockaddr_in*)&server_addr);
    if (server_fd < 0) {
        return -1;
    }

    fprintf(stdout, "server started listening on port: %d\n", server_port);

    if (add_fd_into_pfd(server_fd, server_addr) != 0) {
        destroy_pollfd();
        close(server_fd);
        exit(-1);
    }

    while (1) {
        int return_value = poll(pfd, pfd_count, POLL_TIMEOUT);
        if (return_value < 0) {
            fprintf(stderr, "poll: %s\n", strerror(errno));
            break;
        }
        if (return_value > 0) {
            for (int index = pfd_count - 1; index >= 0; index--) {
                if (pfd[index].revents & (POLLIN | POLLHUP)) {
                    if (pfd[index].fd == server_fd) {
                        handle_new_connection(server_fd);
                    } else {
                        handle_client_data(index);
                    }
                }
            }
        }
        if (return_value == 0) {
            fprintf(stdout, "poll timeout occurred\n");
            close_timeout_clients(server_fd);
        }
    }

    destroy_pollfd();

    return 0;
}
