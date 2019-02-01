# LK

# Introduce
lk是c写的一个基本的网络框架，除OpenSSL之外，不依赖任何库。包含一些功能模块：
## LK-PERF
lk-perf 固定数量进程（非大量fork），事件驱动， 无锁，数据可视化的高性能，web应用性能测试模块。</br>
UI用echarts js实现初略的数据可视化。通过UI可以方便的设置测试用例，通过图表直观的进行数据对比。
## LK-TUNNEL
lk-tunnel SSL tunnel(翻墙模式)/http|https proxy。</br>
双向SSL加密。安全性高。信息只能在ip层面进行跟踪。应用流量只能阻隔，无法篡改，跟踪。</br>
性能高，占用资源极少。适合各种低性能的硬件环境。</br>
跑在1G DDR2 ram的树莓派上，看一个720p的视频，cpu与内存的使用率维持在2%。
## LK-WEB
lk-web 一个小巧简单的web服务器，支持部分http特征。</br>
简单的路由提供webservice。以及静态文件服务。
## LK-socks5
lk-socks5 一个socks5代理模块。client与server使用OpenSSL加密。

# Install
lk的功能模块需要OpenSSL库。解决依赖后。在文件目录运行：
* configure
* make && make install </br>
即可完成安装。
在 centos7 与 debian系raspbian上都成功编译。
安装完成后，在/usr/local/lk目录可看到安装完成后的文件。
运行/usr/local/lk/sbin目录下的elf文件即可使用，但是使用之前可能需要了解配置。
> * /usr/local/lk/conf - 配置文件所在目录
> * /usr/local/lk/logs - pid，日志，缓冲文件所在目录
> * /usr/local/lk/sbin - elf执行文件所在目录
> * /usr/local/lk/www  - HTML资源目录

# Command line parameters
* -stop </br>
作用是停止后台所有lk进程。</br>
stop all process when works in the backend
* -reload </br>
作用是重新启动子进程
reload all worker process

# configuraction
```json
{
	"daemon":true,
	"worker_process":2,

	"reuse_port":false,
	"accept_mutex":false,

	"log_error":true,
	"log_debug":false,

	"sslcrt":"/usr/local/lk/www/certificate/server.crt",
	"sslkey":"/usr/local/lk/www/certificate/server.key",

	"http":{
		"access_log":true,
		"keepalive":false,
		"http_listen":[80],
		"https_listen":[443],
		"home":"/usr/local/lk/www",
		"index":"index.html"
	},
	"tunnel":{
		"mode":"single"
	},
	"perf":{
		"switch":false
	},
	"lktp":{
		"mode":"server"
	},
	"socks5":{
		"mode":"server"
	}

}
```
一个典型的配置文件如上，结构划分为多个部分：
the configuration file can be divided into three part:
* 全局块，一般设置一些共享的信息。
> * daemon - 守护进程开关
> * worker_process - 工作进程数量，为0时，管理进程即为工作进程。
> * reuse_port - socket特性，某些情景优化多进程竞争态。
> * accept_mutex - 信号量锁开关。
> * log_error - error日志开关。
> * log_debug - debug日志开关。（影响性能）
> * sslcrt - SSL证书&公钥。
> * sslkey - SSL私钥。
* http 块，设置一些关于http的信息。
> * access_log - http模块access日志
> * keepalive - http长连接支持开关。
> * http listen - http监听的端口。
> * https listen - https监听的端口。
> * home - HTML资源默认目录。
> * index - HTML资源默认后缀。
* tunnel 块，设置一些挂于tunnel的信息。
> * mode - tunnel工作的模式：（single/server/client）
> * serverip - 当mode为client的时候需要额外指定serverip。
* perf 块，指定性能测试模块的信息。
> * switch - 是否开启性能测试模块。false时不处理web应用性能测试请求。
* lktp 块，关于lktp通信的相关设置。
> * mode - lktp运行的模式，server模式开启5555端口，client模式需要指定serverip字段说明lktp server的ip地址。lktp使用tcp长连接。
* socks5 块，socks5模块的相关设置。
> * mode - socks5模块运行的模式。支持client/server两种模式。</br>
client占用1080端口。server占用3333端口。</br>
以client运行时，需要制定serverip字段，设置server的ip信息。
> * serverip - 以client模式运行时，server的ip地址。

# Tips
* 通过 /perform.html进入web应用性能测试的UI页面。
* tunnel模块使用的端口为7324，7325。client/proxy使用7325。server使用7324。暂不能通过配置修改。
# Benchmark
## lk-perf VS apachebench
宿主 cpu为 3.8Ghz, 内存ddr4-2400，网络为千兆以太</br>
均为http，connection close短连接。
### 单核心
> * lk-perf 虚拟机 1核心，1gb内存
> * ab 虚拟机 1核心，1gb内存
> * 被测机- nginx 1.14，虚拟机 1核心，1gb内存，1进程。

|    object  |   进程 | 并发设置 | 时间     |  test1    |  test2   |   test3     |  cpu      |
|------------|--------|----------|-----------|-----------|---------|-------------|-----------|
|    lk perf |   1    | 30       |   10s     |70679      |70637    |70580        |   99      |
|    ab      |   /    |  1000    |   10s     |72251      |71711    |72433        |   99      |

### 双核心
> * lk perf 虚拟机 2核心，1gb内存
> * ab虚拟机 2核心，1gb内存
> * 被测试的nginx-1.14，虚拟机 2核心，1gb内存，2进程。

|    object   |   进程  | 并发设置 | 时间      |  test1   |test2     |   test3  |  cpu     |
|-------------|---------|----------|-----------|----------|----------|----------|----------|
|    lk perf  |   2     | 30       |   10s     |194177   |195576     |195954    |  97-96   |
|    ab       |   /     |  3000    |   10s     |181247   |161376     |170303    |   100    |
> *  ab 的test2，test3出现了failed的情况。数量在400左右。
> *  双核心数据和单核心的数据比起来，并不是与单核心的数据x2差不多，而是多了不少。这有点不合逻辑，我不知道为什么。也许是因为虚拟机的关系双核心的情况下占用了比预期更多宿主资源导致的。不过这里想说明的是，对比保持了环境的一致性。数据一定程度上总能体现不同对象在同样环境下的差异。
## lk-web VS nginx1.14
### 单核心
> * lk-web 虚拟机 1核心，1gb内存
> * nginx 1.14 虚拟机 1核心 1gb内存
> * 测试机 lk-perf 虚拟机 1核心，1gb内存
lk-perf 采用 30路并发，10s时间

|     object  |    进程    | 时间      |  test1    |  test2  |   test3    |  cpu     |
| ------------|------------|-----------|-----------|---------|-------------|----------|
|    lk web   |   1        |   10s     |77373      |77037    |77475        |   94     |
|    nginx    |   1        |   10s     |70718      |71082    |70215        |   90     |

### 双核心
> * lk-web 虚拟机 2核心，1gb内存
> * nginx 1.14 虚拟机 2核心 1gb内存
> * 测试机 lk-perf 虚拟机 2核心，1gb内存
lk-perf 采用 30路并发，10s时间

|     object |    进程     | 时间      |  test1    |  test2  |   test3     |  cpu     |
|------------|-------------|-----------|-----------|---------|-------------|-----------|
|    lk web  |   2         |   10s     |194854     |193158   |192333       |   92-92   |
|    nginx   |   2         |   10s     |186471     |185138   |184547       |   95-95   |

> * 这里虽然用nginx作为lk-web服务器性能的比较对象。但lk web的作用更多的是为其他模块如 lk perf提供服务。它的功能十分简单，流程也很精简，而nginx功能丰富。 与nginx的性能对比没有太多实际意义。但是能够一定程度上说明lk的性能。
