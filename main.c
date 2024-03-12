/**
 * @file ipk24chat-client.c
 * @brief IPK Project 1 - Chat Client
 * @author Ivan Onufriienko
 * 
*/

#include "main.h"


void print_help() {
	printf("Usage: ipk24chat-client -t protocol -s server [-p port] [-d timeout] [-r retransmissions] [-h]\n");
	printf("Options:\n");
	printf("  -t protocol\t\tProtocol to use (tcp or udp)\n");
	printf("  -s server\t\tServer to connect to\n");
	printf("  -p port\t\tPort to connect to (default 4567)\n");
	printf("  -d timeout\t\tTimeout for retransmissions in ms (default 250)\n");
	printf("  -r retransmissions\tNumber of retransmissions (default 3)\n");
	printf("  -h\t\t\tPrint this help\n");
}



int main(int argc, char *argv[]){
    
    struct addrinfo hints, *res;
	int status;
	char ipstr[INET_ADDRSTRLEN];
	char *hostname = NULL;	
	int opt;
	char *protocol = NULL;	
	int port = 4567;
	int timeout = 250;
	int retransmissions = 3;

	while ((opt = getopt(argc, argv, "t:s:p:d:r:h")) != -1) {
			switch (opt) {
			case 't':
				protocol = optarg;
				break;
			case 's':
				hostname = optarg;
				break;
			case 'p':
				port = atoi(optarg);
				break;
			case 'd':
				timeout = atoi(optarg);
				break;
			case 'r':
				retransmissions = atoi(optarg);
				break;
			case 'h':
				print_help();
				exit(0);
			default:
				
				fprintf(stderr, "Unknown argument: %c\n", opt);
				exit(1);
			}
		}
	
	if (protocol == NULL || hostname == NULL) {
		
		fprintf(stderr, "Protocol and server must be specified\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; 
	hints.ai_socktype = SOCK_STREAM;

	if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0) {
		
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
		return 2;
	}

	struct sockaddr_in *ipv4 = (struct sockaddr_in *)res->ai_addr;
	void *addr = &(ipv4->sin_addr);
	inet_ntop(res->ai_family, addr, ipstr, sizeof ipstr);

	printf("%s resolved to : %s\n", hostname, ipstr);

	freeaddrinfo(res);
	
	if (strcmp(protocol, "tcp") == 0) {
		int return_code = tcp_connect(ipstr, port);
	} else if (strcmp(protocol, "udp") == 0) {
		int return_code = udp_connect(ipstr, port, timeout, retransmissions);
	} else {
		
		fprintf(stderr, "Unknown protocol: %s\n", protocol);
		return 1;
	}
	
	return 0;
    
}