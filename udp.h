/**
 * @file udp.h
 * @brief IPK Project 1 - Chat Client
 * @author Ivan Onufriienko
 * 
*/

#include "help.h"

#define CONFIRM 0x00
#define REPLY   0x01
#define AUTH    0x02
#define JOIN    0x03
#define MSG     0x04
#define ERR     0xFE
#define BYE     0xFF

#define MAX_RECENT_MSG_IDS 65536
/**
 * @brief Function to support special case for udp when sent message needs confirmation
 * @param socket_desc_udp int socket descriptor
 * @param message char* message to be resend if needed
 * @param message_length int message length
 * @param server_addr struct sockaddr* server address
 * @param server_addr_len socklen_t server address length
 * @param expected_message_id uint16_t expected message id
 * @param timeout int timeout
 * @param retransmissions int retransmissions
*/
bool wait_for_confirmation(int socket_desc_udp,unsigned char* message, int message_length, 
                            struct sockaddr* server_addr, socklen_t server_addr_len, 
                            int timeout, int retransmissions);
/**
 * @brief Converts uint16_t number to char array
 * @param num uint16_t number
 * @param buffer char array
*/
void uint16_to_char_array(uint16_t num, char* buffer);   
/**
 * @brief Function to create confirm message
 * @param ref_message_id uint16_t reference message id
 * @param message char* placement for the message
 * @return int size of the message
 */
int create_confirm_message_udp(uint16_t ref_message_id,unsigned char* message);
/**
 * @brief Function to create auth message
 * @param message_id uint16_t message id
 * @param username char* username
 * @param display_name char* display name
 * @param secret char* secret
 * @param message char* placement for the message
 * @return int size of the message
 */
int create_auth_message_udp(uint16_t message_id, char* username, char* display_name, char* secret,unsigned char* message);
/**
 * @brief Function to create join message
 * @param message_id uint16_t message id
 * @param channel_id char* channel id
 * @param display_name char* display name
 * @param message char* placement for the message
 * @return int size of the message
 */
int create_join_message_udp(uint16_t message_id, char* channel_id, char* display_name,unsigned char* message);
/**
 * @brief Function to create msg message
 * @param message_id uint16_t message id
 * @param display_name char* display name
 * @param message_content char* message content
 * @param message char* placement for the message
 * @return int size of the message
 */
int create_msg_message_udp(uint16_t message_id, char* display_name, char* message_content,unsigned char* message);
/**
 * @brief Function to create err message
 * @param message_id uint16_t message id
 * @param display_name char* display name
 * @param message_content char* message content
 * @param message char* placement for the message
 * @return int size of the message
 */
int create_err_message_udp(uint16_t message_id, char* display_name, char* message_content,unsigned char* message);
/**
 * @brief Function to create bye message
 * @param message_id uint16_t message id
 * @param message char* placement for the message
 * @return int size of the message
 */
int create_bye_message_udp(uint16_t message_id,unsigned char* message);
/**
 * @brief Function to handle special case when server sends incorrect message
 * @param msg char* message
 * @param message char* message
 * @param ref_message_id uint16_t reference message id
 * @param timeout int timeout
 * @param retransmissions int retransmissions
*/
void error_udp(char *msg,unsigned char* message, uint16_t ref_message_id, int timeout, int retransmissions);
/**
 * @brief Function to handle command from user
 * @param command char* command
 * @param socket_desc int socket descriptor
 * @param message_id uint16_t message id
 * @param timeout int timeout
 * @param retransmissions int retransmissions
*/
void handle_command_udp(char* command, int socket_desc, uint16_t message_id, int timeout, int retransmissions);
/**
 * @brief Function to handle server reply
 * @param reply char* reply
 * @param timeout int timeout
 * @param retranmissions int retransmissions
 * @param message_id uint16_t message id
*/
void handle_server_reply_udp(char* reply, int timeout, int retranmissions);
/**
 * @brief Function to gracefully exit when SIGINT is received
 * @param signum int signal number
*/
void signal_handler_udp();
/**
 * @brief Function to connect to the server in udp mode
 * @param ipstr char* ip address
 * @param port int port
 * @param timeout int timeout
 * @param retransmissions int retransmissions
 * @return int exit code
*/
int udp_connect(char* ipstr, int port, int timeout, int retransmissions);