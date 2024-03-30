/**
 * @file udp.c
 * @brief IPK Project 1 - Chat Client(UDP protocol implementation)
 * @author Ivan Onufriienko
 * 
*/


#include "udp.h"

// Repeats too often so I've decided to make a macro
#define WAIT_FOR_CONFIRM(message, message_length) \
    if (!wait_for_confirmation(message, message_length)) { \
        fprintf(stderr, "ERR: Did not receive confirmation from server\n"); \
        cleanup(socket_desc_udp, epollfd_udp); \
        exit(EXIT_FAILURE); \
    }

/* Flag for checking if user is authenticated */  
bool authenticated_udp = false;

/* Global variables that cannot be inside of functions because of signal handler*/

// Socket descriptor, epoll file descriptor, epoll event, server address
int socket_desc_udp = -1;                   
int epollfd_udp = -1;
struct epoll_event ev_udp;
struct sockaddr_in server_addr;

// Display name, set by user in /auth or /rename command
char global_display_name_udp[MAX_DNAME] = "unnamed";

// Last command that was sent to the server
char last_command_udp[MAX_DNAME] = "";

// Message ID(what could be else)
uint16_t message_id = 0;

// Timeout and retransmissions which are set on the start of the program
int global_timeout = 0;
int global_retransmissions = 0;

// Keep track of the IDs of confirmed messages
uint16_t confirmed_msg_ids[MAX_RECENT_MSG_IDS];
int confirmed_msg_ids_index = 0;
int confirmed_msg_ids_count = 0;

// I could use an htons function from the library but I've decided to implement it myself
void uint16_to_char_array(uint16_t value, char* char_array) {
    char_array[0] = value >> 8; // High byte
    char_array[1] = value; // Low byte
}

bool wait_for_confirmation(unsigned char* message, int message_length) {
    char buffer[1500];
    struct sockaddr_in sender_addr;
    socklen_t sender_addr_len = sizeof(sender_addr);

    socklen_t server_addr_len = sizeof(server_addr); 

    struct timeval start_time, current_time;
    
    // Get the start time
    gettimeofday(&start_time, NULL); 

    for (int i = 0; i < global_retransmissions; i++) {
        while (1) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(socket_desc_udp, &read_fds);

            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = global_timeout; 

            int activity = select(socket_desc_udp + 1, &read_fds, NULL, NULL, &tv);
            if (activity > 0 && FD_ISSET(socket_desc_udp, &read_fds)) {
                
                ssize_t numBytes = recvfrom(socket_desc_udp, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&sender_addr, &sender_addr_len);
                if (numBytes < 0) {
                    fprintf(stderr, "ERR: Cannot receive message\n");
                    return false;
                }
                
                // Check if the received message is a confirmation message
                if (buffer[0] == 0x00) {
                    uint16_t confirm_ref_id = (uint8_t)buffer[1] << 8 | (uint8_t)buffer[2];
                    if (confirm_ref_id == message_id - 1) {
                        return true;
                    }
                } else {
                    // Not the CONFIRM message process it
                    handle_server_reply_udp(buffer);
                }
            
            } else if (activity == 0) {
                
                // Get the current time
                gettimeofday(&current_time, NULL);

                // Calculate the elapsed time in microseconds
                long elapsed_time = (current_time.tv_sec - start_time.tv_sec) * 1000000 
                                     + (current_time.tv_usec - start_time.tv_usec);

                // Check if the timeout has been reached
                if (elapsed_time >= global_timeout) {
                    
                    // Timeout, need to retransmit the message
                    if (sendto(socket_desc_udp, message, message_length, 0, (struct sockaddr*)&server_addr, server_addr_len) < 0) {
                        fprintf(stderr, "ERR: Cannot retransmit message\n");
                        return false;
                    }
                    break;
                }

            } else {
                fprintf(stderr, "ERR: select error\n");
                return false;
            }
        }
    }

    return false;
}
/***** There are functions for creating messages that follow a IPK24chat protocol.*****/

int create_confirm_message_udp(uint16_t ref_message_id, unsigned char* message) {
    char ref_message_id_char[2];
    uint16_to_char_array(ref_message_id, ref_message_id_char);

    int index = 0;
    message[index++] = CONFIRM; 
    memcpy(message + index, ref_message_id_char, 2);
    index += 2;

    return index;
}

int create_auth_message_udp(char* username, char* secret, unsigned char* message) {
    char message_id_char[2];
    uint16_to_char_array(message_id, message_id_char);

    int index = 0;
    message[index++] = AUTH; 
    memcpy(message + index, message_id_char, 2);
    index += 2;

    memcpy(message + index, username, strlen(username));
    index += strlen(username);

    message[index++] = '\0'; 

    memcpy(message + index, global_display_name_udp, strlen(global_display_name_udp));
    index += strlen(global_display_name_udp);

    message[index++] = '\0'; 

    memcpy(message + index, secret, strlen(secret));
    index += strlen(secret);

    message[index++] = '\0'; 

    message_id++;

    return index;
}

int create_join_message_udp(char* channel_id, unsigned char* message) {
    char message_id_char[2];
    uint16_to_char_array(message_id, message_id_char);

    int index = 0;
    message[index++] = JOIN; 
    memcpy(message + index, message_id_char, 2);
    index += 2;

    memcpy(message + index, channel_id, strlen(channel_id));
    index += strlen(channel_id);

    message[index++] = '\0'; 

    memcpy(message + index, global_display_name_udp, strlen(global_display_name_udp));
    index += strlen(global_display_name_udp);

    message[index++] = '\0'; 

    message_id++;

    return index;
}

int create_msg_message_udp(char* message_content, unsigned char* message) {
    char message_id_char[2];
    uint16_to_char_array(message_id, message_id_char);

    int index = 0;
    message[index++] = MSG; 
    memcpy(message + index, message_id_char, 2);
    index += 2;

    memcpy(message + index, global_display_name_udp, strlen(global_display_name_udp));
    index += strlen(global_display_name_udp);

    message[index++] = '\0'; 

    memcpy(message + index, message_content, strlen(message_content));
    index += strlen(message_content);

    message[index++] = '\0'; 

    message_id++;

    return index;
}

int create_err_message_udp(char* message_content, unsigned char* message) {
    char message_id_char[2];
    uint16_to_char_array(message_id, message_id_char);

    int index = 0;
    message[index++] = ERR; 
    memcpy(message + index, message_id_char, 2);
    index += 2;

    memcpy(message + index, global_display_name_udp, strlen(global_display_name_udp));
    index += strlen(global_display_name_udp);

    message[index++] = '\0'; 

    memcpy(message + index, message_content, strlen(message_content));
    index += strlen(message_content);

    message[index++] = '\0'; 

    message_id++;

    return index;
}

int create_bye_message_udp(unsigned char* message) {
    char message_id_char[2];
    uint16_to_char_array(message_id, message_id_char);

    int index = 0;
    message[index++] = BYE; 
    memcpy(message + index, message_id_char, 2);
    index += 2;

    message_id++;

    return index;
}

void error_udp(char *error, unsigned char* message) {
    fprintf(stderr, "ERR: %s\n", error);
    
    // Entering the error state
    int err_length = create_err_message_udp(error , message);
    sendto(socket_desc_udp, message, err_length, 0, 
            (struct sockaddr*)&server_addr, sizeof(server_addr));
    WAIT_FOR_CONFIRM(message, err_length)
    
    // Entering end state
    int bye_length = create_bye_message_udp(message);
    sendto(socket_desc_udp, message, bye_length, 0, 
            (struct sockaddr*)&server_addr, sizeof(server_addr));
    WAIT_FOR_CONFIRM(message, bye_length)

    // Cleanup and exit
    cleanup(socket_desc_udp, epollfd_udp);
    exit(EXIT_SUCCESS);
}

void handle_command_udp(char* command, int socket_desc_udp) {   
    
    // Extra is used to check if there are any extra parameters
    char extra[MAX_CONTENT] = "";
    
    // Message is used to store the message that will be sent to the server
    unsigned char message[1500];

    if (strncmp(command, "/auth", 5) == 0) {
        
        // If you authenticated why would you authenticate again
        if (authenticated_udp) {
            fprintf(stderr, "ERR: You are already authenticated\n");
            return;
        }

        char username[MAX_USERNAME] = "", secret[MAX_SECRET] = "", 
             display_name[MAX_DNAME] = "";

        sscanf(command, "/auth %s %s %s %99[^\n]", 
                username, secret, display_name, extra);

        // Check if the parameters are valid
        if (strlen(username) == 0 || strlen(secret) == 0 
            || strlen(display_name) == 0 || strlen(extra) > 0
            || !is_alnum_or_dash(username) || !is_alnum_or_dash(secret) 
            || !is_print_or_space(display_name)) {
            fprintf(stderr, "ERR: Invalid parameters for /auth\n");
            return;
        }

        strncpy(global_display_name_udp, display_name, MAX_DNAME);

        int message_length = create_auth_message_udp(username,secret, message);
        sendto(socket_desc_udp, message, message_length, 0, 
                (struct sockaddr*)&server_addr, sizeof(server_addr));
        WAIT_FOR_CONFIRM(message, message_length)

        epoll_ctl(epollfd_udp, EPOLL_CTL_DEL, STDIN_FILENO, &ev_udp);
        
        // Save command to 
        strncpy(last_command_udp, command, 5);

    } else if (strncmp(command, "/join", 5) == 0) {
        
        if(!authenticated_udp){
            fprintf(stderr, "ERR: You must authenticate first\n");
            return;
        }

        char channel_id[MAX_ID] = "";

        // Some black regex magic
        sscanf(command, "/join %s %99[^\n]", channel_id, extra);
        if (strlen(channel_id) == 0 || strlen(extra) > 0 || !is_alnum_or_dash(channel_id)) {
            fprintf(stderr, "ERR: Invalid parameters for /join\n");
            return;
        }

        int message_length = create_join_message_udp(channel_id, message);
        sendto(socket_desc_udp, message, message_length, 0, 
                (struct sockaddr*)&server_addr, sizeof(server_addr));      
        WAIT_FOR_CONFIRM(message, message_length)
        
        epoll_ctl(epollfd_udp, EPOLL_CTL_DEL, STDIN_FILENO, &ev_udp);
        

        strncpy(last_command_udp, command, 5);

    } else if (strncmp(command, "/rename", 7) == 0) {

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

void handle_server_reply_udp(char* reply) {
    uint16_t ref_message_id = (uint8_t)reply[1] << 8 | (uint8_t)reply[2];
    unsigned char message[1500];
    
    // Unwaited CONFIRM 
    if ((unsigned char)reply[0] == CONFIRM) {
        return;
    }

    // Server cannot send messages in this state
    if (strcmp(last_command_udp, "") == 0){
        error_udp("Received message from server before sending any message", message);
    }
    // If it's not the first message
    if (confirmed_msg_ids_count != 0) {
        
        // Look for the message ID in the list of confirmed messages
        for (int i = 0; i < MAX_RECENT_MSG_IDS; i++) {
            
            // Message ID found in the list of confirmed messages
            if (confirmed_msg_ids[i] == ref_message_id) {
                
                // This message has already been confirmed, ignore it
                int confirmation_lenght = create_confirm_message_udp(ref_message_id, message);
                sendto(socket_desc_udp, message, confirmation_lenght, 0, 
                        (struct sockaddr*)&server_addr, sizeof(server_addr));
                
                return;
            }
        }
    }

    // Add the message ID to the list of confirmed messages
    confirmed_msg_ids[confirmed_msg_ids_index] = ref_message_id;
    confirmed_msg_ids_index = (confirmed_msg_ids_index + 1) % MAX_RECENT_MSG_IDS;
    // Increment the count of confirmed messages
    confirmed_msg_ids_count++;
    
    int confirmation_lenght = create_confirm_message_udp(ref_message_id, message);
    sendto(socket_desc_udp, message, confirmation_lenght, 0, 
            (struct sockaddr*)&server_addr, sizeof(server_addr));

    if ((unsigned char)reply[0] == REPLY) {
        
        uint8_t result = reply[3];
        uint16_t reply_ref_id = (uint8_t)reply[1] << 8 | (uint8_t)reply[2];
        
        // Check if the reply is for the last message sent
        if (reply_ref_id != message_id - 1) {
            return;
        }
        
        // If the last command was /auth or /join, add back stdin to epoll
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
        
        // Message must be null terminated
        if (reply[strlen(reply)] != '\0') {
            error_udp("Invalid message format", message);
        }
        
        if (result == 0x01) {
            fprintf(stderr, "Success: %s\n", reply + 6);
            if (strncmp(last_command_udp, "/auth", 5) == 0 ) {
                authenticated_udp = true;
            }
        } else if (result == 0x00) {         
            fprintf(stderr, "Failure: %s\n", reply + 6);
        } else {
            error_udp("Unknown result", message);
        }
        
    } else if ((unsigned char)reply[0] == MSG) {
        
        // Server cannot send messages in this state
        if (!authenticated_udp) {
            error_udp("Message from server before authentication", message);
        }

        if (reply[strlen(reply)] != '\0') {
            error_udp("Invalid message format", message);
        } 
        
        printf("%s: %s\n", reply + 3, reply + 4 + strlen(reply + 3));

    } else if ((unsigned char)reply[0] == ERR) {
        
        if (reply[strlen(reply)] != '\0') {
            error_udp("Invalid message format", message);
        }

        fprintf(stderr, "ERR FROM %s: %s\n", reply + 3, reply + 4 + strlen(reply + 3));

        int message_length = create_bye_message_udp(message);
        sendto(socket_desc_udp, message, message_length, 0, 
                (struct sockaddr*)&server_addr, sizeof(server_addr));
        WAIT_FOR_CONFIRM(message, message_length)

    } else if ((unsigned char)reply[0] == BYE) {
        
        // Server cannot send messages in this state
        if (!authenticated_udp) {
            error_udp("Message from server before authentication", message);
        }
        
        cleanup(socket_desc_udp, epollfd_udp);
        exit(EXIT_SUCCESS);
    } else {
        error_udp("Unknown message type", message);
    }

}

void signal_handler_udp() { 
    unsigned char message[1500];
    
    // If there were no commands sent to the server, just exit
    if(strcmp(last_command_udp, "") != 0){   
        int message_length = create_bye_message_udp(message);
        
        sendto(socket_desc_udp, message, message_length, 0, 
            (struct sockaddr*)&server_addr, sizeof(server_addr));      
        WAIT_FOR_CONFIRM(message, message_length)
    }
    
    cleanup(socket_desc_udp, epollfd_udp);
    exit(EXIT_SUCCESS);
}

int udp_connect(char* ipstr, int port, int timeout, int retransmissions) {  
    
    // Variables for epoll
    struct timeval tv;
    struct epoll_event events[MAX_EVENTS];
    int nfds;

    // Controll C-c signal for graceful exit
    signal(SIGINT, signal_handler_udp);

    // Convert timeout to microseconds
    timeout *= 1000;

    // Set global variables
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
            
            // Check if the event is from socket or stdin
            if (events[n].data.fd == socket_desc_udp) {
                
                // Handle socket event
                char server_reply[1500];
                while(true){
                    
                    // Address structure that will be filled by recvfrom() 
                    struct sockaddr_in addr;
                    socklen_t addrlen = sizeof(addr);

                    // Receive message from server
                    int bytes = recvfrom(socket_desc_udp, server_reply, sizeof(server_reply),
                                            0, (struct sockaddr*) &addr, &addrlen);
                    
                    // There is some message to read
                    if(bytes > 0){     
                        
                        // Copy the port to send next message to
                        uint16_t port = ntohs(addr.sin_port);
                        server_addr.sin_port = htons(port);
                        handle_server_reply_udp(server_reply);           
                    } 
                    
                    // If there are no more messages to read
                    else if (errno == EAGAIN) {
                        break;
                    } 
                    
                    // Some error or server closed the connection
                    else {
                        fprintf(stderr, "ERR: Cannot receive message\n");
                        cleanup(socket_desc_udp, epollfd_udp);
                        exit(EXIT_FAILURE);
                    }

                }
            } else if (events[n].data.fd == STDIN_FILENO) {
                
                // Handle stdin event         
                char buffer[1400];
                unsigned char message[1400];
                if (fgets(buffer, 1400, stdin) == NULL) {
                    
                    // Ctrl+D was pressed, exit the program
                    if (strcmp(last_command_udp, "") != 0) {
                        int message_length = create_bye_message_udp(message);
                        sendto(socket_desc_udp, message, message_length, 0, 
                                (struct sockaddr*)&server_addr, sizeof(server_addr));
                        WAIT_FOR_CONFIRM(message, message_length)
                    }
                    
                    cleanup(socket_desc_udp, epollfd_udp);
                    return EXIT_SUCCESS;
                }
                
                // Why do we need to send newlines?
                if(strlen(buffer) == 1){
                    continue;
                }
                
                // Remove newline character
                buffer[strcspn(buffer, "\n")] = 0;
                
                // Handle command
                if(buffer[0] == '/'){
                    handle_command_udp(buffer, socket_desc_udp);
                }
                
                // Handle message
                else {
                    
                    if(!authenticated_udp){
                        fprintf(stderr, "ERR: You must authenticate first\n");
                        continue;
                    }

                    if (!is_print_or_space(buffer)) {
                        fprintf(stderr, "ERR: Invalid message\n");
                        continue;
                    }

                    int message_length = create_msg_message_udp(buffer, message);
                    if (sendto(socket_desc_udp, message, message_length, 0, 
                                (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
                        fprintf(stderr, "ERR: Cannot send message\n");
                        cleanup(socket_desc_udp, epollfd_udp);
                        return EXIT_FAILURE;
                    }

                    WAIT_FOR_CONFIRM(message, message_length)
                    
                }
            }
        }
    }

    return EXIT_SUCCESS;
}