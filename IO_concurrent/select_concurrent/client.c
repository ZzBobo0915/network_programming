#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "wrap.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

#define MAXLINE 80
		
int main(int argc, char *argv[])
{
	int sockfd;
	char buf[MAXLINE];
	struct sockaddr_in serv_addr;
	sockfd = Socket(AF_INET, SOCK_STREAM, 0);
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(6666);
	inet_pton(AF_INET, "192.168.153.33", &serv_addr.sin_addr);
	Connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	
	while (fgets(buf, MAXLINE, stdin) != NULL) {
		int n = Write(sockfd, buf, strlen(buf));
		n = Read(socket, buf, n);
		if (n == 0) {
			printf("client over!\n");
			break;
		}else 
			Write(STDOUT_FILENO, buf, n);
	}
	return 0; 	
}
