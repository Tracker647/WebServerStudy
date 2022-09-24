#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
const int MAX_EVENTS = 10000;

/* 每个客户连接不停的向服务器发送这个请求 */
static const char *request = "GET http://localhost/index.html HTTP/1.1\r\nConnection: keep-alive\r\n\r\nxxxxxxxxxxxxxxxxxxxxxx";
// static const char *request = "GET / HTTP/1.1\r\nHost: 47.99.137.171:9006\r\nConnection: keep-alive\r\n\r\n\r\n";

int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epoll_fd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLOUT | EPOLLET | EPOLLERR;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}


/* 向服务器写入len字节的数据 */
bool write_nbytes(int sockfd, const char *buffer, int len)
{
    int bytes_write = 0;
    printf("write out %d bytes to socket %d\n", len, sockfd);
    while (1)
    {
        bytes_write = send(sockfd, buffer, len, 0);
        if (bytes_write == -1)
        {
            return false;
        }
        else if (bytes_write == 0)
        {
            return false;
        }

        len -= bytes_write;
        buffer = buffer + bytes_write;
        if (len <= 0)
        {
            return true;
        }
    }
}

/* 从服务器读取数据 */
bool read_once(int sockfd, char *buffer, int len)
{
    int bytes_read = 0;
    memset(buffer, '\0', len);
    bytes_read = recv(sockfd, buffer, len, 0);
    if (bytes_read == -1)
    {
        return false;
    }
    else if (bytes_read == 0)
    {
        return false;
    }

    printf("read in %d bytes from socket %d with content: %s\n", bytes_read, sockfd, buffer);

    return true;
}

void start_test(int epfd, const char *ip, int port, int num){
    struct sockaddr_in serv_addr;
    bzero(&serv_addr,sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    for(int i = 0; i < num; i++){
        int sockfd = socket(PF_INET,SOCK_STREAM,0);
        printf("create client sock %d fd: %d\n",i + 1, sockfd);
        if(sockfd < 0){
            perror("sock create failed");
            continue;
        }
        if(connect(sockfd,(struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0){
            printf("build connection %d\n",i);
            addfd(epfd,sockfd);
        }
    }
}

void close_conn(int epoll_fd, int sockfd)
{
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, sockfd, 0);
    printf("close fd %d\n",sockfd);
    close(sockfd);
}

int main(int argc, char *argv[])
{
    int epfd = epoll_create(5);
    if(argc < 4){
        printf("usage:.%s ip port number\n",argv[0]);
        return 0;
    }
    start_test(epfd,argv[1],atoi(argv[2]),atoi(argv[3]));
    epoll_event event_arr[MAX_EVENTS];
    char rebuf[2048];

    while(1){
        //等待服务端向客户端回传响应
        printf("stress cilent waiting....\n");
        int n_event = epoll_wait(epfd,event_arr,MAX_EVENTS, 2000);
        for(int i = 0; i < n_event; i++){
            int sockfd = event_arr[i].data.fd;
            if(event_arr[i].events & EPOLLIN){
                if(!read_once(sockfd, rebuf, sizeof(rebuf))){
                    printf("read close: ");
                    close_conn(epfd, sockfd);
                }
                printf("read success, sign socket %d EPOLLOUT | EPOLLET | EPOLLERR\n", sockfd);
                struct epoll_event ev;
                ev.events = EPOLLOUT | EPOLLET | EPOLLERR;
                ev.data.fd = sockfd;
                epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
            }
            else if(event_arr[i].events & EPOLLOUT){
                if(!write_nbytes(sockfd,request,strlen(request))){     
                    printf("write close:");            
                    close_conn(epfd, sockfd);
                }

                printf("write success, sign socket %d EPOLLIN| EPOLLET | EPOLLERR\n", sockfd);
                //发送请求后 注册EPOLLIN 等着读服务器响应
                struct epoll_event ev;
                ev.events = EPOLLIN | EPOLLET | EPOLLERR;
                ev.data.fd = sockfd;
                epoll_ctl(epfd, EPOLL_CTL_MOD, sockfd, &ev);
            }
            else if(event_arr[i].events & EPOLLERR | EPOLLRDHUP | EPOLLHUP){
                printf("error(): ");
                close_conn(epfd,sockfd);
            }
        }
    }
}

