/**
 * @file tcp.c
 * @brief IPK Project 1 - Chat Client(TCP protocol implementation)
 * @author Ivan Onufriienko
 * 
*/

#include "tcp.h"
/*--------Global variables--------*/ 

// Saving display name for the whole session
char global_display_name_tcp[MAX_DNAME] = "unnamed";    
// Used to check in what state the user is_token
char last_command_tcp[MAX_DNAME] = "";
// Some allocations for graceful exit
int socket_desc_tcp = -1;                   
int epollfd_tcp = -1;                    
// Flag for checking if user is_token   
bool authenticated_tcp = false;
// Blockade of stdin after /auth and /join
struct epoll_event ev_tcp;

char* create_auth_message_tcp(char* username, char* display_name, char* secret) {
    static char message[1500];
    snprintf(message, sizeof(message), "AUTH %s AS %s USING %s\r\n", username, display_name, secret);
    return message;
}

char* create_join_message_tcp(char* channel_id, char* display_name) {
    static char message[1500];
    snprintf(message, sizeof(message), "JOIN %s AS %s\r\n", channel_id, display_name);
    return message;
}

char* create_msg_message_tcp(char* display_name, char* message_content) {
    static char message[1500];
    snprintf(message, sizeof(message), "MSG FROM %s IS %s\r\n", display_name, message_content);
    return message;
}

char* create_err_message_tcp(char* display_name, char* message_content) {
    static char message[1500];
    snprintf(message, sizeof(message), "ERR FROM %s IS %s\r\n", display_name, message_content);
    return message;
}

char* create_bye_message_tcp() {
    return "BYE\r\n";
}

// This function was used too often, so I decided to create it
void error_tcp(char *msg) {
    
    fprintf(stderr, "ERR: %s\n", msg);
    
    char* err_message = create_err_message_tcp(global_display_name_tcp, msg);
    send(socket_desc_tcp, err_message, strlen(err_message), 0);
    
    char* bye_message = create_bye_message_tcp();
    send(socket_desc_tcp, bye_message, strlen(bye_message), 0);
    
    cleanup(socket_desc_tcp, epollfd_tcp);         
    exit(EXIT_FAILURE);
}

void handle_command_tcp(char* command, int socket_desc_tcp) {
    char extra[MAX_CONTENT] = "";
    
    // Parsing commands
    if (strncmp(command, "/auth", 5) == 0) {
        
        if (authenticated_tcp) {
            fprintf(stderr, "ERR: You are already authenticated\n");
            return;
        }
        
        char username[MAX_USERNAME] = "", secret[MAX_SECRET] = "", display_name[MAX_DNAME] = "";
        
        sscanf(command, "/auth %s %s %s %99[^\n]", username, secret, display_name, extra);
        
        // Check if the parameters are valid
        if (strlen(username) == 0 || strlen(secret) == 0 || strlen(display_name) == 0 || strlen(extra) > 0
            || !is_alnum_or_dash(username) || !is_alnum_or_dash(secret) || !is_print_or_space(display_name)) {
            fprintf(stderr, "ERR: Invalid parameters for /auth\n");
            return;
        }
        
        // Save display name for the whole session
        strncpy(global_display_name_tcp, display_name, MAX_DNAME);
        
        // Send the message to the server
        char* message = create_auth_message_tcp(username, display_name, secret);
        send(socket_desc_tcp, message, strlen(message), 0);
        
        // Block stdin beacause client waits for the server response
        epoll_ctl(epollfd_tcp, EPOLL_CTL_DEL, STDIN_FILENO, &ev_tcp);
        
        // Save the last command for REPLY and graceful exit
        strncpy(last_command_tcp, command, 5);
    } else if (strncmp(command, "/join", 5) == 0) {
        
        if(!authenticated_tcp){
            fprintf(stderr, "ERR: You must authenticate first\n");
            return;
        }
        
        char channel_id[MAX_ID] = "";
        
        sscanf(command, "/join %s %99[^\n]", channel_id, extra);
        
        if (strlen(channel_id) == 0 || strlen(extra) > 0 || !is_alnum_or_dash(channel_id)) {
            fprintf(stderr, "ERR: Invalid parameters for /join\n");
            return;
        }
        
        char* message = create_join_message_tcp(channel_id, global_display_name_tcp);
        send(socket_desc_tcp, message, strlen(message), 0);
        
        epoll_ctl(epollfd_tcp, EPOLL_CTL_DEL, STDIN_FILENO, &ev_tcp);
        
        strncpy(last_command_tcp, command, 5);
    } else if (strncmp(command, "/rename", 7) == 0) {
        
        char display_name[MAX_DNAME] = "";
        
        sscanf(command, "/rename %s %99[^\n]", display_name, extra);
        
        if (strlen(display_name) == 0 || strlen(extra) > 0 || !is_print_or_space(display_name)) {
            fprintf(stderr, "ERR: Invalid parameters for /rename\n");
            return;
        }
        
        strncpy(global_display_name_tcp, display_name, MAX_DNAME);
        
        // First command must be /auth
        if(authenticated_tcp){
        strncpy(last_command_tcp, command, 7);
        }
    } else if (strncmp(command, "/help", 5) == 0) {
        
        sscanf(command, "/help %99[^\n]", extra);
        
        if(strlen(extra) > 0){
            fprintf(stderr, "ERR: Invalid parameters for /help\n");
            return;
        }
        
        print_help_client(); 
        
        // First command must be /auth
        if(authenticated_tcp){
            strncpy(last_command_tcp, command, 5); 
        }    
    } else {
        fprintf(stderr,"ERR: Unknown command: %s", command);
    }
}

void handle_server_reply_tcp(char* reply) {
    char* token = strtok(reply, " ");
    
    // In state start server cannot send a message to client
    if (strcmp(last_command_tcp, "") == 0){
        error_tcp("Server sent a message before user sent a command");
    }
    
    if (strcasecmp(token, "REPLY") == 0) {
        
        // Some controls for the message
        char* status = strtok(NULL, " ");
        char* is_token = strtok(NULL, " ");
        char* message = strtok(NULL, "\r\n");    
        if (message == NULL || is_token == NULL || strcasecmp(is_token, "IS") != 0) {
            error_tcp("Invalid REPLY message from server");
        }
        
        // Bringing back stdin
        if ((strncmp(last_command_tcp, "/auth", 5) == 0) || (strncmp(last_command_tcp, "/join", 5) == 0)){
            ev_tcp.events = EPOLLIN;
            ev_tcp.data.fd = STDIN_FILENO;
            if (epoll_ctl(epollfd_tcp, EPOLL_CTL_ADD, STDIN_FILENO, &ev_tcp) == -1) {
                fprintf(stderr, "ERR: Error_tcp in epoll_ctl\n");
                cleanup(socket_desc_tcp, epollfd_tcp);
                exit(EXIT_FAILURE);
            }
        } 
        
        if (strcasecmp(status, "OK") == 0) {
            fprintf(stderr, "Success: %s\n", message);
            if (strncmp(last_command_tcp, "/auth", 5) == 0) {
                authenticated_tcp = true;
            } 
        } else if (strcasecmp(status, "NOK") == 0) {
            fprintf(stderr, "Failure: %s\n", message);
        } else {
            error_tcp("Unknown status in REPLY message");
        }

    } else if (strcasecmp(token, "MSG") == 0) {
        
        // In auth state server can send only CONFIRM and ERR
        if (!authenticated_tcp) {
            error_tcp("Message from server before authentication");
        }
        
        char* from_token = strtok(NULL, " ");
        char* from = strtok(NULL, " ");
        char* is_token = strtok(NULL, " ");
        char* message = strtok(NULL, "\r\n");     
        if (message == NULL || from_token == NULL || from == NULL || is_token == NULL 
            || strcasecmp(is_token, "IS") != 0 || strcasecmp(from_token, "FROM") != 0 ){
            error_tcp("Invalid MSG message from server");
        }
        
        printf("%s: %s\n", from, message);
    } else if (strcasecmp(token, "ERR") == 0) {
        
        char* from_token = strtok(NULL, " ");
        char* from = strtok(NULL, " ");
        char* is_token = strtok(NULL, " ");
        char* message = strtok(NULL, "\r\n");     
        if (message == NULL || from_token == NULL || from == NULL || is_token == NULL 
            || strcasecmp(is_token, "IS") != 0 || strcasecmp(from_token, "FROM") != 0 ){
            error_tcp("Invalid MSG message from server");
        }
        
        fprintf(stderr, "ERR FROM %s: %s\n", from, message);
        send(socket_desc_tcp, create_bye_message_tcp(), strlen(create_bye_message_tcp()), 0);
        
        cleanup(socket_desc_tcp, epollfd_tcp);
        exit(EXIT_FAILURE);
    } else if (strncmp(token, "BYE", 3) == 0) {
        
        if (!authenticated_tcp) {
            error_tcp("Message from server before authentication");
        }

        if (strtok(NULL, "\r\n") != NULL){
            error_tcp("Invalid BYE message from server");
        }
        
        cleanup(socket_desc_tcp, epollfd_tcp);
        exit(EXIT_SUCCESS);
    } else {
        error_tcp("Unknown message from server");
    }
}

void signal_handler_tcp() {
    // There is no need to send BYE message if user didn't send any command
    if (strcmp(last_command_tcp, "") != 0){
        char* bye_message = create_bye_message_tcp();
        send(socket_desc_tcp, bye_message, strlen(bye_message), 0);
    }
    cleanup(socket_desc_tcp, epollfd_tcp);
    exit(EXIT_SUCCESS);
}

int tcp_connect(char* ipstr, int port) {
    struct sockaddr_in server;
    struct epoll_event events[MAX_EVENTS];

    signal(SIGINT, signal_handler_tcp);

    socket_desc_tcp = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc_tcp == -1){
        cleanup(socket_desc_tcp, epollfd_tcp);
        fprintf(stderr,"ERR: Could not create socket\n");
        return EXIT_FAILURE;
    }
        
    server.sin_addr.s_addr = inet_addr(ipstr);
    server.sin_family = AF_INET;
    server.sin_port = htons( port );

    if (connect(socket_desc_tcp , (struct sockaddr *)&server , sizeof(server)) < 0){
        cleanup(socket_desc_tcp, epollfd_tcp);
        fprintf(stderr,"ERR: Connect error_tcp\n");
        return EXIT_FAILURE;
    }	

    // Create epoll
    epollfd_tcp = epoll_create1(0);
    if(epollfd_tcp == -1){
        cleanup(socket_desc_tcp, epollfd_tcp);
        fprintf(stderr,"ERR: Error_tcp in epoll creation\n");
        return EXIT_FAILURE;
    }

    // Add the socket to the epoll instance
    ev_tcp.events = EPOLLIN;
    ev_tcp.data.fd = socket_desc_tcp;
    epoll_ctl(epollfd_tcp, EPOLL_CTL_ADD, socket_desc_tcp, &ev_tcp);
    
    // Add STDIN_FILENO to the epoll instance
    ev_tcp.events = EPOLLIN;
    ev_tcp.data.fd = STDIN_FILENO;
    epoll_ctl(epollfd_tcp, EPOLL_CTL_ADD, STDIN_FILENO, &ev_tcp);
    
    // Wait for events
    for(;;){
        
        int nfds = epoll_wait(epollfd_tcp, events, MAX_EVENTS, -1);
        if(nfds == -1){
            cleanup(socket_desc_tcp, epollfd_tcp);
            fprintf(stderr,"ERR: Error_tcp in epoll_wait\n");
            return EXIT_FAILURE;
        }
    
        for(int n = 0; n < nfds; ++n){
            
            if(events[n].data.fd == socket_desc_tcp){
                
                // Set socket to non-blocking mode
                int flags = fcntl(socket_desc_tcp, F_GETFL, 0);       
                if (flags == -1) {
                    cleanup(socket_desc_tcp, epollfd_tcp);
                    fprintf(stderr,"ERR: Error_tcp in fcntl 1\n");
                    return EXIT_FAILURE;
                }
                
                flags |= O_NONBLOCK;
                if (fcntl(socket_desc_tcp, F_SETFL, flags) == -1) {
                    cleanup(socket_desc_tcp, epollfd_tcp);
                    fprintf(stderr,"ERR: Error_tcp in fcntl 2\n");
                    return EXIT_FAILURE;
                }

                char server_reply[1500];
                char* message_start = server_reply;
                int total_bytes = 0;
                int bytes;

                // Receive data from the server
                while ((bytes = recv(socket_desc_tcp, server_reply + total_bytes, 1, 0)) > 0) {
                    total_bytes += bytes;

                    // Check if we've received a complete message
                    if (total_bytes >= 2 && server_reply[total_bytes - 2] == '\r' 
                        && server_reply[total_bytes - 1] == '\n') {
                        
                        // Null-terminate the message and handle it
                        server_reply[total_bytes - 2] = '\0';
                        handle_server_reply_tcp(message_start);

                        // Move on to the next message
                        message_start = server_reply + total_bytes;
                    }

                    // Check if we've filled the buffer
                    if (total_bytes == sizeof(server_reply)) {
                        error_tcp("ERR: Message is too long\n");
                        cleanup(socket_desc_tcp, epollfd_tcp);
                        return EXIT_FAILURE;
                    }
                }

                if (bytes == -1) {
                    
                    if (errno == EWOULDBLOCK || errno == EAGAIN) {
                        // No data available, continue with your loop
                    } else {
                        cleanup(socket_desc_tcp, epollfd_tcp);
                        fprintf(stderr,"ERR: Error_tcp in recv\n");
                        return EXIT_FAILURE;
                    }
                } else if (bytes == 0) {
                    cleanup(socket_desc_tcp, epollfd_tcp);
                    fprintf(stderr,"ERR: Server disconnected\n");
                    return EXIT_FAILURE;
                }

            } else if(events[n].data.fd == STDIN_FILENO){
                
                // Read user input and send data to the server
                char buffer[1400];
                if (fgets(buffer, 1400, stdin) == NULL) {
                    
                    // Ctrl+D was pressed, exit the program
                    if (strcmp(last_command_tcp, "") != 0) {          
                        char* bye_message = create_bye_message_tcp();
                        send(socket_desc_tcp, bye_message, strlen(bye_message), 0);
                    } 

                    cleanup(socket_desc_tcp, epollfd_tcp);
                    return EXIT_SUCCESS;  
                }
                
                // Message is_token empty, skip
                if(strlen(buffer) == 1){
                    continue;
                }
                
                // Message is_token either a command or a message
                if (buffer[0] == '/') {
                    handle_command_tcp(buffer, socket_desc_tcp);
                } else {
                    
                    if (!authenticated_tcp) {
                        fprintf(stderr, "ERR: You must authenticate first\n");
                        continue;
                    }
                    
                    // Replace newline with null character
                    buffer[strlen(buffer) - 1] = '\0'; 
                    
                    if (!is_print_or_space(buffer)) {
                        fprintf(stderr, "ERR: Invalid message\n");
                        continue;
                    }
                    
                    char* message = create_msg_message_tcp(global_display_name_tcp, buffer);
                    send(socket_desc_tcp, message, strlen(message), 0);
                }
            }
        }
    }
    cleanup(socket_desc_tcp, epollfd_tcp);
    return EXIT_SUCCESS;
}