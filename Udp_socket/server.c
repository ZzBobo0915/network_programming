#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

int main(void)
{
	int listenfd;
	int i, n;
	struct sockaddr_in serv_addr, clie_addr;
	char buf[80];
	char str[INET_ADDRSTRLEN];
	socklen_t clie_len;
	listenfd = socket(AF_INET, SOCK_DGRAM, 0);
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(6666);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	listen(listenfd, 10);

	while (1) {
		clie_len = sizeof(clie_addr);
		n = recvfrom(listenfd, buf, 80, 0, (struct sockaddr*)&clie_addr, &clie_len);
		if (n == -1) perror("recvfrom error");
		printf("recvfrom from %s at PORT:%d\n",
					inet_ntop(AF_INET, &clie_addr.sin_addr, str, sizeof(str)),
					ntohs(clie_addr.sin_port));
		for (i = 0; i < n; ++i) buf[i] = toupper(buf[i]);
		n = sendto(listenfd, buf, n, 0, (struct sockaddr*)&clie_addr, sizeof(clie_addr));
		if (n == -1)
			perror("sendto error");
	} 

	close(listenfd);
	
	return 0;
}
