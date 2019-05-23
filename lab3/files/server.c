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


#define BIND_IP_ADDR "0.0.0.0"
#define BIND_PORT 8000
#define MAX_RECV_LEN 8192
#define MAX_FILE_LEN 1048576
#define MAX_SEND_LEN 4096
#define MAX_EVENT_NUM 1024
#define handle_error(msg) \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define HTTP_STATUS_200 "200 OK"
#define HTTP_STATUS_404 "404 Not Found"
#define HTTP_STATUS_500 "500 Internal Server Error"

bool file_read_continue(int fd, char *buf, ssize_t *buf_len, ssize_t buf_reach, size_t MAX_SIZE){
    ssize_t new_buf_len;
    while ((*buf_len) < buf_reach){
        new_buf_len = read(fd, buf + (size_t)(*buf_len), MAX_SIZE - (size_t)(*buf_len)); if (new_buf_len == (ssize_t)-1) handle_error("read at file_read_continue");
        if (!new_buf_len) break;
        *buf_len += new_buf_len;
    }
    return (*buf_len) >= buf_reach;
}

int parse_request(int clnt_sock, char* req, struct stat *fstatus){
    ssize_t req_len = read(clnt_sock, req, (size_t)MAX_RECV_LEN); if (req_len == (ssize_t)-1) handle_error("read at parse_request");
    if (req_len < (ssize_t)5 || !file_read_continue(clnt_sock, req, &req_len, (ssize_t)5, (size_t)MAX_RECV_LEN)
        || req[0] != 'G' || req[1] != 'E' || req[2] != 'T' || req[3] != ' ' || req[4] != '/') return -2;
    ssize_t s1 = (ssize_t)3, s2 = (ssize_t)5;
    int k = 0;
    while (s2 < req_len || (s2 >= req_len && file_read_continue(clnt_sock, req, &req_len, s2 + (ssize_t)1, (size_t)MAX_RECV_LEN))){
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
        if (stat(req + s1, fstatus) == -1) handle_error("stat at parse_request"); if (!S_ISREG(fstatus->st_mode)) return -2;
        return req_fd;
    }else{
        return -2;
    }
}

void handle_clnt(int clnt_sock){
    char *req_buf = (char *)malloc(MAX_RECV_LEN * sizeof(char)); if (!req_buf) { fprintf(stderr, "malloc FAIL at req_buf\n"); exit(EXIT_FAILURE); }
    char *response = (char *)malloc(MAX_SEND_LEN * sizeof(char)); if (!response) { fprintf(stderr, "malloc FAIL at response\n"); exit(EXIT_FAILURE); }
    struct stat fstatus;
    int req_file_d = parse_request(clnt_sock, req_buf, &fstatus);
    if (req_file_d == -1){//404
        sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n", HTTP_STATUS_404, (size_t)0);
        size_t response_len = strlen(response);
        if (write(clnt_sock, response, response_len) == -1) handle_error("write at response req_file_d == -1");
    }else if (req_file_d == -2){//500
        sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n", HTTP_STATUS_500, (size_t)0);
        size_t response_len = strlen(response);
        if (write(clnt_sock, response, response_len) == -1) handle_error("write at response req_file_d == -2");
    }else{//200
        sprintf(response, "HTTP/1.0 %s\r\nContent-Length: %zd\r\n\r\n", HTTP_STATUS_200, (size_t)(fstatus.st_size));
        size_t response_len = strlen(response);
        if (write(clnt_sock, response, response_len) == -1) handle_error("write at response");
        ssize_t cur_len;
        char *file_buf = (char *)malloc(MAX_FILE_LEN * sizeof(char)); if (!file_buf) { fprintf(stderr, "malloc FAIL at file_buf\n"); exit(EXIT_FAILURE); }
        while ((cur_len = read(req_file_d, file_buf, (ssize_t)MAX_FILE_LEN))){
            if (cur_len == -1) handle_error("read at responce_file");
            if (write(clnt_sock, file_buf, (size_t)cur_len) == -1) handle_error("write at response_file");
        }
        free(file_buf);
    }
    close(clnt_sock);
    close(req_file_d);
    free(req_buf);
    free(response);
}

void parent_process_wait(int signal){
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0);
}

void handle_epoll(int serv_sock){
    struct epoll_event ep_ev, *events = (struct epoll_event *)malloc(MAX_EVENT_NUM * sizeof(struct epoll_event));
    if (events == NULL){
        fprintf(stderr, "malloc FAIL at handle_epoll");
        exit(EXIT_FAILURE);
    } 
    int ep_fd = epoll_create1(0); if (ep_fd == -1) handle_error("epoll_create");
    ep_ev.data.fd = serv_sock;
    ep_ev.events = EPOLLIN;
    if (-1 == epoll_ctl(ep_fd, EPOLL_CTL_ADD, serv_sock, &ep_ev)) handle_error("epoll_ctl at serv_sock");
    
    struct sockaddr_in clnt_addr; socklen_t clnt_size = sizeof(clnt_addr);
    int nfd, clnt_sock;
    for (;;){
        if ((nfd = epoll_wait(ep_fd, events, MAX_EVENT_NUM, -1)) == -1) {
            perror("epoll_wait"); continue;
        }
        for (int i = 0; i < nfd; ++i){
            if (events[i].data.fd == serv_sock){
                clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_addr, &clnt_size);
                if (clnt_sock == -1){
                    //perror("accept");
                    continue;
                }else{
                    //if (fcntl(clnt_sock, F_SETFL, fcntl(clnt_sock, F_GETFD) | O_NONBLOCK) == -1) continue;
                    ep_ev.events = EPOLLIN | EPOLLONESHOT;
                    ep_ev.data.fd = clnt_sock;
                    if (epoll_ctl(ep_fd, EPOLL_CTL_ADD, clnt_sock, &ep_ev) == -1){
                        perror("epoll_ctl at clnt_sock");
                        close(clnt_sock);
                    }
                }
            }else if (events[i].events & EPOLLIN){
                handle_clnt(events[i].data.fd);
            }
        }
    }
}

int main(){
    int serv_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); if (serv_sock==-1) handle_error("socket at main");
    if (fcntl(serv_sock, F_SETFL, O_NONBLOCK) == -1) handle_error("fcntl at main");
    int flag_on = 1;
    if (-1 == setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, &flag_on, sizeof(flag_on))) handle_error("setsocketopt at main");
    
    struct sockaddr_in serv_addr; memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;//IPv4
    serv_addr.sin_addr.s_addr = inet_addr(BIND_IP_ADDR);//Address
    serv_addr.sin_port = htons(BIND_PORT);//Port
    
    if (bind(serv_sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1) handle_error("bind at main");
    if (listen(serv_sock, SOMAXCONN)==-1) handle_error("listen at main");
    
    signal(SIGCHLD, parent_process_wait);
    signal(SIGPIPE, SIG_IGN);
    int child_process_num = get_nprocs();
    for (int i = 0; i < child_process_num; ++i){
        if (fork() == 0){
            handle_epoll(serv_sock);
            exit(0);
        }
    }
    while (1) sleep(255);
    return 0;
}