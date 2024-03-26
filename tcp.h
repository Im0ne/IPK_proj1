/**
 * @file tcp.h
 * @brief IPK Project 1 - Chat Client
 * @author Ivan Onufriienko
 * 
*/

#include <help.h>


char* create_auth_message_tcp(char* username, char* display_name, char* secret);

char* create_join_message_tcp(char* channel_id, char* display_name);

char* create_msg_message_tcp(char* display_name, char* message_content);

char* create_err_message_tcp(char* display_name, char* message_content);

char* create_bye_message_tcp();

void handle_command_tcp(char* command, int socket_desc, char* display_name, fd_set* fds);

void handle_server_reply_tcp(char* reply, fd_set* fds);

void signal_handler_tcp(int signum);

int tcp_connect(char* ipstr, int port);