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

#define MAX_RECENT_MSG_IDS 100

int create_confirm_message_udp(uint16_t ref_message_id, char* message);

int create_auth_message_udp(uint16_t message_id, char* username, char* display_name, char* secret, char* message);

int create_join_message_udp(uint16_t message_id, char* channel_id, char* display_name, char* message);

int create_msg_message_udp(uint16_t message_id, char* display_name, char* message_content, char* message);

int create_err_message_udp(uint16_t message_id, char* display_name, char* message_content, char* message);

int create_bye_message_udp(uint16_t message_id, char* message);

void handle_command_udp(char* command, int socket_desc, char* display_name, uint16_t message_id, int timeout, int retransmissions);

void handle_server_reply_udp(char* reply, int timeout, int retranmissions, int message_id);

void signal_handler_udp(int signum);

int udp_connect(char* ipstr, int port, int timeout, int retransmissions);