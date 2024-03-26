/**
 * @file tcp.c
 * @brief IPK Project 1 - Chat Client
 * @author Ivan Onufriienko
 * 
*/

#include "tcp.h"
/*--------Global variables--------*/ 

// Saving display name for the whole session
char global_display_name_tcp[MAX_DNAME];    
// Used to check in what state the user is
char last_command_tcp[MAX_DNAME];
// Some allocations for graceful exit
int socket_desc_tcp = -1;                   
int epollfd_tcp = -1;                    
// Flag for checking if user is authenticated   
bool authenticated = false;

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



void handle_command_tcp(char* command, int socket_desc_tcp, char* display_name, fd_set* fds) {
    strncpy(last_command_tcp, command, 7);
    char extra[MAX_CONTENT] = "";
    if (strncmp(command, "/auth", 5) == 0) {
        if (authenticated) {
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
        strncpy(global_display_name_tcp, display_name, MAX_DNAME);
        char* message = create_auth_message_tcp(username, display_name, secret);
        send(socket_desc_tcp, message, strlen(message), 0);
        FD_CLR(STDIN_FILENO, fds); // Remove STDIN_FILENO from the file descriptor set
    } else if (strncmp(command, "/join", 5) == 0) {
        if(!authenticated){
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
        FD_CLR(STDIN_FILENO, fds); // Remove STDIN_FILENO from the file descriptor set
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
        strncpy(global_display_name_tcp, display_name, MAX_DNAME);
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

void error_tcp(char *msg) {
    fprintf(stderr, "ERR: %s\n", msg);
    char* err_message = create_err_message_tcp(global_display_name_tcp, msg);
    send(socket_desc_tcp, err_message, strlen(err_message), 0);
    char* bye_message = create_bye_message_tcp();
    send(socket_desc_tcp, bye_message, strlen(bye_message), 0);
    cleanup(socket_desc_tcp, epollfd_tcp);         
    exit(EXIT_FAILURE);
}

void handle_server_reply_tcp(char* reply, fd_set* fds) {
    char* token = strtok(reply, " ");
    if (strcasecmp(token, "REPLY") == 0) {
        char* status = strtok(NULL, " ");
        strtok(NULL, " "); // Skip "IS"
        char* message = strtok(NULL, "\r\n");
        if (message == NULL) {
            error_tcp("Invalid REPLY message from server");
        }
        if ((strncmp(last_command_tcp, "/auth", 5) == 0) || (strncmp(last_command_tcp, "/join", 5) == 0)){
            FD_SET(STDIN_FILENO, fds); // Add STDIN_FILENO back to the file descriptor set
        } 
        // FSM issue smh
        //else {
        //    error_tcp("REPLY on other than /auth or /join command");
        //}
        if (strcasecmp(status, "OK") == 0) {
            fprintf(stderr, "Success: %s\n", message);

            if (strncmp(last_command_tcp, "/auth", 5) == 0) {
                authenticated = true;
            } 
        } else if (strcasecmp(status, "NOK") == 0) {
            fprintf(stderr, "Failure: %s\n", message);
        } else {
            error_tcp("Unknown status in REPLY message");
        }

    } else if (strcasecmp(token, "MSG") == 0) {
        if (!authenticated) {
            error_tcp("Message from server before authentication");
        }
        strtok(NULL, " "); // Skip "FROM"
        char* from = strtok(NULL, " ");
        strtok(NULL, " "); // Skip "IS"
        char* message = strtok(NULL, "\r\n");
        if (message == NULL || from == NULL) {
            error_tcp("Invalid MSG message from server");
        }
        printf("%s: %s\n", from, message);
    } else if (strcasecmp(token, "ERR") == 0) {
        strtok(NULL, " "); // Skip "FROM"
        char* from = strtok(NULL, " ");
        strtok(NULL, " "); // Skip "IS"
        char* message = strtok(NULL, "\r\n");
        if (message == NULL || from == NULL) {
            error_tcp("Invalid ERR message from server");
        }
        fprintf(stderr, "ERR FROM %s: %s\n", from, message);
        send(socket_desc_tcp, create_bye_message_tcp(), strlen(create_bye_message_tcp()), 0);
        cleanup(socket_desc_tcp, epollfd_tcp);
        exit(EXIT_FAILURE);
    } else if (strncmp(token, "BYE", 3) == 0) {
        if (strtok(NULL, "\r\n") != NULL){
            error_tcp("Invalid BYE message from server");
        }
        cleanup(socket_desc_tcp, epollfd_tcp);
        exit(EXIT_SUCCESS);
    } else {
        error_tcp("Unknown message from server");
    }
}

void signal_handler_tcp(int sig) {
    cleanup(socket_desc_tcp, epollfd_tcp);
    exit(EXIT_SUCCESS);
}

int tcp_connect(char* ipstr, int port) {
    struct sockaddr_in server;
    struct epoll_event events[MAX_EVENTS];
    char display_name[MAX_DNAME];

    signal(SIGINT, signal_handler_tcp);

    socket_desc_tcp = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc_tcp == -1)
    {
        cleanup(socket_desc_tcp, epollfd_tcp);
        fprintf(stderr,"ERR: Could not create socket\n");
        return EXIT_FAILURE;
    }
        
    server.sin_addr.s_addr = inet_addr(ipstr);
    server.sin_family = AF_INET;
    server.sin_port = htons( port );

    if (connect(socket_desc_tcp , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        cleanup(socket_desc_tcp, epollfd_tcp);
        fprintf(stderr,"ERR: Connect error_tcp\n");
        return EXIT_FAILURE;
    }	
    
    // Create a file descriptor set
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    FD_SET(socket_desc_tcp, &fds);
    
    // Wait for events
    for(;;){
        // Make a copy of the file descriptor set because select() modifies it
        fd_set read_fds = fds;
    
        // Wait for data to be available on any file descriptor
        if (select(FD_SETSIZE, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select");
            return EXIT_FAILURE;
        }
    
        for(int fd = 0; fd < FD_SETSIZE; ++fd){
            if (FD_ISSET(fd, &read_fds)) {
                if(fd == socket_desc_tcp){
                    // Receive data from the server
                    char server_reply[1500];
                    int bytes = recv(socket_desc_tcp, server_reply, 1500, 0);
                    if(bytes == -1){
                        cleanup(socket_desc_tcp, epollfd_tcp);
                        fprintf(stderr,"ERR: Error_tcp in recv\n");
                        return EXIT_FAILURE;
                    } else if(bytes == 0){
                        cleanup(socket_desc_tcp, epollfd_tcp);
                        fprintf(stderr,"ERR: Server disconnected\n");
                        return EXIT_FAILURE;
                    }
                    handle_server_reply_tcp(server_reply, &fds);

                } else if(fd == STDIN_FILENO){
                    // Read user input and send data to the server
                    char buffer[1500];
                    if (fgets(buffer, 1500, stdin) == NULL) {
                        // Ctrl+D was pressed, exit the program
                        cleanup(socket_desc_tcp, epollfd_tcp);
                        return EXIT_SUCCESS;
                    }

                    // Message is empty, skip
                    if(strlen(buffer) == 1){
                        continue;
                    }

                    // Message is either a command or a message
                    if (buffer[0] == '/') {
                        handle_command_tcp(buffer, socket_desc_tcp, display_name, &fds);
                    } else {
                        if (!authenticated) {
                            fprintf(stderr, "ERR: You must authenticate first\n");
                            continue;
                        }
                        buffer[strlen(buffer) - 1] = '\0'; // Replace newline with null character
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
    }

    return EXIT_SUCCESS;
}