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



void uint16_to_char_array(uint16_t value, char* char_array) {
    char_array[0] = value >> 8; // High byte
    char_array[1] = value; // Low byte
}

int create_confirm_message_udp(uint16_t ref_message_id, char* message) {
    char ref_message_id_char[2];
    uint16_to_char_array(ref_message_id, ref_message_id_char);

    int index = 0;
    message[index++] = 0x00; 
    memcpy(message + index, ref_message_id_char, 2);
    index += 2;

    return index;
}

int create_reply_message_udp(uint16_t message_id, uint8_t result, uint16_t ref_message_id, char* message_contents, char* reply_message) {
    char message_id_char[2];
    uint16_to_char_array(message_id, message_id_char);

    char ref_message_id_char[2];
    uint16_to_char_array(ref_message_id, ref_message_id_char);

    int index = 0;
    reply_message[index++] = 0x01; 
    memcpy(reply_message + index, message_id_char, 2);
    index += 2;

    reply_message[index++] = result; 
    memcpy(reply_message + index, ref_message_id_char, 2);
    index += 2;

    memcpy(reply_message + index, message_contents, strlen(message_contents));
    index += strlen(message_contents);

    reply_message[index++] = '\0'; 

    return index;
}

int create_auth_message_udp(uint16_t message_id, char* username, char* display_name, char* secret, char* message) {
    char message_id_char[2];
    uint16_to_char_array(message_id, message_id_char);

    int index = 0;
    message[index++] = 0x02; 
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
    
}

int create_msg_message_udp(uint16_t message_id, char* display_name, char* message_content, char* message) {
    
}

int create_err_message_udp(uint16_t message_id, char* display_name, char* message_content, char* message) {
    
}

int create_bye_message_udp(uint16_t message_id, char* message) {
    
}

void handle_command_udp(char* command, int socket_desc_udp, char* display_name, uint16_t message_id) {
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
        int message_lenght = create_auth_message_udp(message_id++, username, display_name, secret, message);
        sendto(socket_desc_udp, message, message_lenght, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
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
        int message_lenght = create_join_message_udp(message_id++, channel_id, global_display_name_udp, message);
        sendto(socket_desc_udp, message, message_lenght, 0, (struct sockaddr*)&server_addr, sizeof(server_addr));
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

void handle_server_reply_udp(char* reply) {

}

void signal_handler_udp(int signum) {
    if (signum == SIGINT) {
        //char* message = create_bye_message_udp(0);
        //if (send(socket_desc_udp, message, strlen(message), 0) < 0) {
        //    perror("ERR: Cannot send message\n");
        //    exit(EXIT_FAILURE);
        //}
        //free(message);
        cleanup(socket_desc_udp, epollfd_udp);
        exit(EXIT_SUCCESS);
    }

}

int udp_connect(char* ipstr, int port, int timeout, int retransmissions) {
    
    struct timeval tv;
    struct epoll_event events[MAX_EVENTS];
    int nfds;
    uint16_t message_id = 0;
    signal(SIGINT, signal_handler_udp);

    // Create socket
    if ((socket_desc_udp = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("ERR: Cannot create socket\n");
        return EXIT_FAILURE;
    }

    // Set timeout
    tv.tv_sec = 0;
    tv.tv_usec = timeout;
    if (setsockopt(socket_desc_udp, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("ERR: Error setting timeout\n");
        return EXIT_FAILURE;
    }

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ipstr, &server_addr.sin_addr) <= 0) {
        perror("ERR: Invalid IP address\n");
        return EXIT_FAILURE;
    }

    // Connect to server
    if (connect(socket_desc_udp, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("ERR: Connect failed\n");
        return EXIT_FAILURE;
    }

    // Create an epoll instance
    epollfd_udp = epoll_create1(0);
    if (epollfd_udp == -1) {
        perror("ERR: epoll_create1");
        exit(EXIT_FAILURE);
    }

    // Add the socket file descriptor to the epoll instance
    ev_udp.events = EPOLLIN;
    ev_udp.data.fd = socket_desc_udp;
    if (epoll_ctl(epollfd_udp, EPOLL_CTL_ADD, socket_desc_udp, &ev_udp) == -1) {
        perror("ERR: epoll_ctl: socket_desc_udp");
        exit(EXIT_FAILURE);
    }

    // Add the stdin file descriptor to the epoll instance
    ev_udp.data.fd = STDIN_FILENO;
    if (epoll_ctl(epollfd_udp, EPOLL_CTL_ADD, STDIN_FILENO, &ev_udp) == -1) {
        perror("ERR: epoll_ctl: stdin");
        exit(EXIT_FAILURE);
    }

    // Epoll loop
    for (;;) {
        nfds = epoll_wait(epollfd_udp, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("ERR: epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (int n = 0; n < nfds; ++n) {
            if (events[n].data.fd == socket_desc_udp) {
                // Handle socket event
                char server_reply[1500];
                ssize_t numBytes = recvfrom(socket_desc_udp, server_reply, sizeof(server_reply) - 1, 0, NULL, NULL);
                if (numBytes < 0) {
                    perror("ERR: Cannot receive message\n");
                    return EXIT_FAILURE;
                }

                server_reply[numBytes] = '\0'; // Null-terminate the received data
                printf("Received: %s", server_reply);
            } else if (events[n].data.fd == STDIN_FILENO) {
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
                    handle_command_udp(buffer, socket_desc_udp, global_display_name_udp, message_id);
                }
                else {
                    // if(!authenticated_udp){
                    //     fprintf(stderr, "ERR: You must authenticate first\n");
                    //     continue;
                    //     }
                    int message_lenght = create_msg_message_udp(message_id++, global_display_name_udp, buffer, message);
                    if (sendto(socket_desc_udp, message, message_lenght, 0, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
                        perror("ERR: Cannot send message\n");
                        return EXIT_FAILURE;
                    }
                }
            }
        }
    }
    

    return EXIT_SUCCESS;
}