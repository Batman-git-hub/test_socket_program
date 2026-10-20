/*
 * pfd.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <arpa/inet.h>

#include <pfd.h>

struct pollfd *pfd;
int pfd_size;
int pfd_count;

struct sockaddr_storage *pfd_addr;
int pfd_addr_size;


void
init_pollfd()
{
    pfd = malloc(INITIAL_PFD_SIZE * sizeof(*pfd));

    pfd_addr = malloc(INITIAL_PFD_SIZE * sizeof(*pfd_addr));

    if (pfd == NULL || pfd_addr == NULL) {
        fprintf(stderr, "malloc: %s\n", strerror(errno));
        destroy_pollfd();
        exit(-1);
    }

    pfd_size = INITIAL_PFD_SIZE;
    pfd_count = 0;

    pfd_addr_size = INITIAL_PFD_SIZE;
}

void
destroy_pollfd()
{
    char ip_addr[INET6_ADDRSTRLEN];
    if (pfd != NULL) {
        for (int index = pfd_count - 1; index >= 0; index--) {
            fprintf(stdout, "Terminating connection -> %s:%d\n"
                , (get_client_address(index, ip_addr, sizeof(ip_addr)) == NULL) ? "unknown" : ip_addr
                , get_client_port(index)
            );
            close(pfd[index].fd);
        }
        free(pfd);
        pfd = NULL;

    }

    if (pfd_addr != NULL) {
        free(pfd_addr);
        pfd_addr = NULL;
    }

    pfd_size = 0;
    pfd_count = 0;

    pfd_addr_size = 0;
}

int
add_fd_into_pfd(int fd, struct sockaddr_storage addr)
{
    if (fd < 0 || pfd == NULL || pfd_addr == NULL) {
        return -1;
    }

    if (pfd_count < 0 || pfd_count >= MAX_PFD_COUNT) {
        return -1;
    }

    if (pfd_count == pfd_size) {
        int new_size = (pfd_size > 0) ? pfd_size * 2 : INITIAL_PFD_SIZE;
        void *pointer = realloc(pfd, new_size * sizeof(*pfd));
        if (pointer == NULL) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            return -1;
        }
        pfd = pointer;
        pfd_size = new_size;
    }

    if (pfd_count == pfd_addr_size) {
        int new_size = (pfd_addr_size > 0) ? pfd_addr_size * 2 : INITIAL_PFD_SIZE;
        void *pointer = realloc(pfd_addr, new_size * sizeof(*pfd_addr));
        if (pointer == NULL) {
            fprintf(stderr, "realloc: %s\n", strerror(errno));
            return -1;
        }
        pfd_addr = pointer;
        pfd_addr_size = new_size;
    }

    pfd[pfd_count].fd = fd;
    pfd[pfd_count].events = POLLIN;
    pfd[pfd_count].revents = 0;

    pfd_addr[pfd_count] = addr;

    pfd_count++;

    return 0;
}

void
delete_fd_from_pfd(int index)
{
    int fd;

    if (index < 0 || index >= pfd_count) {
        return;
    }

    fd = pfd[index].fd;

    pfd[index] = pfd[pfd_count - 1];
    pfd[pfd_count - 1].fd = -1;

    pfd_addr[index] = pfd_addr[pfd_count - 1];

    pfd_count--;

    close(fd);
}

void
close_timeout_clients(int server_fd)
{
    char ip_addr[INET6_ADDRSTRLEN];
    for (int index = pfd_count - 1; index >= 0; index--) {
        if (pfd[index].fd == server_fd) {
            continue;
        }
        fprintf(stdout, "Disconnecting client -> %s:%d\n"
        , (get_client_address(index, ip_addr, sizeof(ip_addr)) == NULL) ? "unknown" : ip_addr
        , get_client_port(index)
        );
        delete_fd_from_pfd(index);
    }
}

char*
get_client_address(int index, char *str, int size)
{
    if (index < 0 || index >= pfd_count) {
        return NULL;
    }
    return get_address(pfd_addr[index], str, size);
}

char*
get_address(struct sockaddr_storage addr, char *str, int size)
{
    void *src;

    if (str == NULL || size < INET6_ADDRSTRLEN) {
        return NULL;
    }

    if (addr.ss_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in*)&addr;
        src = &(ipv4->sin_addr);
    } else if (addr.ss_family == AF_INET6) {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)&addr;
        src = &(ipv6->sin6_addr);
    } else {
        fprintf(stderr, "error: unknown address family\n");
        return NULL;
    }

    if (inet_ntop(addr.ss_family, src, str, size) == NULL) {
        fprintf(stderr, "inet_ntop: %s\n", strerror(errno));
        return NULL;
    }

    return str;
}

int
get_client_port(int index)
{
    return get_port(pfd_addr[index]);
}

int
get_port(struct sockaddr_storage addr)
{
    return ntohs(((struct sockaddr_in*)&addr)->sin_port);
}

void 
print_byte_string(FILE *fp, char *str, int n_bytes)
{
    for (int index = 0; index < n_bytes; index++) {
        if (str[index] >= 32 && str[index] <= 126) {
            fprintf(fp, "%c", str[index]);
        } else if (str[index] == '\r' || str[index] == '\n') {
            fprintf(fp, "%c", str[index]);
        } else {
            fprintf(fp, "\\x%02X", str[index]);
        }
    }
}
