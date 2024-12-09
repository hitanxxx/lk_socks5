#include "common.h"
#include "http_req.h"

static int web_req_line(con_t * c, web_req_t * req);
static int web_req_hdrs(con_t * c, web_req_t * req);
static int web_req_payload(con_t * c, web_req_t * req);



web_req_t * web_req_alloc(void)
{
	web_req_t * rr = mem_pool_alloc(sizeof(web_req_t));
	schk(rr, return NULL);
	rr->cb = web_req_line;
	return rr;
}

void web_req_free(web_req_t * webreq) 
{
	if(webreq) {
		mem_pool_free(webreq);
	}
}

static int web_req_line(con_t * c, web_req_t * req)
{
	unsigned char *p = NULL;
    ///reqline example
    ///GET http://localhost:8080/index.html
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
    };

	for(;;) {
		if(meta_getfree(c->meta) < 1) {
			err("webreq meta full\n");
			return -1;
		}
		if(meta_getlen(c->meta) < 1) {
			int recvd = c->recv(c, c->meta->last, meta_getfree(c->meta));
			if(recvd < 0) {
				if(recvd == -11) {
					return -11;
				}
				err("webreq recv err\n");
				return -1;
			}
			c->meta->last += recvd;
		}

		meta_t * meta = c->meta;
		for(p = meta->pos; p < meta->last; p++) {
			if((*p < 32 || *p > 127) && *p != CR && *p != LF) {
				err("webreq contains non-printable character. [%d]\n", *p);
				return -1;
			}
	        if(req->state == s_method_init) {
	            if(*p == CR || *p == LF || *p == SP) {
	                continue;
	            } 
                req->method.data = p;
                req->state = s_method;
                continue;
	        }

	        if(req->state == s_method) {
	            if (*p == SP) { ///jump out of method state 
	                req->method.len = p - req->method.data;
	                req->state = s_scheme_init;
	                if(req->method.len < 1 || req->method.len > 16) {
	                    err("webreq request line. method string length [%d] illegal\n", req->method.len);
	                    return -1;
	                }
	            }
	        }
    
	        if(req->state == s_scheme_init) { ///this state for storge scheme string data
	            if(*p == SP) { 
	                continue;
	            } else if (*p == '/') { ///if s_sheme frist character is '/', then means no scheme, is uri start
	                req->uri.data = p;
	                req->state = s_uri;
	                continue;
	            }  
                req->scheme.data = p;
                req->state = s_scheme;
                continue;
	        }

	        if(req->state == s_scheme) {
	            if (*p == ':') {
	                req->scheme.len = p - req->scheme.data;
	                req->state = s_scheme_slash;
	                continue;
	            }
	        }

	        if(req->state == s_scheme_slash) {
	            if(*p == '/') {
	                req->state = s_scheme_slash_slash;
	                continue;
	            } else {
	                err("webreq request line. s_scheme_slash illegal [%c]\n", *p);
	                return -1;
	            }
	        }

	        if(req->state == s_scheme_slash_slash) {
	            if(*p == '/') {
	                req->state = s_host_init;
	                continue;
	            } else {
	                err("webreq request line. s_scheme_slash illegal [%c]\n", *p);
	                return -1;
	            }
	        }

	        if(req->state == s_host_init) {
                req->host.data = p;
                req->state = s_host;
                continue;	            
	        }

	        if(req->state == s_host) {
	            if (*p == ':') {
	                req->host.len = p - req->host.data;
	                req->state = s_port_init;
	                continue;
	            } else if (*p == '/') {  ///is s_host have '/', then means no port, is uri
	                req->host.len = p - req->host.data;
	                req->uri.data = p;
	                req->state = s_uri;
	                continue;
	            }
	        }

	        if(req->state == s_port_init) {
                req->port.data = p;
                req->state = s_port;
                continue;
	        }

	        if(req->state == s_port) {
	            if (*p == '/') {
	                req->port.len = p - req->port.data;
	                req->uri.data = p;
	                req->state = s_uri;
	                continue;
	            }
	        }

	        if(req->state == s_uri) {
	            if(*p == SP) {
	                req->uri.len = p - req->uri.data;
	                req->state = s_version_init;
	                continue;
	            }
	        }

	        if(req->state == s_version_init) {
	            req->http_ver.data = p;
	            req->state = s_version;
	            continue;          
	        }

	        if(req->state == s_version) {
	            if (*p == CR) {
	                req->http_ver.len = p - req->http_ver.data;
	                req->state = s_end;
	                continue;
	            }
	        }

	        if(req->state == s_end) {
	            if(*p == LF) {
	           
					meta->pos = p + 1;
					req->state = 0;
					req->cb = web_req_hdrs;

					return req->cb(c, req);
	            } else {
	                err("http req request line. s_end illegal [%c]\n", *p);
	                return -1;
	            }
	        }
    	}
	}
}

static int web_req_hdrs(con_t * c, web_req_t * req)
{
	unsigned char *p = NULL;

	enum {
		s_key_init = 0,
		s_key,
		s_value_init,
		s_value,
		s_end,		  /// end means line end, \r\n
		s_done		  /// done means header finish, \r\n\r\n
	};
	
	for(;;) {
		if(meta_getfree(c->meta) < 1) {
			err("webreq meta full\n");
			return -1;
		}
		if(meta_getlen(c->meta) < 1) {
			int recvd = c->recv(c, c->meta->last, meta_getfree(c->meta));
			if(recvd < 0) {
				if(recvd == -11) {
					return -11;
				}
				err("webreq recv err\n");
				return -1;
			}
			c->meta->last += recvd;
		}

		meta_t * meta = c->meta;
		for(p = meta->pos; p < meta->last; p++) {
			if((*p < 32 || *p > 127) && *p != CR && *p != LF) {
				err("webreq contains non-printable character. [%d]\n", *p);
				return -1;
			}

			if(req->state == s_key_init) {
	            if (*p == CR) {
	                req->state = s_done;
	                continue;
	            } else {
					req->kvs[req->kvn].k.data = p;
	                req->state = s_key;
	                continue;
				}
	        }

	        if(req->state == s_key) {
	             if (*p == ':') {
	                req->kvs[req->kvn].k.len = p - req->kvs[req->kvn].k.data;
	                req->state = s_value_init;
	                continue;
	            }
	        }

	        if(req->state == s_value_init) {
	            if (*p == SP) { /// do nothing
	                continue;
	            } else {
	                req->kvs[req->kvn].v.data = p;
	                req->state = s_value;
	                continue;
	            }
	        }

	        if(req->state == s_value) {
	             if (*p == CR) {
	                req->kvs[req->kvn].v.len = p - req->kvs[req->kvn].v.data;
	                req->state = s_end;
	                continue;
	            }
	        }

	        if(req->state == s_end) {
	            if(*p == LF) {
					req->kvn ++;
					req->state = s_key_init;
	                continue;
	            } else {
	                err("req headers. s_end illegal [%c]\n", *p);
	                return -1;
	            }
	        }

	        if(req->state == s_done) {
	            if(*p == LF) {
					c->meta->pos = p + 1;
					req->cb = web_req_payload;

					int i = 0;
					for(i = 0; i < req->kvn; i++) {
						if(strncasecmp((char*)req->kvs[i].k.data, "Connection", req->kvs[i].k.len) == 0) {
							if(strncasecmp((char*)req->kvs[i].v.data, "Keep-Alive", req->kvs[i].v.len) == 0) {
								req->fkeepalive = 1;
							}
						}
					}

					if(strncasecmp((char*)req->method.data, "GET", req->method.len) == 0) {
						req->method_typ = HTTP_METHOD_GET;
					} else if (strncasecmp((char*)req->method.data, "GET", req->method.len) == 0) {
						req->method_typ = HTTP_METHOD_POST;
					}
					
					return req->cb(c, req);
	            } else {
	                err("req headers. s_done illegal [%c]\n", *p);
	                return -1;
	            }
	        }
		}
	}
}

static int web_req_payload(con_t * c, web_req_t * req)
{
	if(!req->payload) {
		int i = 0;
		for(i = 0; i < req->kvn; i++) {
			if(strncasecmp((char*)req->kvs[i].k.data, "Content-Length", req->kvs[i].k.len) == 0) {
				req->payloadn = strtol((char*)req->kvs[i].v.data, NULL, 10);
				if(req->payloadn < 0) {
					err("webreq payloadn illegal. [%d]\n", req->payloadn);
					return -1;
				}
				if(req->payloadn > (10*1024*1024)) {
					err("ebreq payloadn too big. [%d]\n", req->payloadn);
					return -1;
				}
			}
		}
		
		if(req->payloadn > 0) {
			schk(0 == meta_alloc(&req->payload, req->payloadn), return -1);
			int remain = meta_getlen(c->meta);
			if(remain) {
				schk(0 == meta_pdata(req->payload, c->meta->pos, meta_getlen(c->meta)), return -1);
			}
		} else {
			return 0;
		}
	}

	while(meta_getlen(req->payload) < req->payloadn) {
		int recvd = c->recv(c, req->payload->last, meta_getfree(req->payload));
		if(recvd < 0) {
			if(recvd == -11) {
				return -11;
			}
			err("webreq recv payload err.\n");
			return -1;
		}
		req->payload->last += recvd;
	}
	return 0;
}


