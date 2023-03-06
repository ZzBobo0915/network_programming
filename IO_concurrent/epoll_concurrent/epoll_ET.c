#include <stdio.h>
#include <sys/socket.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/epoll.h>
#include <errno.h>

#define MAX_EVENTS 1024
#define BUFLEN 4098
#define SERV_PORT 6666

/* TO DO: epoll基于非阻塞I/O事件驱动 */

/* 描述就绪文件描述符相关信息 */
struct myevent_s{
	int fd;                                            //要监听的文件描述符
	int events;                                        //对应的监听事件
	void *arg;                                         //泛型参数
	void (*call_back)(int fd, int events, void *arg);  //回掉函数
	int status;                                        //是否在监听:1在 0不在
	char buf[BUFLEN];                                  //
	int len;                                           //
	long last_active;                                  //记录每次加入红黑树的时间值
};

int g_efd;  //保存红黑树树根
struct myevent_s g_events[MAX_EVENTS+1];  // +1->listenfd


/* 自定义事件,用来设置myevent_s的参数,主要用来设置回调函数 */
void eventset(struct myevent_s *ev, int fd, void (*call_back)(int, int, void*), void *arg)
{
	ev->fd = fd;
	ev->call_back = call_back;
	ev->events = 0;
	ev->status = 0;
	ev->arg = arg;
	if (ev->len <= 0){
		memset(ev->buf, 0, sizeof(ev->buf));
		ev->len = 0;
	}
	ev->last_active = time(NULL);

	return ;
}

/* 向epoll监听的红黑树中添加一个文件描述符 */
void eventadd(int efd, int events, struct myevent_s *ev)
{
	struct epoll_event epv = {0, {0}};
	int op;
	epv.data.ptr = ev;
	epv.events = ev->events = events;  //EPOLLIN或EPOLLPUT
	
	if (ev->status == 0) {  // 是否在红黑树上
		op = EPOLL_CTL_ADD;
		ev->status = 1;
	}

	if (epoll_ctl(efd, op, ev->fd, &epv) < 0)  // 实际添加/修改
		printf("event add failed [fd=%d], events[%d]\n", ev->fd, events);
	else 
		printf("event add OK [fd=%d], op=%d, events[%0X]\n", ev->fd, op, events);
	
	return ;	
}

void recvdata(int, int, void*);
void eventdel(int, struct myevent_s*);

/* 发送回给客户端的数据 */
void senddata(int fd, int events, void *arg)
{
	struct myevent_s *ev = (struct myevent_s*)arg;
    int len;

	len = send(fd, ev->buf, ev->len, 0);  // 直接将数据, 回写给客户端,未作处理

	eventdel(g_efd, ev);  // 从红黑树移除
	if (len > 0) {
		printf("send[fd=%d], [%d]%s\n", fd, len, ev->buf);
		eventset(ev, fd, recvdata, ev);  // 将该fd的回调函数改为recvdata
		eventadd(g_efd, EPOLLIN, ev);       // 添加到红黑树上,监听其读事件
	}else {
		close(ev->fd);  // 关闭连接
		printf("send[fd=%d] error %s]", fd, strerror(errno));
	}

	return;
}

/* 读取客户端发来的数据 */
void recvdata(int fd, int events, void *arg)
{
	struct myevent_s *ev = (struct myevent_s*)arg;
	int len;
	/* 读文件描述符,数据存入buf中,在网络编程中,read函数与flag=0的recv函数作用相同,同理还有write和send*/
	len = recv(fd, ev->buf, sizeof(ev->buf), 0);  	

	eventdel(g_efd, ev);  // 将该节点从红黑树上删除

	if (len > 0) {
		ev->len = len;
		ev->buf[len] = '\0';  // 手动添加结束标记'\0'
		printf("C[%d]:%s\n", fd, ev->buf);
	
		eventset(ev, fd, senddata, ev);  // 设置该fd对应的回调函数为senddata
		eventadd(g_efd, EPOLLOUT, ev);   // 讲fd加入红黑树g_efd中,监听其写事件
	}else if (len = 0) {
		close(ev->fd);
		/* ev-g_events 地址相减得到偏移元素位置 */
		printf("[fd=%d] pos[%ld] closed\n", fd, ev-g_events);
	}else {
		close(ev->fd);
		printf("recv[fd=%d] error[%d]:%s\n", fd, errno, strerror(errno));
	}

	return ;
}


/* 从epoll监听红黑树中删除一个文件描述符 */
void eventdel(int efd, struct myevent_s *ev)
{
	struct epoll_event epv = {0, {0}};

	if (ev->status != 1) return;  // 不在树上,不需要删除

	epv.data.ptr = NULL;
	ev->status = 0;  // 修改状态
	epoll_ctl(efd, EPOLL_CTL_DEL, ev->fd, &epv);  // 摘除
	
	return ;
}

/* 当前文件描述符就绪,epoll返回,调用该函数 与客户端建立链接 */
void acceptconn(int lfd, int events, void *arg)
{
    struct sockaddr_in cin;
	socklen_t len = sizeof(cin);
	
	int cfd, i;
	if ((cfd = accept(lfd, (struct sockaddr*)&cin, &len)) == -1) {
		if (errno != EAGAIN && errno != EINTR) {
			// 暂时不作处理
		}
 		// __func__ 表示当前函数的名字 __LINE__ 表示当前行号
		printf("%s accept, %s\n", __func__, strerror(errno));
	}

	do {
		for (i = 0; i < MAX_EVENTS; i++) {  // 从全局数组中找出一个空闲元素,跳出
			if (g_events[i].status == 0) break;
		}

		if (i == MAX_EVENTS) {
			printf("%s: max connect limit[%d].\n", __func__, MAX_EVENTS);
			break;
		}

		int flag = 0;
		if ((flag = fcntl(cfd, F_SETFL, O_NONBLOCK)) < 0) {  // 讲cfd设置为非阻塞
			printf("%s: fcntl nonblocking failed, %s\n", __func__, strerror(errno));
			break;
		}

		/* 给cfd设置一个myevent_s结构体,回调函数 设置为recvdata */
		eventset(&g_events[i], cfd, recvdata, &g_events[i]);
		eventadd(g_efd, EPOLLIN, &g_events[i]);  // 将cfd添加到红黑树g_efd中,监听读事件
	} while (0);

	printf("new connect [%s:%d][time:%ld], pos[%d].\n",
			inet_ntoa(cin.sin_addr), ntohs(cin.sin_port), g_events[i].last_active, i);

	return ;
}

/* 创建socket 初始化lfd */
void initlistensocket(int g_efd, int port){
	struct sockaddr_in sin;
	
	int lfd = socket(AF_INET, SOCK_STREAM, 0);
	fcntl(lfd, F_SETFL, O_NONBLOCK);  // 设置非阻塞

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr.s_addr = htonl(INADDR_ANY);

	bind(lfd, (struct sockaddr*)&sin, sizeof(sin));
	listen(lfd, 20);

	/*  */
	eventset(&g_events[MAX_EVENTS], lfd, acceptconn, &g_events[MAX_EVENTS]);

	/* 将lfd添加到监听红黑树中 */
	eventadd(g_efd, EPOLLIN, &g_events[MAX_EVENTS]);

	return ;
}

int main(int argc, char *argv[]){
	unsigned short port = SERV_PORT;
	if (argc == 2) port = atoi(argv[1]);  // 假如用户指定端口
	
	g_efd = epoll_create(MAX_EVENTS+1);  // 创建红黑树
    
	initlistensocket(g_efd, port);  // 初始化监听socket *重点函数*
	
	struct epoll_event events[MAX_EVENTS+1];  // 保存已经满足就绪事件的文件描述符,为epoll_wait服务
	printf("server running:port[%d]\n", port);
	
	int checkpos = 0, i;

	while (1) {
		/*用来就行超时验证 */
		long now = time(NULL);  // 当前时间
		
		for (i = 0; i < 100; i++, checkpos++) {  // 一次循环检测100个
			if (checkpos == MAX_EVENTS) checkpos = 0;  // checkpos循环过MAX_EVENTS次 归零
			if (g_events[checkpos].status != 1) continue;  // 不在红黑树上

			long duration = now - g_events[checkpos].last_active;  // 客户端不活跃时间

			if (duration >= 60) {
				close(g_events[checkpos].fd);  // 关闭与该客户端的链接
				printf("[fd=%d] timeout\n", g_events[checkpos].fd);
				eventdel(g_efd, &g_events[checkpos]);  // 从监听红黑树上删除节点
			}
		}		
	
		/* 监听红黑树g_efd,将满足的事件描述符添加到events数组中,1s没有事件满足,返回0 */
		int nfd = epoll_wait(g_efd, events, MAX_EVENTS+1, 1000);
		if (nfd < 0) {
			printf("epoll_wait error, exit!\n");
			break;
		}
		for (i = 0; i < nfd; ++i) {
			/* 使用自定义结构体myevent_s类型指针,接收联合体中data的void *ptr成员 */
			struct myevent_s *ev = (struct myevent_s*)events[i].data.ptr;

			if ((events[i].events & EPOLLIN) && (ev->events & EPOLLIN)) {  // 读就绪事件
				ev->call_back(ev->fd, events[i].events, ev->arg);
			}
			if ((events[i].events & EPOLLOUT) && (ev->events & EPOLLOUT)) {  // 写就绪事件
				ev->call_back(ev->fd, events[i].events, ev->arg);
			}
		}
	}
	
	/* 退出前释放所有资源 */
	return 0;
}
