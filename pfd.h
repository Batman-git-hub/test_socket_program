/*
 * pfd.h
 */

#ifndef PFD_H
#define PFD_H

#include <poll.h>

#define INITIAL_PFD_SIZE 2
#define MAX_PFD_COUNT 32

extern struct pollfd *pfd;
extern int pfd_size;
extern int pfd_count;

extern struct sockaddr_storage *pfd_addr;
extern int pfd_addr_size;


void init_pollfd();
void destroy_pollfd();

int add_fd_into_pfd(int fd, struct sockaddr_storage addr);
void delete_fd_from_pfd(int index);
void close_timeout_clients(int server_fd);

char* get_address(struct sockaddr_storage addr, char *str, int size);
char* get_client_address(int index, char *str, int size);

int get_port(struct sockaddr_storage addr);
int get_client_port(int index);

void print_byte_string(FILE *fp, char *str, int n_bytes);

#endif
