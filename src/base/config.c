#include "common.h"

static config_t g_config;

#define pdbg(format, ...) printf( "[%u]-%s:%d "format, (unsigned)time(NULL), __func__, __LINE__, ##__VA_ARGS__)


static status config_parse( char * str )
{
    int i = 0;
    cJSON * root = cJSON_Parse( str );
    if( root ) {
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
        cJSON * s5_local_auth = cJSON_GetObjectItem( root, "s5_local_auth" );

        cJSON * http_arr = cJSON_GetObjectItem( root, "http_arr" );
        cJSON * https_arr = cJSON_GetObjectItem( root, "https_arr" );
        cJSON * http_home = cJSON_GetObjectItem( root, "http_home" );
        cJSON * http_index = cJSON_GetObjectItem( root, "http_index" );

        if( sys_daemon ) {
            g_config.sys_daemon = sys_daemon->valueint;
        }
        if( sys_process ) {
            g_config.sys_process_num = sys_process->valueint;
        }
        if( sys_loglevel ) {
            g_config.sys_log_level = sys_loglevel->valueint;
        }
        pdbg("sys_daemon [%d]\n", g_config.sys_daemon );
        pdbg("sys_process [%d]\n", g_config.sys_process_num);
        pdbg("sys_log_level [%d]\n", g_config.sys_log_level);


        if( ssl_crt_path ) {
            strncpy( g_config.ssl_crt_path, cJSON_GetStringValue(ssl_crt_path), sizeof(g_config.ssl_crt_path)-1 );
        }
        if( ssl_key_path ) {
            strncpy( g_config.ssl_key_path, cJSON_GetStringValue(ssl_key_path), sizeof(g_config.ssl_key_path)-1 );
        }
        pdbg("ssl_crt_path [%s]\n", g_config.ssl_crt_path );
        pdbg("ssl_key_path [%s]\n", g_config.ssl_key_path);


        if(s5_mode) {
            g_config.s5_mode = s5_mode->valueint;
        }
        if(s5_serv_port) {
            g_config.s5_serv_port = s5_serv_port->valueint;
        }
        if(s5_serv_auth_path) {
            strncpy( g_config.s5_serv_auth_path, cJSON_GetStringValue(s5_serv_auth_path), sizeof(g_config.s5_serv_auth_path)-1 );
        }
        pdbg("s5_mode [%d]\n", g_config.s5_mode );
        pdbg("s5_serv_port [%d]\n", g_config.s5_serv_port);
        pdbg("s5_serv_auth_path [%s]\n", g_config.s5_serv_auth_path);


        if( s5_local_port ) {
            g_config.s5_local_port = s5_local_port->valueint;
        }
        if(s5_local_serv_port) {
            g_config.s5_local_serv_port = s5_local_serv_port->valueint;
        }
        if(s5_local_serv_ip) {
            strncpy( g_config.s5_local_serv_ip, cJSON_GetStringValue(s5_local_serv_ip), sizeof(g_config.s5_local_serv_ip)-1 );
        }
        if(s5_local_auth) {
            strncpy( g_config.s5_local_auth, cJSON_GetStringValue(s5_local_auth), sizeof(g_config.s5_local_auth)-1 );
        }
        pdbg("s5_local_port [%d]\n", g_config.s5_local_port );
        pdbg("s5_local_serv_port [%d]\n", g_config.s5_local_serv_port );
        pdbg("s5_local_serv_ip [%s]\n", g_config.s5_local_serv_ip );
        pdbg("s5_local_auth [%s]\n", g_config.s5_local_auth );


        if(http_arr) {
            for(i = 0; i < cJSON_GetArraySize(http_arr); i ++) {
                g_config.http_arr[i] = cJSON_GetArrayItem(http_arr, i)->valueint;
            }
        }
        if(https_arr) {
            for(i = 0; i < cJSON_GetArraySize(https_arr); i ++) {
                g_config.https_arr[i] = cJSON_GetArrayItem(https_arr, i)->valueint;
            }
        }
        if(http_home) {
            strncpy( g_config.http_home, cJSON_GetStringValue(http_home), sizeof(g_config.http_home)-1 );
        }
        if(http_index) {
            strncpy( g_config.http_index, cJSON_GetStringValue(http_index), sizeof(g_config.http_index)-1 );
        }
        g_config.http_num = cJSON_GetArraySize(http_arr);
        for(i = 0; i < g_config.http_num; i ++) {
            pdbg("[%d]\n", g_config.http_arr[i] );
        }
        g_config.https_num = cJSON_GetArraySize(https_arr);
        for(i = 0; i < g_config.https_num; i ++) {
            pdbg("[%d]\n", g_config.https_arr[i] );
        }
        pdbg("http_home [%s]\n", g_config.http_home );
        pdbg("http_index [%s]\n", g_config.http_index );
        
        cJSON_Delete(root);
    } else {
        err("cjson parse config file failed\n");
        return ERROR;
    }
    return OK;
}

inline config_t * config_get(  )
{
    return &g_config;
}

/// @brief read and parse config file form file
/// @param  
/// @return 
status config_init ( void )
{
    memset( &g_config, 0, sizeof(config_t) );
    g_config.sys_log_level = 0xff; /// full level
    struct stat fstat;
    meta_t * fmeta = NULL;
    int fsz = 0;
    int ffd = 0;
    int rc = 0;

    memset( &fstat, 0, sizeof(struct stat) );
    do {
        if( stat( S5_PATH_CFG, &fstat ) < 0 ) {
            err("config stat file failed. [%d]\n", errno );
            break;
        }
        fsz = (int)fstat.st_size;
        /// need to fsz+1 to storge '\0'
        if( OK != meta_alloc( &fmeta, fsz+1 ) ) {
            pdbg("config alloc meta to storge failed. [%d]\n", errno );
            break;
        }
        ffd = open( S5_PATH_CFG, O_RDONLY );
        if( ffd <= 0 ) {
            pdbg("config open file failed. [%d]\n", errno );
            break;
        }

        while( fmeta->last - fmeta->pos < fsz ) {
            rc = read( ffd, fmeta->last, fmeta->end - fmeta->last );
            if( rc <= 0 ) {
                pdbg("config read file failed. [%d]\n", errno );
                break;
            }
            fmeta->last += rc;
        }while(0);

        pdbg("===== config start =====\n");
        pdbg("%s", fmeta->pos );
        pdbg("===== config end =====\n");
        if( OK != config_parse( (char*)fmeta->pos ) ) {
            pdbg("config parse failed\n");
            break;
        }

    } while(0);

    if( ffd )
        close(ffd);

    if(fmeta)
        meta_free(fmeta);

    return OK;
}

status config_end ( void )
{
	return OK;
}
