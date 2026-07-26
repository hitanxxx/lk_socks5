#include "common.h"

static config_t g_config = {0};

inline config_t *config_get() { return &g_config; }

static void config_info(void) {
    ahead_dbg("----------------------\n");
    ahead_dbg("| daemon enable:     [%s]\n", g_config.sys_daemon ? "enabled" : "disabled");
    ahead_dbg("| process number:    [%d]\n",  g_config.sys_process_num);
    ahead_dbg("| log level:         [%s]\n", g_config.sys_log_level >= 0x2 ? "debug" : (g_config.sys_log_level >= 0x1 ? "info" : "error"));

    ahead_dbg("| work mode:         [%s]\n", g_config.s5_mode == TLS_TUNNEL_C ? "client" : (g_config.s5_mode == TLS_TUNNEL_S) ? "server" : "screct server");
    if (g_config.s5_mode == TLS_TUNNEL_C) {
        ahead_dbg("| local info:       [0.0.0.0:%d]\n", g_config.s5_local_port);
        ahead_dbg("| serv info:        [%s:%d]\n", g_config.s5_local_serv_ip, g_config.s5_local_serv_port);
        ahead_dbg("| authorization key: [%s]\n", g_config.s5_local_auth);
    } else {
        if (g_config.s5_mode == TLS_TUNNEL_S_SCRECT) {
            ahead_dbg("| serv local listne ports:\n");
            for (int i = 0; i < g_config.https_num; i++) {
                ahead_dbg("%d ", g_config.https_arr[i]);
            }
            ahead_dbg("\n");
        } else {
            ahead_dbg("| serv local listen port:  [%d]\n", g_config.s5_serv_port);
        }
        ahead_dbg("| serv gateway (for dns resolve): [%s]\n", g_config.s5_serv_gw);
        ahead_dbg("| serv authorization db: [%s]\n", g_config.s5_serv_auth_path);
    }
    ahead_dbg("----------------------\n");
    return;
}

static int config_valid(void) {
    if (g_config.sys_daemon < 0) g_config.sys_daemon = 0;
    if (g_config.sys_daemon > 1) g_config.sys_daemon = 1;
    if (g_config.sys_process_num > 8) {
        ahead_err("ezcfg. sys_process_num [%d] adjust to 8\n", g_config.sys_process_num);
        g_config.sys_process_num = 8;
    }
    if (!sys_file_exist(g_config.ssl_crt_path)) {
        ahead_err("ezcfg. s5 ssl crt empty\n");
        return -1;
    }
    if (!sys_file_exist(g_config.ssl_key_path)) {
        ahead_err("ezcfg. s5 ssl key empty\n");
        return -1;
    }
    
    if (g_config.s5_mode == TLS_TUNNEL_C) {
        if (!g_config.s5_local_port) {
            ahead_err("ezcfg. s5 Client mode. local port empty\n");
            return -1;
        }
        if (!g_config.s5_local_serv_port) {
            ahead_err("ezcfg. s5 Client mode. server port empty\n");
            return -1;
        }
        if (g_config.s5_local_serv_ip[0] == 0 ||
            inet_addr(g_config.s5_local_serv_ip) == INADDR_NONE) {
            ahead_err("ezcfg. s5 Client mode. server ip illegal\n");
            return -1;
        }
        if (g_config.s5_local_auth[0] == 0) {
            ahead_err("ezcfg. s5 Client mode. auth info enpty\n");
            return -1;
        }
    } else if (g_config.s5_mode == TLS_TUNNEL_S || g_config.s5_mode == TLS_TUNNEL_S_SCRECT) {
        if (g_config.s5_mode == TLS_TUNNEL_S_SCRECT) {
            if (g_config.https_num < 1) {
                ahead_err("ezcfg. s5 Serv screct mode. no https port\n");
                return -1;
            }
        } else {
            if (!g_config.s5_serv_port) {
                ahead_err("ezccfg. s5 Serv mode. server port illegal\n");
                return -1;
            }
        }
        if (g_config.s5_serv_auth_path[0] == 0) {
            ahead_err("ezcfg. s5 Serv mode. auth path illegal\n");
            return -1;
        }
        if (g_config.s5_serv_gw[0] == 0) {
            ahead_err("ezcfg. s5 Serv mode. gw ip illegal\n");
            return -1;
        }
    }else {
        ahead_err("ezcfg. s5_mode not support yet (Client/Server/Server_in_screct_mode)\n");
        return -1;
    }
    return 0;
}

static int config_parse(char *str) {
    int i = 0;
    cJSON *root = cJSON_Parse(str);
    if (root) {
        cJSON *sys_daemon = cJSON_GetObjectItem(root, "sys_daemon");
        cJSON *sys_process = cJSON_GetObjectItem(root, "sys_process");
        cJSON *sys_loglevel = cJSON_GetObjectItem(root, "sys_log_level");
        if (sys_daemon) g_config.sys_daemon = sys_daemon->valueint;
        if (sys_process) g_config.sys_process_num = sys_process->valueint;
        if (sys_loglevel) g_config.sys_log_level = sys_loglevel->valueint;

        cJSON *ssl_crt_path = cJSON_GetObjectItem(root, "ssl_crt_path");
        cJSON *ssl_key_path = cJSON_GetObjectItem(root, "ssl_key_path");
        if (ssl_crt_path) strncpy(g_config.ssl_crt_path, cJSON_GetStringValue(ssl_crt_path), sizeof(g_config.ssl_crt_path) - 1);
        if (ssl_key_path) strncpy(g_config.ssl_key_path, cJSON_GetStringValue(ssl_key_path), sizeof(g_config.ssl_key_path) - 1);

        cJSON *s5_mode = cJSON_GetObjectItem(root, "s5_mode");
        cJSON *s5_serv_port = cJSON_GetObjectItem(root, "s5_serv_port");
        cJSON *s5_serv_auth_path = cJSON_GetObjectItem(root, "s5_serv_auth_path");
        cJSON *s5_serv_gw = cJSON_GetObjectItem(root, "s5_serv_gw");
        if (s5_mode) g_config.s5_mode = s5_mode->valueint;
        if (s5_serv_port) g_config.s5_serv_port = s5_serv_port->valueint;
        if (s5_serv_auth_path) strncpy(g_config.s5_serv_auth_path, cJSON_GetStringValue(s5_serv_auth_path), sizeof(g_config.s5_serv_auth_path) - 1);
        if (s5_serv_gw) strncpy(g_config.s5_serv_gw, cJSON_GetStringValue(s5_serv_gw), sizeof(g_config.s5_serv_gw) - 1);

        cJSON *s5_local_port = cJSON_GetObjectItem(root, "s5_local_port");
        cJSON *s5_local_serv_port = cJSON_GetObjectItem(root, "s5_local_serv_port");
        cJSON *s5_local_serv_ip = cJSON_GetObjectItem(root, "s5_local_serv_ip");
        cJSON *s5_local_auth = cJSON_GetObjectItem(root, "s5_local_auth");
        if (s5_local_port) g_config.s5_local_port = s5_local_port->valueint;
        if (s5_local_serv_port) g_config.s5_local_serv_port = s5_local_serv_port->valueint;
        if (s5_local_serv_ip) strncpy(g_config.s5_local_serv_ip, cJSON_GetStringValue(s5_local_serv_ip), sizeof(g_config.s5_local_serv_ip) - 1);
        if (s5_local_auth) strncpy(g_config.s5_local_auth, cJSON_GetStringValue(s5_local_auth), sizeof(g_config.s5_local_auth) - 1);

        cJSON *http_arr = cJSON_GetObjectItem(root, "http_arr");
        cJSON *https_arr = cJSON_GetObjectItem(root, "https_arr");
        cJSON *http_home = cJSON_GetObjectItem(root, "http_home");
        cJSON *http_index = cJSON_GetObjectItem(root, "http_index");
        if (http_arr) {
            for (i = 0; i < cJSON_GetArraySize(http_arr); i++)
                g_config.http_arr[i] = cJSON_GetArrayItem(http_arr, i)->valueint;
        }
        if (https_arr) {
            for (i = 0; i < cJSON_GetArraySize(https_arr); i++)
                g_config.https_arr[i] = cJSON_GetArrayItem(https_arr, i)->valueint;
        }
        if (http_home)
            strncpy(g_config.http_home, cJSON_GetStringValue(http_home), sizeof(g_config.http_home) - 1);
        if (http_index)
            strncpy(g_config.http_index, cJSON_GetStringValue(http_index), sizeof(g_config.http_index) - 1);

        g_config.http_num = cJSON_GetArraySize(http_arr);
        g_config.https_num = cJSON_GetArraySize(https_arr);

        cJSON_Delete(root);
    } else {
        ahead_err("cjson parse config file failed\n");
        return -1;
    }

    return 0;
}

int config_init(void) {
    memset(&g_config, 0, sizeof(config_t));
    g_config.sys_log_level = 0xff; /// full level

    int ret = 0;
    sys_data_t *fdata = sys_file_read_value(S5_PATH_CFG);
    if (fdata) {
        ///ahead_dbg("config string:\n");
        ///ahead_dbg("%s", fdata->data);
        if (0 != config_parse(fdata->data)) ret = -1;
        if (0 != config_valid()) ret = -1;
        config_info();
        sys_free(fdata);
    }
    return ret;
}

