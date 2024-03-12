/**
 * @file tcp.h
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

#define MAX_EVENTS 5
#define MAX_CONTENT 1400
#define MAX_DNAME 20
#define MAX_ID 20
#define MAX_SECRET 128



char* create_auth_message(char* username, char* display_name, char* secret);

char* create_join_message(char* channel_id, char* display_name);

char* create_msg_message(char* display_name, char* message_content);

char* create_err_message(char* display_name, char* message_content);

char* create_reply_message(char* status, char* message_content);

char* create_bye_message();

void handle_command(char* command, int socket_desc, char* display_name);

void print_help_tcp();

void cleanup(int socket_desc, int epollfd);

int tcp_connect(char* ipstr, int port);