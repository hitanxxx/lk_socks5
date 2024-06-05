#include "common.h"
#include "http_req.h"


static int http_req_set_value(http_req_t * request, string_t * value, unsigned int offsetof);
static int http_req_set_connection(http_req_t * request, string_t * value, unsigned int offsetof);

static http_req_value_t headers[] = 
{
    {   string("Host"),              http_req_set_value,         offsetof(http_req_headers_t, host)},
    {   string("Content-Length"),    http_req_set_value,         offsetof(http_req_headers_t, content_length)},
    {   string("Connection"),        http_req_set_connection,    offsetof(http_req_headers_t, connection)},
    {   string("Transfer-Encoding"), http_req_set_value,         offsetof(http_req_headers_t, transfer_encoding)},
    {   string("User-Agent"),        http_req_set_value,         offsetof(http_req_headers_t, user_agent)}
};

int http_req_have_body(    http_req_t * req)
{
    /// if not POST and not PUT method, then don't need have body
    if((0 != strncmp((char*)req->method.data, "POST", req->method.len)) &&
        (0 != strncmp((char*)req->method.data, "PUT", req->method.len))) {
        //dbg("http req method [%.*s] not allow have body\n", req->method.len, req->method.data );
        return 0;
    }

    /// if have content header, means have body
    if(req->content_type == HTTP_BODY_TYPE_CONTENT ||
        req->content_type == HTTP_BODY_TYPE_CHUNK) {
        return 1;
    }
    return 0;
}

static int http_req_set_value(http_req_t * request, string_t * value, unsigned int offsetof)
{
    string_t ** h = (string_t**)((void*)&request->headers+offsetof);
    *h = value;
    return 0;
}

static int http_req_set_connection(http_req_t * request, string_t * value, unsigned int offsetof)
{
    string_t ** h = (string_t**)((void*)&request->headers+offsetof);
    *h = value;
    request->keepalive = (value->len > (int32)l_strlen("close")) ? 1 : 0;
    return 0;
}

static int http_req_find_header( string_t * str, http_req_value_t ** header)
{
    int32 i = 0;
    for(i = 0; i < (int32)(sizeof(headers)/sizeof(headers[0])); i++) {
        if(headers[i].name.len == str->len && !strncasecmp((char*)headers[i].name.data, (char*)str->data, (int)str->len)) {
            *header = &headers[i];
            return 0;
        }
    }
    return -1;
}

static int http_req_recv(con_t * c, meta_t * meta)
{
    if(meta_len(meta->pos, meta->last) > 0) {
        return 0;
    }
    int recvn = c->recv(c, meta->last, meta_len(meta->last, meta->end));
    if(recvn < 0) {
        if(recvn == -1) {
            return -1;
        }
        return -11;
    }
    meta->last += recvn;
    return 0;
}

static int http_req_headers_analysis(http_req_t * req, meta_t * meta)
{
    unsigned char *p = NULL;
    int line_end = 0;
    int all_end = 0;

    enum {
        s_key_init = 0,
        s_key,
        s_value_init,
        s_value,
        s_end,        /// end means line end, \r\n
        s_done        /// done means header finish, \r\n\r\n
    } state;

    state = req->state;
    for(p = meta->pos; p < meta->last; p++) {
        if(state == s_key_init) {
            if( (*p >= 'A' && *p <= 'Z') ||
                (*p >= 'a' && *p <= 'z') ||
                (*p >= '0' && *p <= '9') ||
                *p == '-' || *p == '@' ||
                *p == '_'
            ) {
                req->header_k.data = p;
                state = s_key;
                continue;
            } else if (*p == CR) {
                state = s_done;
                continue;
            } else {
                err("req headers. s_key_init illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_key) {
            if( (*p >= 'A' && *p <= 'Z') ||
                (*p >= 'a' && *p <= 'z') ||
                (*p >= '0' && *p <= '9') ||
                *p == '-' || *p == '@' ||
                *p == '_'
            ) {  /// do nothing
                continue;
            } else if (*p == ':') {
                req->header_k.len = p - req->header_k.data;
                state = s_value_init;
                continue;
            } else {
                err("req headers. s_key illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_value_init) {
            if( (*p >= 'A' && *p <= 'Z') ||
                (*p >= 'a' && *p <= 'z') ||
                (*p >= '0' && *p <= '9') ||
                *p == '-' || *p == '@' ||
                *p == '_' || *p == '.' ||
                *p == '(' || *p == ')' ||
                *p == ';' || *p == '/' ||
                *p == ',' || *p == '=' ||
                *p == '*' || *p == '+' ||
                *p == ':' || *p == '"' ||
                *p == '?'
            ) {
                req->header_v.data = p;
                state = s_value;
                continue;
            } else if (*p == SP) { /// do nothing
                continue;
            } else {
                err("req headers, s_value_init illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_value) {
            if( *p == SP ||
                (*p >= 'A' && *p <= 'Z') ||
                (*p >= 'a' && *p <= 'z') ||
                (*p >= '0' && *p <= '9') ||
                *p == '-' || *p == '@' ||
                *p == '_' || *p == '.' ||
                *p == '(' || *p == ')' ||
                *p == ';' || *p == '/' ||
                *p == ',' || *p == '=' ||
                *p == '*' || *p == '+' ||
                *p == ':' || *p == '"' ||
                *p == '?'
            ) {  /// do nothing 
                continue;
            } else if (*p == CR) {
                req->header_v.len = p - req->header_v.data;
                state = s_end;
                continue;
            } else {
                err("req headers. s_value illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_end) {
            if(*p == LF) {
                line_end = 1;
                break;
            } else {
                err("req headers. s_end illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_done) {
            if(*p == LF) {
                all_end = 1;
                break;
            } else {
                err("req headers. s_done illegal [%c]\n", *p);
                return -1;
            }
        }
    }

    if(line_end) {
        meta->pos = p+1;
        req->state = s_key_init;
        return 0;
    } else if (all_end) {
        meta->pos = p;
        if(p < meta->last) {
            meta->pos = p+1;
        }
        req->state = 0;
        return 1;
    } else {
        meta->pos = p;
        req->state = state;
        return -11;
    }
}

static int http_req_headers( http_req_t * req)
{
    int rc = -11;
    con_t * c = req->c;
    http_req_value_t * headers;

    if(0 != mem_arr_create(&req->headers.list, sizeof(string_t))) {
        err("http req headers list create failed\n");
        return -1;
    }
        
    for(;;) {
        if(rc == -11) {
            rc = http_req_recv(c, c->meta);
            if(rc < 0) {
                return rc;
            }
        }
        rc = http_req_headers_analysis(req, c->meta);
        if(rc == 0) { ///line fin
            if(0 == http_req_find_header(&req->header_k, &headers)) {
                string_t * headers_obj = (string_t*)mem_arr_push(req->headers.list);
                if(!headers_obj) {
                    err("http req headers alloc headerobj form mem arr failed\n");
                    return -1;
                }
                headers_obj->data = req->header_v.data;
                headers_obj->len = req->header_v.len;
                if(headers->handler)
                    headers->handler(req, headers_obj, headers->offsetof);
            }
            continue;
        } else if(rc == 1) {  ///all fin
            if(req->headers.user_agent) {     
                access_log("%.*s - %s - %.*s\n",
                    meta_len(req->req_line_start, req->req_line_end),
                    req->req_line_start,
                    inet_ntoa(req->c->addr.sin_addr),
                    req->headers.user_agent->len, req->headers.user_agent->data);            
            } else {
                access_log("%.*s - %s - null\n", 
                    meta_len(req->req_line_start, req->req_line_end),
                    req->req_line_start,
                    inet_ntoa(req->c->addr.sin_addr));    
            }
            req->content_type = HTTP_BODY_TYPE_NULL;
            if (req->headers.transfer_encoding || req->headers.content_length) {
                if(req->headers.transfer_encoding) {
                    req->content_type = HTTP_BODY_TYPE_CHUNK;
                } else {
                    req->content_type = HTTP_BODY_TYPE_CONTENT;
                    char str[32] = {0};
                    memcpy(str, req->headers.content_length->data, req->headers.content_length->len);
                    req->content_len = strtol(str, NULL, 10);
                }
            } 
            return 1;
        }else if(rc == -1) {
            err("http req headers analysis failed\n");
            return -1;
        }
        
        if(c->meta->last == c->meta->end) {
            err("http req headers too large");
            return -1;
        }
    }
}

/// fsm to analysis the http request request line 
static int http_req_request_line_analysis(http_req_t * req, meta_t * meta)
{
    unsigned char *p = NULL;
    int finish = 0;

    /// complete request line format 
    /// method URL http_version

    /// complete URL format
    /// http://www.google.com/key?value
    /// scheme://host:port/path?query
    enum {
        s_method_init = 0,
        s_method,
        s_scheme_init,
        s_scheme,
        s_scheme_slash,
        s_scheme_slash_slash,
        s_host_init,
        s_host,
        s_port_init,
        s_port,
        s_uri,
        s_version_init,
        s_version,
        s_end
    } state;

    state = req->state;
    for(p = meta->pos; p < meta->last; p++) {  ///fsm character check
        if(state == s_method_init) {
            if(*p == CR || *p == LF || *p == SP) {
                continue;
            } else if (*p >= 'A' && *p <= 'Z') {
                req->req_line_start = p;
                req->method.data = p;
                state = s_method;
                continue;
            } else {
                err("http req request line. s_init illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_method) {
            if(*p >= 'A' && *p <= 'Z') { ///do nothing
                continue;
            } else if (*p == SP) { ///jump out of method state 
                req->method.len = p - req->method.data;
                state = s_scheme_init;
                if(req->method.len < 1 || req->method.len > 16) {
                    err("http req request line. method string len [%d] illegal\n", req->method.len);
                    return -1;
                }
            } else {
                err("http req request line. s_method illegal [%c]\n", *p);
                return -1;
            }
        }
    
        if(state == s_scheme_init) { ///this state for storge scheme string data
            if(*p == SP) { ///do nothing
                continue;
            } else if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {
                req->scheme.data = p;
                state = s_scheme;
                continue;
            } else if (*p == '/') { ///if s_sheme frist character is '/', then means no scheme, is uri start
                req->uri.data = p;
                state = s_uri;
                continue;
            } else {
                err("http req request line. s_scheme_init illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_scheme) {
            if((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z')) {  ///do nothing
                continue;
            } else if (*p == ':') {
                req->scheme.len = p - req->scheme.data;
                state = s_scheme_slash;
                continue;
            } else {
                err("http req request line. s_scheme illegal. [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_scheme_slash) {
            if(*p == '/') {
                state = s_scheme_slash_slash;
                continue;
            } else {
                err("http req request line. s_scheme_slash illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_scheme_slash_slash) {
            if(*p == '/') {
                state = s_host_init;
                continue;
            } else {
                err("http req request line. s_scheme_slash illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_host_init) {
            if( (*p >= 'A' && *p <= 'Z') ||
                (*p >= 'a' && *p <= 'z') ||
                (*p >= '0' && *p <= '9') ||
                *p == '.' ||
                *p == '-' ||
                *p == '_'
            ) {
                req->host.data = p;
                state = s_host;
                continue;
            } else {
                err("http req request line. s_host_init illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_host) {
            if( (*p >= 'A' && *p <= 'Z') ||
                (*p >= 'a' && *p <= 'z') ||
                (*p >= '0' && *p <= '9') ||
                *p == '.' ||
                *p == '-' ||
                *p == '_'
            ) { ///do nothing 
                continue;
            } else if (*p == ':') {
                req->host.len = p - req->host.data;
                state = s_port_init;
                continue;
            } else if (*p == '/') {  ///is s_host have '/', then means no port, is uri
                req->host.len = p - req->host.data;
                req->uri.data = p;
                state = s_uri;
                continue;
            } else {
                err("http req request line. s_host illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_port_init) {
            if(*p >= '0' && *p <= '9') {
                req->port.data = p;
                state = s_port;
                continue;
            } else {
                err("http req request line. s_port_init illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_port) {
            if(*p >= '0' && *p <= '9') { ///do nothing
                continue;
            } else if (*p == '/') {
                req->port.len = p - req->port.data;
                req->uri.data = p;
                state = s_uri;
                continue;
            } else {
                err("http req request line. s_port illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_uri) {
            if( (*p >= '0' && *p <= '9') ||
                (*p >= 'a' && *p <= 'z') ||
                (*p >= 'A' && *p <= 'Z') ||
                *p == '.' || *p == '@' ||
                *p == '?' || *p == '&' ||
                *p == '=' || *p == '/' ||
                *p == '_' || *p == '-' ||
                *p == '*' || *p == '+'
            ) { /// do nothing 
                continue;
            } else if(*p == SP) {
                req->uri.len = p - req->uri.data;
                state = s_version_init;
                continue;
            } else {
                err("http req request line. s_uri illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_version_init) {
            if( (*p >= '0' && *p <= '9') ||
                (*p >= 'a' && *p <= 'z') ||
                (*p >= 'A' && *p <= 'Z') ||
                *p == '.' || *p == '/' 
            ) {
                req->http_ver.data = p;
                state = s_version;
                continue;
            } else {
                err("http req request line, s_version_init illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_version) {
            if( (*p >= '0' && *p <= '9') ||
                (*p >= 'a' && *p <= 'z') ||
                (*p >= 'A' && *p <= 'Z') ||
                *p == '.' || *p == '/' 
            ) { /// do nothing 
                continue;
            } else if (*p == CR) {
                req->http_ver.len = p - req->http_ver.data;
                state = s_end;
                req->req_line_end = p;
                continue;
            } else {
                err("http req request line, s_version illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == s_end) {
            if(*p == LF) {
                finish = 1;
                break;
            } else {
                err("http req request line. s_end illegal [%c]\n", *p);
                return -1;
            }
        }
    }

    if(finish) {
        meta->pos = p;
        if(p < meta->last) {
            meta->pos = p + 1;
        }
        req->state = 0;
        return 1;
    } else {
        meta->pos = p;
        req->state = state;
        return -11;
    }
}


static int http_req_request_line(http_req_t * req)
{
    int32 rc = -11;
    con_t * c = req->c;

    for(;;) {
        if(rc == -11) {
            rc = http_req_recv(c, c->meta);
            if(rc < 0) {
                return rc;
            }
        }
        rc = http_req_request_line_analysis(req, c->meta);
        if(rc == 1) { ///fin
            if(req->method.len > 0) {
                if(req->method.len == strlen("GET") && !strncmp((char*)req->method.data, "GET", req->method.len)) {
                    req->method_type = HTTP_METHOD_GET;
                } else if (req->method.len == strlen("POST") && !strncmp((char*)req->method.data, "POST", req->method.len)) {
                    req->method_type = HTTP_METHOD_POST;
                } else if (req->method.len == strlen("PUT") && !strncmp((char*)req->method.data, "PUT", req->method.len)) {
                    req->method_type = HTTP_METHOD_PUT;
                } else if (req->method.len == strlen("DELETE") && !strncmp((char*)req->method.data, "DELETE", req->method.len)) {
                    req->method_type = HTTP_METHOD_DELETE;
                } else if (req->method.len == strlen("CONNECT") && !strncmp((char*)req->method.data, "CONNECT", req->method.len)) {
                    req->method_type = HTTP_METHOD_CONNECT;
                } else if (req->method.len == strlen("HEAD") && !strncmp((char*)req->method.data, "HEAD", req->method.len)) {
                    req->method_type = HTTP_METHOD_HEAD;
                } else {
                    err("http req method not support. [%.*s]\n", req->method.len, req->method.data);
                    return -1;
                }
            }
            req->cb = http_req_headers;
            return req->cb(req);
        } else if (rc == -1) {
            err("http req line analysis failed\n");
            return -1;
        }
        if(c->meta->last == c->meta->end) {
            err("http req line too long\n");
            return -1;
        }
    }
}

static int http_req_alloc(http_req_t ** req)
{
    http_req_t * nreq = mem_pool_alloc(sizeof(http_req_t));
    if(!nreq) {
        err("http req alloc nreq failed\n");
        return -1;
    }
    *req = nreq;
    return 0;
}

inline static void http_req_clear_string(string_t * str)
{
    str->len = 0;
    str->data = NULL;
}

int http_req_free(http_req_t * req)
{
    req->c = NULL;
    req->cb = NULL;
    req->state = 0;

    req->req_line_start = NULL;
    req->req_line_end = NULL;
    
    http_req_clear_string(&req->method);
    http_req_clear_string(&req->scheme);
    http_req_clear_string(&req->host);
    http_req_clear_string(&req->uri);
    http_req_clear_string(&req->port);
    http_req_clear_string(&req->http_ver);
    
    http_req_clear_string(&req->header_k);
    http_req_clear_string(&req->header_v);
    
    if(req->headers.list) {
        mem_arr_free(req->headers.list);
        memset(&req->headers, 0, sizeof(http_req_headers_t));
    }
    req->keepalive = 0;
    req->content_len = 0;
    req->content_type = 0;
    
    mem_pool_free(req);
    return 0;
}


int http_req_create(con_t * c, http_req_t ** request)
{
    /*
        http request 

        <request line>    /// this module process
        <headers>        /// this module process
        <request body>    /// process by http body module
    */
    http_req_t * req_n = NULL;
    if(0 != http_req_alloc(&req_n)) {
        err("http req alloc new failed\n");
        return -1;
    }
    req_n->c = c;
    req_n->state = 0;
    req_n->cb = http_req_request_line;
    *request = req_n;
    return 0;
}

int http_req_init_module(void)
{
    return 0;
}

int http_req_end_module(void)
{
    return 0;
}
