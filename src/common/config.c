#include "common.h"

static config_t g_config;

static int config_parse(char * str)
{
    int i = 0;
    cJSON * root = cJSON_Parse(str);
    if(root) {
        cJSON * sys_daemon = cJSON_GetObjectItem(root, "sys_daemon");
        cJSON * sys_process = cJSON_GetObjectItem(root, "sys_process");
        cJSON * sys_loglevel = cJSON_GetObjectItem(root, "sys_log_level");
        if(sys_daemon)
            g_config.sys_daemon = sys_daemon->valueint;
        if(sys_process)
            g_config.sys_process_num = sys_process->valueint;
        if(sys_loglevel)
            g_config.sys_log_level = sys_loglevel->valueint;
        ///dbg("sys_daemon: [%d]\n", g_config.sys_daemon);
        ///dbg("sys_process: [%d]\n", g_config.sys_process_num);
        ///dbg("sys_log_level: [%d]\n", g_config.sys_log_level);


        cJSON * ssl_crt_path = cJSON_GetObjectItem(root, "ssl_crt_path");
        cJSON * ssl_key_path = cJSON_GetObjectItem(root, "ssl_key_path");
        if(ssl_crt_path)
            strncpy(g_config.ssl_crt_path, cJSON_GetStringValue(ssl_crt_path), sizeof(g_config.ssl_crt_path)-1);
        if(ssl_key_path)
            strncpy(g_config.ssl_key_path, cJSON_GetStringValue(ssl_key_path), sizeof(g_config.ssl_key_path)-1);
        ///dbg("ssl_crt_path: [%s]\n", g_config.ssl_crt_path);
        //dbg("ssl_key_path: [%s]\n", g_config.ssl_key_path);


        cJSON * s5_mode = cJSON_GetObjectItem(root, "s5_mode");
        cJSON * s5_serv_port = cJSON_GetObjectItem(root, "s5_serv_port");
        cJSON * s5_serv_auth_path = cJSON_GetObjectItem(root, "s5_serv_auth_path");
        cJSON * s5_serv_gw = cJSON_GetObjectItem(root, "s5_serv_gw");
        if(s5_mode)
            g_config.s5_mode = s5_mode->valueint;
        if(s5_serv_port)
            g_config.s5_serv_port = s5_serv_port->valueint;
        if(s5_serv_auth_path)
            strncpy(g_config.s5_serv_auth_path, cJSON_GetStringValue(s5_serv_auth_path), sizeof(g_config.s5_serv_auth_path)-1);
        if( s5_serv_gw )
            strncpy(g_config.s5_serv_gw, cJSON_GetStringValue(s5_serv_gw), sizeof(g_config.s5_serv_gw)-1);
        ///dbg("s5_mode: [%d]\n", g_config.s5_mode);
        ///dbg("s5_serv_port: [%d]\n", g_config.s5_serv_port);
        ///dbg("s5_serv_auth_path: [%s]\n", g_config.s5_serv_auth_path);
        ///dbg("s5_serv_gw: [%s]\n", g_config.s5_serv_gw);


        cJSON * s5_local_port = cJSON_GetObjectItem(root, "s5_local_port");
        cJSON * s5_local_serv_port = cJSON_GetObjectItem(root, "s5_local_serv_port");
        cJSON * s5_local_serv_ip = cJSON_GetObjectItem(root, "s5_local_serv_ip");
        cJSON * s5_local_auth = cJSON_GetObjectItem(root, "s5_local_auth");
        if(s5_local_port)
            g_config.s5_local_port = s5_local_port->valueint;
        if(s5_local_serv_port)
            g_config.s5_local_serv_port = s5_local_serv_port->valueint;
        if(s5_local_serv_ip)
            strncpy(g_config.s5_local_serv_ip, cJSON_GetStringValue(s5_local_serv_ip), sizeof(g_config.s5_local_serv_ip)-1);
        if(s5_local_auth)
            strncpy(g_config.s5_local_auth, cJSON_GetStringValue(s5_local_auth), sizeof(g_config.s5_local_auth)-1);
        ///dbg("s5_local_port: [%d]\n", g_config.s5_local_port);
        ///dbg("s5_local_serv_port: [%d]\n", g_config.s5_local_serv_port);
        ///dbg("s5_local_serv_ip: [%s]\n", g_config.s5_local_serv_ip);
        //dbg("s5_local_auth: [%s]\n", g_config.s5_local_auth);
        

        cJSON * http_arr = cJSON_GetObjectItem(root, "http_arr");
        cJSON * https_arr = cJSON_GetObjectItem(root, "https_arr");
        cJSON * http_home = cJSON_GetObjectItem(root, "http_home");
        cJSON * http_index = cJSON_GetObjectItem(root, "http_index");
        if(http_arr) {
            for(i = 0; i < cJSON_GetArraySize(http_arr); i++)
                g_config.http_arr[i] = cJSON_GetArrayItem(http_arr, i)->valueint;
        }
        if(https_arr) {
            for(i = 0; i < cJSON_GetArraySize(https_arr); i++)
                g_config.https_arr[i] = cJSON_GetArrayItem(https_arr, i)->valueint;
        }
        if(http_home)
            strncpy(g_config.http_home, cJSON_GetStringValue(http_home), sizeof(g_config.http_home)-1);
        if(http_index)
            strncpy(g_config.http_index, cJSON_GetStringValue(http_index), sizeof(g_config.http_index)-1);
        g_config.http_num = cJSON_GetArraySize(http_arr);
        ///for(i = 0; i < g_config.http_num; i++) 
        ///    dbg("[%d]\n", g_config.http_arr[i]);
            
        g_config.https_num = cJSON_GetArraySize(https_arr);
        ///for(i = 0; i < g_config.https_num; i++)
        ///    dbg("[%d]\n", g_config.https_arr[i]);
            
        ///dbg("http_home: [%s]\n", g_config.http_home);
        ///dbg("http_index: [%s]\n", g_config.http_index);
        cJSON_Delete(root);
    } else {
        err("cjson parse config file failed\n");
        return -1;
    }
    return 0;
}

inline config_t * config_get()
{
    return &g_config;
}

/// @brief read and parse config file form file
/// @param  
/// @return 
int config_init(void)
{
    memset(&g_config, 0, sizeof(config_t));
    g_config.sys_log_level = 0xff; ///full level
    struct stat fstat;
    int fsz = 0;
    int ffd = 0;
    char * fbuf = NULL;
    int freadn = 0;
    int rc = 0;

    memset(&fstat, 0, sizeof(struct stat));
    do {
        schk(0 == stat(S5_PATH_CFG, &fstat), break);
        fsz = (int)fstat.st_size;
        fbuf = sys_alloc(fsz + 1); /// need to fsz+1 to storge '\0'
        schk(fbuf, break);
        
        ffd = open(S5_PATH_CFG, O_RDONLY);
        schk(ffd > 0, break);
        while(freadn < fsz) {
            rc = read(ffd, fbuf + freadn, fsz + 1 - freadn);
            schk(rc > 0, break);
            freadn += rc;
        }

        ahead_dbg("config string:\n");
        ahead_dbg("%s", fbuf);
        schk(0 == config_parse(fbuf), break);
    } while(0);
    if(ffd) 
        close(ffd);
    if(fbuf) 
        sys_free(fbuf);
    return 0;
}

int config_end(void)
{
    return OK;
}
