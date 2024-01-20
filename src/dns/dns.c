#include "common.h"
#include "dns.h"

#define DNS_REC_MAX    1024

typedef struct dns_cache_s {
    queue_t     queue;
    long long   expire_msec;
    char        query[DOMAIN_LENGTH+1];
    char        addr[4];
} dns_cache_t;

typedef struct dns_ctx_s
{
    queue_t     usable;
    queue_t     use;
    int         recn;
    queue_t     record_mng;
    dns_cycle_t pool[0];
} dns_ctx_t;
static dns_ctx_t * dns_ctx = NULL;


static status dns_rec_add( char * query, char * addr, int msec )
{
    dns_cache_t * rdns = calloc(1, sizeof(dns_cache_t));
    if( !rdns ) {
        err("rdns alloc failed. [%d]\n", errno );
        return ERROR;
    }
    queue_init( &rdns->queue );
    strncpy( rdns->query, query, sizeof(rdns->query) );
    rdns->addr[0] = addr[0];
    rdns->addr[1] = addr[1];
    rdns->addr[2] = addr[2];
    rdns->addr[3] = addr[3];
    rdns->expire_msec = systime_msec() + msec;
    queue_insert_tail( &dns_ctx->record_mng, &rdns->queue );
    ///debug("dns cache add entry: [%s]. ttl [%d] msec\n", query, msec );
    return OK;
}

status dns_rec_find( char * query, char * out_addr )
{   
    queue_t * q = queue_head( &dns_ctx->record_mng );
    queue_t * n = NULL;
    dns_cache_t * rdns = NULL;
    int found = 0;
    long long current_msec = systime_msec();
    
    if( queue_empty( &dns_ctx->record_mng ) ) {
        return ERROR;
    }

    while( q != queue_tail(&dns_ctx->record_mng) ) {
        n = queue_next(q);
        rdns = ptr_get_struct( q, dns_cache_t, queue );

        if( current_msec < rdns->expire_msec ) {
            if( (strlen(rdns->query) == strlen(query)) && (0 == strcmp( rdns->query, query )) ) {  
                if( out_addr ) {
                    out_addr[0] = rdns->addr[0];
                	out_addr[1] = rdns->addr[1];
                	out_addr[2] = rdns->addr[2];
                	out_addr[3] = rdns->addr[3];
                }
            	found = 1;
                if( dns_ctx->recn <= 8 ) {
                    break;
                }
            }
        } else {
            queue_remove( q );
            free(rdns);
        }
        
        q = n;
    }

    if( dns_ctx->recn > 8 ) {
        dns_ctx->recn = 0;
    }
    dns_ctx->recn++;
    return ( found ? OK : ERROR );
}

inline static char * dns_get_serv( )
{
    /// try to get gateway 
    if( strlen(config_get()->s5_serv_gw) > 0 ) {
        return config_get()->s5_serv_gw;
    } else {
        return "8.8.8.8";
    }
}

static status dns_alloc( dns_cycle_t ** cycle )
{
    queue_t * q = NULL;
    dns_cycle_t * dns = NULL;
    if( queue_empty(&dns_ctx->usable) ) {
        err("dns usable queue empty\n");
        return ERROR;
    }
    q = queue_head(&dns_ctx->usable);
    queue_remove( q );
    
    queue_insert_tail(&dns_ctx->use, q);
    dns = ptr_get_struct(q, dns_cycle_t, queue);
    *cycle = dns;
    return OK;
}

static status dns_free( dns_cycle_t * cycle )
{
    queue_remove( &cycle->queue );
    queue_insert_tail( &dns_ctx->usable, &cycle->queue );
    return OK;
}

status dns_over( dns_cycle_t * cycle )
{
	if( cycle ) {
		if( cycle->c ) {
			net_free( cycle->c );
			cycle->c = NULL;
		}
		cycle->cb = NULL;
        memset( cycle->query, 0, DOMAIN_LENGTH+1 );
        memset( &cycle->answer, 0, sizeof(dns_record_t) );
        
		dns_free( cycle );
	}
	return OK;
}

status dns_create( dns_cycle_t ** dns_cycle )
{
	dns_cycle_t * local_cycle = NULL;
    meta_t * meta = NULL;
    
	if( OK != dns_alloc( &local_cycle ) ) {
        err("dns create alloc failed\n");
        return ERROR;
    }

	do {
		if( OK != net_alloc( &local_cycle->c ) ) {
			err("dns alloc conn failed\n");
			break;
		}
        
        meta = &local_cycle->dns_meta;
        meta->start = meta->pos = meta->last = local_cycle->dns_buffer;
        meta->end = meta->start + DNS_BUFFER_LEN;
        
		local_cycle->c->data = local_cycle;
		*dns_cycle = local_cycle;
		return OK;
	} while(0);
	
	if( local_cycle ) {
		dns_over( local_cycle );
	}
	return ERROR;
}


static void dns_stop( dns_cycle_t * cycle, status rc )
{
    if( cycle && cycle->c && cycle->c->fd ) {
        event_opt( cycle->c->event, cycle->c->fd, EV_NONE );
    }
    
	if( rc != OK && rc != ERROR ) {
		err("status not support\n");
		rc = ERROR;
	}
	
	cycle->dns_status = rc;
	if( cycle->cb ) {
        cycle->cb( cycle->cb_data );
    }
}

static inline void dns_cycle_timeout( void * data )
{
	dns_stop( (dns_cycle_t *)data, ERROR );
}

status dns_response_process( dns_cycle_t * cycle )
{
	unsigned char * p = NULL;
	int state_len = 0, cur = 0;
    meta_t * meta = &cycle->dns_meta;
    
	enum  
	{
		STATE_NAME_START,
		STATE_NAME_START_NEXT,
		STATE_RDATA_START,
		STATE_RDATA,
		STATE_DATA_START,
		STATE_DATA
	} state = STATE_NAME_START;
    
	/*
    parse dns answer
	 */
    p = meta->pos + sizeof(dns_header_t) + cycle->qname_len + sizeof(dns_question_t);
	for( ; p < meta->last; p++ ) {
		if( state == STATE_NAME_START ) {
            /*
            get name frist char
            if char & 0xc0 mean name is point type
            if !(char & 0xc0) means name is string
            (string type do nothing util 0 comes)
            */
		
			if( (*p) & 0xc0 ) {
				cycle->answer.name = p;
				state = STATE_NAME_START_NEXT;
				continue;
			} else {
				if( *p == '0' )
				{	
					state = STATE_RDATA_START;
					cur = 0;
					state_len = sizeof(dns_rdata_t);
					continue;
				}
			}
		}
		if( state == STATE_NAME_START_NEXT ) {
			cur = 0;
			state_len = sizeof(dns_rdata_t);
			state = STATE_RDATA_START;
			continue;
		}
		if( state == STATE_RDATA_START ) {
			cycle->answer.rdata = (dns_rdata_t * )p;
			state = STATE_RDATA;
		}
		if( state == STATE_RDATA ) {
			cur++;
			if( cur >= state_len ) {
				state = STATE_DATA_START;
				cur = 0;
				state_len = ntohs(cycle->answer.rdata->data_len);
				continue;
			}
		}
		if( state == STATE_DATA_START ) {
			cycle->answer.rdata_data = p;
			state = STATE_DATA;
		}
		if( state == STATE_DATA ) {
			cur++;
			if( cur >= state_len ) {

                unsigned int rttl = ntohl(cycle->answer.rdata->ttl);
                unsigned short rtyp = ntohs( cycle->answer.rdata->type );
                unsigned short rdatan = ntohs(cycle->answer.rdata->data_len);

			    /// if this answer is a A TYPE answer (IPV4), return ok
			    if( rtyp == 0x0001 ) {
                    if( OK == dns_rec_find( (char*)cycle->query, NULL ) ) {
                        /// do nothing, record already in cache 
                    } else {
                        if( rttl > 0 && rdatan > 0 ) {
        			        dns_rec_add( (char*)cycle->query, (char*)cycle->answer.rdata_data, 1000*rttl );
                        }
                    }
					return OK;
				} else if ( rtyp == 0x0005 ) {
				    ///debug("dns answer type CNAME, ignore\n");
				} else if ( rtyp == 0x0002 ) {
                    ///debug("dns answer type NAME SERVER, ignore\n");
				} else if ( rtyp == 0x000f ) {
                    ///debug("dns answer type MAIL SERVER, ignore\n");
				}
				state = STATE_NAME_START;
				cur = 0;
			}
		}
	}
	return ERROR;
}

status dns_response_recv( event_t * ev )
{
	connection_t * c = ev->data;
	dns_cycle_t * cycle = c->data;
    meta_t * meta = &cycle->dns_meta;
    status rc = 0;
	dns_header_t * header = NULL;
	/// dns response contains dns request 
    size_t want_len = sizeof(dns_header_t) + cycle->qname_len + sizeof(dns_question_t);
	
    while( meta_len( meta->pos, meta->last) < want_len ) {
        ssize_t size = udp_recvs( c, meta->last, meta_len( meta->last, meta->end ) );
        if( size <= 0 ) {
            if( size == AGAIN ) {
                // add timer for recv
                timer_set_data( &c->event->timer, (void*)cycle );
                timer_set_pt( &c->event->timer, dns_cycle_timeout );
                timer_add( &c->event->timer, DNS_TIMEOUT );
                return AGAIN;
            }
            err("dns recv response failed, errno [%d]\n", errno );
            dns_stop( cycle, ERROR );
            return ERROR;
        }
        meta->last += size;
    }
	timer_del( &c->event->timer );
    
	header = (dns_header_t*)meta->pos;
    if( ntohs(header->question_count) < 1 ) {
        err("dns response question count [%d], illegal\n", header->question_count );
        dns_stop( cycle, ERROR );
        return ERROR;
    }
	if( ntohs(header->answer_count) < 1 ) {
	    err("dns response answer count [%d], illegal\n", header->answer_count );
	    dns_stop( cycle, ERROR );
	    return ERROR;
	}
	
    // make dns connection event invalidate
	rc = dns_response_process( cycle );
    dns_stop( cycle, rc );
	return rc;
}

status dns_request_send( event_t * ev )
{
	connection_t * c = ev->data;
	dns_cycle_t * cycle = c->data;
    meta_t * meta = &cycle->dns_meta;
	ssize_t rc = 0;

	while( meta_len( meta->pos, meta->last ) > 0 ) {
		rc = udp_sends( c, meta->pos, meta_len( meta->pos, meta->last ) );
		if( rc <= 0 ) {
			if( errno == EAGAIN ) {
				// add timer for send
				timer_set_data( &c->event->timer, (void*)cycle );
				timer_set_pt( &c->event->timer, dns_cycle_timeout );
				timer_add( &c->event->timer, DNS_TIMEOUT );
				return AGAIN;
			}
			err("dns send request failed, errno [%d]\n", errno );
			dns_stop( cycle, ERROR );
			return ERROR;
		}
		meta->pos += rc;
	}
	timer_del( &c->event->timer );
	// clear meta buffer
	meta->pos = meta->last = meta->start;

	event_opt( ev, c->fd, EV_R );
	ev->write_pt 	= NULL;
	ev->read_pt 	= dns_response_recv;
	return ev->read_pt( ev );
}

int dns_request_host2qname( unsigned char * host, unsigned char * qname )
{
    int i = 0;
    char stack[256] = {0};
    int stackn = 0;
    int qnamen = 0;

    while( i < strlen((char*)host) ) {

        if( host[i] == '.' ) {
            qname[qnamen++] = stackn+0x30;    /// int convert to string
            /// copy stack into qname 
            memcpy( qname+qnamen, stack, stackn );
            qnamen += stackn;
            /// clear stack
            memset( stack, 0, sizeof(stack) );
            stackn = 0;
        } else {
            /// push into stack
            stack[stackn++] = host[i];
        }
        i ++;
    }
    /// append last part
    if( stackn > 0 ) {
        qname[qnamen++] = stackn+0x30;    /// int convert to string
        memcpy( qname+qnamen, stack, stackn );
        qnamen += stackn;
    }
    
    qname[qnamen++] = 0x30; /// '0' means end 
    return qnamen;
}


static status dns_request_packet( event_t * ev )
{
    connection_t * c = ev->data;
    dns_cycle_t * cycle = c->data;
    dns_header_t * header   = NULL;
    unsigned char * qname   = NULL;
    dns_question_t * qinfo  = NULL;
    meta_t * meta = &cycle->dns_meta;

    /// fill in dns packet header 
    header = (dns_header_t*)meta->last;
    header->id = (unsigned short) htons( 0xbeef );
    header->flag = htons(0x100);
    header->question_count = htons(1);
    header->answer_count = 0;
    header->auth_count = 0;
    header->add_count = 0;
    meta->last += sizeof(dns_header_t);

	/// convert www.google.com -> 3www6google3com0
    qname = meta->last;
    cycle->qname_len = dns_request_host2qname( cycle->query, qname );
    meta->last += cycle->qname_len;
	
    qinfo = (dns_question_t*)meta->last;
    qinfo->qtype = htons(0x0001);   /// question type is IPV4
    qinfo->qclass = htons(0x0001);
    meta->last += sizeof(dns_question_t);

	event_opt( c->event, c->fd, EV_W );
	ev->write_pt = dns_request_send;
	return ev->write_pt( ev );
}

status dns_start( dns_cycle_t * cycle )
{
	connection_t * c = cycle->c;
	struct sockaddr_in addr;
	
	addr.sin_family = AF_INET;
	addr.sin_port = htons( 53 );    /// dns procotol default port 
	addr.sin_addr.s_addr = inet_addr( dns_get_serv() );

    do {
        c->fd = socket(AF_INET, SOCK_DGRAM, 0 );
        if( -1 == c->fd ) {
            err("dns socket open failed, errno [%d]\n", errno );
            break;
        }
        if( OK != net_socket_reuseaddr( c->fd ) ) {
            err("dns socket set reuseaddr failed\n" );
            break;
        }
        if( OK != net_socket_nbio( c->fd ) ) {
            err("dns socket set nonblock failed\n" );
            break;
        }
        
        memcpy( &c->addr, &addr, sizeof(c->addr) );
        c->event->read_pt = NULL;
        c->event->write_pt = dns_request_packet;
        return c->event->write_pt( c->event );
    } while(0);
    
    dns_stop( cycle, ERROR );
    return ERROR;
}

status dns_init( void )
{
    uint32 i = 0;
    if( dns_ctx ) {
        err("dns init this not empty\n");
        return ERROR;
    }
    dns_ctx = l_safe_malloc( sizeof(dns_ctx_t) + sizeof(dns_cycle_t)*MAX_NET_CON );
    if( !dns_ctx ) {
        err("dns init alloc dns pool failed, [%d]\n", errno );
        return ERROR;
    }
    
    queue_init(&dns_ctx->usable);
    queue_init(&dns_ctx->use);
    queue_init(&dns_ctx->record_mng);
    
    for( i = 0; i < MAX_NET_CON; i ++ ) {
        queue_insert( &dns_ctx->usable, &dns_ctx->pool[i].queue );
    }
    return OK;
}

status dns_end( void )
{
    if( dns_ctx ) {
        dns_cache_t * rdns = NULL;
        queue_t * q = queue_head( &dns_ctx->record_mng );
        queue_t * n = NULL;
        /// free dns cache if need 
        while( q != queue_tail(&dns_ctx->record_mng) ) {
    
            n = queue_next(q);
            
            rdns = ptr_get_struct( q, dns_cache_t, queue );
            queue_remove( q );
            free(rdns);
            
            q = n;
        }
        l_safe_free(dns_ctx);
        dns_ctx = NULL;
    }
    return OK;
}
