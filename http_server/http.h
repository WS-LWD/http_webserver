#pragma once
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
using namespace std;
#include<iostream>
 
class http
{
public:
    /*线程读写状态的2种情况
    READ:   当前线程处于读状态  /读数据
    WRITE:  当前线程处于写状态  /响应数据
    */
    enum THREAD_RDWR {M_READ = 0, M_WRIET};
    /* 主状态机的3种可能状态
    CHECK_STATE_REQUSTLINE:      当前正在分析请求行
    CHECK_STATE_HREADER:         当前正在分析头部字段 
    CHECK_STATE_REQUESTBOBY:     当前正在分析请求体
    */
    enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HREADER, CHECK_STATE_REQUESTBOBY};

    /*状态机的3种情况：
    LINE_OK:     读取到一个完整的行
    LINE_BAD:    读取到的行有错误/行错误
    LINE_OPEN:   读到行数据不完整
    */
    enum LINE_STATE {LINE_OK = 0, LINE_BAD, LINE_OPEN};

    /*服务处理http请求的结果
    NO_REQUEST:     请求不完整，需要继续获取请求
    GET_LINE:       表示获取到完整的请求
    BAD_REQUEST:    表示客户端请求语法错误
    NO_MOD_REQUEST: 表示客户端没权限访问该资源
    NOT_FOUND:      表示请求资源不存在或出错
    INET_ERROR:     表示服务器内部错误
    CLOSE_CONNECT:  表示接连已关闭
    */
    enum HTTP_CODE {NO_REQUEST = 0, GET_LINE, BAD_REQUEST, NO_MOD_REQUEST, INET_ERROR, CLOSE_CONNECT, NOT_FOUND};

public:
    static const int METHOD_LEN = 32;               //协议的长度
    static const int VERSION_LEN = 32;              //版本的长度
    static const int FILENAME_LEN = 256;            //文件名的长度
    static const int READ_BUFFER_SIZE = 2048;       //读缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 1024;      //写缓冲区的大小
public:
    static int m_epollfd;             
    static int m_customer_count;
public:
    http(){};
    ~http(){};
    void init(int fd, const sockaddr_in& client_addr);
    void init_data();

    void process();
    bool read_data();
    bool write_data();

    //获取一行的数据
    void get_line();         
    void request_line();
    void hreader_request();
    void boby_request();


    const char*get_file_type(const char* name);
    //响应http请求
    void response();      
    //将状态行、消息报头写入写缓冲池
    void response_hread(const char* status_codes); 
    //将响应正文写入映射区
    void response_boby(const char* m_url);
    void unmap();

    void close_connect();
    void http_resolution();

private:
    int m_sockfd;
    sockaddr_in m_addr;
    bool is_connect;                                //是否保持连接

    THREAD_RDWR thread_rdwr;                        //线程读写状态
    CHECK_STATE m_check_state;                      //主状态机的状态
    HTTP_CODE m_http_code;                          //服务处理http请求的结果
    LINE_STATE m_line_state;                        //状态机的状态

    char read_buf[READ_BUFFER_SIZE];                //读缓冲区
    int m_read_len;                                 //读到的字节总长度
    int m_read_begin;                               //解析数据的开始位置
    int m_read_index;                               //读read_buf数据的下标

    char write_buf[WRITE_BUFFER_SIZE];              //写缓冲区
    int m_write_len;                                //写缓冲区的总长度

    int byte_to_send;                               //待发送字节长度
    int byte_have_send;                             //已经发送的字节数
    int m_file_len;
    char *m_file_addr;                              //文件被mmap映射的首地址

    struct iovec m_iv[2];                           // 我将采用writev来执行写操作，所以定义下面两个成员，其中m_iv_count表示被写内存块的数量
    int m_iv_count;                                 // m_iv_count表示被写内存块的数量
    
    struct stat st;
    const char *resource_directory = "/home/lwd/webserver_lwd/html_data";     //资源目录
    char method[METHOD_LEN];                                            //存储协议
    char url[FILENAME_LEN];                                             //存储url
    char version[VERSION_LEN];                                          //存储版本号
};
