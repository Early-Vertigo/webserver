readme

功能：
    客户端连接到服务器，实现并发可以做到多个客户端连接服务器
    除非主动调用close，会一直保持连接，服务器会因此被占用文件描述符，多线程下的fd被占满影响效率
    通过设定事件，如果时间内没有进行任何通信就关闭超时的连接
    通过二叉树实现，保证根节点是最小的，将每一个连接封装为定时器，保存在小根堆里