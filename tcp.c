

#include "tcp.h"

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
    printf("Command \t\t Parameters \t\t Description\n");
    printf("/auth \t\t username secret display_name \t\t Authenticate with the server\n");
    printf("/join \t\t channel_id \t\t Join a channel\n");
    printf("/rename \t\t display_name \t\t Change your display name\n");
    printf("/help \t\t \t\t Print this help\n");
}

void handle_command(char* command, int socket_desc, char* display_name) {
    char* token = strtok(command, " ");
    if (strcmp(token, "/auth") == 0) {
        char* username = strtok(NULL, " ");
        char* secret = strtok(NULL, " ");
        char* display_name = strtok(NULL, " ");
        char* message = create_auth_message(username, display_name, secret);
        send(socket_desc, message, strlen(message), 0);
    } else if (strcmp(token, "/join") == 0) {
        char* channel_id = strtok(NULL, " ");
        char* message = create_join_message(channel_id, display_name);
        send(socket_desc, message, strlen(message), 0);
    } else if (strcmp(token, "/rename") == 0) {
        display_name = strtok(NULL, " ");
    } else if (strcmp(token, "/help") == 0) {
        print_help_tcp();
    } else {
        printf("Unknown command: %s\n", command);
    }
}

void cleanup(int socket_desc, int epollfd) {
    if (socket_desc != -1) {
        close(socket_desc);
    }
    if (epollfd != -1) {
        close(epollfd);
    } 
}

int tcp_connect(char* ipstr, int port) {
    int socket_desc = -1;
    int epollfd = -1;
    struct sockaddr_in server;
    char *message, server_reply[2000];
    struct epoll_event ev, events[MAX_EVENTS];
    char display_name[MAX_DNAME];

    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        cleanup(socket_desc, epollfd);
        fprintf(stderr,"Could not create socket");
        return 1;
    }
        
    server.sin_addr.s_addr = inet_addr(ipstr);
    server.sin_family = AF_INET;
    server.sin_port = htons( port );

    if (connect(socket_desc , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        cleanup(socket_desc, epollfd);
        fprintf(stderr,"connect error");
        return 1;
    }	
    puts("Connected");

    // Create epoll
    epollfd = epoll_create1(0);
    if(epollfd == -1){
        cleanup(socket_desc, epollfd);
        fprintf(stderr,"Error in epoll creation.\n");
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
            fprintf(stderr,"Error in epoll_wait.\n");
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
                    fprintf(stderr,"Error in recv.\n");
                    return 1;
                } else if(bytes == 0){
                    printf("Server disconnected.\n");
                    return 1;
                }
                printf("Server: %s\n", server_reply);

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
                    char* message = create_msg_message(display_name, buffer);
                    send(socket_desc, message, strlen(message), 0);
                }
                
            }
        }
    }

    return 0;
}