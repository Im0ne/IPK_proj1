/**
 * @file udp.c
 * @brief IPK Project 1 - Chat Client
 * @author Ivan Onufriienko
 * 
*/


#include "udp.h"
/*--------Global variables--------*/ 

// Saving display name for the whole session
char global_display_name_udp[MAX_DNAME] = "";    
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
int global_timeout = 250000;
int global_retransmissions = 3;
uint16_t confirmed_msg_ids[MAX_RECENT_MSG_IDS];
int confirmed_msg_ids_index = 0;
int confirmed_msg_ids_count = 0;

void uint16_to_char_array(uint16_t value, char* char_array) {
    char_array[0] = value >> 8; // High byte
    char_array[1] = value; // Low byte
}

bool wait_for_confirmation(int socket_desc_udp,unsigned char* message, int message_length, 
                            struct sockaddr* server_addr, socklen_t server_addr_len, 
                            int timeout, int retransmissions) {
    char buffer[1500];
    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);

    // Set socket to non-blocking
    int flags = fcntl(socket_desc_udp, F_GETFL, 0);
    fcntl(socket_desc_udp, F_SETFL, flags | O_NONBLOCK);

    time_t start_time = time(NULL);

    for (int i = 0; i <= retransmissions; i++) {
        while (1) {
            ssize_t numBytes = recvfrom(socket_desc_udp, buffer, sizeof(buffer) - 1, 0, 
                                        (struct sockaddr*)&sender_addr, &sender_addr_len);
            if (numBytes < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    // No data available, check timeout
                    if (time(NULL) - start_time >= timeout) {
                        // Timeout, need to retransmit the message
                        if (sendto(socket_desc_udp, message, message_length, 0, server_addr, server_addr_len) < 0) {
                            fprintf(stderr, "ERR: Cannot retransmit message\n");
                            return false;
                        }
                        start_time = time(NULL);
                        break;
                    }
                } else {
                    fprintf(stderr, "ERR: Cannot receive message\n");
                    return false;
                }
            } else {
                // Check if the received message is a confirmation message
                if (buffer[0] == 0x00) {
                    return true;
                } else {
                    // Not the CONFIRM message process it
                    handle_server_reply_udp(buffer, timeout, retransmissions);
                }
            }
        }
    }

    return false;
}

int create_confirm_message_udp(uint16_t ref_message_id, unsigned char* message) {
    char ref_message_id_char[2];
    uint16_to_char_array(ref_message_id, ref_message_id_char);

    int index = 0;
    message[index++] = CONFIRM; 
    memcpy(message + index, ref_message_id_char, 2);
    index += 2;

    return index;
}

int create_auth_message_udp(uint16_t message_id, char* username, 
                            char* display_name, char* secret, unsigned char* message) {
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

int create_join_message_udp(uint16_t message_id, char* channel_id, 
                            char* display_name,unsigned char* message) {
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

int create_msg_message_udp(uint16_t message_id, char* display_name, 
                            char* message_content, unsigned char* message) {
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

int create_err_message_udp(uint16_t message_id, char* display_name, 
                           char* message_content, unsigned char* message) {
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

int create_bye_message_udp(uint16_t message_id, unsigned char* message) {
    char message_id_char[2];
    uint16_to_char_array(message_id, message_id_char);

    int index = 0;
    message[index++] = BYE; 
    memcpy(message + index, message_id_char, 2);
    index += 2;

    return index;
}

void error_udp(char *msg,unsigned char* message, uint16_t ref_message_id, 
                int timeout, int retransmissions) {
    fprintf(stderr, "ERR: %s\n", msg);
    
    int err_length = create_err_message_udp(ref_message_id, 
                                            global_display_name_udp, 
                                            msg , message);
    sendto(socket_desc_udp, message, err_length, 0, 
            (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (!wait_for_confirmation(socket_desc_udp, message, 
                                err_length, (struct sockaddr*)&server_addr, 
                                sizeof(server_addr), timeout, 
                                retransmissions)) {
        fprintf(stderr, "ERR: Did not receive confirmation from server\n");
        cleanup(socket_desc_udp, epollfd_udp);
        exit(EXIT_FAILURE);
    }
    
    int message_length = create_bye_message_udp(message_id++, message);
    sendto(socket_desc_udp, message, message_length, 0, 
            (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (!wait_for_confirmation(socket_desc_udp, message, 
                                message_length, (struct sockaddr*)&server_addr, 
                                sizeof(server_addr), timeout, 
                                retransmissions)) {
        fprintf(stderr, "ERR: Did not receive confirmation from server\n");
        cleanup(socket_desc_udp, epollfd_udp);
        exit(EXIT_FAILURE);
    }
    cleanup(socket_desc_udp, epollfd_udp);
    exit(EXIT_SUCCESS);
}

void handle_command_udp(char* command, int socket_desc_udp, 
                        uint16_t message_id, 
                        int timeout, int retransmissions) {
    
    char extra[MAX_CONTENT] = "";
    unsigned char message[1500];
    if (strncmp(command, "/auth", 5) == 0) {
        if (authenticated_udp) {
            fprintf(stderr, "ERR: You are already authenticated\n");
            return;
        }
        char username[MAX_USERNAME] = "", secret[MAX_SECRET] = "", 
                display_name[MAX_DNAME] = "";
        sscanf(command, "/auth %s %s %s %99[^\n]", 
                username, secret, display_name, extra);
        if (strlen(username) == 0 || strlen(secret) == 0 
            || strlen(display_name) == 0 || strlen(extra) > 0
            || !is_alnum_or_dash(username) || !is_alnum_or_dash(secret) 
            || !is_print_or_space(display_name)) {
            fprintf(stderr, "ERR: Invalid parameters for /auth\n");
            return;
        }
        strncpy(global_display_name_udp, display_name, MAX_DNAME);
        int message_length = create_auth_message_udp(message_id++, 
                                                    username, display_name, 
                                                    secret, message);
        sendto(socket_desc_udp, message, message_length, 0, 
                (struct sockaddr*)&server_addr, sizeof(server_addr));
        epoll_ctl(epollfd_udp, EPOLL_CTL_DEL, STDIN_FILENO, &ev_udp);
        if (!wait_for_confirmation(socket_desc_udp, message, message_length, 
                                    (struct sockaddr*)&server_addr, sizeof(server_addr), 
                                    timeout, retransmissions)) {
            fprintf(stderr, "ERR: Did not receive confirmation from server\n");
            cleanup(socket_desc_udp, epollfd_udp);
            exit(EXIT_FAILURE);
        }
        strncpy(last_command_udp, command, 5);
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
        int message_length = create_join_message_udp(message_id++, 
                                                    channel_id, 
                                                    global_display_name_udp, 
                                                    message);
        sendto(socket_desc_udp, message, message_length, 0, 
                (struct sockaddr*)&server_addr, sizeof(server_addr));
        epoll_ctl(epollfd_udp, EPOLL_CTL_DEL, STDIN_FILENO, &ev_udp);
        if (!wait_for_confirmation(socket_desc_udp, message, 
                                    message_length, (struct sockaddr*)&server_addr, 
                                    sizeof(server_addr), 
                                    timeout, retransmissions)) {
            fprintf(stderr, "ERR: Did not receive confirmation from server\n");
            cleanup(socket_desc_udp, epollfd_udp);
            exit(EXIT_FAILURE);
        }
        strncpy(last_command_udp, command, 5);
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
        strncpy(last_command_udp, command, 7);
    } else if (strncmp(command, "/help", 5) == 0) {
        sscanf(command, "/help %99[^\n]", extra);
        if(strlen(extra) > 0){
            fprintf(stderr, "ERR: Invalid parameters for /help\n");
            return;
        }
        print_help_client();
        if (authenticated_udp){
            strncpy(last_command_udp, command, 5);
        }      
    } else {
        fprintf(stderr,"ERR: Unknown command: %s\n", command);
    }
}

void handle_server_reply_udp(char* reply, int timeout, int retransmissions) {
    uint16_t ref_message_id = (uint8_t)reply[1] << 8 | (uint8_t)reply[2];
    unsigned char message[1500];
    
    if ((unsigned char)reply[0] == CONFIRM) {
        return;
    }
    if (confirmed_msg_ids_count != 0) {
        for (int i = 0; i < MAX_RECENT_MSG_IDS; i++) {
            if (confirmed_msg_ids[i] == ref_message_id) {
                // This message has already been confirmed, ignore it
                int confirmation_lenght = create_confirm_message_udp(ref_message_id, message);
                sendto(socket_desc_udp, message, confirmation_lenght, 0, 
                        (struct sockaddr*)&server_addr, sizeof(server_addr));
                return;
            }
        }
    }
    confirmed_msg_ids[confirmed_msg_ids_index] = ref_message_id;
    confirmed_msg_ids_index = (confirmed_msg_ids_index + 1) % MAX_RECENT_MSG_IDS;
    confirmed_msg_ids_count++;
    int confirmation_lenght = create_confirm_message_udp(ref_message_id, message);
    sendto(socket_desc_udp, message, confirmation_lenght, 0, 
            (struct sockaddr*)&server_addr, sizeof(server_addr));

    if ((unsigned char)reply[0] == REPLY) {
        uint8_t result = reply[3];
        if ((strncmp(last_command_udp, "/auth", 5) == 0) 
            || (strncmp(last_command_udp, "/join", 5) == 0)){
            ev_udp.events = EPOLLIN;
            ev_udp.data.fd = STDIN_FILENO;
            if (epoll_ctl(epollfd_udp, EPOLL_CTL_ADD, STDIN_FILENO, &ev_udp) == -1) {
                fprintf(stderr, "ERR: epoll_ctl: stdin");
                cleanup(socket_desc_udp, epollfd_udp);
                exit(EXIT_FAILURE);
            }
        }
        if (reply[strlen(reply)] != '\0') {
            error_udp("Invalid message format", message, ref_message_id, timeout, retransmissions);
        }
        if (result == 0x01) {
            fprintf(stderr, "Success: %s\n", reply + 6);
            if (strncmp(last_command_udp, "/auth", 5) == 0 ) {
                authenticated_udp = true;
            }
        } else if (result == 0x00) {         
            fprintf(stderr, "Failure: %s\n", reply + 6);
        } else {
            error_udp("Unknown result", message, ref_message_id, timeout, retransmissions);
        }
        
    } else if ((unsigned char)reply[0] == MSG) {
        if (reply[strlen(reply)] != '\0') {
            error_udp("Invalid message format", message, ref_message_id, timeout, retransmissions);
        }
        printf("%s: %s\n", reply + 3, reply + 4 + strlen(reply + 3));
    } else if ((unsigned char)reply[0] == ERR) {
        if (reply[strlen(reply)] != '\0') {
            error_udp("Invalid message format", message, ref_message_id, timeout, retransmissions);
        }
        fprintf(stderr, "ERR FROM %s: %s\n", reply + 3, reply + 4 + strlen(reply + 3));
        int bye_length = create_bye_message_udp(message_id, message);
        sendto(socket_desc_udp, message, bye_length, 0, 
                (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (!wait_for_confirmation(socket_desc_udp, message, bye_length, 
                                    (struct sockaddr*)&server_addr, 
                                    sizeof(server_addr), timeout, 
                                    retransmissions)) {
            fprintf(stderr, "ERR: Did not receive confirmation from server\n");
            cleanup(socket_desc_udp, epollfd_udp);
            exit(EXIT_FAILURE);
        }
    } else if ((unsigned char)reply[0] == BYE) {
        cleanup(socket_desc_udp, epollfd_udp);
        exit(EXIT_SUCCESS);
    } else {
        error_udp("Unknown message type", message, ref_message_id, timeout, retransmissions);
    }

}

void signal_handler_udp() { 
    unsigned char message[1500];
    if(strcmp(last_command_udp, "") != 0){   
        int bye_length = create_bye_message_udp(message_id, message);
        sendto(socket_desc_udp, message, bye_length, 0, 
            (struct sockaddr*)&server_addr, sizeof(server_addr));
        if (!wait_for_confirmation(socket_desc_udp, message, bye_length, 
                                    (struct sockaddr*)&server_addr, 
                                    sizeof(server_addr), global_timeout, global_retransmissions)) {
            fprintf(stderr, "ERR: Did not receive confirmation from server\n");
            cleanup(socket_desc_udp, epollfd_udp);
            exit(EXIT_FAILURE);
        }
    }
    cleanup(socket_desc_udp, epollfd_udp);
    exit(EXIT_SUCCESS);
}

int udp_connect(char* ipstr, int port, int timeout, int retransmissions) {
    
    struct timeval tv;
    struct epoll_event events[MAX_EVENTS];
    int nfds;
    
    signal(SIGINT, signal_handler_udp);
    timeout *= 1000;
    global_retransmissions = retransmissions;
    global_timeout = timeout;
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

    // Create an epoll instance
    epollfd_udp = epoll_create1(0);
    if (epollfd_udp == -1) {
        fprintf(stderr, "ERR: epoll_create1");
        cleanup(socket_desc_udp, epollfd_udp);
        exit(EXIT_FAILURE);
    }

    // Add the socket file descriptor to the epoll instance
    ev_udp.events = EPOLLIN;
    ev_udp.data.fd = socket_desc_udp;
    if (epoll_ctl(epollfd_udp, EPOLL_CTL_ADD, socket_desc_udp, &ev_udp) == -1) {
        fprintf(stderr, "ERR: epoll_ctl: socket_desc_udp");
        cleanup(socket_desc_udp, epollfd_udp);
        exit(EXIT_FAILURE);
    }

    // Add the stdin file descriptor to the epoll instance
    ev_udp.data.fd = STDIN_FILENO;
    if (epoll_ctl(epollfd_udp, EPOLL_CTL_ADD, STDIN_FILENO, &ev_udp) == -1) {
        fprintf(stderr, "ERR: epoll_ctl: stdin");
        cleanup(socket_desc_udp, epollfd_udp);
        exit(EXIT_FAILURE);
    }

    // Epoll loop
    for (;;) {
        nfds = epoll_wait(epollfd_udp, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            fprintf(stderr, "ERR: epoll_wait");
            cleanup(socket_desc_udp, epollfd_udp);
            exit(EXIT_FAILURE);
        }

        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == socket_desc_udp) {
                // Handle socket event
                char server_reply[1500];
                while(true){
                    /* Address structure that will be filled by recvfrom() */
                    struct sockaddr_in addr;
                    socklen_t addrlen = sizeof(addr);

                    int bytes = recvfrom(socket_desc_udp, server_reply, sizeof(server_reply),
                                            0, (struct sockaddr*) &addr, &addrlen);
                    if(bytes > 0){     
                        // Copy the port
                        uint16_t port = ntohs(addr.sin_port);
                        server_addr.sin_port = htons(port);
                        handle_server_reply_udp(server_reply, timeout, retransmissions);           
                    } 
                    else if (errno == EAGAIN) {
                        break;
                    }
                    else {
                        fprintf(stderr, "ERR: Cannot receive message\n");
                        cleanup(socket_desc_udp, epollfd_udp);
                        exit(EXIT_FAILURE);
                    }
                }
            } else if (events[n].data.fd == STDIN_FILENO) {
                // Handle stdin event         
                char buffer[1500];
                unsigned char message[1500];
                if (fgets(buffer, 1500, stdin) == NULL) {
                    // Ctrl+D was pressed, exit the program
                    if (strcmp(last_command_udp, "") != 0) {
                        int message_length = create_bye_message_udp(message_id, message);
                        sendto(socket_desc_udp, message, message_length, 0, 
                                (struct sockaddr*)&server_addr, sizeof(server_addr));
                        if (!wait_for_confirmation(socket_desc_udp, message, message_length, 
                                                    (struct sockaddr*)&server_addr, 
                                                    sizeof(server_addr), 
                                                    timeout, retransmissions)) {
                            fprintf(stderr, "ERR: Did not receive confirmation from server\n");
                            cleanup(socket_desc_udp, epollfd_udp);
                            exit(EXIT_FAILURE);
                        }
                    }
                    cleanup(socket_desc_udp, epollfd_udp);
                    return EXIT_SUCCESS;
                }
                
                if(strlen(buffer) == 1){
                    continue;
                }
                buffer[strcspn(buffer, "\n")] = 0;
                if(buffer[0] == '/'){
                    handle_command_udp(buffer, socket_desc_udp, 
                                        message_id++, timeout, 
                                        retransmissions);
                }
                else {
                    if(!authenticated_udp){
                        fprintf(stderr, "ERR: You must authenticate first\n");
                        continue;
                        }
                    int message_length = create_msg_message_udp(message_id++, 
                                                                global_display_name_udp, 
                                                                buffer, message);
                    if (sendto(socket_desc_udp, message, message_length, 0, 
                                (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
                        fprintf(stderr, "ERR: Cannot send message\n");
                        cleanup(socket_desc_udp, epollfd_udp);
                        return EXIT_FAILURE;
                    }

                    // Wait for confirmation from the server
                    if (!wait_for_confirmation(socket_desc_udp, message, message_length, 
                                                (struct sockaddr*)&server_addr, 
                                                sizeof(server_addr), 
                                                timeout, retransmissions)) {
                        fprintf(stderr, "ERR: Did not receive confirmation from server\n");
                        cleanup(socket_desc_udp, epollfd_udp);
                        return EXIT_FAILURE;
                    }
                    
                }
            }
        }
    }
    

    return EXIT_SUCCESS;
}