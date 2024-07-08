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
    if(!p) {
        err("webser g_api_list mem list push failed\n");
        return -1;
    }
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
    if(!nweb) {
        err("nweb alloc failed\n");
        return -1;
    }
    *webser = nweb;
    return 0;
}

static int webser_over(webser_t * webser)
{
    /// different with webser_free -> not free connection
    /// used for http keepalive. free webser but not free conection
	meta_t * m = NULL;
    meta_t * n = NULL;
	
	webser->data = NULL;
    webser->type = 0;

    if(webser->http_req) {
        http_req_ctx_exit(webser->http_req);
        webser->http_req = NULL;
    }
    if(webser->http_payload) {
        http_payload_ctx_exit(webser->http_payload);
        webser->http_payload = NULL;
    }
    webser->api = NULL;
  
    if(webser->ffd) {
        close(webser->ffd);
        webser->ffd = 0;
    }
    webser->fsize = 0;
    webser->fsend = 0;

    webser->http_rsp_mime = NULL;
    webser->http_rsp_code = 0;

	m = webser->rsp_meta_header;
	while(m) {
		n = m->next;
		meta_free(m);
		m = n;
	}
	m = webser->rsp_meta_body;
	while(m) {
		n = m->next;
		meta_free(m);
		m = n;
	}
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

    if(meta_getlen(c->meta) > 0) {
        memcpy(c->meta->start, c->meta->pos, meta_getlen(c->meta));
    }
    meta_clr(c->meta);
    c->meta->last += meta_getlen(c->meta);
    webser_over(webser);
    
    ev->write_pt = NULL;
    ev->read_pt = webser_start;
    event_opt(ev, c->fd, EV_R);
    return ev->read_pt(ev);
}


static char* webser_rsp_mime_find(unsigned char * str, int len)
{
    if(!str && !len) {
        return "Content-type: application/octet-stream\r\n";
    } else {
        return ezhash_find(g_web_ctx->mime_hash, (char*)str);
    }
}

void webser_rsp_mime(webser_t * webser, char * mimetype)
{
    webser->http_rsp_mime = webser_rsp_mime_find((unsigned char*)mimetype, l_strlen(mimetype));
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
    
    if(webser->http_rsp_code == 200 && webser->http_req->fkeepalive) {
        ev->write_pt = webser_keepalive;
        return ev->write_pt(ev);
    }
    webser_free(webser);
    return 0;
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
            if(readn <= 0) {
                err("webser read file, errno [%d]\n", errno );
                webser_free(webser);
                return -1;
            }
            meta->last += readn;
        }
    }
    timer_del(&ev->timer);
    
    if(webser->http_rsp_code == 200 && webser->http_req->fkeepalive) {
        ev->write_pt = webser_keepalive;
        return ev->write_pt(ev);
    } 
    webser_free(webser);
    return 0;
}

static int webser_rsp_payload_send(event_t * ev)
{
    con_t * c = ev->data;
    webser_t * webser = c->data;

    if(webser->type == WEBSER_FILE) {
        if(0 != meta_alloc(&webser->rsp_meta_body, WEBSER_BODY_META_LENGTH-sizeof(meta_t))) { ///for mem pool alloc sign 
            err("webser rsp body meta alloc failed\n");
            webser_free(webser);
            return -1;
        }
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
		if(0 != meta_alloc(&webser->rsp_meta_body, 4096)) {
			err("meta alloc err\n");
			webser_free(webser);
			return -1;
		}
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
	if(0 != meta_alloc(&n, 4096)) {
		err("meta alloc err\n");
		webser_free(webser);
		return -1;
	}
	p->next = n;
	memcpy(n->last, buf, strlen(buf));
	n->last += strlen(buf);
	return 0;
}

static int webser_rsp_header_send(event_t * ev)
{
    con_t * c = ev->data;
    webser_t * webser = c->data;

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

	int fpayload = 0;
	if(webser->type == WEBSER_FILE) {
		if(webser->fsize > 0) {
            fpayload = 1;
        }
	} else if(webser->type == WEBSER_API) {
		if(webser->rsp_meta_body) {
            fpayload = 1;
        } 
	}
	if(!fpayload) {
        if(webser->http_rsp_code == 200 && webser->http_req->fkeepalive) {
            ev->write_pt = webser_keepalive;
            return ev->write_pt(ev);
        }
        webser_free(webser);
        return 0;
    }
    ev->write_pt = webser_rsp_payload_send;
    return ev->write_pt(ev);
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
		if(0 != meta_alloc(&webser->rsp_meta_header, 4096)) {
			err("meta alloc err\n");
			webser_free(webser);
			return -1;
		}
	}

    meta_t * p = NULL;
    meta_t * n = NULL;
	meta_t * m = webser->rsp_meta_header;
	while(m) {
		p = m;
		int meta_freen = m->end - m->last;
		if(meta_freen > strlen(buf)) {
			memcpy(m->last, buf, strlen(buf));
			m->last += strlen(buf);
			return 0;
		}
		m = m->next;
	}
	if(0 != meta_alloc(&n, 4096)) {
		err("meta alloc err\n");
		webser_free(webser);
		return -1;
	}
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
    webser_rsp_header_push(webser, webser->http_rsp_mime ? webser->http_rsp_mime : "Content-type: application/octet-stream\r\n");
    if(webser->http_req->fkeepalive) {
        webser_rsp_header_push(webser, "Connection: keep-alive\r\n");
    } else {
        webser_rsp_header_push(webser, "Connection: close\r\n");
    }
	if(webser->type == WEBSER_FILE) {
		webser_rsp_header_push(webser, "Content-Length: %d\r\n", webser->fsize);
	} else if (webser->type == WEBSER_API) {
		int len = 0;
		meta_t * m = webser->rsp_meta_body;
		while(m) {
			len += m->last - m->pos;
			m = m->next;
		}
		webser_rsp_header_push(webser, "Content-Length: %d\r\n", len);
	}
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
    int http_code = 400;
	
    char path[WEBSER_LENGTH_PATH_STR] = {0};
    char * p = path;
    int pn = 0;
    
    pn += snprintf(p+pn, WEBSER_LENGTH_PATH_STR-pn, "%s", config_get()->http_home);
    if(webser->http_req->uri.len > 0) {
        pn += snprintf(p+pn, WEBSER_LENGTH_PATH_STR-pn, "%.*s", webser->http_req->uri.len, webser->http_req->uri.data);
        // if url is a dir, need to add index path
        if(webser->http_req->uri.data[webser->http_req->uri.len-1] == '/') {
            pn += snprintf(p+pn, WEBSER_LENGTH_PATH_STR-pn, "%s", config_get()->http_index);
        }
    } else {
        pn += snprintf(p+pn, WEBSER_LENGTH_PATH_STR-pn, "/%s", config_get()->http_index);
    }
    
    char * postfix = strrchr(path, '.');
    if(postfix) {
        webser->http_rsp_mime = webser_rsp_mime_find((unsigned char*)postfix, strlen(postfix));
    } else {
        webser->http_rsp_mime = webser_rsp_mime_find((unsigned char*)NULL, 0); 
    }
    webser->fsize = webser->fsend = 0;
    struct stat st;
    if(0 != stat(path, &st)) {
        err("webser stat check file [%s] failed, [%d]\n", path, errno);
        http_code = 400;
    } else {
        if(st.st_mode & S_IFMT) {
            if(st.st_mode & S_IRUSR) {
                webser->ffd = open(path, O_RDONLY);
                if(-1 == webser->ffd) {
                    err("webser open static file failed [%s], errno [%d]\n", path, errno);
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
        if(webser->http_req->method_typ != HTTP_METHOD_GET) {
            err("webser static req. method not support\n");
            webser_free(webser);
            return -1;
        }
        webser->type = WEBSER_FILE;
        ev->read_pt = webser_req_file;
    }
    return ev->read_pt(ev);
}

int webser_req_payload(webser_t * web)
{	
    if(!web->http_payload) {
        if(0 != http_payload_ctx_init(web->c, &web->http_payload, (web->api->body_need == 1) ? 1 : 0)) {
            err("http_payload_create failed\n");
            return -1;
        }
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
        if(0 != meta_alloc(&c->meta, 4096)) {
            err("webser con meta alloc failed\n");
            net_free(c);
            return -1;
        }
    }
    
    if(0 != webser_alloc(&webser)) {
        err("webser con webser alloc failed\n");
        net_free(c);
        return -1;
    }
    webser->c = c;
    c->data = webser;

    ///start http request parse, try to read http request form socket and parse it 
    if(0 != http_req_ctx_init(c, &webser->http_req)) {
        err("webser alloc req failed\n");
        webser_free(webser);
        return -1;
    }
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
        if(0 != meta_alloc(&c->meta, 8192)) {
            err("webser alloc con meta failed\n");
            net_free(c);
            return -1;
        }
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
    if(htonl(S5_AUTH_LOCAL_MAGIC) != header->magic) {
	    ev->read_pt = webser_start;
    	return ev->read_pt(ev);
    } 
    ev->read_pt = s5_srv_transport;
    return ev->read_pt(ev);
}

static int webser_accept_cb_ssl_check(event_t * ev)
{
    con_t * c = ev->data;
    if(!c->ssl->f_handshaked) {
        err("webser handshake failed\n");
        net_free(c);
        return -1;
    }
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
    if(0 != ssl_create_connection(c, L_SSL_SERVER)) {
        err("webser ssl con create failed\n");
        net_free(c);
        return -1;
    }

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
    if(0 != ezhash_create(&g_web_ctx->mime_hash, 128)) {
        err("webser create mime hash failed\n");
        return -1;
    }    
    ezhash_add(g_web_ctx->mime_hash, ".html",  "Content-type: text/html\r\n");
    ezhash_add(g_web_ctx->mime_hash, ".js",    "Content-type: application/x-javascript\r\n");
    ezhash_add(g_web_ctx->mime_hash, ".json",  "Content-type: application/json\r\n");
    ezhash_add(g_web_ctx->mime_hash, ".png",   "Content-type: image/png\r\n");
    ezhash_add(g_web_ctx->mime_hash, ".jpg",   "Content-type: image/jpeg\r\n");
    ezhash_add(g_web_ctx->mime_hash, ".jpeg",  "Content-type: image/jpeg\r\n");
    ezhash_add(g_web_ctx->mime_hash, ".gif",   "Content-type: image/gif\r\n");
    ezhash_add(g_web_ctx->mime_hash, ".ico",   "Content-type: image/x-icon\r\n");
    ezhash_add(g_web_ctx->mime_hash, ".css",   "Content-type: text/css\r\n");
    ezhash_add(g_web_ctx->mime_hash, ".txt",   "Content-type: text/plain\r\n");
    ezhash_add(g_web_ctx->mime_hash, ".htm",   "Content-type: text/html\r\n");
    ezhash_add(g_web_ctx->mime_hash, ".mp3",   "Content-type: audio/mpeg\r\n");
    ezhash_add(g_web_ctx->mime_hash, ".m3u8",  "Content-type: application/x-mpegURL\r\n");
    ezhash_add(g_web_ctx->mime_hash, ".ts",    "Content-type: video/MP2T\r\n");
    return 0;
}


/// @brief webserver module init 
/// @param  
/// @return 
int webser_init(void)
{
    int ret = -1;
    if(g_web_ctx) {
        err("g_web_ctx not empty\n");
        return -1;
    }

    do {
        g_web_ctx = mem_pool_alloc(sizeof(g_web_t));
        if(!g_web_ctx) {
            err("webser alloc this failed, [%d]\n", errno);
            return -1;
        }
        
        /// init mime hash 
        if(0 != webser_init_mimehash()) {
            mem_pool_free(g_web_ctx);
            err("webser mime hash init failed\n");
            return -1;
        }
        
        /// init webserv api data
        if(0 != mem_arr_create(&g_web_ctx->g_api_list, sizeof(webser_api_t))) {
            err("webser create api mem arr create failed\n");
            break;
        }
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
