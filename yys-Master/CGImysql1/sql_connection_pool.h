#ifndef _CONNECTION_POOL_
#define _CONNECTION_POOL_

#include <stdio.h>
#include <list>
#include <mysql/mysql.h>
#include <error.h>
#include <string.h>
#include <iostream>
#include "../lock/locker.h"
#include "../log/log.h"

using namespace std;

//连接池

class connection_pool{
    public:
//初始化连接对象，以及数据库的基本信息，将对象放入list队列保存，并初始化信号量，确保同步获取对象资源
        void init(string url, string User, string PassWord, string DataBaseName, int Port,int MaxConn, int close_log);

        MYSQL *GetConnection();  //获取数据库连接
        bool ReleaseConnection(MYSQL *conn); //释放连接
        int GetFreeConn(); //获取当前空闲连接数
        void DestroyPool(); //销毁所有连接

        //单例模式
        static connection_pool *GetInstance();
    private:
        connection_pool();
       ~connection_pool();

       int m_MaxConn;  //最大连接数
       int m_CurConn;  //当前已使用的连接数
       int m_FreeConn; //当前空闲的连接数

       locker lock;
       list<MYSQL *> connList; //双向链表连接池
       sem reserve;//信号量对象，控制连接池资源的使用

    public:

       string m_url;    //主机地址
       string m_Port;  //数据库端口号
       string m_User; //登陆数据库用户名
       string m_PassWord;  //登陆数据库密码
       string m_DatabaseName; //使用数据库名
       int  m_close_log; //日志开关
};

//RALL机制，资源获取即初始化，保证内存不被泄漏
class connectionRALL{

public:
    connectionRALL(MYSQL **con, connection_pool *connPool);
    ~connectionRALL();

private:
    MYSQL *conRAII;
    connection_pool *poolRAII;
};

#endif
