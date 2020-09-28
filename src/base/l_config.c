#include "l_base.h"

gobal_conf_t conf;

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

static status config_parse_global( ljson_node_t * json )
{
	ljson_node_t *root_obj, *v;

	json_get_child( json, 1, &root_obj );
	if( OK == json_get_obj_bool(root_obj, "daemon", l_strlen("daemon"), &v ) ) 
	{
		conf.base.daemon = (v->node_type == JSON_TRUE) ? 1 : 0;
	}
	if ( OK == json_get_obj_num(root_obj, "worker_process", l_strlen("worker_process"), &v ) ) 
	{
		conf.base.worker_process = (uint32)v->num_i;
		if( conf.base.worker_process > MAXPROCESS ) 
		{
			err(" work_procrss too much, [%d]\n", conf.base.worker_process );
			return ERROR;
		}
	}
	if( OK == json_get_obj_str(root_obj, "sslcrt", l_strlen("sslcrt"), &v ) ) 
	{
		strncpy( conf.base.sslcrt, v->name.data, v->name.len );
		if( OK != access( conf.base.sslcrt, F_OK ) ) 
		{
			err(" sslcrt file [%s] not found\n", conf.base.sslcrt );
			return ERROR;
		}
	}
	if( OK == json_get_obj_str(root_obj, "sslkey", l_strlen("sslkey"), &v ) ) 
	{
		strncpy( conf.base.sslkey, v->name.data, v->name.len );
		if( OK != access( conf.base.sslkey, F_OK ) ) 
		{
			err(" sslkey file [%s] not found\n", conf.base.sslkey );
			return ERROR;
		}
	}
	if( OK == json_get_obj_bool(root_obj, "log_error", l_strlen("log_error"), &v ) ) 
	{
		conf.log.error = (v->node_type == JSON_TRUE) ? 1 : 0;
	}
	if( OK == json_get_obj_bool(root_obj, "log_debug", l_strlen("log_debug"), &v ) ) 
	{
		conf.log.debug = (v->node_type == JSON_TRUE) ? 1 : 0;
	}
	return OK;
}

static status config_parse_http( ljson_node_t * json )
{
	ljson_node_t * root_obj, *http_obj;
	ljson_node_t * port, *ssl_port;
	ljson_node_t * home, *index;
	ljson_node_t * v;
	queue_t * q;
	int32 rc;
	int i;

	json_get_child( json, 1, &root_obj );
	if( OK == json_get_obj_obj( root_obj, "http", l_strlen("http"), &http_obj ) )
	{
		if( OK == json_get_obj_arr( http_obj, "port", l_strlen("port"), &port ) )
		{
			i = 1;
			while( OK == json_get_child( port, i, &v ) )
			{
				conf.http.ports[conf.http.port_n++] = v->num_i;
				i++;
			}
		}

		if( OK == json_get_obj_arr( http_obj, "ssl_port", l_strlen("ssl_port"), &ssl_port ) )
		{
			i = 1;
			while( OK == json_get_child( ssl_port, i, &v ) )
			{
				conf.http.ssl_ports[conf.http.ssl_portn++] = v->num_i;
				i++;
			}
		}

		rc = json_get_obj_str( http_obj, "home", l_strlen("home"), &home );
		if( OK != rc )
		{
			err("http need home\n");
		}
		if( home->name.len > FILEPATH_LENGTH )
		{
			err("config http home too long\n");
			return ERROR;
		}
		if( home->name.data[home->name.len-1] == '/' ) home->name.len -= 1;
		strncpy( conf.http.home, home->name.data, l_min( sizeof(conf.http.home), home->name.len ) );
		
		rc = json_get_obj_str( http_obj, "index", l_strlen("index"), &index );
		if( OK != rc )
		{
			err("http need index\n");
		}
		strncpy( conf.http.index, index->name.data, (index->name.len <= sizeof(conf.http.index) ) ? index->name.len: sizeof(conf.http.index) );
	}
	return OK;
}

static status config_parse_socks5( ljson_node_t * json )
{
	ljson_node_t * root_obj, *socks5_obj, *v;
	status rc;

	json_get_child( json, 1, &root_obj );
	if( OK == json_get_obj_obj(root_obj, "socks5", l_strlen("socks5"), &socks5_obj ) ) 
	{
		rc = json_get_obj_str(socks5_obj, "mode", l_strlen("mode"), &v );
		if( rc == ERROR ) 
		{
			err(" tunnel need specify a valid 'mode'\n" );
			return ERROR;
		}
		
		if ( OK == l_strncmp_cap( v->name.data, v->name.len, "server", l_strlen("server") ) ) 
		{
			// server mode
			conf.socks5_mode = SOCKS5_SERVER;
			rc = json_get_obj_num( socks5_obj, "serverport", l_strlen("serverport"), &v );
			if( rc == ERROR ) 
			{
				err("socks5 server mode need server port\n");
				return ERROR;
			}
			conf.socks5.server.server_port = (uint32)v->num_i;
			rc = json_get_obj_str( socks5_obj, "serverauthfile", l_strlen("serverauthfile"), &v );
			if( rc == ERROR )
			{
				err("socks5 server auth file not specific\n");
				return ERROR;
			}
			strncpy( conf.socks5.server.authfile, v->name.data, v->name.len );
		} 
		else if ( OK == l_strncmp_cap( v->name.data, v->name.len, "client", l_strlen("client") ) ) 
		{
			// client mode
			conf.socks5_mode = SOCKS5_CLIENT;
			rc = json_get_obj_str(socks5_obj, "serverip", l_strlen("serverip"), &v );
			if( rc == ERROR ) 
			{
				err("socks5 client need specify a valid 'serverip'\n" );
				return ERROR;
			}
			strncpy( conf.socks5.client.server_ip, v->name.data, v->name.len );

			rc = json_get_obj_num( socks5_obj, "serverport", l_strlen("serverport"), &v );
			if( rc == ERROR )
			{
				err("socks5 client mode need server port\n");
				return ERROR;
			}
			conf.socks5.client.server_port = (uint32)v->num_i;
			rc = json_get_obj_num( socks5_obj, "localport", l_strlen("localport"), &v );
			if( rc == ERROR ) 
			{
				err("socks5 client mode need local port\n");
				return ERROR;
			}
			conf.socks5.client.local_port = (uint32)v->num_i;
			
			rc = json_get_obj_str(socks5_obj, "client_username", l_strlen("client_username"), &v );
			if( rc == ERROR ) 
			{
				err("socks5 client need specify a valid 'serverip'\n" );
				return ERROR;
			}
			strncpy( conf.socks5.client.user, v->name.data, (v->name.len <= sizeof(conf.socks5.client.user)) ? v->name.len : sizeof(conf.socks5.client.user));
		
			rc = json_get_obj_str(socks5_obj, "client_user_passwd", l_strlen("client_user_passwd"), &v );
			if( rc == ERROR ) 
			{
				err("socks5 client need specify a valid 'serverip'\n" );
				return ERROR;
			}
			strncpy( conf.socks5.client.passwd, v->name.data, (v->name.len <= sizeof(conf.socks5.client.passwd)) ? v->name.len : sizeof(conf.socks5.client.passwd) );
		} 
		else 
		{
			err("socks5 invalid 'mode' [%.*s] ( 'server' or 'client' )\n", v->name.len, v->name.data );
			return ERROR;
		}
	}
	return OK;
}

static status config_parse( ljson_node_t * json )
{
	if( OK != config_parse_global( json ) ) 
	{
		err(" parse global\n" );
		return ERROR;
	}
	if( OK != config_parse_socks5( json ) )
	{
		err(" parse socks5\n" );
		return ERROR;
	}
	if( OK != config_parse_http( json ) )
	{
		err(" parse http\n" );
		return ERROR;
	}
	return OK;
}

static status config_start( void )
{
	ljson_ctx_t * ctx = NULL;
	int rc = OK;

	if( OK != config_get( &conf.meta, L_PATH_CONFIG ) )
	{
		err(" configuration file open" );
		rc = ERROR;
		goto failed;
	}
	printf( " configuration file:\n[%.*s]\n", meta_len( conf.meta->pos, conf.meta->last ), conf.meta->pos);
	if( OK != json_ctx_create( &ctx ) ) 
	{
		err(" json ctx create\n" );
		rc = ERROR;
		goto failed;
	}
	if( OK != json_decode( ctx, conf.meta->pos, conf.meta->last ) ) 
	{
		err(" configuration file json decode failed\n" );
		rc = ERROR;
		goto failed;
	}
	if( OK != config_parse( &ctx->root ) ) 
	{
		err(" config parse failed\n" );
		rc = ERROR;
		goto failed;
	}
failed:
	if( conf.meta ) {
		meta_free( conf.meta );
		conf.meta = NULL;
	}
	if( ctx ) {
		json_ctx_free( ctx );
	}
	return rc;
}

status config_init ( void )
{
	memset( &conf, 0, sizeof(gobal_conf_t) );
	conf.log.debug = 1;
	conf.log.error = 1;

	if( OK != config_start(  ) ) 
	{
		return ERROR;
	}
	return OK;
}

status config_end ( void )
{
	if( conf.meta )
	{
		meta_free( conf.meta );
	}
	return OK;
}
