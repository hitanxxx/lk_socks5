#include "l_base.h"

gobal_conf_t conf;

status config_get ( meta_t ** meta, char * path )
{
    int32_t fd = 0;
    struct stat stconf;
    uint32 len = 0, read_len = 0;
    status rc = ERROR;
    meta_t * local_meta = NULL;
    
    do
    {
        if( stat( path, &stconf ) < 0 )
        {
            err("config config.json file not found\n");
            break;
        }
        len = (uint32)stconf.st_size;
        
        if( len > CONF_SETTING_LENGTH )
        {
            err("config.json too large, limit 32KB\n" );
            break;
        }
        
        if( OK != meta_alloc( meta, len ) )
        {
            err("config meta alloc failed\n");
            break;
        }
        
        fd = open( path, O_RDONLY );
        if( -1 == fd )
        {
            err("config can't open config.json failed, [%d]\n", errno );
            break;
        }
        
        local_meta = * meta;
        while( read_len < len )
        {
            if( rc == read( fd, local_meta->last, len ) )
            {
                err("config read config.json data failed, [%d]\n", errno );
                break;
            }
            read_len += rc;
        }

        local_meta->last += len;
        rc = OK;
    } while(0);
    
    if( fd )
    {
        close(fd);
    }
    return rc;
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
            err("config parse base 'work_procrss' too much, [%d]\n", conf.base.worker_process );
            return ERROR;
        }
    }
    if( OK == json_get_obj_str(root_obj, "sslcrt", l_strlen("sslcrt"), &v ) )
    {
        memcpy( conf.base.sslcrt, v->name.data, l_min( sizeof(conf.base.sslcrt)-1, v->name.len )  );
        if( OK != access( (char*)conf.base.sslcrt, F_OK ) )
        {
            err("config parse base 'sslcrt [%s] not found\n", conf.base.sslcrt );
            return ERROR;
        }
    }
    if( OK == json_get_obj_str(root_obj, "sslkey", l_strlen("sslkey"), &v ) )
    {
        memcpy( conf.base.sslkey, v->name.data, l_min( sizeof(conf.base.sslkey)-1, v->name.len ) );
        if( OK != access( (char*)conf.base.sslkey, F_OK ) )
        {
            err("config parse base 'sslkey' [%s] not found\n", conf.base.sslkey );
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
    if( OK == json_get_obj_str( root_obj, "authfile", l_strlen("authfile"), &v ) )
    {
        memcpy( conf.base.authfile, v->name.data, l_min( sizeof(conf.base.authfile)-1, v->name.len ) );
    }
	return OK;
}

static status config_parse_http( ljson_node_t * json )
{
    ljson_node_t * root_obj, *http_obj;
    ljson_node_t * port, *ssl_port;
    ljson_node_t * home, *index;
    ljson_node_t * v;
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
            err("config parse http, no specify 'home'\n");
        }
        if( home->name.len > FILEPATH_LENGTH )
        {
            err("config parse http, 'home' too long\n");
            return ERROR;
        }
        if( home->name.data[home->name.len-1] == '/' ) home->name.len -= 1;
        memcpy( conf.http.home, home->name.data, l_min( sizeof(conf.http.home)-1, home->name.len ) );
        
        rc = json_get_obj_str( http_obj, "index", l_strlen("index"), &index );
        if( OK != rc )
        {
            err("config parse http, not specify 'index'\n");
        }
        memcpy( conf.http.index, index->name.data, l_min( sizeof(conf.http.index)-1, index->name.len ) );
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
            err("config parse s5, not specify 'mode'\n" );
            return ERROR;
        }
        
        if ( OK == l_strncmp_cap( v->name.data, v->name.len, (unsigned char*)"server", l_strlen("server") ) )
        {
            // server mode
            conf.socks5_mode = SOCKS5_SERVER;
            rc = json_get_obj_num( socks5_obj, "serverport", l_strlen("serverport"), &v );
            if( rc == ERROR )
            {
                err("config parse s5, server mode not specify 'serverport'\n");
                return ERROR;
            }
            conf.socks5.server.server_port = (uint32)v->num_i;
        }
        else if ( OK == l_strncmp_cap( v->name.data, v->name.len, (unsigned char*)"server_secret", l_strlen("server_secret") ) )
        {
            conf.socks5_mode = SOCKS5_SERVER_SECRET;
        }
        else if ( OK == l_strncmp_cap( v->name.data, v->name.len, (unsigned char*)"client", l_strlen("client") ) )
        {
            // client mode
            conf.socks5_mode = SOCKS5_CLIENT;
            rc = json_get_obj_str(socks5_obj, "serverip", l_strlen("serverip"), &v );
            if( rc == ERROR )
            {
                err("config parse s5, client mode not specify 'serverip'\n" );
                return ERROR;
            }
            memcpy( conf.socks5.client.server_ip, v->name.data, l_min( sizeof(conf.socks5.client.server_ip)-1, v->name.len ) );

            rc = json_get_obj_num( socks5_obj, "serverport", l_strlen("serverport"), &v );
            if( rc == ERROR )
            {
                err("config parse s5, client mode not specify 'serverport'\n");
                return ERROR;
            }
            conf.socks5.client.server_port = (uint32)v->num_i;
            rc = json_get_obj_num( socks5_obj, "localport", l_strlen("localport"), &v );
            if( rc == ERROR )
            {
                err("config parse s5, client mode not specify 'localport'\n");
                return ERROR;
            }
            conf.socks5.client.local_port = (uint32)v->num_i;
            
            rc = json_get_obj_str(socks5_obj, "client_username", l_strlen("client_username"), &v );
            if( rc == ERROR )
            {
                err("config parse s5, client mode not specify 'client_username'\n" );
                return ERROR;
            }
            memcpy( conf.socks5.client.user, v->name.data, l_min(sizeof(conf.socks5.client.user)-1, v->name.len) );
        
            rc = json_get_obj_str(socks5_obj, "client_user_passwd", l_strlen("client_user_passwd"), &v );
            if( rc == ERROR )
            {
                err("config parse s5, client mode not specify 'client_user_passwd'\n" );
                return ERROR;
            }
            memcpy( conf.socks5.client.passwd, v->name.data, l_min(sizeof(conf.socks5.client.passwd)-1, v->name.len) );
        }
        else
        {
            err("socks5 invalid 'mode' [%.*s] (server/client/server_secret)\n", v->name.len, v->name.data );
            return ERROR;
        }
    }
    return OK;
}

static status config_parse( ljson_ctx_t * json_ctx )
{
    ljson_node_t * root = &json_ctx->root;

    if( OK != config_parse_global( root ) )
    {
        err("config parse global\n" );
        return ERROR;
    }
    if( OK != config_parse_socks5( root ) )
    {
        err("config parse socks5\n" );
        return ERROR;
    }
    if( OK != config_parse_http( root ) )
    {
        err("config parse http\n" );
        return ERROR;
    }
    return OK;
}

static status config_start( void )
{
    ljson_ctx_t * json_ctx = NULL;
    status rc = ERROR;

    do
    {
        if( OK != config_get( &conf.meta, L_PATH_CONFIG ) )
        {
            err("config configuration file data get\n");
            break;
        }
        printf("configuration file:\n[%.*s]\n", meta_len( conf.meta->pos, conf.meta->last ), conf.meta->pos );
        
        if( OK != json_ctx_create( &json_ctx ) )
        {
            err("config start json ctx create\n" );
            break;
        }
        if( OK != json_decode( json_ctx, conf.meta->pos, conf.meta->last ) )
        {
            err("config configuration file json decode failed\n" );
            break;
        }
        if( OK != config_parse( json_ctx ) )
        {
            err("config parse failed\n" );
            break;
        }
        rc = OK;
    } while(0);

    if( conf.meta )
    {
        meta_free( conf.meta );
        conf.meta = NULL;
    }
    if( json_ctx )
    {
        json_ctx_free( json_ctx );
    }
    printf("config start over.\n");
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
