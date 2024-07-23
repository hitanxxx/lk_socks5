#include "common.h"
#include "http_payload.h"


static int http_payload_push(http_payload_t * ctx, char * data, int datan)
{
    if(!datan) return 0;

    if(!ctx->payload) {
		if(0 != meta_alloc(&ctx->payload, HTTP_PAYLOAD_BUFN)) {
			err("meta alloc err\n");
			return -1;
		}
	}
    
    meta_t * p = NULL;
    meta_t * n = NULL;
	meta_t * m = ctx->payload;
	while(m) {
		p = m;
		if(meta_getfree(m) > datan) {
			memcpy(m->last, data, datan);
			m->last += datan;
			return 0;
		}
		m = m->next;
	}
	if(0 != meta_alloc(&n, HTTP_PAYLOAD_BUFN)) {
		err("meta alloc err\n");
		return -1;
	}
	p->next = n;
	memcpy(n->last, data, datan);
	n->last += datan;
    return 0;
}

static int http_payload_chunk_analysis(http_payload_t * ctx)
{
    int rc = -11;
    unsigned char * p = NULL;
    
    enum {
        chunk_init = 0,
        chunk_hex,
        chunk_hex_fin,
        chunk_part,
        chunk_part_fin_cr,
        chunk_part_fin_crlf
    } state;

    /*
        chunk data format
        ...
        | data len(hex) |
        | \r\n            |
        | data          |
        | \r\n          |
        ...
        | data len(0)   |
        | \r\n          |
        END
    */
    
    state = ctx->state;
    for(p = ctx->meta->pos; p < ctx->meta->last; p++) {
        if(state == chunk_init) {
            if((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
                ctx->hex[ctx->hexn++] = *p;
                state = chunk_hex;
                continue;
            } else {
                err("http payload chunk analysis. chunk_init illegal, [%c]\n", *p);
                return -1;
            }
        }
        
        if (state == chunk_hex) {
            if( (*p >= '0' && *p <= '9') ||
                (*p >= 'a' && *p <= 'f') ||
                (*p >= 'A' && *p <= 'F') ||
                (*p == 'x') || (*p == 'X')
            ) {
                ctx->hex[ctx->hexn++] = *p;
                continue;
            } else if (*p == CR) {
                state = chunk_hex_fin;
                continue;
            } else {
                err("http payload chunk analysis. chunk_hex illegal [%c]\n", *p);
                return -1;
            }
        }
        
        if (state == chunk_hex_fin) {
            if(*p == LF) {
                ctx->ilen = strtol((char*)ctx->hex, NULL, 16);
                ctx->in = 0;
                if(ctx->ilen < 0) {
                    err("http payload chunk analysis. chunk hexn [%d] illegal\n", ctx->ilen);
                    return -1;
                }
                state = chunk_part;
                continue;
            } else {
                err("http payload chunk analysis. chunk_hex_fin illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == chunk_part) {
            int payloadn = l_min(ctx->ilen - ctx->in, ctx->meta->last - p);
            if(ctx->fcache) {
                schk(0 == http_payload_push(ctx, (char*)p, payloadn), return -1);
            }
            p += payloadn;
            ctx->in += payloadn;
            if(ctx->in >= ctx->ilen) {
                state = chunk_part_fin_cr;
            }
        }

        if(state == chunk_part_fin_cr) {
            if(*p == CR) {
                state = chunk_part_fin_crlf;
                continue;
            } else {
                err("http payload chunk analysis. chunk_part_fin_cr illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == chunk_part_fin_crlf) {
            if(*p == LF) {
                if(ctx->ilen == 0) {
                    rc = 1;
                    break;
                }
                state = chunk_init;
                continue;
            }
        }
    }
    ctx->state = state;
    meta_clr(ctx->meta);
    return rc;
}

static int http_payload_chunk(http_payload_t * ctx)
{
    for(;;) {
        if(meta_getlen(ctx->meta) < 1) {
            int recvn = ctx->c->recv(ctx->c, ctx->meta->last, meta_getfree(ctx->meta));
            if(recvn < 0) {
                if(recvn == -1) {
                    err("http payload recv err\n");
                    return -1;
                }
                return -11;
            }
            ctx->meta->last += recvn;
        }
 
        int ret = http_payload_chunk_analysis(ctx);
        if (ret == -1) {
            err("http payload chunk analysis failed\n");
            return -1;
        } else if(ret == 1) {
            return 1;
        }
    }
}

static int http_payload_content(http_payload_t * ctx)
{    
    unsigned char buf[4096] = {0};
    for(;;) {    
        int recvn = ctx->c->recv(ctx->c, buf, sizeof(buf));
        if(recvn < 0) {    
            if(recvn == -1) {
                err("http payload recv failed\n");
                return -1;
            }
            return -11;
        }
        if(ctx->fcache) {
            schk(0 == http_payload_push(ctx, (char*)buf, recvn), return -1);
        }
        ctx->in += recvn;
        if(ctx->in > ctx->ilen) {
            return 1;
        }
    }
}

static int http_payload_start(http_payload_t * ctx)
{
    if(ctx->fchunk) {
        if(meta_getlen(ctx->c->meta)) {
            memcpy(ctx->meta->last, ctx->c->meta->pos, meta_getlen(ctx->c->meta));
            ctx->meta->last += meta_getlen(ctx->c->meta);
            ctx->c->meta->pos += meta_getlen(ctx->c->meta);
        }
        ctx->cb = http_payload_chunk;
    } else {
        if(meta_getlen(ctx->c->meta) > 0) {
            int bodyn = l_min(meta_getlen(ctx->c->meta), ctx->ilen);
            if(ctx->fcache) schk(0 == http_payload_push(ctx, (char*)ctx->c->meta->pos, bodyn), return -1);
            ctx->c->meta->pos += bodyn;
            ctx->in += bodyn;
        }
        if(ctx->in >= ctx->ilen) return 1;
        ctx->cb = http_payload_content;
    }
    return ctx->cb(ctx);
}


int http_payload_ctx_exit(http_payload_t *ctx)
{
    if(ctx) {
        if(ctx->meta) {
            meta_free(ctx->meta);
        }
        if(ctx->payload) {
            meta_free(ctx->payload);
        }
        mem_pool_free(ctx);
    }
    return 0;
}

int http_payload_ctx_init(con_t * c, http_payload_t ** ctx, int enable_cache)
{
    http_payload_t * nctx = mem_pool_alloc(sizeof(http_payload_t));
    if(!nctx) {
        err("http payload ctx alloc failed\n");
        return -1;
    }
    if(0 != meta_alloc(&nctx->meta, HTTP_PAYLOAD_BUFN)) {
        err("http payload meta alloc err\n");
        mem_pool_free(nctx);
        return -1;
    }
    if(0 != meta_alloc(&nctx->payload, HTTP_PAYLOAD_BUFN)) {
        err("http payload meta alloc err\n");
        meta_free(nctx->meta);
        mem_pool_free(nctx);
        return -1;
    }
    nctx->c = c;
    nctx->state = 0;
    nctx->fcache = ((enable_cache == 1) ? 1 : 0);
    nctx->cb = http_payload_start;
    *ctx = nctx;
    return 0;
}

int http_payload_init_module(void)
{
    return 0;
}

int http_payload_end_module(void)
{
    return 0;
}
