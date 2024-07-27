# ngx-http-redis-module

通过key在redis中获取value,并将value的内容返回给浏览器

## Directives
```
Syntax:	 redis-visit on | off;
Default: redis-visit off;
Context: http, server, location
```
如果关闭，将不会去访问`redis`



```
Syntax:	 redis-host [redis host ip|redis host domain];
Default: redis-host 127.0.0.1;
Context: http, server, location
```
redis 的主机ip或者域名



```
syntax:	 redis-port port;
default: redis-port 6379;
context: http, server, location
```
redis 的端口



```
syntax:	 redis-user user;
default: no default value;
context: http, server, location
```
redis 的用户



```
syntax:	 redis-password password;
default: no default value;
context: http, server, location
```
redis 的密码



```
syntax:	 redis-cache on | off;
default: redis-cache off;
context: location
```
是否开启 nginx 本地缓存 redis 的 key value 对



```
syntax:	 redis-cache-miss on | off;
default: redis-cache-miss off;
context: location
```
是否开启 nginx 本地缓存 redis 的 未命中 key


```
syntax:	 redis-cache-timeout time;
default: redis-cache-timeout 60s;
context: http, server, location
```
nginx 本地缓存 redis 的 超时时间


```
syntax:	 redis-cache-hash-size size;
default: redis-cache-hash-size 1031;
context: http, server, location
```
nginx 本地缓存 redis 的 hash 桶大小



```
syntax:	 redis-cache-size size;
default: redis-cache-size 1M;
context: http, server, location
```
nginx 本地缓存 redis 的所使用的内存大小



```
syntax:	 redis-conn-timeout time;
default: redis-conn-timeout 10s;
context: http, server, location
```
连接redis的超时时间



```
syntax:	 redis-send-timeout time;
default: redis-send-timeout 10s;
context: http, server, location
```
发送请求到redis的超时时间



```
syntax:	 redis-read-timeout time;
default: redis-read-timeout 10s;
context: http, server, location
```
读取redis响应的超时时间



```
syntax:	 redis-select num;
default: redis-select 0;
context: http, server, location
```
选择redis道数


```
syntax:	 redis-get key;
default: no default value;
context: http, server, location
```
redis 的 key



```
syntax:	 redis-content-type contentType;
default: redis-content-type text/plain;
context: http, server, location
```
redis 返回 value 的类型

