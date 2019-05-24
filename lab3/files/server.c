#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/sysinfo.h>
#include <signal.h>
#include <sys/sendfile.h>
#include <pthread.h>

#define BIND_IP_ADDR "0.0.0.0"
#define BIND_PORT 8000
#define MAX_RECV_LEN 8192
#define MAX_FILE_LEN 1048576
#define MAX_SEND_LEN 4096
#define MAX_EVENT_NUM 256
#define MAX_THREAD_NUM 16
#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define HTTP_STATUS_200 "200 OK"
#define HTTP_STATUS_404 "404 Not Found"
#define HTTP_STATUS_500 "500 Internal Server Error"

//variable// set for threads
pthread_t pthreadid[MAX_THREAD_NUM];
int epollfd_pool[MAX_THREAD_NUM];

//functions//
int parse_request(int clnt_sock, char* req, struct stat *fstatus){
    ssize_t req_len = (ssize_t)0;
    const char end_ind[4] = {'\r', '\n', '\r', '\n'}; int ind = 0;
    for (ssize_t cur_len = read(clnt_sock, req + req_len, (size_t)MAX_RECV_LEN - (size_t)req_len);;cur_len = read(clnt_sock, req + req_len, (size_t)MAX_RECV_LEN - (size_t)req_len)){
        if (cur_len == (ssize_t)-1){
            if (errno == EINTR) continue;
            else{ perror("read req at parse_request"); return -2;}
        }
        if (cur_len == (ssize_t)0) continue;
        for (int i = req_len; i < req_len + cur_len; ++i)
            if (req[i] == end_ind[ind]){
                if (++ind == 4) break;
            }else ind = 0;
        req_len += cur_len;
        if (ind == 4) break;
        if (req_len == MAX_RECV_LEN) break;
    }
    if (ind != 4){
        char *temp_buf = malloc(MAX_RECV_LEN * sizeof(char)); if (temp_buf ==  NULL){ fprintf(stderr, "malloc temp_buf at parse_request\n"); return -2; }
        for (ssize_t cur_len = read(clnt_sock, temp_buf, (size_t)MAX_RECV_LEN);;cur_len = read(clnt_sock, temp_buf, (size_t)MAX_RECV_LEN)){
            if (cur_len == (ssize_t)-1){
                if (errno == EINTR) continue;
                else{ perror("read temp_buf at parse_request"); free(temp_buf); return -2;}
            }
            if (cur_len == (ssize_t)0) continue;
            for (int i = 0; i < cur_len; ++i)
                if (temp_buf[i] == end_ind[ind]){
                    if (++ind == 4) break;
                }else ind = 0;
            if (ind == 4) break;
        }
        free(temp_buf);
    }
    if (req_len < (ssize_t)5 || req[0] != 'G' || req[1] != 'E' || req[2] != 'T' || req[3] != ' ' || req[4] != '/') return -2;
    ssize_t s1 = (ssize_t)3, s2 = (ssize_t)5;
    int k = 0;
    while (s2 < req_len){
        if (req[s2] == ' ') break;
        if (req[s2] == '/'){
            if (req[s2 - (ssize_t)1] == '.' && req[s2 - (ssize_t)2] == '.' && req[s2 - (ssize_t)3] == '/') --k;
            else ++k;
            if (k < 0) return -2; 
        }
        ++s2;
    }
    if (req[s2] == ' '){
        req[s2] = '\0'; req[s1] = '.';
        int req_fd = open(req + s1, O_RDONLY); if (req_fd == -1) return -1;//404
        if (stat(req + s1, fstatus) == -1) {perror("stat at parse_request");return -2;} if (!S_ISREG(fstatus->st_mode)) return -2;
        return req_fd;
    }else return -2;
}

size_t write_stable(int fd, const void *buf, size_t count){
    ssize_t written_len = 0;
    for (ssize_t cur_len = write(fd, buf + written_len, count);; cur_len = write(fd, buf + written_len, count)){
        if (cur_len == -1){
            if (errno == EINTR) continue;
            else{ perror("write at write_stable"); return written_len;}
        }
        written_len += cur_len;
        if ((size_t)cur_len == count) break;
        count -= (size_t)cur_len;
    }
    return (size_t)written_len;
}

void handle_clnt(int clnt_sock){
    char *req_buf = (char *)malloc(MAX_RECV_LEN * sizeof(char));
    if (!req_buf) { fprintf(stderr, "malloc req_buf at handle_clnt\n"); close(clnt_sock); return; }
    char *response = (char *)malloc(MAX_SEND_LEN * sizeof(char));
    if (!response) { fprintf(stderr, "malloc response at handle_clnt\n"); free(req_buf); close(clnt_sock); return; }
    struct stat fstatus;
    int req_file_d = parse_request(clnt_sock, req_buf, &fstatus);
    if (req_file_d == -1){//404
        sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n", HTTP_STATUS_404, (size_t)0);
        size_t response_len = strlen(response); 
        if (write_stable(clnt_sock, response, response_len) != response_len) fprintf(stderr, "write at response 404\n");
    }else if (req_file_d == -2){//500
        sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n", HTTP_STATUS_500, (size_t)0);
        size_t response_len = strlen(response);
        if (write_stable(clnt_sock, response, response_len) != response_len) fprintf(stderr, "write at response 500\n");
    }else{//200
        sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n", HTTP_STATUS_200, (size_t)(fstatus.st_size));
        size_t response_len = strlen(response);
        if (write_stable(clnt_sock, response, response_len) != response_len) { fprintf(stderr, "write at response 200\n"); goto END_FUNC;}
        off_t offset = 0;
        while (offset < fstatus.st_size){
            ssize_t cur_len = sendfile( clnt_sock, req_file_d , &offset , fstatus.st_size - offset );
            if (cur_len == -1) { perror("sendfile"); goto END_FUNC; }
        }
    }
END_FUNC:
    close(clnt_sock);
    close(req_file_d);
    free(req_buf);
    free(response);
}

void *handle_epoll(void *no_use){
    pthread_t selfid = pthread_self();
    struct epoll_event *events = (struct epoll_event *)malloc(MAX_EVENT_NUM * sizeof(struct epoll_event));
    if (events == NULL){ fprintf(stderr, "malloc at handle_epoll\n"); return NULL;} 
    int epollfd = -1;
    for (int i = 0; i < MAX_THREAD_NUM; ++i) if (selfid == pthreadid[i]){
        epollfd = epollfd_pool[i];
        break;
    }
    if (epollfd == -1) { fprintf(stderr, "epollfd at handle_epoll\n"); return NULL;} 
    for (int nfd;;){
        if ((nfd = epoll_wait(epollfd, events, MAX_EVENT_NUM, -1)) == -1) {
            perror("epoll_wait"); continue;
        }
        for (int i = 0; i < nfd; ++i){
            handle_clnt(events[i].data.fd);
        }
    }
    free(events);
    return NULL;
}

int main(){
    int serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); if (serv_sock==-1) handle_error("socket at main");
    int flag_on = 1;
    if (-1 == setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &flag_on, sizeof(flag_on))) handle_error("setsocketopt at main");
    
    struct sockaddr_in serv_addr; memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;//IPv4
    serv_addr.sin_addr.s_addr = inet_addr(BIND_IP_ADDR);//Address
    serv_addr.sin_port = htons(BIND_PORT);//Port
    
    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1) handle_error("bind at main");
    if (listen(serv_sock, SOMAXCONN)==-1) handle_error("listen at main");

    signal(SIGPIPE, SIG_IGN);

    pthread_attr_t pthreadattr;
    pthread_attr_init(&pthreadattr);
    pthread_attr_setdetachstate(&pthreadattr,PTHREAD_CREATE_DETACHED);
    for (int i = 0; i < MAX_THREAD_NUM; ++i){
        if ((epollfd_pool[i] = epoll_create1(0)) == -1) handle_error("epoll_create1 at main");
        if (pthread_create(&pthreadid[i], &pthreadattr, handle_epoll, NULL) != 0) handle_error("pthread_create at main");
    }
    struct sockaddr_in clnt_addr; socklen_t clnt_size = sizeof(clnt_addr);
    struct epoll_event epollev; int clnt_sock, k = 0;
    while (1){
        if ((clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_size)) == -1){
            perror("accept at main");
            continue;
        }
        epollev.data.fd = clnt_sock;
        epollev.events = EPOLLIN | EPOLLONESHOT;
        epoll_ctl(epollfd_pool[k], EPOLL_CTL_ADD, clnt_sock, &epollev);
        k = (k + 1)% MAX_THREAD_NUM;
    }
    return 0;
}