/**
 * @file udp.h
 * @brief IPK Project 1 - Chat Client
 * @author Ivan Onufriienko
 * 
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <netdb.h>	
#include <sys/epoll.h>
#include <getopt.h>
#include <signal.h>


int udp_connect(char* ipstr, int port, int timeout, int retransmissions);