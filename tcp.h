/**
 * @file tcp.h
 * @brief IPK Project 1 - Chat Client
 * @author Ivan Onufriienko
 * 
*/

#include "help.h"

/**
 * @brief Function to create confirm message
 * @param ref_message_id uint16_t reference message id
 * @return char* message
 */
char* create_auth_message_tcp(char* username, char* display_name, char* secret);
/**
 * @brief Function to create join message
 * @param channel_id char* channel id
 * @param display_name char* display name
 * @return char* message
 */
char* create_join_message_tcp(char* channel_id, char* display_name);
/**
 * @brief Function to create message message
 * @param display_name char* display name
 * @param message_content char* message content
 * @return char* message
 */
char* create_msg_message_tcp(char* display_name, char* message_content);
/**
 * @brief Function to create error message
 * @param display_name char* display name
 * @param message_content char* message content
 * @return char* message
 */
char* create_err_message_tcp(char* display_name, char* message_content);
/**
 * @brief Function to create bye message
 * @return char* message
 */
char* create_bye_message_tcp();
/**
 * @brief Function to handle command from user
 * @param command char* command
 * @param socket_desc int socket descriptor
 */
void handle_command_tcp(char* command, int socket_desc);
/**
 * @brief Function to handle server reply
 * @param reply char* reply
 */
void handle_server_reply_tcp(char* reply);
/**
 * @brief Function to handle special case when server sends incorrect message
 * @param msg char* message
 * @param message char* message
 * @param ref_message_id uint16_t reference message id
 * @param timeout int timeout
 * @param retransmissions int retransmissions
*/
void error_tcp(char *msg);
/**
 * @brief Function to gracefully exit when SIGINT is raised
 */
void signal_handler_tcp();
/**
 * @brief Function to connect to the server via TCP
 * @param ipstr char* ip address
 * @param port int port
 * @return int exit code
 */
int tcp_connect(char* ipstr, int port);