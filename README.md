# LKSOCKS5

# Introduce
lk-socks5 一个由c编写的socks5代理软件。支持linux系统。client与server使用OpenSSL加密。迅速，快捷。</br>使用LK网络框架驱动，业务代码量较少。可以作为熟悉socks5协议的历程使用。

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

	"socks5":{
		"mode":"server"
	}

}
```
一个典型的配置文件如下
> * daemon - 守护进程开关
> * worker_process - 工作进程数量，为0时，管理进程即为工作进程。
> * reuse_port - socket特性，某些情景优化多进程竞争态。
> * accept_mutex - 信号量锁开关。
> * log_error - error日志开关。
> * log_debug - debug日志开关。（影响性能）
> * sslcrt - SSL证书&公钥。
> * sslkey - SSL私钥。
* socks5 块，socks5模块的相关设置。
> * mode - socks5模块运行的模式。支持client/server两种模式。</br>
client占用1080端口。server占用3333端口。</br>
以client运行时，需要制定serverip字段，设置server的ip信息。
> * serverip - 以client模式运行时，server的ip地址。
