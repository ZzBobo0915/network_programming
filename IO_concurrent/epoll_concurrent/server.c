#include <stdio.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <string.h>
#include "wrap.h"
#include <stdlib.h>
#include <sys/socket.h>

#define MAXLINE 1024

int main(int arg, char *argc[])
{
	int listenfd, connfd, epfd, i;
	struct sockaddr_in serv_addr, clie_addr;
	socklen_t clie_len;
	char buf[1024];
	char str[INET_ADDRSTRLEN];  //16

	listenfd = Socket(AF_INET, SOCK_STREAM, 0);
	bzero(&serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(6666);
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	Bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
	Listen(listenfd, 20);
	
	epfd = epoll_create(MAXLINE);  // 创建epoll监听红黑树
	struct epoll_event tmp, ep[MAXLINE];  // tmp临时存储,ep存储所有的
    // 初始化
	tmp.events = EPOLLIN;
	tmp.data.fd = listenfd;
	// listenfd加入监听
	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &tmp);

	for (;;) {
		int ret = epoll_wait(epfd, ep, 1024, -1);  // 返回满足监听个数
		if (ret == -1) perr_exit("epoll_wait error");
		for (i = 0; i < ret; ++i) {
			if (ep[i].data.fd == listenfd) {  // listenfd监听到,说明有新的连接进入
				clie_len = sizeof(clie_addr);
				connfd = Accept(listenfd, (struct sockaddr*)&clie_addr, &clie_len);
				printf("received from %s at PORT:%d\n",
                        inet_ntop(AF_INET, &clie_addr.sin_addr, str, sizeof(str)),
                        ntohs(clie_addr.sin_port));		
				// 初始化
				tmp.events = EPOLLIN;
				tmp.data.fd = connfd;
				epoll_ctl(epfd, EPOLL_CTL_ADD, connfd, &tmp);  // 添加到监听红黑树
			} else {  // 说明是别的文件描述符的操作
				int n;
				n = Read(ep[i].data.fd, buf, sizeof(buf));
				if (n == 0) {  // 说明对方关闭读连接,服务端这里也要关闭a
					printf("%s:%d has been closed.\n",
                        	inet_ntop(AF_INET, &clie_addr.sin_addr, str, sizeof(str)),
                        	ntohs(clie_addr.sin_port));
					Close(ep[i].data.fd);  // 关闭连接
					epoll_ctl(epfd, EPOLL_CTL_DEL, ep[i].data.fd, NULL);  // 从监听红黑树中删除	
				} else {
					int j;
					for (j = 0; j < n; ++j) buf[j] = toupper(buf[j]);
					Write(ep[i].data.fd, buf, n);
					Write(STDOUT_FILENO, buf, n);
				}
			}	
		}
			
	}
	
	close(listenfd);
	return 0;
}
