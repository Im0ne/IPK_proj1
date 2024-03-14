/**
 * @file tcp.c
 * @brief IPK Project 1 - Chat Client
 * @author Ivan Onufriienko
 * 
*/

#include "tcp.h"
/*--------Global variables--------*/ 

// Saving display name for the whole session
char global_display_name[MAX_DNAME];    
// Some allocations for graceful exit
int socket_desc = -1;                   
int epollfd = -1;                    
// Flag for checking if user is authenticated   
bool authenticated = false;
// Used to check in what state the user is
char last_command[MAX_DNAME];
// Blockade of stdin after /auth and /join
struct epoll_event ev;

char* create_auth_message(char* username, char* display_name, char* secret) {
    static char message[MAX_CONTENT];
    snprintf(message, sizeof(message), "AUTH %s AS %s USING %s\r\n", username, display_name, secret);
    return message;
}

char* create_join_message(char* channel_id, char* display_name) {
    static char message[MAX_CONTENT];
    snprintf(message, sizeof(message), "JOIN %s AS %s\r\n", channel_id, display_name);
    return message;
}

char* create_msg_message(char* display_name, char* message_content) {
    static char message[MAX_CONTENT];
    snprintf(message, sizeof(message), "MSG FROM %s IS %s\r\n", display_name, message_content);
    return message;
}

char* create_err_message(char* display_name, char* message_content) {
    static char message[MAX_CONTENT];
    snprintf(message, sizeof(message), "ERR FROM %s IS %s\r\n", display_name, message_content);
    return message;
}

char* create_bye_message() {
    return "BYE\r\n";
}

void print_help_tcp() {
    printf("Command \t Parameters \t\t\t Description\n");
    printf("/auth \t\t username secret display_name \t Authenticate with the server\n");
    printf("/join \t\t channel_id \t\t\t Join a channel\n");
    printf("/rename \t display_name \t\t\t Change your display name\n");
    printf("/help \t\t \t\t\t\t Print this help\n");
}

void cleanup(int socket_desc, int epollfd) {
    if (socket_desc != -1) {       
        close(socket_desc);
    }
    if (epollfd != -1) {
        close(epollfd);
    } 
}

bool is_alnum_or_dash(const char *str) {
    while (*str) {
        if (!isalnum((unsigned char)*str) && *str != '-')
            return false;
        str++;
    }
    return true;
}

bool is_print_or_space(const char *str) {
    while (*str) {
        if (!isprint((unsigned char)*str) && *str != ' ' && *str != '\n')
            return false;
        str++;
    }
    return true;
}

void handle_command(char* command, int socket_desc, char* display_name) {
    strncpy(last_command, command, MAX_DNAME);
    char extra[MAX_DNAME] = "";
    if (strncmp(command, "/auth", 5) == 0) {
        char username[MAX_DNAME] = "", secret[MAX_DNAME] = "", display_name[MAX_DNAME] = "";
        sscanf(command, "/auth %s %s %s %99[^\n]", username, secret, display_name, extra);
        if (strlen(username) == 0 || strlen(secret) == 0 || strlen(display_name) == 0 || strlen(extra) > 0
            || !is_alnum_or_dash(username) || !is_alnum_or_dash(secret) || !is_print_or_space(display_name)) {
            fprintf(stderr, "ERR: Invalid parameters for /auth\n");
            return;
        }
        strncpy(global_display_name, display_name, MAX_DNAME);
        char* message = create_auth_message(username, display_name, secret);
        send(socket_desc, message, strlen(message), 0);
        epoll_ctl(epollfd, EPOLL_CTL_DEL, STDIN_FILENO, &ev);
    } else if (strncmp(command, "/join", 5) == 0) {
        if(!authenticated){
            fprintf(stderr, "ERR: You must authenticate first\n");
            return;
        }
        char channel_id[MAX_DNAME] = "";
        sscanf(command, "/join %s %99[^\n]", channel_id, extra);
        if (strlen(channel_id) == 0 || strlen(extra) > 0 || !is_alnum_or_dash(channel_id)) {
            fprintf(stderr, "ERR: Invalid parameters for /join\n");
            return;
        }
        char* message = create_join_message(channel_id, global_display_name);
        send(socket_desc, message, strlen(message), 0);
        epoll_ctl(epollfd, EPOLL_CTL_DEL, STDIN_FILENO, &ev);
    } else if (strncmp(command, "/rename", 7) == 0) {
        if(!authenticated){
            fprintf(stderr, "ERR: You must authenticate first\n");
            return;
        }
        char display_name[MAX_DNAME] = "";
        sscanf(command, "/rename %s %99[^\n]", display_name, extra);
        if (strlen(display_name) == 0 || strlen(extra) > 0 || !is_print_or_space(display_name)) {
            fprintf(stderr, "ERR: Invalid parameters for /rename\n");
            return;
        }
        strncpy(global_display_name, display_name, MAX_DNAME);
    } else if (strncmp(command, "/help", 5) == 0) {
        sscanf(command, "/help %99[^\n]", extra);
        if(strlen(extra) > 0){
            fprintf(stderr, "ERR: Invalid parameters for /help\n");
            return;
        }
        print_help_tcp();      
    } else {
        fprintf(stderr,"ERR: Unknown command: %s\n", command);
    }
}

void handle_server_reply(char* reply) {
    char* token = strtok(reply, " ");
    if (strcmp(token, "REPLY") == 0) {
        char* status = strtok(NULL, " ");
        strtok(NULL, " "); // Skip "IS"
        char* message = strtok(NULL, "\r\n");
        if (strcmp(status, "OK") == 0) {
            fprintf(stderr, "Success: %s\n", message);

            if (strncmp(last_command, "/auth", 5) == 0) {
                authenticated = true;
            }
        } else if (strcmp(status, "NOK") == 0) {
            fprintf(stderr, "Failure: %s\n", message);
        }
        if ((strncmp(last_command, "/auth", 5) == 0) || (strncmp(last_command, "/join", 5) == 0)){
            ev.events = EPOLLIN;
            ev.data.fd = STDIN_FILENO;
            epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);
        }
    } else if (strcmp(token, "MSG") == 0) {
        strtok(NULL, " "); // Skip "FROM"
        char* from = strtok(NULL, " ");
        strtok(NULL, " "); // Skip "IS"
        char* message = strtok(NULL, "\r\n");
        printf("%s: %s\n", from, message);
    } else if (strcmp(token, "ERR") == 0) {
        strtok(NULL, " "); // Skip "FROM"
        char* from = strtok(NULL, " ");
        strtok(NULL, " "); // Skip "IS"
        char* message = strtok(NULL, "\r\n");
        fprintf(stderr, "ERR FROM %s: %s\n", from, message);
        send(socket_desc, create_bye_message(), strlen(create_bye_message()), 0);
        cleanup(socket_desc, epollfd);
        exit(EXIT_FAILURE);
    } else if (strcmp(token, "BYE") == 0) {
        cleanup(socket_desc, epollfd);
        exit(EXIT_SUCCESS);
    }
    else {
        send(socket_desc, create_err_message(global_display_name, "Unknown message"), 
        strlen(create_err_message(global_display_name, "Unknown message")), 0);
        send(socket_desc, create_bye_message(), strlen(create_bye_message()), 0);
        cleanup(socket_desc, epollfd);
        fprintf(stderr, "ERR: Unknown message from server\n");
        exit(EXIT_FAILURE);
    }
}

void signal_handler(int sig) {
    cleanup(socket_desc, epollfd);
    exit(EXIT_SUCCESS);
}

int tcp_connect(char* ipstr, int port) {
    struct sockaddr_in server;
    char *message, server_reply[2000];
    struct epoll_event ev, events[MAX_EVENTS];
    char display_name[MAX_DNAME];

    signal(SIGINT, signal_handler);

    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        cleanup(socket_desc, epollfd);
        fprintf(stderr,"ERR: Could not create socket");
        return EXIT_FAILURE;
    }
        
    server.sin_addr.s_addr = inet_addr(ipstr);
    server.sin_family = AF_INET;
    server.sin_port = htons( port );

    if (connect(socket_desc , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        cleanup(socket_desc, epollfd);
        fprintf(stderr,"ERR: Connect error.\n");
        return EXIT_FAILURE;
    }	

    // Create epoll
    epollfd = epoll_create1(0);
    if(epollfd == -1){
        cleanup(socket_desc, epollfd);
        fprintf(stderr,"ERR: Error in epoll creation.\n");
        return EXIT_FAILURE;
    }

    // Add the socket to the epoll instance
    ev.events = EPOLLIN;
    ev.data.fd = socket_desc;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, socket_desc, &ev);
    
    // Add STDIN_FILENO to the epoll instance
    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);
    
    // Wait for events
    for(;;){
        int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if(nfds == -1){
            cleanup(socket_desc, epollfd);
            fprintf(stderr,"ERR: Error in epoll_wait.\n");
            return EXIT_FAILURE;
        }
    
        for(int n = 0; n < nfds; ++n){
            if(events[n].data.fd == socket_desc){
                // Clear the buffer
                memset(server_reply, 0, sizeof(server_reply));
                // Receive data from the server
                int bytes = recv(socket_desc, server_reply, 2000, 0);
                if(bytes == -1){
                    cleanup(socket_desc, epollfd);
                    fprintf(stderr,"ERR: Error in recv.\n");
                    return EXIT_FAILURE;
                } else if(bytes == 0){
                    cleanup(socket_desc, epollfd);
                    fprintf(stderr,"ERR: Server disconnected.\n");
                    return EXIT_FAILURE;
                }
                handle_server_reply(server_reply);

            } else if(events[n].data.fd == STDIN_FILENO){
                // Read user input and send data to the server
                char buffer[1500];
                if (fgets(buffer, 1500, stdin) == NULL) {
                    // Ctrl+D was pressed, exit the program
                    cleanup(socket_desc, epollfd);
                    return EXIT_SUCCESS;
                }
                
                // Message is empty, skip
                if(strlen(buffer) == 1){
                    continue;
                }
                
                // Message is either a command or a message
                if (buffer[0] == '/') {
                    handle_command(buffer, socket_desc, display_name);
                } else {
                    if (!authenticated) {
                        fprintf(stderr, "ERR: You must authenticate first\n");
                        continue;
                    }
                    if (!is_print_or_space(buffer)) {
                        fprintf(stderr, "ERR: Invalid message\n");
                        continue;
                    }
                    char* message = create_msg_message(global_display_name, buffer);
                    send(socket_desc, message, strlen(message), 0);
                }
                
            }
        }
    }

    return EXIT_SUCCESS;
}