/**
 * @file tcp.c
 * @brief IPK Project 1 - Chat Client
 * @author Ivan Onufriienko
 * 
*/

#include "tcp.h"

char global_display_name[MAX_DNAME];
int socket_desc = -1;
int epollfd = -1;
bool authenticated = false;
char last_command[MAX_DNAME];

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

char* create_reply_message(char* status, char* message_content) {
    static char message[MAX_CONTENT];
    snprintf(message, sizeof(message), "REPLY %s IS %s\r\n", status, message_content);
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

void handle_command(char* command, int socket_desc, char* display_name) {
    strncpy(last_command, command, MAX_DNAME);
    char extra[MAX_DNAME] = "";
    if (strncmp(command, "/auth", 5) == 0) {
        char username[MAX_DNAME] = "", secret[MAX_DNAME] = "", display_name[MAX_DNAME] = "";
        sscanf(command, "/auth %s %s %s %99[^\n]", username, secret, display_name, extra);
        if (strlen(username) == 0 || strlen(secret) == 0 || strlen(display_name) == 0 || strlen(extra) > 0) {
            fprintf(stderr, "ERR: Invalid parameters for /auth\n");
            return;
        }
        strncpy(global_display_name, display_name, MAX_DNAME);
        char* message = create_auth_message(username, display_name, secret);
        send(socket_desc, message, strlen(message), 0);
    } else if (strncmp(command, "/join", 5) == 0) {
        if(!authenticated){
            fprintf(stderr, "ERR: You must authenticate first\n");
            return;
        }
        char channel_id[MAX_DNAME] = "";
        sscanf(command, "/join %s %99[^\n]", channel_id, extra);
        if (strlen(channel_id) == 0 || strlen(extra) > 0) {
            fprintf(stderr, "ERR: Invalid parameters for /join\n");
            return;
        }
        char* message = create_join_message(channel_id, display_name);
        send(socket_desc, message, strlen(message), 0);
    } else if (strncmp(command, "/rename", 7) == 0) {
        if(!authenticated){
            fprintf(stderr, "ERR: You must authenticate first\n");
            return;
        }
        sscanf(command, "/rename %s %99[^\n]", display_name, extra);
        if (strlen(display_name) == 0 || strlen(extra) > 0) {
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
        char* message = strtok(NULL, "\r\n");
        if (strcmp(status, "OK") == 0) {
            fprintf(stderr, "Success: %s\n", message);
            strtok(NULL, " "); // Skip "IS"
            if (strncmp(last_command, "/auth", 5) == 0) {
                authenticated = true;
            }
        } else if (strcmp(status, "NOK") == 0) {
            fprintf(stderr, "Failure: %s\n", message);
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
        fprintf(stderr, "ERR %s: %s\n", from, message);
    } else if (strcmp(token, "BYE") == 0) {
        cleanup(socket_desc, epollfd);
        exit(0);
    }
    else {
        send(socket_desc, create_err_message(global_display_name, "Unknown command"), 
        strlen(create_err_message(global_display_name, "Unknown command")), 0);
        send(socket_desc, create_bye_message(), strlen(create_bye_message()), 0);
        cleanup(socket_desc, epollfd);
        exit(0);
    }
}

void signal_handler(int sig) {
    send(socket_desc, create_bye_message(), strlen(create_bye_message()), 0);
    cleanup(socket_desc, epollfd);
    exit(0);
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
        return 1;
    }
        
    server.sin_addr.s_addr = inet_addr(ipstr);
    server.sin_family = AF_INET;
    server.sin_port = htons( port );

    if (connect(socket_desc , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        cleanup(socket_desc, epollfd);
        fprintf(stderr,"ERR: Connect error.\n");
        return 1;
    }	

    // Create epoll
    epollfd = epoll_create1(0);
    if(epollfd == -1){
        cleanup(socket_desc, epollfd);
        fprintf(stderr,"ERR: Error in epoll creation.\n");
        return 1;
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
            return 1;
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
                    return 1;
                } else if(bytes == 0){
                    cleanup(socket_desc, epollfd);
                    fprintf(stderr,"ERR: Server disconnected.\n");
                    return 1;
                }
                handle_server_reply(server_reply);

            } else if(events[n].data.fd == STDIN_FILENO){
                // Read user input and send data to the server
                char buffer[2000];
                fgets(buffer, 2000, stdin);
                if(strlen(buffer) == 1){
                    continue;
                }
                if (buffer[0] == '/') {
                    handle_command(buffer, socket_desc, display_name);
                } else {
                    if (!authenticated) {
                        fprintf(stderr, "ERR: You must authenticate first\n");
                        continue;
                    }
                    char* message = create_msg_message(global_display_name, buffer);
                    send(socket_desc, message, strlen(message), 0);
                }
                
            }
        }
    }

    return 0;
}