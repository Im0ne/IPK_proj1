/**
 * @file udp.c
 * @brief IPK Project 1 - Chat Client
 * @author Ivan Onufriienko
 * 
*/


#include "udp.h"
/*--------Global variables--------*/ 

// Saving display name for the whole session
char global_display_name_udp[MAX_DNAME] = "me";    
// Used to check in what state the user is
char last_command_udp[MAX_DNAME] = "";
// Flag for checking if user is authenticated   
bool authenticated_udp = false;
// Some allocations for graceful exit
int socket_desc_udp = -1;                   
int epollfd_udp = -1;                    
// Blockade of stdin after /auth and /join
struct epoll_event ev_udp;
struct sockaddr_in server_addr;
// UDP issue smh smh
uint16_t message_id = 0;
uint16_t recent_msg_ids[MAX_RECENT_MSG_IDS] = {0};
int recent_msg_ids_index = 0;

void uint16_to_char_array(uint16_t value, char* char_array) {
    char_array[0] = value >> 8; // High byte
    char_array[1] = value; // Low byte
}

bool wait_for_confirmation(int socket_desc_udp, char* message, int message_length, struct sockaddr* server_addr, socklen_t server_addr_len, uint16_t expected_message_id, int timeout, int retransmissions) {
    char buffer[1500];
    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);

    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = timeout;

    for (int i = 0; i < retransmissions; i++) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(socket_desc_udp, &fds);

        int activity = select(socket_desc_udp + 1, &fds, NULL, NULL, &tv);

        if (activity > 0 && FD_ISSET(socket_desc_udp, &fds)) {
            ssize_t numBytes = recvfrom(socket_desc_udp, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&sender_addr, &sender_addr_len);
            if (numBytes < 0) {
                fprintf(stderr, "ERR: Cannot receive message\n");
                cleanup(socket_desc_udp, epollfd_udp);
                return false;
            }

            // Check if the received message is a confirmation message with the expected message ID
            if (buffer[0] == 0x00 && ((uint8_t)buffer[1] << 8 | (uint8_t)buffer[2]) == expected_message_id) {
                return true;
            }
        } else if (activity == 0) {
            // Timeout, need to retransmit the message
            if (sendto(socket_desc_udp, message, message_length, 0, server_addr, server_addr_len) < 0) {
                fprintf(stderr, "ERR: Cannot retransmit message\n");
                cleanup(socket_desc_udp, epollfd_udp);
                return false;
            }
        } else {
            fprintf(stderr, "ERR: select error\n");
            cleanup(socket_desc_udp, epollfd_udp);
            return false;
        }
    }

    return false;
}

int create_confirm_message_udp(uint16_t ref_message_id, char* message) {
    char ref_message_id_char[2];
    uint16_to_char_array(ref_message_id, ref_message_id_char);

    int index = 0;
    message[index++] = CONFIRM; 
    memcpy(message + index, ref_message_id_char, 2);
    index += 2;

    return index;
}

int create_auth_message_udp(uint16_t message_id, char* username, char* display_name, char* secret, char* message) {
    char message_id_char[2];
    uint16_to_char_array(message_id, message_id_char);

    int index = 0;
    message[index++] = AUTH; 
    memcpy(message + index, message_id_char, 2);
    index += 2;

    memcpy(message + index, username, strlen(username));
    index += strlen(username);

    message[index++] = '\0'; 

    memcpy(message + index, display_name, strlen(display_name));
    index += strlen(display_name);

    message[index++] = '\0'; 

    memcpy(message + index, secret, strlen(secret));
    index += strlen(secret);

    message[index++] = '\0'; 

    return index;
}

int create_join_message_udp(uint16_t message_id, char* channel_id, char* display_name, char* message) {
    char message_id_char[2];
    uint16_to_char_array(message_id, message_id_char);

    int index = 0;
    message[index++] = JOIN; 
    memcpy(message + index, message_id_char, 2);
    index += 2;

    memcpy(message + index, channel_id, strlen(channel_id));
    index += strlen(channel_id);

    message[index++] = '\0'; 

    memcpy(message + index, display_name, strlen(display_name));
    index += strlen(display_name);

    message[index++] = '\0'; 
    return index;
}

int create_msg_message_udp(uint16_t message_id, char* display_name, char* message_content, char* message) {
    char message_id_char[2];
    uint16_to_char_array(message_id, message_id_char);

    int index = 0;
    message[index++] = MSG; 
    memcpy(message + index, message_id_char, 2);
    index += 2;

    memcpy(message + index, display_name, strlen(display_name));
    index += strlen(display_name);

    message[index++] = '\0'; 

    memcpy(message + index, message_content, strlen(message_content));
    index += strlen(message_content);

    message[index++] = '\0'; 

    return index;
}

int create_err_message_udp(uint16_t message_id, char* display_name, char* message_content, char* message) {
    char message_id_char[2];
    uint16_to_char_array(message_id, message_id_char);

    int index = 0;
    message[index++] = ERR; 
    memcpy(message + index, message_id_char, 2);
    index += 2;

    memcpy(message + index, display_name, strlen(display_name));
    index += strlen(display_name);

    message[index++] = '\0'; 

    memcpy(message + index, message_content, strlen(message_content));
    index += strlen(message_content);

    message[index++] = '\0'; 

    return index;
}

int create_bye_message_udp(uint16_t message_id, char* message) {
    char message_id_char[2];
    uint16_to_char_array(message_id, message_id_char);

    int index = 0;
    message[index++] = BYE; 
    memcpy(message + index, message_id_char, 2);
    index += 2;

    return index;
}

void error_udp(char *msg, char* message, uint16_t ref_message_id, int timeout, int retransmissions) {
    fprintf(stderr, "ERR: %s\n", msg);
    int err_length = create_err_message_udp(ref_message_id, global_display_name_udp, msg , message);
    sendto(socket_desc_udp, message, err_length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (!wait_for_confirmation(socket_desc_udp, message, err_length, (struct sockaddr*)&server_addr, sizeof(server_addr), message_id - 1, timeout, retransmissions)) {
        fprintf(stderr, "ERR: Did not receive confirmation from server\n");
        cleanup(socket_desc_udp, epollfd_udp);
        exit(EXIT_FAILURE);
    }
    int message_length = create_bye_message_udp(message_id++, message);
    sendto(socket_desc_udp, message, message_length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (!wait_for_confirmation(socket_desc_udp, message, message_length, (struct sockaddr*)&server_addr, sizeof(server_addr), message_id - 1, timeout, retransmissions)) {
        fprintf(stderr, "ERR: Did not receive confirmation from server\n");
        cleanup(socket_desc_udp, epollfd_udp);
        exit(EXIT_FAILURE);
    }
    cleanup(socket_desc_udp, epollfd_udp);
    exit(EXIT_SUCCESS);
}

void handle_command_udp(char* command, int socket_desc_udp, char* display_name, uint16_t message_id, int timeout, int retransmissions, fd_set* fds) {
    strncpy(last_command_udp, command, 7);
    char extra[MAX_CONTENT] = "";
    char message[1500];
    if (strncmp(command, "/auth", 5) == 0) {
        if (authenticated_udp) {
            fprintf(stderr, "ERR: You are already authenticated\n");
            return;
        }
        char username[MAX_USERNAME] = "", secret[MAX_SECRET] = "", display_name[MAX_DNAME] = "";
        sscanf(command, "/auth %s %s %s %99[^\n]", username, secret, display_name, extra);
        if (strlen(username) == 0 || strlen(secret) == 0 || strlen(display_name) == 0 || strlen(extra) > 0
            || !is_alnum_or_dash(username) || !is_alnum_or_dash(secret) || !is_print_or_space(display_name)) {
            fprintf(stderr, "ERR: Invalid parameters for /auth\n");
            return;
        }
        strncpy(global_display_name_udp, display_name, MAX_DNAME);
        int message_length = create_auth_message_udp(message_id++, username, display_name, secret, message);
        sendto(socket_desc_udp, message, message_length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        FD_CLR(STDIN_FILENO, fds);
        if (!wait_for_confirmation(socket_desc_udp, message, message_length, (struct sockaddr*)&server_addr, sizeof(server_addr), message_id - 1, timeout, retransmissions)) {
            fprintf(stderr, "ERR: Did not receive confirmation from server\n");
            cleanup(socket_desc_udp, epollfd_udp);
            exit(EXIT_FAILURE);
        }
    } else if (strncmp(command, "/join", 5) == 0) {
        if(!authenticated_udp){
            fprintf(stderr, "ERR: You must authenticate first\n");
            return;
        }
        char channel_id[MAX_ID] = "";
        sscanf(command, "/join %s %99[^\n]", channel_id, extra);
        if (strlen(channel_id) == 0 || strlen(extra) > 0 || !is_alnum_or_dash(channel_id)) {
            fprintf(stderr, "ERR: Invalid parameters for /join\n");
            return;
        }
        int message_length = create_join_message_udp(message_id++, channel_id, global_display_name_udp, message);
        sendto(socket_desc_udp, message, message_length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        FD_CLR(STDIN_FILENO, fds);
        if (!wait_for_confirmation(socket_desc_udp, message, message_length, (struct sockaddr*)&server_addr, sizeof(server_addr), message_id - 1, timeout, retransmissions)) {
            fprintf(stderr, "ERR: Did not receive confirmation from server\n");
            exit(EXIT_FAILURE);
        }
    } else if (strncmp(command, "/rename", 7) == 0) {
        if(!authenticated_udp){
            fprintf(stderr, "ERR: You must authenticate first\n");
            return;
        }
        char display_name[MAX_DNAME] = "";
        sscanf(command, "/rename %s %99[^\n]", display_name, extra);
        if (strlen(display_name) == 0 || strlen(extra) > 0 || !is_print_or_space(display_name)) {
            fprintf(stderr, "ERR: Invalid parameters for /rename\n");
            return;
        }
        strncpy(global_display_name_udp, display_name, MAX_DNAME);
    } else if (strncmp(command, "/help", 5) == 0) {
        sscanf(command, "/help %99[^\n]", extra);
        if(strlen(extra) > 0){
            fprintf(stderr, "ERR: Invalid parameters for /help\n");
            return;
        }
        print_help_client();      
    } else {
        fprintf(stderr,"ERR: Unknown command: %s\n", command);
    }
}

void handle_server_reply_udp(char* reply, int timeout, int retransmissions, int message_id, fd_set* fds) {
    uint16_t ref_message_id = (uint8_t)reply[1] << 8 | (uint8_t)reply[2];
    if (reply[0] == CONFIRM) {
        
        return;
    } 
    // Check if the message ID is in the list of recent message IDs
    char message[1500];
    for (int i = 0; i < MAX_RECENT_MSG_IDS; i++) {
        if (recent_msg_ids[i] == ref_message_id) {
            // This is a duplicate message, send a confirmation and return
            int confirmation_lenght = create_confirm_message_udp(ref_message_id, message);
            sendto(socket_desc_udp, message, confirmation_lenght, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
            return;
        }
    }

    // This is a new message, add its ID to the list of recent message IDs
    recent_msg_ids[recent_msg_ids_index] = ref_message_id;
    recent_msg_ids_index = (recent_msg_ids_index + 1) % MAX_RECENT_MSG_IDS;
    int confirmation_lenght = create_confirm_message_udp(ref_message_id, message);
    sendto(socket_desc_udp, message, confirmation_lenght, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    
    if (reply[0] == REPLY) {
        uint8_t result = reply[3];
        if ((strncmp(last_command_udp, "/auth", 5) == 0) || (strncmp(last_command_udp, "/join", 5) == 0)){
            FD_SET(STDIN_FILENO, fds);
        }
        if (result == 0x01) {
            fprintf(stderr, "Success: %s\n", reply + 5);
            if (strncmp(last_command_udp, "/auth", 5) == 0) {
                authenticated_udp = true;
            }
        } else if (result == 0x00) {         
            fprintf(stderr, "Failure: %s\n", reply + 5);
        }
        else {
            error_udp("Unknown result", message, ref_message_id, timeout, retransmissions);
        }
    } else if (reply[0] == MSG) {
        printf("%s: %s\n", reply + 3, reply + 4 + strlen(reply + 3));
    } else if (reply[0] == ERR) {
        fprintf(stderr, "ERR: %s\n", reply + 5);
        int message_length = create_bye_message_udp(message_id++, message);
        sendto(socket_desc_udp, message, message_length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (!wait_for_confirmation(socket_desc_udp, message, message_length, (struct sockaddr*)&server_addr, sizeof(server_addr), message_id - 1, timeout, retransmissions)) {
            fprintf(stderr, "ERR: Did not receive confirmation from server\n");
            cleanup(socket_desc_udp, epollfd_udp);
            exit(EXIT_FAILURE);
        }
        cleanup(socket_desc_udp, epollfd_udp);
        exit(EXIT_SUCCESS);
    } else if (reply[0] == BYE) {
        cleanup(socket_desc_udp, epollfd_udp);
        exit(EXIT_SUCCESS);
    } else {
        error_udp("Unknown message type", message, ref_message_id, timeout, retransmissions);
    }

}

void signal_handler_udp(int signum) { 
    char message[1500];
    if(last_command_udp == ""){
        cleanup(socket_desc_udp, epollfd_udp);
        exit(EXIT_SUCCESS);
    }
    int message_length = create_bye_message_udp(message_id++, message);
    sendto(socket_desc_udp, message, message_length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (!wait_for_confirmation(socket_desc_udp, message, message_length, (struct sockaddr*)&server_addr, sizeof(server_addr), 0, 1000, 3)) {
        fprintf(stderr, "ERR: Did not receive confirmation from server\n");
        cleanup(socket_desc_udp, epollfd_udp);
        exit(EXIT_FAILURE);
    }
    cleanup(socket_desc_udp, epollfd_udp);
    exit(EXIT_SUCCESS);
}

int udp_connect(char* ipstr, int port, int timeout, int retransmissions) {
    
    struct timeval tv;
    fd_set fds;
    int nfds;
    
    signal(SIGINT, signal_handler_udp);
    timeout *= 1000;
    // Create socket
    if ((socket_desc_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        fprintf(stderr, "ERR: Cannot create socket\n");
        cleanup(socket_desc_udp, epollfd_udp);
        return EXIT_FAILURE;
    }

    // Set timeout
    tv.tv_sec = 0;
    tv.tv_usec = timeout;
    if (setsockopt(socket_desc_udp, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        fprintf(stderr, "ERR: Error setting timeout\n");
        cleanup(socket_desc_udp, epollfd_udp);
        return EXIT_FAILURE;
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ipstr, &server_addr.sin_addr) <= 0) {
        fprintf(stderr, "ERR: Invalid IP address\n");
        cleanup(socket_desc_udp, epollfd_udp);
        return EXIT_FAILURE;
    }

    // Epoll loop
    for (;;) {
        FD_ZERO(&fds);
        FD_SET(socket_desc_udp, &fds);
        FD_SET(STDIN_FILENO, &fds);

        nfds = select(socket_desc_udp + 1, &fds, NULL, NULL, &tv);

        if (nfds == -1) {
            fprintf(stderr, "ERR: select error\n");
            cleanup(socket_desc_udp, epollfd_udp);
            exit(EXIT_FAILURE);
        }
    
        if (FD_ISSET(socket_desc_udp, &fds)) {
            
            // Handle socket event
            char server_reply[1500];
            struct sockaddr_in sender_addr;
            socklen_t sender_addr_len = sizeof(sender_addr);
            ssize_t numBytes = recvfrom(socket_desc_udp, server_reply, sizeof(server_reply) - 1, 0, (struct sockaddr*)&sender_addr, &sender_addr_len);
            if (numBytes < 0) {
                fprintf(stderr, "ERR: Cannot receive message\n");
                cleanup(socket_desc_udp, epollfd_udp);
                return EXIT_FAILURE;
            }
            handle_server_reply_udp(server_reply, timeout, retransmissions, message_id, &fds);
        } else if (FD_ISSET(STDIN_FILENO, &fds)) {
            // Handle stdin event         
            char buffer[1500];
            char message[1500];
            if (fgets(buffer, 1500, stdin) == NULL) {
                // Ctrl+D was pressed, exit the program
                cleanup(socket_desc_udp, epollfd_udp);
                return EXIT_SUCCESS;
            }
            if(strlen(buffer) == 1){
                continue;
            }
            if(buffer[0] == '/'){
                handle_command_udp(buffer, socket_desc_udp, global_display_name_udp, message_id, timeout, retransmissions, &fds);
            }
            else {
                if(!authenticated_udp){
                    fprintf(stderr, "ERR: You must authenticate first\n");
                    continue;
                    }
                int message_length = create_msg_message_udp(message_id++, global_display_name_udp, buffer, message);
                if (sendto(socket_desc_udp, message, message_length, 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
                    fprintf(stderr, "ERR: Cannot send message\n");
                    cleanup(socket_desc_udp, epollfd_udp);
                    return EXIT_FAILURE;
                }
                // Wait for confirmation from the server
                if (!wait_for_confirmation(socket_desc_udp, message, message_length, (struct sockaddr*)&server_addr, sizeof(server_addr), message_id - 1, timeout, retransmissions)) {
                    fprintf(stderr, "ERR: Did not receive confirmation from server\n");
                    cleanup(socket_desc_udp, epollfd_udp);
                    return EXIT_FAILURE;
                }
                
            }
        }
    
    }
    

    return EXIT_SUCCESS;
}