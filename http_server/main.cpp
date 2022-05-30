#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <iostream>
#include"threadpool.h"
#include"http.h"
using namespace std;

#define PORT 80
#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

extern void addfd(int epollfd, int fd, bool one_shot);

void addsig(int sig, void(handler)(int)){ // handler回调函数，用来处理信号
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler =handler;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

int main()
{
    int listenfd;
    struct sockaddr_in server_addr;
    addsig(SIGPIPE, SIG_IGN);

    threadpool<http>*pool = nullptr;
    try
    {
        pool = new threadpool<http>;
    }
    catch(...)
    {
        return 0;
    }
    

    http* user = new http[MAX_FD];
    char buf[1024];
    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        fprintf(stderr, "socket error:%s\n", strerror(errno));
        exit(1);
    }

    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)); //端口复用

    bzero(&server_addr, sizeof(server_addr));   //

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if(bind(listenfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1)
    {
        fprintf(stderr, "bind error:%s\n", strerror(errno));
        exit(1);
    }
    
    if(listen(listenfd, 128) == -1)
    {
        fprintf(stderr, "listen error:%s\n", strerror(errno));
        exit(1);
    }

    int epollfd = epoll_create(128);

    http::m_epollfd = epollfd;          //将epoll的根设置为静态

    epoll_event temp, events[MAX_EVENT_NUMBER];
    addfd(epollfd, listenfd, false);

    while(true)
    {
        int fd_num = epoll_wait(epollfd, events, 10000, -1);
        if(fd_num == -1)
        {
            fprintf(stderr, "epoll_wait error:%s\n", strerror(errno));
            exit(1);
        }
        for(int i = 0; i < fd_num; i++)
        {   
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd)
            {
                
                struct sockaddr_in client_addr;
                socklen_t client_len = sizeof(client_addr);
                int client_fd = accept(listenfd, (struct sockaddr*)&client_addr, &client_len);
                
                if(client_fd == -1)
                {
                    fprintf(stderr, "accept error:%s\n", strerror(errno));
                    exit(1);
                }
                if(http::m_customer_count >= MAX_FD)
                {
                    
                    close(client_fd);
                    continue;
                }
                user[client_fd].init(client_fd, client_addr);
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                user[sockfd].close_connect();
            }
            else if(events[i].events & EPOLLIN)
            {
                //user[sockfd].read_data();
                printf("-----------------read sockfd = %d---------------------\n", sockfd);
                if(user[sockfd].read_data())
                {
                   pool->append(&user[sockfd]);
                }
                else
                {
                    user[sockfd].close_connect();
                }
            }
            else if(events[i].events & EPOLLOUT)
            {
                printf("-----------------write sockfd = %d---------------------\n", sockfd);
                pool->append(&user[sockfd]);
            }
        }
    }
    close(epollfd);
    close(listenfd);
    delete[] user;
    delete pool;
    return 0;
}