# Lab3 实验报告

## 编译选项

本人直接编程在单个文件 server.c 里，要向树莓派环境编译时，只需要添加 `-O2` `--std=c99` 选项即可。

例如，在本人机器上使用 Lab1 所用的交叉编译工具时，编译指令为 :
```
arm-linux-gnueabihf-gcc server.c -o server -O2 --std=c99
```

至于运行的话
```
./server
```

## 实验设计

### “合理的假定“

虽说我们可以通过一切形式构造出长度超过 1M 字符数的请求内容，但是对于那些我们可以解析并确认合法（既不是 `500 Internal Server Error` 也不是 `404 Not Found`）的 ”请求“，应当是：
- 开头的前五个字符必定是 `GET /` 。否则，或者 HTTP 方法不是 `GET` ，或者是不以 / 开头的非法路径（当然也包括没有路径）。
- 路径长度不超过 4096 个字符。否则，即使我们将其完整解析，调用 `open` 等函数打开文件时，系统也无法识别它。
- 后面的 HTTP 请求协议，Host 等是被本实验 ”确保“ 无误的。

也就意味着，我们不必读取完整的请求内容，甚至于只需要读前几千个字符（本人读取了 8192 个字符）。

### 必做：解析和检验 HTTP 头

鉴于 `read` 和 `write` 偶尔出现的，不能读全指定字节数的情况，本人的代码中都包含了诸多 ”循环读取“ 的行为。

解析判定包括如下几个部分：
- 请求内容至多读到 8192 字节，若请求内容总数不足 5 字节，或前5个字节不是 `GET /`，或在 8192 字节内找不到分割 PATH 字段的第二个空格，则判定为 ”500“ 情形。
- 在 PATH 字段内，判定该路径将会到达的层次，中途任何一个时刻企图到达 ”根目录“ （server 所在目录）上层目录的行为（比如 `/123/../../`）被认定非法并返回 ”500“
- 将 PATH 字段的前一个分割空格替换为 '.' 字符，后一个空格替换为 '\0'，将从'.' 开始的字符串作为路径传递给 `open` 和 `stat` 函数（无视，或者任凭在这个字符串区间内仍有其他 '\0' 字符出现的情况），若 `open` 失败，返回 ”404“；再若 `stat` 返回的状态表明文件不是常规文件（`!S_ISREG(fstatus->st_mode)`），则返回 ”500“。
- 若一切非法情况都没有发生，则获得目标文件的文件描述符

### 必做：实现读取请求文件内容

使用 `read` 和 `write` 以及必要的循环，完整读取并写出请求文件的内容。`Content-Length` 响应头在解析请求的时候就已经可以用 `stat` 获得。

### 必做：实现错误和异常处理

在任何导致程序将无法继续下去的函数调用报错（但下面将会提到的 epoll 使用中，一些函数调用报错并不属于这种类型），如 bind 失败，listen 失败，malloc 失败，read、write 失败等会使用 `perror(message)` 报出错误信息并终止程序（或子进程）。

而一些应当不影响程序继续执行的调用错误，则只向 `stderr` 送出信息。

### 必做：多进程；选做：epoll

获取本机 CPU 的核心数，并创建等量的子进程。每个子进程维护一个 epoll 列表。在每个 epoll 列表里：
- 被 `listen` 的请求 socket 被 `fcntl` 设定为非阻塞模式，并以 `EPOLLIN` level-triggered 事件模式添加到敏感列表。
- 当请求 socket 被 `epoll_wait` 获取时，尝试 `accept` 它（若 accept 失败，则什么都不做），否则将 accept 到的服务 socket 以阻塞式，事件模式 `EPOLLEN | EPOLLONESHOT` 添加到敏感列表。
- 当服务 socket 被获取，执行处理过程。

siege 测试效果：树莓派上运行。

`-c 5 -r 1`，10M 文件：

```
Transactions:		           5 hits
Availability:		      100.00 %
Elapsed time:		       21.55 secs
Data transferred:	       50.00 MB
Response time:		       16.63 secs
Transaction rate:	        0.23 trans/sec
Throughput:		        2.32 MB/sec
Concurrency:		        3.86
Successful transactions:           5
Failed transactions:	           0
Longest transaction:	       21.55
Shortest transaction:	       13.99
```

`-c 100 -r 100`  ，125Byte 文件：

```
Transactions:		       10000 hits
Availability:		      100.00 %
Elapsed time:		        9.30 secs
Data transferred:	        1.19 MB
Response time:		        0.09 secs
Transaction rate:	     1075.27 trans/sec
Throughput:		        0.13 MB/sec
Concurrency:		       96.48
Successful transactions:       10000
Failed transactions:	           0
Longest transaction:	        0.17
Shortest transaction:	        0.02

```