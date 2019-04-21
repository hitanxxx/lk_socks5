#include "lk.h"

config_t conf;

// config_get ----
status config_get ( meta_t ** meta, char * path )
{
	int fd;
	struct stat stconf;
	uint32 length;
	meta_t * new;

	if( stat( path, &stconf ) < 0 ) {
		err(" config.json not found"   );
		return ERROR;
	}
	length = (uint32)stconf.st_size;
	if( length > CONF_SETTING_LENGTH ) {
		err(" config.json too big, limit [32768byte]\n" );
		return ERROR;
	}
	if( OK != meta_alloc( &new, length ) ) {
		err(" meta alloc\n" );
		return ERROR;
	}
	fd = open( path, O_RDONLY );
	if( ERROR == fd ) {
		err(" can't open config.json\n" );
		meta_free( new );
		return ERROR;
	}
	if( ERROR == read( fd, new->last, length ) ) {
		err(" read config.json\n" );
		meta_free( new );
		close( fd );
		return ERROR;
	}
	new->last += length;
	close(fd);
	*meta = new;
	return OK;
}
// config_parse_global ----------
static status config_parse_global( json_node_t * json )
{
	char str[1024] = {0};
	json_node_t *root_obj, *v;

	json_get_child( json, 1, &root_obj );
	if( OK == json_get_obj_bool(root_obj, "daemon", l_strlen("daemon"), &v ) ) {
		conf.daemon = (v->type == JSON_TRUE) ? 1 : 0;
	}
	if ( OK == json_get_obj_num(root_obj, "worker_process", l_strlen("worker_process"), &v ) ) {
		conf.worker_process = (uint32)v->num;
		if( conf.worker_process > MAXPROCESS ) {
			err(" work_procrss too much, [%d]\n", conf.worker_process );
			return ERROR;
		}
	}
	if( OK == json_get_obj_str(root_obj, "sslcrt", l_strlen("sslcrt"), &v ) ) {
		if( v->name.len > L_SSL_CERT_PATH_LEN ) {
			err(" ssl cert file path to long, limit = 128\n" );
			return ERROR;
		}
		conf.sslcrt.data = v->name.data;
		conf.sslcrt.len = v->name.len;
		memset( str, 0, sizeof(str) );
		l_memcpy( str, conf.sslcrt.data, conf.sslcrt.len );
		if( OK != access( str, F_OK ) ) {
			err(" sslcrt file [%.*s] not found\n",
			conf.sslcrt.len, conf.sslcrt.data );
			conf.sslcrt.len = 0;
		}
	}
	if( OK == json_get_obj_str(root_obj, "sslkey", l_strlen("sslkey"), &v ) ) {
		if( v->name.len > L_SSL_KEY_PATH_LEN ) {
			err(" ssl cert file path to long, limit = 128\n" );
			return ERROR;
		}
		conf.sslkey.data = v->name.data;
		conf.sslkey.len = v->name.len;
		memset( str, 0, sizeof(str) );
		l_memcpy( str, conf.sslkey.data, conf.sslkey.len );
		if( OK != access( str, F_OK ) ) {
			err(" sslkey file [%.*s] not found\n",
		 	conf.sslkey.len, conf.sslkey.data );
			conf.sslkey.len = 0;
		}
	}
	if( OK == json_get_obj_bool(root_obj, "log_error", l_strlen("log_error"), &v ) ) {
		conf.log_error = (v->type == JSON_TRUE) ? 1 : 0;
	}
	if( OK == json_get_obj_bool(root_obj, "log_debug", l_strlen("log_debug"), &v ) ) {
		conf.log_debug = (v->type == JSON_TRUE) ? 1 : 0;
	}
	return OK;
}

// config_parse_socks5 -------------
static status config_parse_socks5( json_node_t * json )
{
	json_node_t * root_obj, *socks5_obj, *v;
	status rc;

	json_get_child( json, 1, &root_obj );
	if( OK == json_get_obj_obj(root_obj, "socks5", l_strlen("socks5"), &socks5_obj ) ) 
	{
		rc = json_get_obj_str(socks5_obj, "mode", l_strlen("mode"), &v );
		if( rc == ERROR ) {
			err(" tunnel need specify a valid 'mode'\n" );
			return ERROR;
		}
		// server
		// client
		if ( OK == l_strncmp_cap( v->name.data, v->name.len, "server", l_strlen("server") ) ) {
			conf.socks5_mode = SOCKS5_SERVER;
			rc = json_get_obj_num( socks5_obj, "serverport", l_strlen("serverport"), &v );
			if( rc == ERROR ) {
				err("socks5 server mode need server port\n");
				return ERROR;
			}
			conf.socks5_server_port = (uint32)v->num;
		} else if ( OK == l_strncmp_cap( v->name.data, v->name.len, "client", l_strlen("client") ) ) {
			conf.socks5_mode = SOCKS5_CLIENT;
			rc = json_get_obj_str(socks5_obj, "serverip", l_strlen("serverip"), &v );
			if( rc == ERROR ) {
				err("socks5 client need specify a valid 'serverip'\n" );
				return ERROR;
			}
			conf.socks5_serverip.data = v->name.data;
			conf.socks5_serverip.len = v->name.len;
			rc = json_get_obj_num( socks5_obj, "serverport", l_strlen("serverport"), &v );
			if( rc == ERROR ) {
				err("socks5 client mode need server port\n");
				return ERROR;
			}
			conf.socks5_server_port = (uint32)v->num;
			rc = json_get_obj_num( socks5_obj, "localport", l_strlen("localport"), &v );
			if( rc == ERROR ) {
				err("socks5 client mode need local port\n");
				return ERROR;
			}
			conf.socks5_local_port = (uint32)v->num;
		} else {
			err("socks5 invalid 'mode' [%.*s] ( 'server' or 'client' )\n",
			v->name.len, v->name.data );
			return ERROR;
		}
	}
	return OK;
}
// config_parse -----
static status config_parse( json_node_t * json )
{
	if( OK != config_parse_global( json ) ) {
		err(" parse global\n" );
		return ERROR;
	}
	if( OK != config_parse_socks5( json ) ) {
		err(" parse socks5\n" );
		return ERROR;
	}
	return OK;
}
// config_start ----
static status config_start( void )
{
	json_ctx_t * ctx = NULL;

	if( OK != config_get( &conf.meta, L_PATH_CONFIG ) ) {
		err(" configuration file open" );
		goto failed;
	}
	debug( " configuration file:\n[%.*s]\n", meta_len( conf.meta->pos, conf.meta->last ), conf.meta->pos);
	if( OK != json_ctx_create( &ctx ) ) {
		err(" json ctx create\n" );
		goto failed;
	}
	if( OK != json_decode( ctx, conf.meta->pos, conf.meta->last ) ) {
		err(" configuration file json decode failed\n" );
		goto failed;
	}
	if( OK != config_parse( &ctx->root ) ) {
		err(" config parse failed\n" );
		goto failed;
	}
	json_ctx_free( ctx );
	return OK;
failed:
	if( conf.meta ) {
		meta_free( conf.meta );
	}
	conf.meta = NULL;
	if( ctx ) {
		json_ctx_free( ctx );
	}
	ctx = NULL;
	return ERROR;
}
// config_init ----
status config_init ( void )
{
	memset( &conf, 0, sizeof(config_t) );
	conf.log_debug = 1;
	conf.log_error = 1;

	if( OK != config_start(  ) ) {
		return ERROR;
	}
	return OK;
}
// config_end -----
status config_end ( void )
{
	if( conf.meta ) {
		meta_free( conf.meta );
	}
	return OK;
}
