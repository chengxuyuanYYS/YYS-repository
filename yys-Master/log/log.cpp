#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <stdarg.h>
#include "log.h"
#include <pthread.h>
using namespace std;

log::log()
{
    m_count = 0; 
    m_is_async = false;
}

log::~log()
{
    if(m_fp != NULL)
    {
        fclose(m_fp);
    }
}
//异步需要设置阻塞队列的长度，同步不需要设置
bool log::init(const char *file_name,int close_log, int log_buf_size , int split_lines, int max_queue_size)
{
    //如果设置了max_queue_size，则设置为异步
    if ( max_queue_size >= 1 )
    {
        m_is_async = true;
        m_log_queue = new block_queue<string>(max_queue_size);
        pthread_t tid;
        //flush_log_thread为回调函数，这里表示创建线程异步写日志
        pthread_create(&tid, NULL, flush_log_thread, NULL);
    }

    m_close_log =close_log; //是否关闭日志
    m_log_buf_size = log_buf_size; //日志缓冲区大小
    m_buf = new char[m_log_buf_size]; //
    memset(m_buf, '\0' , m_log_buf_size);
    m_split_lines = split_lines; //日志最大行数
    //时间相关参数
    time_t t = time(NULL);
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;

    const char *p = strrchr(file_name, '/');//匹配最后一个/后面的字符，即文件名
    char log_full_name[256] = {0};//日志全名

    if ( p == NULL)//默认年月日
    {
    snprintf(log_full_name,255, "%d_%02d_%02d_%s", my_tm.tm_year + 1900,my_tm.tm_mon + 1,my_tm.tm_mday, file_name);
    }
    else//否则年月日+日志名
    {
        strcpy(log_name, p + 1);
        strncpy(dir_name, file_name, p- file_name + 1 );
        snprintf( log_full_name, 255, "%s%d_%02d_%02d_%s", dir_name,my_tm.tm_year + 1900,my_tm.tm_mon + 1,my_tm.tm_mday,log_name);
    }
    
    m_today = my_tm.tm_mday;//保存今天的时间

    m_fp = fopen(log_full_name, "a");//打开日志文件，若没有则创建
    if( m_fp == NULL)
        return false;
    return true;
}
//写日志
void log::write_log(int level, const char *format, ...)
{
    //获取当前时间
    struct timeval now = {0,0};
    gettimeofday(&now, NULL);
    time_t t = now.tv_sec;
    struct tm *sys_tm = localtime(&t);
    struct tm my_tm = *sys_tm;
    char s[16] = {0};
    switch( level )
    {
        case 0:
            strcpy(s, "[debug]:");
            break;
        case 1:
            strcpy(s, "[info]:");
            break;
        case 2:
            strcpy(s,"[warn]:");
            break;
        case 3:
            strcpy(s,"[erro]:");
            break;
        default:
            strcpy(s,"[info]:");
            break;
    }
    //写入一行log,对m_count++,m_split_lines最大行数
    m_mutex.lock();
    m_count++;
    //如果不是今天或者文件已经到达最大行数了
    if (m_today != my_tm.tm_mday || m_count % m_split_lines == 0)//everyday log
    {
        char new_log[250] ={0};
        fflush(m_fp);
        fclose(m_fp);
        char tail[16] = {0};

        snprintf(tail, 16 ,"%d_%02_%02d_",my_tm.tm_year + 1900, my_tm.tm_mon + 1,my_tm.tm_mday);
//如果不是今天，那先创建文件
        if(m_today != my_tm.tm_mday)
        {
            snprintf(new_log , 255 ,"%s%s%s",dir_name, tail,log_name);
            m_today = my_tm.tm_mday;
            m_count= 0;
        }
        else{//如果是今天但已经超出当前文件的最大行，应当新创建文件
            snprintf(new_log,255 ,"%s%s%s.%lld",dir_name,tail,log_name,m_count/m_split_lines);
        }
        m_fp = fopen(new_log, "a");
    }

    m_mutex.unlock();
/*VA_LIST的用法： 
（1）首先在函数里定义一具VA_LIST型的变量，这个变量是指向参数的指针； 
（2）然后用VA_START宏初始化变量刚定义的VA_LIST变量； 
（3）然后用VA_ARG返回可变的参数，VA_ARG的第二个参数是你要返回的参数的类型（如果函数有多个可变参数的，依次调用VA_ARG获取各个参数）； 
（4）最后用VA_END宏结束可变参数的获取。*/
    va_list valst;
    va_start(valst,format);

    string log_str;
    m_mutex.lock();
    //写入的具体时间内容格式
    int n = snprintf(m_buf, 48 ,"%d-%02d-%02d %02d:%02d:%02d.%06ld %s ",
            my_tm.tm_year + 1900,my_tm.tm_mon + 1,my_tm.tm_mday,my_tm.tm_hour,my_tm.tm_min,my_tm.tm_sec, now.tv_usec, s);

/*vsnprintf函数
头文件：#include  <stdarg.h>
函数原型：int vsnprintf(char *str, size_t size, const char *format, va_list ap);
函数说明：将可变参数格式化输出到一个字符数组
参数：str输出到的数组，size指定大小，防止越界，format格式化参数，ap可变参数列表函数用法*/
  //写日志
    int m = vsnprintf(m_buf + n,m_log_buf_size - 1,format ,valst);
    m_buf[n + m] = '\n';
    m_buf[n + m + 1]= '\0';
    log_str = m_buf;

    m_mutex.unlock();

    if(m_is_async && !m_log_queue->full()) //如果异步并且队列没有满
    {

        m_log_queue->push(log_str);
    }
    else//如果同步
    {

        m_mutex.lock();
        fputs(log_str.c_str(),m_fp);
        m_mutex.unlock();
    }
    va_end(valst);
}
void log::flush(void)
{
    m_mutex.lock();
    //强制刷新写入流缓冲区
    fflush(m_fp);
    m_mutex.unlock();
}
