/**
 * @file help.h
 * @brief IPK Project 1 - Chat Client
 * @author Ivan Onufriienko
 * 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>	
#include <sys/epoll.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define MAX_EVENTS 1

#define MAX_DNAME 20
#define MAX_ID 20
#define MAX_SECRET 128
#define MAX_USERNAME 20
#define MAX_CONTENT 1400

/**
 * @brief Function to check if string is alphanumeric or dash
 * @param str const char* string
 * @return bool true if string is alphanumeric or dash, false otherwise
*/
bool is_alnum_or_dash(const char *str);
/**
 * @brief Function to check if string is alphanumeric
 * @param str const char* string
 * @return bool true if string is alphanumeric, false otherwise
*/
bool is_print_or_space(const char *str);
/**
 * @brief Function to print client usage
*/
void print_help_client();
/**
 * @brief Function to close descriptor and epoll if needed
*/
void cleanup(int socket_desc, int epollfd);