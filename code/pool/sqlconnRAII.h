/*
 * @Author       : mark
 * @Date         : 2020-06-19
 * @copyleft Apache 2.0
 */ 

#ifndef SQLCONNRAII_H
#define SQLCONNRAII_H
#include "sqlconnpool.h"

/* 资源在对象构造初始化 资源在对象析构时释放*/
class SqlConnRAII {
public:
    // MYSQL** 二级指针-传一级指针只是改指针变量的值
    // 为什么要赋值，是为了通过该类析构释放资源
    SqlConnRAII(MYSQL** sql, SqlConnPool *connpool) {
        assert(connpool);
        *sql = connpool->GetConn();
        sql_ = *sql;
        connpool_ = connpool;
    }
    
    ~SqlConnRAII() {
        if(sql_) { connpool_->FreeConn(sql_); } // 关闭时，只是将可用的sql_放回connpool_
    }
    
private:
    MYSQL *sql_;
    SqlConnPool* connpool_;
};

#endif //SQLCONNRAII_H