#include "common.h"
#include "web_api.h"
#include "dns.h"
#include "tls_tunnel_s.h"
#include "http_req.h"
#include "webser.h"


typedef struct {
    mem_arr_t * g_api_list;
    ezhash_t * mime_hash;
} g_web_t;
static g_web_t * g_web_ctx = NULL;

static int webser_start(con_t * c);

static int webser_api_find(webser_t * web)
{
    int i = 0;
    webser_api_t *api_entry = NULL;
    if(!g_web_ctx->g_api_list) {
        return ERROR;
    }
    for(i = 0; i < g_web_ctx->g_api_list->elem_num; i++) {
        api_entry = mem_arr_get(g_web_ctx->g_api_list, i + 1);
        if(!api_entry) {
            continue;
        }
		
        if(strlen(api_entry->key) != web->webreq->uri.len) {
		    continue;
		}

        if(strncmp(api_entry->key, (char*)web->webreq->uri.data, web->webreq->uri.len)) {
			continue;
		}

        if(web->webreq->method_typ != api_entry->method) {
			continue;
		}

        web->api = api_entry;
        return 0;
    }
    return -1;
}

int webser_api_reg(char * key, ev_cb cb, enum http_process_status method_type, char http_req_body_need)
{
    webser_api_t * p = mem_arr_push(g_web_ctx->g_api_list);
    schk(p, return -1);
    int keyn = 0;
    if(strlen(key) > (sizeof(p->key)-1)) {
        keyn = sizeof(key)-1;
    } else {
        keyn = strlen(key);
    }
    strncpy(p->key, key, keyn);
    p->handler = cb;
    p->method = method_type;
    p->body_need = http_req_body_need;
    return 0;
}

static int web_calloc(webser_t ** webser)
{
    webser_t * nweb = mem_pool_alloc(sizeof(webser_t));
    schk(nweb, return -1);
    *webser = nweb;
    return 0;
}

static void web_cfree(void * data)
{
    webser_t * webc = data;
    if(webc->ffd) close(webc->ffd);
      if(webc->webreq) web_req_free(webc->webreq);
    meta_free(webc->rsp_hdr);
    meta_free(webc->rsp_payload);
    
    mem_pool_free(webc);
}

inline static int web_cexp(con_t * c)
{
    return net_close(c);
}

static int webser_try_read(con_t * c)
{
    char buf[1] = {0};

    int n = recv(c->fd, buf, 1, MSG_PEEK);
    if(n < 0) {
        if((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            return -11;
        }
        err("peek recv err. <%d:%s>\n", errno, strerror(errno));
        net_close(c);
        return -1;
    }
    if (n == 1) {
        return 0;
    }
    err("peek recv peer closed\n");
    net_close(c);
    return -1;
}

static int webser_keepalive(con_t * c)
{
    webser_t * webc = c->data;

    ///process pipeline 
    int remain = meta_getlen(c->meta);
    if(remain) {
        memcpy(c->meta->start, c->meta->pos, remain);
        c->meta->pos = c->meta->start;
        c->meta->last = c->meta->pos + remain;
    } else {
        meta_clr(c->meta);
    }
    web_cfree(webc);
    
    c->ev->read_cb = webser_start;
    c->ev->write_cb = NULL;
    return c->ev->read_cb(c);
}

void * webser_rsp_mime(webser_t * webser, char * mimetype)
{
    void * typ = ezhash_find(g_web_ctx->mime_hash, mimetype, strlen(mimetype));
    return typ ? typ : "text/plain";
}

static int webser_rsp_payload_send_api(con_t * c)
{    
    webser_t * webc = c->data;
    
    if(webc->rsp_payload) {
        int rc = c->send_chain(c, webc->rsp_payload);
        if(rc < 0)  {
            if(rc == -11) {
                tm_add(c, web_cexp, WEB_TMOUT);
                return -11;
            }
            err("webser send resp body failed\n");
            net_close(c);
            return -1;
        }
    }
    tm_del(c);
    
    if(webc->webreq->fkeepalive) {
        c->ev->write_cb = webser_keepalive;
        return c->ev->write_cb(c);
    }
    net_close(c);
    return 0;
}

static int webser_rsp_payload_send_ff(con_t * c)
{       
    webser_t * webc = c->data;
    meta_t * meta = webc->rsp_payload;

    for(;;) {
        if(meta_getlen(meta) > 0) {
            int sendn = c->send(c, meta->pos, meta_getlen(meta));
            if(sendn < 0) {
                if(sendn == -11) {
                    tm_add(c, web_cexp, WEB_TMOUT);
                    return -11;
                }
                   err("webser file content send error\n");
                net_close(c);
                return -1;
            }
            meta->pos += sendn;
            webc->fsend += sendn;
        }
        if(webc->fsend >= webc->fsize) break;
        if(meta_getlen(meta) == 0) meta_clr(meta);
        
        if(meta_getfree(meta) > 0) {
            int readn = read(webc->ffd, meta->last, meta_getfree(meta));
            if(readn <= 0) {
                err("webserv rff err. [%d] [%d] [%d]\n", errno, webc->fsend, webc->fsize);
                net_close(c);
                return -1;
            }
            meta->last += readn;
        }
    }
    tm_del(c);
    
    if(webc->webreq->fkeepalive) {
        c->ev->write_cb = webser_keepalive;
        return c->ev->write_cb(c);
    }
    net_close(c);
    return 0;
}

static int webser_rsp_payload_send(con_t * c)
{
    webser_t * webc = c->data;

    if(webc->type == WEBSER_FILE) {
        schk(0 == meta_alloc(&webc->rsp_payload, WEB_PAYLOAD_LEN - sizeof(meta_t)), {net_close(c);return -1;});
        c->ev->write_cb = webser_rsp_payload_send_ff;
        return c->ev->write_cb(c);
    } else if (webc->type == WEBSER_API) {
        c->ev->write_cb = webser_rsp_payload_send_api;
        return c->ev->write_cb(c);
    } 
    err("webser type not support. [%d]\n", webc->type);
    net_close(c);
    return -1;
}

static int webser_rsp_hdr_send(con_t * c)
{
    webser_t * webc = c->data;
    int fpayload = 0;

    int rc = c->send_chain(c, webc->rsp_hdr);
    if(rc < 0) {
        if(rc == -11) {
            tm_add(c, web_cexp, WEB_TMOUT);
            return -11;
        }
        err("webser resp head send failed\n");
        net_close(c);
        return -1;
    }
    tm_del(c);

    fpayload = (webc->type == WEBSER_FILE ? (webc->fsize > 0 ? 1 : 0) : (webc->rsp_payload ? 1 : 0));
    if(fpayload) {
        c->ev->write_cb = webser_rsp_payload_send;
        return c->ev->write_cb(c);
    }
    if(webc->webreq->fkeepalive) {
        c->ev->write_cb = webser_keepalive;
        return c->ev->write_cb(c);
    }
    net_close(c);
    return 0;
}

/// @brief for easy add http rsp header string 
/// @param webser 
/// @param str 
static int webser_rsp_hdrp(con_t * c, const char * str, ...) 
{
    webser_t * webc = c->data;
    
    va_list argslist;
    const char * args = str;
    char buf[1024] = {0};

    va_start(argslist, str);
    vsnprintf(buf, sizeof(buf)-1, args, argslist);
    va_end(argslist);


    if(!webc->rsp_hdr) {
        schk(0 == meta_alloc(&webc->rsp_hdr, 4096), {net_close(c);return -1;});        
    }
    meta_t * n = webc->rsp_hdr;
    meta_t * p = NULL;
    while(n) {
        p = n;
        if(meta_getfree(n) > strlen(buf)) {
            schk(0 == meta_pdata(n, buf, strlen(buf)), return -1);
            return 0;
        }
        n = n->next;
    }
    schk(0 == meta_alloc(&n, 4096), {net_close(c);return -1;});
    schk(0 == meta_pdata(n, buf, strlen(buf)), {net_close(c);return -1;});
    p->next = n;
    return 0;
}

int webser_rsp(con_t * c, int rcode, char * exthdr, void * payload, int payloadn)
{
    webser_t * webc = c->data;
    int ret = -1;
    
    do {
        if(webc->type == WEBSER_API && payload && payloadn) {
            schk(0 == meta_alloc(&webc->rsp_payload, payloadn), break);
            schk(0 == meta_pdata(webc->rsp_payload, payload, payloadn), break);
        }

        if(rcode == 200) {
            schk(0 == webser_rsp_hdrp(c, "HTTP/1.1 200 OK\r\n"), break);
        } else if (rcode == 400) {
            schk(0 == webser_rsp_hdrp(c, "HTTP/1.1 400 Bad Request"), break);
        } else if (rcode == 403) {
            schk(0 == webser_rsp_hdrp(c, "HTTP/1.1 403 Forbidden\r\n"), break);
        } else if (rcode == 404) {
            schk(0 == webser_rsp_hdrp(c, "HTTP/1.1 404 Not Found\r\n"), break);
        } else if (rcode == 500) {
            schk(0 == webser_rsp_hdrp(c, "HTTP/1.1 500 Internal Server Error\r\n"), break);
        } else {
            schk(0 == webser_rsp_hdrp(c, "HTTP/1.1 400 Bad Request"), break);
        }
        schk(0 == webser_rsp_hdrp(c, "Server: s5\r\n"), break);
        schk(0 == webser_rsp_hdrp(c, "Date: %s\r\n", systime_gmt()), break);    
        schk(0 == webser_rsp_hdrp(c, "Connection: %s\r\n", webc->webreq->fkeepalive ? "keep-alive" : "close"), break);
        schk(0 == webser_rsp_hdrp(c, "Content-Length: %d\r\n", (webc->type == WEBSER_FILE) ? webc->fsize : meta_getlens(webc->rsp_payload)), break);
        if(exthdr) {
            schk(0 == webser_rsp_hdrp(c, "%s", exthdr), break);
        }
        schk(0 == webser_rsp_hdrp(c, "\r\n"), break);
        ret = 0;
    } while(0);

    if(ret != 0) {
        net_close(c);
        return -1;
    }
    
    c->ev->read_cb = webser_try_read;
    c->ev->write_cb = webser_rsp_hdr_send;
    return c->ev->write_cb(c);
}

static int webser_req_file(con_t * c)
{
    webser_t * webc = c->data;
    struct stat st;
    int http_code = 400;

    char p[2048] = {0};
    char * s = p;
    char * e = p + sizeof(p);
    
    s += snprintf(s, e-s, "%s", config_get()->http_home);
    if(webc->webreq->uri.len > 0) {
        s += snprintf(s, e-s, "%.*s", webc->webreq->uri.len, webc->webreq->uri.data);
        if(webc->webreq->uri.data[webc->webreq->uri.len -1] == '/') 
            s += snprintf(s, e-s, "%s", config_get()->http_index);
    } else {
        s += snprintf(s, e-s, "/%s", config_get()->http_index);
    }
    char * postfix = strrchr(p, '.');
 
    webc->fsize = webc->fsend = 0;
    if(0 != stat(p, &st)) {
        err("webser stat reqfile [%s] err, [%d]\n", p, errno);
        http_code = 400;
    } else {
        if(st.st_mode & S_IFMT) {
            if(st.st_mode & S_IRUSR) {
                webc->ffd = open(p, O_RDONLY);
                if(-1 == webc->ffd) {
                    err("webser open reqfile [%s] err, [%d]\n", p, errno);
                    http_code = 500;
                }
                http_code = 200;
                webc->fsize = st.st_size;
            } else {
                http_code = 403;
            }
        } else {
            http_code = 404;
        }
    }

    char exthdr[128] = {0};
    sprintf(exthdr, "Content-type: %s\r\n", (char*)webser_rsp_mime(webc, postfix));
    return webser_rsp(c, http_code, exthdr, NULL, 0);
}

int webser_req_api(con_t * c)
{
    webser_t * webc = c->data;
    return webc->api->handler(c);
}

static int webser_req_router(con_t * c)
{
    webser_t * webc = c->data;

    int rc = webser_api_find(webc);
    if(rc == 0) {
        webc->type = WEBSER_API;
        c->ev->read_cb = webser_req_api;
        c->ev->write_cb = NULL;
    } else {
        webc->type = WEBSER_FILE;
        schk(HTTP_METHOD_GET == webc->webreq->method_typ, 
            {net_close(c);return -1;});
        
        char uri[1024] = {0};
        memcpy(uri, webc->webreq->uri.data, webc->webreq->uri.len > 1024 ? 1024 : webc->webreq->uri.len);
        schk(!strstr(uri, "../"), 
            {net_close(c);return -1;});
        
        c->ev->read_cb = webser_req_file;
        c->ev->write_cb = NULL;
    }
    return c->ev->read_cb(c);
}

static int webser_req_proc(con_t * c)
{
    webser_t * webc = c->data;

    int rc = webc->webreq->cb(c, webc->webreq);
    if(rc < 0) {
        if(rc == -11) {
            tm_add(c, web_cexp, WEB_TMOUT);
            return -11;
        }
        err("webser process request header failed\n");
        net_close(c);
        return -1;
    }
    tm_del(c);

    /// if don't have body, excute down 
    c->ev->read_cb = webser_req_router;
    c->ev->write_cb = NULL;
    return c->ev->read_cb(c);
}

static int webser_start(con_t * c)
{
    webser_t * webc = NULL;

    ///init web connection page and meta memory for use
    schk(0 == meta_alloc(&c->meta, 4096), {net_close(c); return -1;});
    schk(0 == web_calloc(&webc), {net_close(c); return -1;});
    webc->c = c;
    c->data = webc;
    c->data_cb = web_cfree;

    ///start http request parse, try to read http request form socket and parse it 
    schk(webc->webreq = web_req_alloc(), {net_close(c); return -1;});
    
    c->ev->read_cb = webser_req_proc;
    c->ev->write_cb = NULL;
    return c->ev->read_cb(c);
}

/// @brief  transfer request to s5 module 
/// @param ev 
/// @return 
static int webser_transfer_to_tlstunnel(con_t * c)
{
    meta_t * meta = NULL;
    ssize_t rc = 0;
    
    if(!c->meta) schk(meta_alloc(&c->meta, 8192) == 0, {net_free(c);return -1;});

    /// try to recv TLS tunnel authorization header 
    meta = c->meta;
    while(meta_getlen(meta) < sizeof(tls_tunnel_auth_t)) {
        rc = c->recv(c, meta->last, meta_getfree(meta));
        if(rc < 0) {
            if(rc == -11) {
                tm_add(c, web_cexp, WEB_TMOUT);
                return -11;
            }
            if(rc == -1) {
                err("webser procotol route recv failed\n");
                net_close(c);
                return -1;
            }
        }
        meta->last += rc;
    }
    tm_del(c);

    tls_tunnel_auth_t * auth = (tls_tunnel_auth_t*)meta->pos;
    if(htonl(TLS_TUNNEL_AUTH_MAGIC_NUM) != auth->magic) {
        c->ev->read_cb = webser_start;
        c->ev->write_cb = NULL;
        return c->ev->read_cb(c);
    } 
    c->ev->read_cb = tls_tunnel_s_start;
    c->ev->write_cb = NULL;
    return c->ev->read_cb(c);
}

int webser_accept_cb_ssl(con_t * c)
{
    if(!c->ssl) schk(ssl_create_connection(c, L_SSL_SERVER) == 0, {net_close(c); return -1;});

    if(!c->ssl->f_handshaked) {
        int rc = ssl_handshake(c);
        if(rc < 0) {
            if(rc == -11) {
                tm_add(c, web_cexp, 5000);
                return -11;
            }
            err("webser ssl handshake failed\n");
            net_close(c);
            return -1;
        }
    }

    c->recv = ssl_read;
    c->send = ssl_write;
    c->send_chain = ssl_write_chain;

    c->ev->read_cb = (config_get()->s5_mode == TLS_TUNNEL_S_SCRECT ? webser_transfer_to_tlstunnel : webser_start);
    c->ev->write_cb = NULL;
    return c->ev->read_cb(c);
}

int webser_accept_cb(con_t * c)
{
    c->ev->read_cb = webser_start;
    c->ev->write_cb = NULL;
    return c->ev->read_cb(c);
}

static int webser_init_mimehash( )
{
    schk(0 == ezhash_create(&g_web_ctx->mime_hash, 97), return -1);
    
    ezhash_add(g_web_ctx->mime_hash, ".html", strlen(".html"),
        "text/html", strlen("text/html"));
    
    ezhash_add(g_web_ctx->mime_hash, ".js", strlen(".js"),
        "application/x-javascript", strlen("application/x-javascript"));
    
    ezhash_add(g_web_ctx->mime_hash, ".json", strlen(".json"),
        "application/json", strlen("application/json"));
    
    ezhash_add(g_web_ctx->mime_hash, ".png", strlen(".png"),
        "image/png", strlen("image/png"));
    
    ezhash_add(g_web_ctx->mime_hash, ".jpg", strlen(".jpg"),
        "image/jpeg", strlen("image/jpeg"));
    
    ezhash_add(g_web_ctx->mime_hash, ".jpeg", strlen(".jpeg"),
        "image/jpeg", strlen("image/jpeg"));
    
    ezhash_add(g_web_ctx->mime_hash, ".gif", strlen(".gif"),
        "image/gif", strlen("image/gif"));
    
    ezhash_add(g_web_ctx->mime_hash, ".ico", strlen(".ico"),
        "image/x-icon", strlen("image/x-icon"));
    
    ezhash_add(g_web_ctx->mime_hash, ".css", strlen(".css"),
        "text/css", strlen("text/css"));
    
    ezhash_add(g_web_ctx->mime_hash, ".txt", strlen(".txt"),
        "text/plain", strlen("text/plain"));
    
    ezhash_add(g_web_ctx->mime_hash, ".htm", strlen(".htm"),
        "text/html", strlen("text/html"));
    
    ezhash_add(g_web_ctx->mime_hash, ".mp3", strlen(".mp3"),
        "audio/mpeg", strlen("audio/mpeg"));
    
    ezhash_add(g_web_ctx->mime_hash, ".m3u8", strlen(".m3u8"),
        "application/x-mpegURL", strlen("application/x-mpegURL"));
    
    ezhash_add(g_web_ctx->mime_hash, ".ts", strlen(".ts"),
        "video/MP2T", strlen("video/MP2T"));
    return 0;
}


/// @brief webserver module init 
/// @param  
/// @return 
int webser_init(void)
{
    int ret = -1;
    schk(!g_web_ctx, return -1);

    do {
        schk(g_web_ctx = mem_pool_alloc(sizeof(g_web_t)), return -1);
        
        /// init mime hash 
        schk(webser_init_mimehash() == 0, break);
        
        /// init webserv api data
        schk(mem_arr_create(&g_web_ctx->g_api_list, sizeof(webser_api_t)) == 0, break);

        webapi_init();
        ret = 0;
    } while(0);

    if(0 != ret) {
        if(g_web_ctx) {
            if(g_web_ctx->g_api_list) {
                mem_arr_free(g_web_ctx->g_api_list);
                g_web_ctx->g_api_list = NULL;
            }
            mem_pool_free(g_web_ctx);
            g_web_ctx = 0;
        }
    }
    return (0 == ret ? 0 : -1);
}

int webser_end(void)
{
    if(g_web_ctx) {
        if(g_web_ctx->mime_hash) {
            ezhash_free(g_web_ctx->mime_hash);
            g_web_ctx->mime_hash = NULL;
        }
        
        if(g_web_ctx->g_api_list) {
            mem_arr_free(g_web_ctx->g_api_list);
            g_web_ctx->g_api_list = NULL;
        }
        mem_pool_free(g_web_ctx);
    }
    return 0;
}
