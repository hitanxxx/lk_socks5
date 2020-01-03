# LKSOCKS5

# Introduce
lksocks5 is a socks5 proxy written by ANSIC, it needs OpenSSL library .
# Features
* Support private auth protocol
* Support event dirvers
* Support bidirectional event buffer

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
* config.json 
 > * If run in server mode, the configuration file was like that.
 
```json
{
	"daemon":true,
	"worker_process":2,

	"log_error":true,
	"log_debug":false,

	"sslcrt":"/usr/local/lksocks5/certificate/server.crt",
	"sslkey":"/usr/local/lksocks5/certificate/server.key",

	"socks5":{
		"mode":"server",
		"serverport"12345,
		"serverauthfile":"/usr/local/lksocks5/config/auth.json"
	}
}
```

> * if run in client mode, the configuration file was like that.

``` json
{
	"daemon":true,
	...
	"socks5":{
		"mode":"client",
		"serverip":"1.1.1.1",
		"serverport"12345,
		"localport":1080,
		"client_username":"user",
		"client_user_passwd":"passwd"
	}
}
```

* auth.json 
> * auth.json use for server, it's a user infomation table.

``` json
{
	"socks5_user_database":
	[
		{ "username":"admin", "passwd":"admin" }
	]
}
```
* a typical configuration file can be  divided into a few parts:

* normal block:
> * daemon - witch of daemon process
> * worker_process - identification worker process number
> * log_error - switch of log error
> * log_debug - switch of log debug
> * sslcrt - ssl certificate file public key
> * sslkey - ssl certificate file private key

* socks5 block:
> * mode - identification work mode of socks5 module.
> * serverip - identification socks5 server ipaddress.
> * serverport - identification socks5 server port.
> * serverauthfile - identification socks5 server's user informations.
> * localport - identification socks5 client port.
> * client_username - identification socks5 client's auth username.
> * client_user_passwd - identification socks5 client's auth passwd.

# tips
recommended use firefox browser use lkscosk5, chosen to use socks5 proxy DNS request.
