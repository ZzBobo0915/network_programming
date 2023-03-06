#include <stdio.h>
#include <stdlib.h>
#include "wrap.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>

#define MAXLINE 80

int main(int argc, char *argv[])
{
	// maxfd表示最大文件描述符
	int i, maxi, maxfd, listenfd, connfd, sockfd;
	
	char buf[MAXLINE];
	char str[INET_ADDRSTRLEN];  // 16
	// FD_SETSIZE默认为1024; nready为每次select返回值
	int nready, client[FD_SETSIZE];
	// allset为总的文件描述符集  
	fd_set rset, allset;
    socklen_t clie_len;
	ssize_t n;
	struct sockaddr_in serv_addr, clie_addr;
	listenfd = Socket(AF_INET, SOCK_STREAM, 0);
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(6666);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	Bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	Listen(listenfd, 20);  // 默认最大为128
	
	maxfd = listenfd;  // 初始化
	maxi = -1;  // client[]的下标
	

	for (i = 0; i < FD_SETSIZE; ++i) client[i] = -1;  // 初始化client
	
	FD_ZERO(&allset);  // 清空allset
	FD_SET(listenfd, &allset);  // 构造select监控文件描述符集
	
	for (;;) {
		rset = allset;  // 每次循环都从新设置监控信号集
		nready = select(maxfd+1, &rset, NULL, NULL, NULL);
		if (nready < 0) perr_exit("select error");
		if (FD_ISSET(listenfd, &rset)) {  // new client connection
				clie_len = sizeof(clie_addr);
				connfd = Accept(listenfd, (struct sockaddr*)&clie_addr, &clie_len);
				printf("received from %s at PORT:%d\n",
                        inet_ntop(AF_INET, &clie_addr.sin_addr, str, sizeof(str)),
                        ntohs(clie_addr.sin_port));
                for (i = 0; i < FD_SETSIZE; ++i) {
					if (client[i] < 0){ 
						client[i] = connfd;  // 保存accepet返回的文件描述符到client
						break;
					}
				}
				if (i == FD_SETSIZE) {  // 达到上限
					fputs("too many clients!\n", stderr);
					exit(1);
				}
				FD_SET(connfd, &allset);  // 添加新的文件描述符到集合中
				if (connfd > maxfd) maxfd = connfd;  // 是否大于最大描述符
				if (i > maxi) maxi = i;  // 更新client的最大下标值
				// 如果没有更多的就绪文件描述符,就继续返回上面select监听
                if (--nready == 0) continue; 			
		}
		// 能走到这一步说明有需要处理的文件描述符
		for (i = 0; i <= maxi; ++i) {  // 检测哪个client有效
			if ((sockfd = client[i]) < 0) continue;
			if (FD_ISSET(sockfd, &rset)) {
				if ((n = Read(sockfd, buf, MAXLINE)) == 0) {  // sockfd的client关闭
					printf("%s:%d has been closed.\n",
                        	inet_ntop(AF_INET, &clie_addr.sin_addr, str, sizeof(str)),
                        	ntohs(clie_addr.sin_port));
					Close(sockfd);
					FD_CLR(sockfd, &allset);  // 解除监控这个文件描述符,以后不再监控他(要在allset中解除)
					client[i] = -1;
				}else {
					int j;
					for (j = 0; j < n; j++) buf[j] = toupper(buf[j]);
					Write(sockfd, buf, n);
					Write(STDOUT_FILENO, buf, n);
				}
				if (--nready == 0) break;  // 同上
			}
		}
	}
	close(listenfd);

	return 0;
}
	
