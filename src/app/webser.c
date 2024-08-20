#include "common.h"
#include "web_api.h"
#include "dns.h"
#include "s5_server.h"
#include "http_req.h"
#include "http_payload.h"
#include "webser.h"


typedef struct
{
    mem_arr_t * g_api_list;
    ezhash_t * mime_hash;
} g_web_t;
static g_web_t * g_web_ctx = NULL;

static int webser_start(event_t * ev);

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
        if(strlen(api_entry->key) != web->http_req->uri.len)
            continue;

        if(strncmp(api_entry->key, (char*)web->http_req->uri.data, web->http_req->uri.len))
            continue;

        if(web->http_req->method_typ != api_entry->method)
            continue;

        web->api = api_entry;
        return 0;
    }
    return -1;
}

int webser_api_reg(char * key, event_pt cb, enum http_process_status method_type, char http_req_body_need)
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

static int webser_alloc(webser_t ** webser)
{
    webser_t * nweb = mem_pool_alloc(sizeof(webser_t));
    schk(nweb, return -1);
    *webser = nweb;
    return 0;
}

static int webser_over(webser_t * webser)
{
    /// different with webser_free -> not free connection
    /// used for http keepalive. free webser but not free conection
	
	webser->data = NULL;
    webser->type = 0;

    if(webser->http_req) {
        http_req_ctx_exit(webser->http_req);
    }
    
    if(webser->http_payload) {
        http_payload_ctx_exit(webser->http_payload);
    }
    
    if(webser->ffd) {
        close(webser->ffd);
    }
     
    webser->fsize = 0;
    webser->fsend = 0;

    webser->http_rsp_mime = NULL;
    webser->http_rsp_code = 0;

    meta_free(webser->rsp_meta_header);
    meta_free(webser->rsp_meta_body);
	
    mem_pool_free(webser);
    return 0;
}

int webser_free(webser_t * webser)
{
    net_free(webser->c);
    webser_over(webser);
    return 0;
}

inline static void webser_timeout_con(void * data)
{
    net_free((con_t*)data);
}

inline void webser_timeout_cycle(void * data)
{
    webser_free((webser_t*)data);
}

static int webser_try_read(event_t * ev)
{
    con_t * c = ev->data;
    webser_t * webser = c->data;
    char buf[1] = {0};

    int n = recv(c->fd, buf, 1, MSG_PEEK);
    if(n < 0) {
        if((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
            return -11;
        }
        err("peek recv err. <%d:%s>\n", errno, strerror(errno));
        webser_free(webser);
        return -1;
    }
    if (n == 1) {
        return 0;
    }
    err("peek recv peer closed\n");
    webser_free(webser);
    return -1;
}

static int webser_keepalive(event_t * ev)
{
    con_t* c = ev->data;
    webser_t * webser = c->data;

    ///process pipeline 
    if(meta_getlen(c->meta) > 0) {
        memcpy(c->meta->start, c->meta->pos, meta_getlen(c->meta));
    }
    meta_clr(c->meta);
    c->meta->last += meta_getlen(c->meta);
    ///end of process pipeline
    webser_over(webser);
    
    ev->write_pt = NULL;
    ev->read_pt = webser_start;
    event_opt(ev, c->fd, EV_R);
    return ev->read_pt(ev);
}

void webser_rsp_mime(webser_t * webser, char * mimetype)
{
    webser->http_rsp_mime = ezhash_find(g_web_ctx->mime_hash, mimetype, strlen(mimetype));
}

static int webser_rsp_payload_send_api(event_t * ev)
{	
    con_t * c = ev->data;
    webser_t * webser = c->data;
    
    if(webser->rsp_meta_body) {
        int rc = c->send_chain(c, webser->rsp_meta_body);
        if(rc < 0)  {
            if(rc == -1) {
                err("webser send resp body failed\n");
                webser_free(webser);
                return -1;
            }
            timer_set_data(&ev->timer, webser);
            timer_set_pt(&ev->timer, webser_timeout_cycle);
            timer_add(&ev->timer, WEBSER_TIMEOUT);
            return -11;
        }
    }
    timer_del(&ev->timer);
    
    if(webser->http_req->fkeepalive) {
        ev->write_pt = webser_keepalive;
        return ev->write_pt(ev);
    }
    return webser_free(webser);
}

static int webser_rsp_payload_send_ff(event_t * ev)
{	   
    con_t * c = ev->data;
    webser_t * webser = c->data;
    meta_t * meta = webser->rsp_meta_body;

    for(;;) {
        if(meta_getlen(meta) > 0) {
            int sendn = c->send(c, meta->pos, meta_getlen(meta));
            if(sendn < 0) {
                if(sendn == -1) {
                    err("webser file content send error\n");
                    webser_free(webser);
                    return -1;
                }
                timer_add(&ev->timer, WEBSER_TIMEOUT);
                return -11;
            }
            meta->pos += sendn;
            webser->fsend += sendn;
            if(webser->fsend >= webser->fsize) {
				break;
			}
        }
        if(meta_getlen(meta) == 0) meta_clr(meta);
        
        if(meta_getfree(meta) > 0) {
            int readn = read(webser->ffd, meta->last, meta_getfree(meta));
            schk(readn > 0, {webser_free(webser); return -1;});
            meta->last += readn;
        }
    }
    timer_del(&ev->timer);
    
    if(webser->http_req->fkeepalive) {
        ev->write_pt = webser_keepalive;
        return ev->write_pt(ev);
    }
    return webser_free(webser);
}

static int webser_rsp_payload_send(event_t * ev)
{
    con_t * c = ev->data;
    webser_t * webser = c->data;

    if(webser->type == WEBSER_FILE) {
        schk(0 == meta_alloc(&webser->rsp_meta_body, WEBSER_BODY_META_LENGTH-sizeof(meta_t)), {webser_free(webser); return -1;});
        ev->write_pt = webser_rsp_payload_send_ff;
        return ev->write_pt(ev);
    } else if (webser->type == WEBSER_API) {
        ev->write_pt = webser_rsp_payload_send_api;
        return ev->write_pt(ev);
    } 
	err("webser type not support. [%d]\n", webser->type);
	webser_free(webser);
	return -1;
}

/// @brief for easy add http rsp payload string 
/// @param webser 
/// @param str 
int webser_rsp_payload_push(webser_t * webser, const char * str, ...)
{
    va_list argslist;
    const char * args = str;
    char buf[4096] = {0};

	va_start(argslist, str);
    vsnprintf(buf, sizeof(buf)-1, args, argslist);
    va_end(argslist);

	if(!webser->rsp_meta_body) {
        schk(meta_alloc(&webser->rsp_meta_body, 4096) == 0, {webser_free(webser);return -1;});
	}
    
    meta_t * p = NULL;
    meta_t * n = NULL;
	meta_t * m = webser->rsp_meta_body;
	while(m) {
		p = m;
		if(meta_getfree(m) > strlen(buf)) {
			memcpy(m->last, buf, strlen(buf));
			m->last += strlen(buf);
			return 0;
		}
		m = m->next;
	}
    schk(meta_alloc(&n, 4096) == 0, {webser_free(webser);return -1;});
	p->next = n;
	memcpy(n->last, buf, strlen(buf));
	n->last += strlen(buf);
	return 0;
}

static int webser_rsp_header_send(event_t * ev)
{
    con_t * c = ev->data;
    webser_t * webser = c->data;
    int fpayload = 0;

    int rc = c->send_chain(c, webser->rsp_meta_header);
    if(rc < 0) {
        if(rc == -1) {
            err("webser resp head send failed\n");
            webser_free(webser);
            return -1;
        }
        timer_set_data(&ev->timer, webser);
        timer_set_pt(&ev->timer, webser_timeout_cycle);
        timer_add(&ev->timer, WEBSER_TIMEOUT);
        return -11;
    }
    timer_del(&ev->timer);

	fpayload = (webser->type == WEBSER_FILE ? (webser->fsize > 0 ? 1 : 0) : (webser->rsp_meta_body ? 1 : 0));
    if(fpayload) {
        ev->write_pt = webser_rsp_payload_send;
        return ev->write_pt(ev);
    }
    if(webser->http_req->fkeepalive) {
        ev->write_pt = webser_keepalive;
        return ev->write_pt(ev);
    }
    return webser_free(webser);
}

/// @brief for easy add http rsp header string 
/// @param webser 
/// @param str 
int webser_rsp_header_push(webser_t * webser, const char * str, ...) 
{
    va_list argslist;
    const char * args = str;
    char buf[4096] = {0};

	va_start(argslist, str);
    vsnprintf(buf, sizeof(buf)-1, args, argslist);
    va_end(argslist);

	if(!webser->rsp_meta_header) {
        schk(meta_alloc(&webser->rsp_meta_header, 4096) == 0, {webser_free(webser);return -1;});		
	}

    meta_t * p = NULL;
    meta_t * n = NULL;
	meta_t * m = webser->rsp_meta_header;
	while(m) {
		p = m;
		if(meta_getfree(m) > strlen(buf)) {
			memcpy(m->last, buf, strlen(buf));
			m->last += strlen(buf);
			return 0;
		}
		m = m->next;
	}
    schk(meta_alloc(&n, 4096) == 0, {webser_free(webser);return -1;});
	p->next = n;
	memcpy(n->last, buf, strlen(buf));
	n->last += strlen(buf);
	return 0;
}

int webser_rsp_send(event_t * ev)
{
    con_t * c = ev->data;
	webser_t * webser = c->data;
	
	if(webser->http_rsp_code == 200) {
		webser_rsp_header_push(webser, "HTTP/1.1 200 OK\r\n");
	} else if (webser->http_rsp_code == 400) {
		webser_rsp_header_push(webser, "HTTP/1.1 400 Bad Request\r\n");
	} else if (webser->http_rsp_code == 404) {
		webser_rsp_header_push(webser, "HTTP/1.1 404 Not Found\r\n");
	} else if (webser->http_rsp_code == 403) {
		webser_rsp_header_push(webser, "HTTP/1.1 403 Forbidden\r\n");
	} else if (webser->http_rsp_code == 500) {
		webser_rsp_header_push(webser, "HTTP/1.1 500 Internal Server Error\r\n");
	} else {
		webser_rsp_header_push(webser, "HTTP/1.1 400 Bad Request\r\n");
	}
    webser_rsp_header_push(webser, "Server: SSSSS\r\n");
    webser_rsp_header_push(webser, "Accept-Charset: utf-8\r\n");
    webser_rsp_header_push(webser, "Date: %s\r\n", systime_gmt());	
    webser_rsp_header_push(webser, "Content-type: %s\r\n", webser->http_rsp_mime ? webser->http_rsp_mime : "application/octet-stream");
    webser_rsp_header_push(webser, "Connection: %s\r\n", webser->http_req->fkeepalive ? "keep-alive" : "close");
    webser_rsp_header_push(webser, "Content-Length: %d\r\n", (webser->type == WEBSER_FILE) ? webser->fsize : meta_getlens(webser->rsp_meta_body));
    webser_rsp_header_push(webser, "\r\n");
	
    ev->read_pt = webser_try_read;
    ev->write_pt = webser_rsp_header_send;
    event_opt(ev, c->fd, EV_R|EV_W);
    return ev->write_pt(ev);
}

int webser_rsp_code(webser_t * webser, int http_code)
{
    webser->http_rsp_code = http_code;
    return 0;
}

static int webser_req_file(event_t * ev)
{
    con_t * c = ev->data;
    webser_t * webser = c->data;
    struct stat st;
    int http_code = 400;

    char p[2048] = {0};
    char * s = p;
    char * e = p + sizeof(p);
    
    s += snprintf(s, e-s, "%s", config_get()->http_home);
    if(webser->http_req->uri.len > 0) {
        s += snprintf(s, e-s, "%.*s", webser->http_req->uri.len, webser->http_req->uri.data);
        if(webser->http_req->uri.data[webser->http_req->uri.len -1] == '/') 
            s += snprintf(s, e-s, "%s", config_get()->http_index);
    } else {
        s += snprintf(s, e-s, "/%s", config_get()->http_index);
    }
    
    char * postfix = strrchr(p, '.');
    if(postfix) {
        webser_rsp_mime(webser, postfix);
    }
    webser->fsize = webser->fsend = 0;
    
    if(0 != stat(p, &st)) {
        err("webser stat check file [%s] failed, [%d]\n", p, errno);
        http_code = 400;
    } else {
        if(st.st_mode & S_IFMT) {
            if(st.st_mode & S_IRUSR) {
                webser->ffd = open(p, O_RDONLY);
                if(-1 == webser->ffd) {
                    err("webser open static file failed [%s], errno [%d]\n", p, errno);
                    http_code = 500;
                }
                http_code = 200;
                webser->fsize = st.st_size;
            } else {
                http_code = 403;
            }
        } else {
            http_code = 404;
        }
    }

   	webser_rsp_code(webser, http_code);
    ev->read_pt = webser_rsp_send;
    return ev->read_pt(ev);
}

int webser_req_api(event_t * ev)
{
    con_t * c = ev->data;
    webser_t * webser = c->data;
    return webser->api->handler(ev);
}

static int webser_req_router(event_t * ev)
{
    con_t * c = ev->data;
    webser_t * webser = c->data;

    int rc = webser_api_find(webser);
    if(rc == 0) {
        webser->type = WEBSER_API;
        ev->read_pt = webser_req_api; 
    } else {
        ///static request only support GET method
        schk(webser->http_req->method_typ == HTTP_METHOD_GET, {webser_free(webser);return -1;});
        webser->type = WEBSER_FILE;
        ev->read_pt = webser_req_file;
    }
    return ev->read_pt(ev);
}

int webser_req_payload(webser_t * web)
{	
    if(!web->http_payload) {
        schk(http_payload_ctx_init(web->c, &web->http_payload, (web->api->body_need == 1) ? 1 : 0) == 0, return -1);
        web->http_payload->fchunk = web->http_req->payload_typ == HTTP_BODY_TYPE_CHUNK ? 1 : 0;
        if(!web->http_payload->fchunk) web->http_payload->ilen = web->http_req->payload_contentn;
    }
    int rc = web->http_payload->cb(web->http_payload);
    if(rc == -1) {
        err("webser payload proc failed\n");
        return rc;
    } else if (rc == -11) {
        return -11;
    }
    return 1;
}

static int webser_req_header(event_t * ev)
{
    con_t * c = ev->data;
    webser_t * webser = c->data;

    int rc = webser->http_req->cb(webser->http_req);
    if(rc < 0) {
        if(rc == -1) {
            err("webser process request header failed\n");
            webser_free(webser);
            return -1;
        }
        timer_set_data(&ev->timer, webser);
        timer_set_pt(&ev->timer, webser_timeout_cycle);
        timer_add(&ev->timer, WEBSER_TIMEOUT);
        return -11;
    }
    timer_del(&ev->timer);

    /// if don't have body, excute down 
    ev->read_pt	= webser_req_router;
    return ev->read_pt(ev);
}

static int webser_start(event_t * ev)
{
    con_t * c = ev->data;
    webser_t * webser = NULL;

    ///init web connection page and meta memory for use
    if(!c->meta) {
        schk(meta_alloc(&c->meta, 4096) == 0, {net_free(c); return -1;});
    }
    schk(webser_alloc(&webser) == 0, {net_free(c); return -1;});
    webser->c = c;
    c->data = webser;

    ///start http request parse, try to read http request form socket and parse it 
    schk(http_req_ctx_init(c, &webser->http_req) == 0, {webser_free(webser); return -1;});
    ev->write_pt = NULL;
    ev->read_pt = webser_req_header;
    return ev->read_pt(ev);
}

/// @brief  transfer request to s5 module 
/// @param ev 
/// @return 
static int webser_transfer_to_s5(event_t * ev)
{
    con_t * c = ev->data;
    meta_t * meta = NULL;
    ssize_t rc = 0;
    
    if(!c->meta) {
        schk(meta_alloc(&c->meta, 8192) == 0, {net_free(c);return -1;});
    }

    /// try to recv s5 private authorization header 
    meta = c->meta;
    while(meta_getlen(meta) < sizeof(s5_auth_t)) {
        rc = c->recv(c, meta->last, meta_getfree(meta));
        if(rc < 0) {
            if(rc == -1) {
                err("webser procotol route recv failed\n");
                net_free(c);
                return -1;
            }
            timer_set_data(&ev->timer, c);
            timer_set_pt(&ev->timer, webser_timeout_con);
            timer_add(&ev->timer, WEBSER_TIMEOUT);
            return -11;
        }
        meta->last += rc;
    }
    timer_del(&ev->timer);

    s5_auth_t * header = (s5_auth_t*)meta->pos;
    if(htonl(S5_AUTH_MAGIC) != header->magic) {
	    ev->read_pt = webser_start;
    	return ev->read_pt(ev);
    } 
    ev->read_pt = s5_srv_transport;
    return ev->read_pt(ev);
}

static int webser_accept_cb_ssl_check(event_t * ev)
{
    con_t * c = ev->data;
    schk(c->ssl->f_handshaked, {net_free(c); return -1;});
    timer_del(&c->event->timer);

    c->recv = ssl_read;
    c->send = ssl_write;
    c->send_chain = ssl_write_chain;

    ev->write_pt = NULL;
    ev->read_pt = (config_get()->s5_mode == SOCKS5_SERVER_SECRET ? webser_transfer_to_s5 : webser_start);
    return ev->read_pt(ev);
}

int webser_accept_cb_ssl(event_t * ev)
{
    con_t * c = ev->data;
    int rc = net_check_ssl_valid(c);
    if(rc != 0) {
        if(rc == -11) {
            timer_set_data(&ev->timer, c);
            timer_set_pt(&ev->timer, webser_timeout_con);
            timer_add(&ev->timer, WEBSER_TIMEOUT);
            return -11;
        }
        err("webser check net ssl failed\n");
        net_free(c);
        return -1;
    }
    schk(ssl_create_connection(c, L_SSL_SERVER) == 0, {net_free(c); return -1;});

    rc = ssl_handshake(c->ssl);
    if(rc < 0) {
        if(rc == -11) {
            c->ssl->cb = webser_accept_cb_ssl_check;
            timer_set_data(&ev->timer, c);
            timer_set_pt(&ev->timer, webser_timeout_con);
            timer_add(&ev->timer, WEBSER_TIMEOUT);
            return -11;
        }
        err("webser ssl handshake failed\n");
        net_free(c);
        return -1;
    }
    return webser_accept_cb_ssl_check(ev);
}

int webser_accept_cb(event_t * ev)
{
    ev->read_pt = webser_start;
    return ev->read_pt(ev);
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
