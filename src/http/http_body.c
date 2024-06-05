#include "common.h"
#include "http_body.h"

static int http_body_alloc(http_body_t ** body)
{
    http_body_t * nbd = mem_pool_alloc(sizeof(http_body_t));
    if(!nbd){
        err("http body alloc nbd failed\n");
        return -1;
    }
    *body = nbd;
    return 0;
}

int http_body_free(http_body_t * bd)
{
    bd->c = NULL;
    bd->state = 0;
    bd->cb = NULL;

    bd->body_type = HTTP_BODY_TYPE_NULL;
    bd->body_cache = 0;

    bd->content_len = 0;
    bd->content_recvd = 0;
    
    bd->hex_len = 0;
    memset(bd->hex_str, 0, sizeof(bd->hex_str));

    bd->chunk_pos = NULL;
    bd->chunk_part_cur = 0;
    bd->chunk_part_len = 0;

    meta_t * m = bd->body_head;
    meta_t * n = NULL;
    while(m) {
        n = m->next;
        meta_free(m);
        m = n;
    }
    
    bd->body_head = NULL;
    bd->body_last = NULL;
    bd->body_len = 0;
    bd->body_status = 0;

    // todo: free the metalist for stroge chunk raw data
    bd->chunk_meta = NULL;

    if(bd->body_dump) {
        meta_free(bd->body_dump);
        bd->body_dump = NULL;
    }
    mem_pool_free(bd);
    return 0;
}

static int http_body_chunk_analysis(http_body_t * bd)
{
    status rc = -11;
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
    
    state = bd->state;
    for(p = bd->chunk_pos; p < bd->body_last->last; p++) {
        if(state == chunk_init) {
            if((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
                bd->hex_str[bd->hex_len++] = *p;
                state = chunk_hex;
                continue;
            } else {
                err("http body chunk analysis. chunk_init illegal, [%c]\n", *p);
                return -1;
            }
        }
        
        if (state == chunk_hex) {
            if( (*p >= '0' && *p <= '9') ||
                (*p >= 'a' && *p <= 'f') ||
                (*p >= 'A' && *p <= 'F') ||
                (*p == 'x') || (*p == 'X')
            ) {
                bd->hex_str[bd->hex_len++] = *p;
                continue;
            } else if (*p == CR) {
                state = chunk_hex_fin;
                continue;
            } else {
                err("http body chunk analysis. chunk_hex illegal [%c]\n", *p);
                return -1;
            }
        }
        
        if (state == chunk_hex_fin) {
            if(*p == LF) {
                int hexn = strtol((char*)bd->hex_str, NULL, 16);
                bd->body_len += hexn;
                if(hexn >= 0) {
                    bd->chunk_part_len = hexn;
                    bd->chunk_part_cur = 0;
                    state = chunk_part;
                    if(hexn > 0) { ///todo: alloc a meta for storge chunk raw data
                        continue;
                    }
                } else {
                    err("http body chunk analysis. chunk hexn [%d] illegal\n",  hexn);
                    return -1;
                }
            } else {
                err("http body chunk analysis. chunk_hex_fin illegal [%c]\n",  *p);
                return -1;
            }
        }

        if(state == chunk_part) {  /// todo: copy it into chunk raw data meta
            if(bd->chunk_part_len > 0)
                bd->chunk_part_cur ++;
            
            if(bd->chunk_part_cur >= bd->chunk_part_len) {
                state = chunk_part_fin_cr;
                continue;
            }
        }

        if(state == chunk_part_fin_cr) {
            if(*p == CR) {
                state = chunk_part_fin_crlf;
                continue;
            } else {
                err("http body chunk analysis. chunk_part_fin_cr illegal [%c]\n", *p);
                return -1;
            }
        }

        if(state == chunk_part_fin_crlf) {
            if(*p == LF) {
                if(bd->chunk_part_len == 0) {
                    rc = 1;
                    break;
                }
                state = chunk_init;
                continue;
            }
        }
    }
    bd->state = state;
    bd->chunk_pos = p;
    if(rc == 1) {
        if(p < bd->body_last->last) {
            bd->chunk_pos = p+1;         
        }
    }
    return rc;
}

static int http_body_chunk(http_body_t * bd)
{
    int rc;
    meta_t * meta_n = NULL;
    for(;;) {
        if(meta_len(bd->body_last->last, bd->body_last->end) <= 0) {
            if(0 != meta_alloc(&meta_n, ENTITY_BODY_BUFFER_SIZE)) {
                err("http body chunk alloc append meta failed\n");
                return -1;
            }
            bd->body_last->next = meta_n;
            bd->body_last = meta_n;
            bd->chunk_pos = bd->body_last->pos;
        }

        if(bd->chunk_pos == bd->body_last->last) {
            rc = bd->c->recv(bd->c, bd->body_last->last, meta_len(bd->body_last->last, bd->body_last->end));
            if(rc < 0) {
                if(rc == -1) {
                    err("http body recv failed\n");
                }
                return rc;
            } 
            bd->body_last->last += rc;
        }

        rc = http_body_chunk_analysis(bd);
        if (rc == -1) {
            err("http body chunk analysis failed\n");
            return -1;
        } else if(rc == 1) {
            bd->body_status |= HTTP_BODY_STAT_DONE_CACHE;
            return 1;
        }
    }
}

static int http_body_content(http_body_t * bd)
{
    meta_t * meta_n = NULL;
    int rc = 0;
    
    for(;;) {  
        if(meta_len(bd->body_last->last, bd->body_last->end) <= 0) {
            if(0 != meta_alloc(&meta_n, ENTITY_BODY_BUFFER_SIZE)) {
                err("http body content alloc append meta failed\n");
                return -1;
            }
            bd->body_last->next = meta_n;
            bd->body_last = meta_n;
        }
        
        rc = bd->c->recv(bd->c, bd->body_last->last, meta_len(bd->body_last->last, bd->body_last->end));
        if(rc < 0) {    
            if(rc == -1) {
                err("http body recv failed\n");
                return -1;
            }
            return -11;
        }

        if(bd->body_cache) /// only cache enable, change the recv meta position
            bd->body_last->last += rc;
        
        bd->content_recvd += rc;
        if(bd->content_recvd >= bd->content_len) {
            bd->body_len = bd->content_len;
            bd->body_status = (bd->body_cache ? HTTP_BODY_STAT_DONE_CACHE : HTTP_BODY_STAT_DONE_CACHENO);
            return 1;
        }
    }
}

int http_body_dump(http_body_t * bd)
{
    if(bd->body_len <= 0) {
        return -1;
    }

    meta_t * cur = NULL;
    meta_t * meta = NULL;

    if(0 != meta_alloc(&meta, bd->body_len)) {
        err("http body dump alloc meta failed\n");
        return -1;
    }

    ///copy data form body chain into dumo meta
    cur = bd->body_head;
    while(cur) {
        memcpy(meta->last, cur->pos, cur->last - cur->pos);
        meta->last += (cur->last - cur->pos);
        cur = cur->next;
    }

    /// check length
    if(meta->last - meta->pos != bd->body_len) {
        err("meta datan [%d] != bodylen [%d]\n", meta->last - meta->pos, bd->body_len);
        return -1;
    } else {
        bd->body_dump = meta;
        return 0;
    }
}

static int http_body_start(http_body_t * bd)
{
    if(bd->body_type == HTTP_BODY_TYPE_NULL) {
        bd->body_status |= HTTP_BODY_STAT_DONE_CACHENO;
        return 1;
    } else if (bd->body_type == HTTP_BODY_TYPE_CONTENT || bd->body_type == HTTP_BODY_TYPE_CHUNK) {
        int remainn = meta_len( bd->c->meta->pos, bd->c->meta->last ); ///get remian body data
        if(0 != meta_alloc(&bd->body_head, remainn + ENTITY_BODY_BUFFER_SIZE)) { ///alloc frist meta to stroge the content data
            err("http body alloc header failed\n");
            return -1;
        }
        bd->body_last = bd->body_head;

        if(bd->body_type == HTTP_BODY_TYPE_CONTENT) {  /// content length type http body
            if(remainn > 0) {
                if(bd->body_cache) {
                    memcpy(bd->body_last->last, bd->c->meta->pos, remainn);
                    bd->body_last->last += remainn;
                }
                bd->c->meta->pos += remainn;
                bd->content_recvd += remainn;
                if(bd->content_recvd >= bd->content_len) {
                    bd->body_len = bd->content_len;
                    bd->body_type |= (bd->body_cache ? HTTP_BODY_STAT_DONE_CACHE : HTTP_BODY_STAT_DONE_CACHENO);
                    return 1;
                }
            }
            bd->cb = http_body_content;
            return bd->cb(bd);
        } else {  ///chunked type http body
            if(remainn > 0) {  ///whatever body cache enbale, chunked type need to copy the remain data
                memcpy(bd->body_last->last, bd->c->meta->pos, remainn);
                bd->body_last->last += remainn;
            }
            bd->c->meta->pos += remainn;
            bd->chunk_pos = bd->body_last->pos;
            bd->cb = http_body_chunk;
            return bd->cb(bd);
        }
    } else {
        err("http body not support this type [%d]\n", bd->body_type);
        return -1;
    }
}


int http_body_create(con_t * c, http_body_t ** body, int discard)
{
    http_body_t * body_n;
    if(0 != http_body_alloc(&body_n)) {
        err("http body alloc cycle failed\n");
        return -1;
    }
    body_n->c = c;
    body_n->state = 0;
    body_n->body_cache = ((discard == 1) ? 0 : 1);
    body_n->cb = http_body_start;
    *body = body_n;
    return 0;
}


int http_body_init_module(void)
{
    return 0;
}

int http_body_end_module(void)
{
    return 0;
}
