#include "common.h"

static config_t g_config;

#define pdbg(format, ...) printf( "[%u]-%s:%d "format, (unsigned)time(NULL), __func__, __LINE__, ##__VA_ARGS__)


static status config_parse( char * str )
{
    int i = 0;
    cJSON * root = cJSON_Parse( str );
    if( root )
    {
        cJSON * sys_daemon = cJSON_GetObjectItem(root, "sys_daemon");
        cJSON * sys_process = cJSON_GetObjectItem(root, "sys_process");
        cJSON * sys_loglevel = cJSON_GetObjectItem(root, "sys_log_level");

        cJSON * ssl_crt_path = cJSON_GetObjectItem( root, "ssl_crt_path" );
        cJSON * ssl_key_path = cJSON_GetObjectItem( root, "ssl_key_path" );

        cJSON * s5_mode = cJSON_GetObjectItem( root, "s5_mode" );
        cJSON * s5_serv_port = cJSON_GetObjectItem( root, "s5_serv_port" );
        cJSON * s5_serv_auth_path = cJSON_GetObjectItem( root, "s5_serv_auth_path" );

        cJSON * s5_local_port = cJSON_GetObjectItem( root, "s5_local_port" );
        cJSON * s5_local_serv_port = cJSON_GetObjectItem( root, "s5_local_serv_port" );
        cJSON * s5_local_serv_ip = cJSON_GetObjectItem( root, "s5_local_serv_ip" );
        cJSON * s5_local_usrname = cJSON_GetObjectItem( root, "s5_local_usrname" );
        cJSON * s5_local_passwd = cJSON_GetObjectItem( root, "s5_local_passwd" );

        cJSON * http_arr = cJSON_GetObjectItem( root, "http_arr" );
        cJSON * https_arr = cJSON_GetObjectItem( root, "https_arr" );
        cJSON * http_home = cJSON_GetObjectItem( root, "http_home" );
        cJSON * http_index = cJSON_GetObjectItem( root, "http_index" );

        if( sys_daemon )
            g_config.sys_daemon 	= sys_daemon->valueint;
        if( sys_process )
            g_config.sys_process	= sys_process->valueint;
        if( sys_loglevel )
            g_config.sys_log_level	= sys_loglevel->valueint;
        pdbg("sys_daemon [%d]\n", g_config.sys_daemon );
        pdbg("sys_process [%d]\n", g_config.sys_process);
        pdbg("sys_log_level [%d]\n", g_config.sys_log_level);


        if( ssl_crt_path )
            strncpy( g_config.ssl_crt_path, cJSON_GetStringValue(ssl_crt_path), sizeof(g_config.ssl_crt_path)-1 );
        if( ssl_key_path )
            strncpy( g_config.ssl_key_path, cJSON_GetStringValue(ssl_key_path), sizeof(g_config.ssl_key_path)-1 );
        pdbg("ssl_crt_path [%s]\n", g_config.ssl_crt_path );
        pdbg("ssl_key_path [%s]\n", g_config.ssl_key_path);


        if(s5_mode)
            g_config.s5_mode	= s5_mode->valueint;
        if(s5_serv_port)
            g_config.s5_serv_port	= s5_serv_port->valueint;
        if(s5_serv_auth_path)
            strncpy( g_config.s5_serv_auth_path, cJSON_GetStringValue(s5_serv_auth_path), sizeof(g_config.s5_serv_auth_path)-1 );
        pdbg("s5_mode [%d]\n", g_config.s5_mode );
        pdbg("s5_serv_port [%d]\n", g_config.s5_serv_port);
        pdbg("s5_serv_auth_path [%s]\n", g_config.s5_serv_auth_path);


        if( s5_local_port )
            g_config.s5_local_port = s5_local_port->valueint;
        if(s5_local_serv_port)
            g_config.s5_local_serv_port = s5_local_serv_port->valueint;
        if(s5_local_serv_ip)
            strncpy( g_config.s5_local_serv_ip, cJSON_GetStringValue(s5_local_serv_ip), sizeof(g_config.s5_local_serv_ip)-1 );
        if(s5_local_usrname)
            strncpy( g_config.s5_local_usrname, cJSON_GetStringValue(s5_local_usrname), sizeof(g_config.s5_local_usrname)-1 );
        if(s5_local_passwd)
            strncpy( g_config.s5_local_passwd, cJSON_GetStringValue(s5_local_passwd), sizeof(g_config.s5_local_passwd)-1 );
        pdbg("s5_local_port [%d]\n", g_config.s5_local_port );
        pdbg("s5_local_serv_port [%d]\n", g_config.s5_local_serv_port );
        pdbg("s5_local_serv_ip [%s]\n", g_config.s5_local_serv_ip );
        pdbg("s5_local_usrname [%s]\n", g_config.s5_local_usrname );
        pdbg("s5_local_passwd [%s]\n", g_config.s5_local_passwd );


        if(http_arr)
        {
            for(i = 0; i < cJSON_GetArraySize(http_arr); i ++)
                g_config.http_arr[i]   = cJSON_GetArrayItem(http_arr, i)->valueint;
        }
        if(https_arr)
        {
            for(i = 0; i < cJSON_GetArraySize(https_arr); i ++)
                g_config.https_arr[i]   = cJSON_GetArrayItem(https_arr, i)->valueint;
        }
        if(http_home)
            strncpy( g_config.http_home, cJSON_GetStringValue(http_home), sizeof(g_config.http_home)-1 );
        if(http_index)
            strncpy( g_config.http_index, cJSON_GetStringValue(http_index), sizeof(g_config.http_index)-1 );
        g_config.http_num   = cJSON_GetArraySize(http_arr);
        for(i = 0; i < g_config.http_num; i ++)
            pdbg("[%d]\n", g_config.http_arr[i] );
        g_config.https_num   = cJSON_GetArraySize(https_arr);
        for(i = 0; i < g_config.https_num; i ++)
            pdbg("[%d]\n", g_config.https_arr[i] );
        pdbg("http_home [%s]\n", g_config.http_home );
        pdbg("http_index [%s]\n", g_config.http_index );
        
        cJSON_Delete(root);
    }
    else
    {
        err("cjson parse config file failed\n");
        return ERROR;
    }
    return OK;
}

config_t * config_get(  )
{
    return &g_config;
}

status config_init ( void )
{
    memset( &g_config, 0, sizeof(config_t) );
    g_config.sys_log_level	=	0xff;
    struct stat stconf;
    int filesz = 0;
    meta_t * meta = NULL;
    int fd = 0;
    int rc = 0;

    memset( &stconf, 0, sizeof(struct stat) );
    do 
    {
        if( stat( L_PATH_CONFIG, &stconf ) < 0 )
        {
            err("stat config file failed. [%d]\n", errno );
            break;
        }
        filesz = (int)stconf.st_size;

        if( OK != meta_alloc( &meta, filesz ) )
        {
            pdbg("alloc meta failed. [%d]\n", errno );
            break;
        }
        fd = open( L_PATH_CONFIG, O_RDONLY );
        if( fd <= 0 )
        {
            pdbg("open config file failed. [%d]\n", errno );
            break;
        }

        while( meta->last - meta->pos < filesz )
        {
            rc = read( fd, meta->last, meta->end - meta->last );
            if( rc <= 0 )
            {
                pdbg("read config all [%d] cur [%d] failed. [%d]\n", filesz, (int)(meta->last - meta->pos), errno );
                break;
            }
            meta->last += rc;
        }while(0);

        pdbg("%s", meta->pos );
        config_parse( (char*)meta->pos );

    }	while(0);

    if( fd )
        close(fd);

    if(meta)
        meta_free(meta);

    return OK;
}

status config_end ( void )
{
	return OK;
}