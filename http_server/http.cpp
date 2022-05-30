#include"http.h"
#include<iostream>
using namespace std;

int http::m_customer_count = 0;                     //记录客户数量
int http::m_epollfd = -1;                           //将epoll的根设置为静态

//设置非阻塞
void setnonblockfd(int fd)                      
{
    int old_option = fcntl(fd, F_GETFL);       // 通过fcntl可以改变已打开的文件性质。fcntl针对描述符提供控制，F_GeTFL:Get file status flags.
    old_option = old_option | O_NONBLOCK;             // 加上非阻塞
    fcntl(fd, F_SETFL, old_option);             // 设置文件描述符状态标志
}

//将套接字挂上红黑树
void addfd(int epollfd, int fd, bool one_shot)  
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP;
    if(one_shot)
    {
        event.events |= EPOLLONESHOT;   // 防止不同的线程或者进程在处理同一个SOCKET的事件
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblockfd(fd);
}

//将套接字从红黑树摘下
void remove_fd(int epollfd, int fd)            
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
    cout <<"成功关闭 fd = "<< fd << endl;
}

//修改套接字的触发条件
void modfd(int epollfd, int fd, int ev)        
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;// 再把EPOLLONESHOT加回来（因为已经触发过一次了）
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);        // 参数准备齐全，修改指定的epoll文件描述符上的事件（EPOLL_CTL_MOD：修改）
}

const char* http::get_file_type(const char* name)
{
	const char* dot;
	dot = strrchr(name, '.');
	if (dot == NULL)
    {
		return "text/plain; charset=utf-8";
    }
	if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0)
    {
		return "text/html; charset=utf-8";
    }
    if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0)
		return "image/jpeg";
    if (strcmp(dot, ".gif") == 0)
		return "image/gif";
    if (strcmp(dot, ".png") == 0)
		return "image/png";
    if (strcmp(dot, ".css") == 0)
		return "text/css";
    if (strcmp(dot, ".au") == 0)
		return "audio/basic";
    if (strcmp(dot, ".wav") == 0)
		return "audio/wav";
    if (strcmp(dot, ".avi") == 0)
		return "video/x-msvideo";
    if (strcmp(dot, ".mov") == 0 || strcmp(dot, ".qt") == 0)
		return "video/quicktime";
    if (strcmp(dot, ".mpeg") == 0 || strcmp(dot, ".mpe") == 0)
		return "video/mpeg";
    if (strcmp(dot, ".vrml") == 0 || strcmp(dot, ".wrl") == 0)
		return "model/vrml";
    if (strcmp(dot, ".midi") == 0 || strcmp(dot, ".mid") == 0)
		return "audio/mpeg";
    if (strcmp(dot, ".mp3") == 0)
		return "audio/mp3";
    if (strcmp(dot, ".ogg") == 0)
		return "application/ogg";
    if (strcmp(dot, ".pac") == 0)
		return "application/x-ns-proxy-autoconfig";
    return "text/plain; charset=utf-8";
}

void http::init(int fd, const sockaddr_in &client_addr)
{
    m_sockfd = fd;
    m_addr = client_addr;
    int reuse=1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)); //端口复用

    addfd(m_epollfd, m_sockfd, true);
    init_data();
    m_customer_count++;
}


void http::init_data()
{
    is_connect = true;

    thread_rdwr = M_READ;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_line_state = LINE_OK;
    m_http_code = GET_LINE;

    bzero(read_buf, sizeof(read_buf));
    m_read_len = 0;
    m_read_begin = 0;
    m_read_index = 0;

    bzero(write_buf, sizeof(write_buf));
    m_write_len = 0;

    byte_to_send = 0;
    byte_have_send = 0;
    m_file_len = 0;

    bzero(method, sizeof(method));
    bzero(url, sizeof(url));
    bzero(version, sizeof(version));
}


void http::close_connect()
{
    remove_fd(m_epollfd, m_sockfd);
    init_data();
    m_sockfd = -1;
    m_customer_count--;
}

bool http::read_data()          //接收http请求写入读缓冲区
{
    while(true)
    {
        int ret = recv(m_sockfd, read_buf + m_read_len, READ_BUFFER_SIZE - m_read_len, 0);
        if(ret == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK) 
            {
                // 没有数据Connection
                break;
            }
            fprintf(stderr, "epoll_wait error:%s\n", strerror(errno));
            return false;
        }
        else if(ret == 0)
        {
            return false;
        }
        m_read_len += ret;
    }
    cout << "---------------http请求:------------" << endl;
    write(STDOUT_FILENO, read_buf, strlen(read_buf));
    return true;
}

void http::http_resolution()
{
    bool is_end = true;     //分析完http结束循环的变量
    while (is_end)
    {
       
        switch (m_check_state)
        {
            
            case CHECK_STATE_REQUESTLINE:       //分析http请求的请求行
            {
                request_line();
                if(m_line_state != LINE_OK || m_http_code != GET_LINE)
                {
                    is_end = false;
                    break;
                }
                m_check_state = CHECK_STATE_HREADER;
                
                break;
            }
            case CHECK_STATE_HREADER:           //分析http请求的头部
            {
                hreader_request();
                if(m_line_state != LINE_OK)
                    is_end = false;
                break;
            }
            case CHECK_STATE_REQUESTBOBY:       //分析http请求的请求体
            {
                boby_request();                 //在这不解析请求体
                is_end = false;
                break;
            }
            default:
            {
                is_end = false;
                break;
            }
        }
    }
    
    response(); //响应http请求    
    thread_rdwr = M_WRIET;
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
    return;
}

void http::get_line()
{
    char temp;
    while(m_read_index < m_read_len)
    {	
    	temp = read_buf[m_read_index]; 
    	if(read_buf[m_read_index] == '\r')
    	{
    		if((m_read_index +1) == m_read_len)
    		{
    			m_line_state = LINE_OPEN;
    			return;
    		}
    		else if(read_buf[m_read_index + 1] == '\n')
    		{
    			m_line_state = LINE_OK;
    			read_buf[m_read_index++] = '\0';;
    			read_buf[m_read_index++] = '\0';
    			return;
    		}
    	}
    	else if(temp == '\n')
    	{
    		if((m_read_index > 1) && read_buf[m_read_index - 1] == '\r')
    		{
    			read_buf[m_read_index - 1] = '\0';
    			read_buf[m_read_index++] = '\0';
    			m_line_state  = LINE_OK;
    			return;
    		}
    		m_line_state  = LINE_BAD;
    		return;
    	}
    	m_read_index++;
    }
    m_line_state = LINE_OPEN;
    return;
}

void http::request_line()
{
    char *text = 0;
    char path[FILENAME_LEN];
    get_line();
    text = read_buf + m_read_begin;
    m_read_begin = m_read_index;

    if(m_line_state == LINE_BAD)
    {
        m_http_code = BAD_REQUEST;
        return;
    }
    else if (m_line_state == LINE_OPEN)
    {
        m_http_code = NO_REQUEST;
        return;
    }
    sscanf(text, "%[^ ] %[^ ] %[^ ]", method, url, version);
    char *pos = strchr(url, '?');
	if(pos)
    {
		*pos = '\0';
		printf("real url: %s\n", url);
    }   
	sprintf(path, "/home/lwd/webserver_lwd/html_data%s", url);
    printf("m_path: %s\n", path);
	//判断文件是否存在，如果存在就响应200 OK，同时发送相应的html 文件,如果不存在，就响应 404 NOT FOUND.
	
    if(stat(path, &st)==-1) //文件不存在或是出错
    {
        m_http_code = NOT_FOUND;
        return;
	}
    else if (!( st.st_mode & S_IROTH )) // 判断访问权限
    {
        m_http_code = NO_MOD_REQUEST;
        return;
    }
    else if ( S_ISDIR( st.st_mode ) ) // 判断是否是目录
    {
        m_http_code = BAD_REQUEST;
        return;
    }
}

void http::hreader_request()
{
    char *text = 0;

    while(true)
    {
        get_line();
        text = read_buf + m_read_begin;
        m_read_begin = m_read_index;
        if(text[0] == '\0')
        {
            m_check_state = CHECK_STATE_REQUESTBOBY;
            m_http_code = GET_LINE;
            return;
        }
        if(m_line_state == LINE_BAD)
        {
            m_http_code = BAD_REQUEST;
            return;
        }
        else if (m_line_state == LINE_OPEN)
        {
            m_http_code = NO_REQUEST;
            return;
        }   
    }
}

void http::boby_request()
{
    return;
}

void http::response()
{
    switch (m_http_code)
    {
        case NO_REQUEST:
        {
            break;
        }
        case GET_LINE:
        {
            response_hread(" 200 OK\r\n");
            response_boby(url);
            break;
        }
        case BAD_REQUEST:
        {
            response_hread(" 400 BAD REQUEST\r\n");
            response_boby("/error/error_400.html");
            break;
        }
        case NO_MOD_REQUEST:
        {
            response_hread(" 403 Forbidden\r\n");
            response_boby("/error/error_403.html");
            break;
        }
        case NOT_FOUND:
        {
            response_hread(" 404 NOT FOUND\r\n");
            response_boby("/error/error_404.html");
            break;
        }
        case INET_ERROR:
        {
            response_hread(" 500 Internal Sever Error\r\n");
            response_boby("/error/error_500.html");
            break;
        }
        case CLOSE_CONNECT:
        {
            break;
        }
        default:
            break;
    }
}

void http::response_hread(const char* status_code)
{

	int fileid = 0;
	char tmp[64];
    const char *type;
    char m_connect[32];
    char file_type[64];
    
    strcpy(write_buf, version);
	strcat(write_buf, status_code);
	strcat(write_buf, "Server:LWD Server\r\n");
    
    cout << "m_http_code = " << m_http_code <<endl;
    if(m_http_code != GET_LINE)
    {
        is_connect = true;
        type = "text/html; charset=utf-8";
    }
    else
    {
        type = get_file_type(url);
        cout << "type1 = " << type <<'\n';
    }
    snprintf(file_type, 64, "Content-Type: %s\r\n", type);
    strcat(write_buf, file_type);
    is_connect = false;
    //keep-alive
    if(is_connect)
    {
        snprintf(m_connect, 32, "Connection: %s\r\n", "keep-alive");
    }
    else
    {
        snprintf(m_connect, 32, "Connection: %s\r\n", "close");
    }
    
    strcat(write_buf, m_connect);
	
	snprintf(tmp, 64, "Content-Length: %ld\r\n\r\n", st.st_size); //st.st_size
	strcat(write_buf, tmp);

    m_write_len = strlen(write_buf);
    write(STDOUT_FILENO, write_buf, m_write_len);

    m_iv[0].iov_base = write_buf;
    m_iv[0].iov_len = m_write_len;
    m_iv_count = 1;
    byte_to_send += m_write_len;

}

void http::response_boby(const char* m_url)
{
    char file_path[256] = {0};
    strcat(file_path, resource_directory);
    strcat(file_path, m_url);
    struct stat file_st;
    if(stat(file_path, &file_st)==-1) //文件不存在或是出错
    {
        return;
	}
    // 以只读方式打开文件
    int fd = open( file_path, O_RDONLY );
    // 创建内存映射
    m_file_addr = ( char* )mmap( 0, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0 );
    close( fd );
    m_iv[1].iov_base = m_file_addr;
    m_iv[1].iov_len = file_st.st_size;
    m_file_len = file_st.st_size;
    byte_to_send += file_st.st_size;
    //write(STDOUT_FILENO, m_file_addr, file_st.st_size);
    m_iv_count = 2;
    
}

bool http::write_data()
{
    int ret;
    cout << "---------------响应：------------" << endl;
    write(STDOUT_FILENO, write_buf, strlen(write_buf));
    if(byte_to_send == 0)
    {
        init_data();
        modfd( m_epollfd, m_sockfd, EPOLLIN ); 
        return true;
    }
    while(1) {
        // 分散写
        ret = writev(m_sockfd, m_iv, m_iv_count);
        if ( ret <= -1 ) 
        {
            if(errno == EAGAIN)
            {
                continue;  
            }
            else if(errno == EINTR)
            {
                continue;
            }
            else
            {
                unmap();
                fprintf(stderr, "send. reason: %s\n", strerror(errno));
			    return false;
            }
        }

        byte_have_send += ret;
        byte_to_send -= ret;
        
        if (byte_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_addr + (byte_have_send - m_write_len);
            m_iv[1].iov_len = byte_to_send;
        }
        else
        {
            m_iv[0].iov_base = write_buf + byte_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - byte_have_send;
        }

        if (byte_to_send <= 0)
        {
            // 没有数据要发送了
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);
            if (is_connect)
            {
                init_data();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
    return true;
}

void http::unmap() {
    if(m_file_addr)
    {
        munmap(m_file_addr, m_file_len);
        m_file_addr = 0;
    }
}

void http::process()
{
    //解析读缓存区的内容
    switch (thread_rdwr)
    {
    case M_READ:
    {
        http_resolution();
        cout << endl;
        break;
    }
    case M_WRIET:
    {
        if(!write_data())
        {   
            close_connect();
        }
        break;
    }
    default:
        break;
    }
}