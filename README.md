# nginx-source-analyze
analyze nginx source
nginx  epoll 流程
1、ngx_event_process_init 在这个函数中，将监听的socket 放入epoll中

2、 如果没有使用accept锁，直接 ngx_add_event(rev（ngx_event_t）, NGX_READ_EVENT, 0)  将事件加入到epoll中


3、如果使用accept锁：ngx_process_events_and_timers--》ngx_trylock_accept_mutex--》ngx_enable_accept_events
         c = ls[i].connection; //从套接字的连接槽中取出一个连接，将连接的读fd  放到epoll中。
        ngx_add_event(c->read, NGX_READ_EVENT, 0)

4、监听套接字事件处理——读：ngx_event_accept函数

    
    调用accept得到连接套接字，为其分配一个connection_t槽位，并初始化connection_t对象： 
        c->recv = ngx_recv;
        c->send = ngx_send;
        c->recv_chain = ngx_recv_chain;
        c->send_chain = ngx_send_chain;

5、
    并调用ngx_http_init_connection初始化connection_t对象，并加入定时器中。
        rev = c->read;
        rev->handler = ngx_http_init_request;                //设置读事件初始handler
        c->write->handler = ngx_http_empty_handler;

6、
    调用ngx_epoll_add_connection（）监听连接套接字的读写事件。
        ee.events = EPOLLIN|EPOLLOUT|EPOLLET;
        ee.data.ptr = (void *) ((uintptr_t) c | c->read->instance);
        if (epoll_ctl(ep, EPOLL_CTL_ADD, c->fd, &ee) == -1) { }
        c->read->active = 1;
        c->write->active = 1;

7、连接套接字的读写事件处理：
        ngx_event_accept-》ngx_http_init_connection中设置read/write handler
ngx_http_init_request() ：设置连接套接字读事件的处理函数为ngx_http_process_request_line，负责读取完所有的请求数据。ngx_http_process_request_line：连接套接字读事件处理函数，解析请求行

    (a) 读事件是超时事件，关闭连接
    (b) 循环体内（循环读取缓存区的数据，并分析）：
                b1: ngx_http_read_request_header读取一片数据: 先调用ngx_unix_recv, 再调用ngx_handle_read_event(重新把读事件注册到epoll中，每次epoll_wait后，fd的事件类型将会清空，需要再次注册读写事件。但是我不知道哪里设置了active ==0 ）

                b2: ngx_http_parse_request_line（）里面的内容：解析读到的一片数据
                b3: ngx_http_parse_request_line函数返回了错误，则直接给客户端返回400错误； 如果返回NGX_AGAIN，则需要判断一下是否是由于缓冲区空间不够，还是已读数据不够。如果是缓冲区大小不够了，nginx会调用ngx_http_alloc_large_header_buffer函数来分配另一块大缓冲区，如果大缓冲区还不够装下整个请求行，nginx则会返回414错误给客户端，否则分配了更大的缓冲区并拷贝之前的数据之后，继续调用ngx_http_read_request_header函数读取数据来进入请求行自动机处理，直到请求行解析结束；           
                如果返回了NGX_OK，则表示请求行被正确的解析出来了，这时先记录好请求行的起始地址以及长度，并将请求uri的path和参数部分保存在请求结构的uri字段，请求方法起始位置和长度保存在method_name字段，http版本起始位置和长度记录在http_protocol字段。还要从uri中解析出参数以及请求资源的拓展名，分别保存在args和exten字段。 
            rev->handler = ngx_http_process_request_headers;    //设置连接套接字的读事件处理函数
            ngx_http_process_request_headers(rev);
ngx_http_process_request_headers：连接套接字读事件处理函数，解析请求头域
            先调用了ngx_http_read_request_header（）函数读取数据，如果当前连接并没有数据过来，再直接返回，等待下一次读事件到来，如果读到了一些数据则调用ngx_http_parse_header_line（）函数来解析，同样的该解析函数实现为一个有限状态机，逻辑很简单，只是根据http协议来解析请求头，每次调用该函数最多解析出一个请求头，该函数返回4种不同返回值，表示不同解析结果。
            返回NGX_OK，表示解析出了一行请求头
            返回NGX_AGAIN，表示当前接收到的数据不够，一行请求头还未结束，需要继续下一轮循环。
            返回NGX_HTTP_PARSE_INVALID_HEADER，表示请求头解析过程中遇到错误，一般为客户端发送了不符合协议规范的头部，此时nginx返回400错误；
      返回NGX_HTTP_PARSE_HEADER_DONE，表示所有请求头已经成功的解析，调用                                                                                                       rc = ngx_http_process_request_header(r);
            if (rc != NGX_OK) {
                return;
            }
            ngx_http_process_request(r);   //后面就要进入请求处理阶段了。


ngx_int_t
ngx_handle_read_event(ngx_event_t *rev, ngx_uint_t flags)
{
    if (ngx_event_flags & NGX_USE_CLEAR_EVENT) {     //边缘触发

        /* kqueue, epoll */
        if (!rev->active && !rev->ready) {         //ready==0 数据读完了EAGAIN  active ==0 哪里设置了 
//就目前发现只用del_conn/del_event会设置active为0，但实际上不用显示调用del_event，不理解
            if (ngx_add_event(rev, NGX_READ_EVENT, NGX_CLEAR_EVENT)  //NGX_CLEAR_EVENT  == EPOLLET
                == NGX_ERROR)
            {
                return NGX_ERROR;
            }
        }
        return NGX_OK;
    } 
