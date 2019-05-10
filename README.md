# LKSOCKS5

# Introduce
lksocks5 is a socks5 proxy written by ANSIC, it needs OpenSSL library .
# Install
*  configure
*  make && make install </br>
it's compiled success in centos7 and debian8 .
after install, you can find some file or folder in "/usr/local/lksocks5" .
> * /usr/local/lksocks5/conf - the configuration file use
> * /usr/local/lksocks5/logs - pid，log，temp file use
> * /usr/local/lksocks5/sbin - elf file use
> * /usr/local/lksocks5/certificate - ssl certificate file use

# Command line parameters
* -stop </br>
stop all process in the backend</br>
* -reload </br>
restart all work process

# configuraction
```json
{
	"daemon":true,
	"worker_process":2,

	"reuse_port":false,
	"accept_mutex":false,

	"log_error":true,
	"log_debug":false,

	"sslcrt":"/usr/local/lksocks5/certificate/server.crt",
	"sslkey":"/usr/local/lksocks5/certificate/server.key",

	"socks5":{
		"mode":"server"
	}
}
```
a typical configuration file just like this:
* normal block:
> * daemon - witch of daemon process
> * worker_process - identification worker process number
> * log_error - switch of log error
> * log_debug - switch of log debug
> * sslcrt - ssl certificate file public key
> * sslkey - ssl certificate file private key
* socks5 block:
> * mode - identification work mode of socks5 module</br>
> * serverip - identification socks5 server ipaddress 
> * serverport - identification socks5 server port
> * clientport - identification socks5 client port 
# tips
recommended use firefox browser use lkscosk5, chosen to use socks5 proxy DNS request.
