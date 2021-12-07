# 多线程网络服务器

## 概述
+ 使用Reactor事件处理模型
+ 使用kqueue实现IO复用
+ 使用线程池实现并发处理请求
+ 实现主从状态机解析HTTP请求

## 使用

```
$ make
$ ./server ip port
```

## TODO

+ 日志系统
+ 定时器处理非活动连接
+ 实现HTTP响应

