
/*
 * Copyright (C) Igor Sysoev
 * Copyright (C) Nginx, Inc.
 */


#ifndef _NGX_PROCESS_H_INCLUDED_
#define _NGX_PROCESS_H_INCLUDED_


#include <ngx_setaffinity.h>
#include <ngx_setproctitle.h>


typedef pid_t       ngx_pid_t;

#define NGX_INVALID_PID  -1

typedef void (*ngx_spawn_proc_pt) (ngx_cycle_t *cycle, void *data);

typedef struct {
    ngx_pid_t           pid; // 当前工作进程id
    int                 status;  //当前进程的退出状态
    ngx_socket_t        channel[2]; // sockertpar 创建的socket 用于进程间通信

    ngx_spawn_proc_pt   proc; //proc，指向工作进程执行的函数。*data通常用来指向进程的上下文结构，*name为新建进程的名称，默认为"new binary process"
    void               *data;
    char               *name;

    unsigned            respawn:1; //进程状态-重新创建的
    unsigned            just_spawn:1;// 首次创建的
    unsigned            detached:1; //是否已经分离
    unsigned            exiting:1; //是否正在退出
    unsigned            exited:1;//是否已经退出
} ngx_process_t;


typedef struct {
    char         *path;
    char         *name;
    char *const  *argv;
    char *const  *envp;
} ngx_exec_ctx_t;


#define NGX_MAX_PROCESSES         1024

#define NGX_PROCESS_NORESPAWN     -1//子进程退出时,父进程不会再次创建, 该标记用在创建 "cache loader process".
#define NGX_PROCESS_JUST_SPAWN    -2// 当 nginx -s reload 时, 如果还有未加载的 proxy_cache_path, 则需要再次创建 "cache loader process"加载,并用 NGX_PROCESS_JUST_SPAWN给这个进程做记号,防止 "master会向老的worker进程,老的cache manager进程,老的cache loader进程(如果存在)发送NGX_CMD_QUIT或SIGQUIT" 时,误以为这个进程是老的cache loader进程. 
#define NGX_PROCESS_RESPAWN       -3 //子进程异常退出时,master会重新创建它, 如当worker或cache manager异常退出时,父进程会重新创建它.
#define NGX_PROCESS_JUST_RESPAWN  -4// 当 nginx -s reload 时, master会向老的worker进程,老的cache manager进程,老的cache loader进程(如果存在)发送 ngx_write_channel(NGX_CMD_QUIT)(如果失败则发送SIGQUIT信号);  该标记用来标记进程数组中哪些是新创建的子进程;其他的就是老的子进程
#define NGX_PROCESS_DETACHED      -5 //热代码替换


#define ngx_getpid   getpid

#ifndef ngx_log_pid
#define ngx_log_pid  ngx_pid
#endif


ngx_pid_t ngx_spawn_process(ngx_cycle_t *cycle,
    ngx_spawn_proc_pt proc, void *data, char *name, ngx_int_t respawn);
ngx_pid_t ngx_execute(ngx_cycle_t *cycle, ngx_exec_ctx_t *ctx);
ngx_int_t ngx_init_signals(ngx_log_t *log);
void ngx_debug_point(void);


#if (NGX_HAVE_SCHED_YIELD)
#define ngx_sched_yield()  sched_yield()
#else
#define ngx_sched_yield()  usleep(1)
#endif


extern int            ngx_argc;
extern char         **ngx_argv;
extern char         **ngx_os_argv;

extern ngx_pid_t      ngx_pid;
extern ngx_socket_t   ngx_channel;
extern ngx_int_t      ngx_process_slot;
extern ngx_int_t      ngx_last_process;
extern ngx_process_t  ngx_processes[NGX_MAX_PROCESSES];


#endif /* _NGX_PROCESS_H_INCLUDED_ */
