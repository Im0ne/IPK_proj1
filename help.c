/**
 * @file help.c
 * @brief IPK Project 1 - Chat Client
 * @author Ivan Onufriienko
 * 
*/

#include "help.h"



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
        if (!isprint((unsigned char)*str) && *str != ' ')
            return false;
        str++;
    }
    return true;
}

void print_help_client() {
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
