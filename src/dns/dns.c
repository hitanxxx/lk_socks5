#include "common.h"
#include "dns.h"

typedef struct private_dns
{
    queue_t     usable;
    queue_t     use;
    dns_cycle_t pool[0];
} private_dns_t;
private_dns_t * this = NULL;

inline static char * l_dns_get_serv( void )
{
	return "8.8.8.8";
}

static status l_dns_alloc( dns_cycle_t ** cycle )
{
    queue_t * q = NULL;
    dns_cycle_t * dns = NULL;
    if( queue_empty(&this->usable) )
    {
        err("dns usable queue empty\n");
        return ERROR;
    }
    q = queue_head(&this->usable);
    queue_remove( q );
    
    queue_insert_tail(&this->use, q);
    dns = ptr_get_struct(q, dns_cycle_t, queue);
    *cycle = dns;
    return OK;
}

static status l_dns_free( dns_cycle_t * cycle )
{
    queue_remove( &cycle->queue );
    queue_insert_tail( &this->usable, &cycle->queue );
    return OK;
}

status l_dns_over( dns_cycle_t * cycle )
{
	if( cycle )
	{
		if( cycle->c )
		{
			net_free( cycle->c );
			cycle->c = NULL;
		}
		cycle->cb = NULL;
        memset( cycle->query, 0, DOMAIN_LENGTH+1 );
        memset( &cycle->answer, 0, sizeof(dns_record_t) );
        
		l_dns_free( cycle );
	}
	return OK;
}

status l_dns_create( dns_cycle_t ** dns_cycle )
{
	dns_cycle_t * local_cycle = NULL;
    meta_t * meta = NULL;
    
	if( OK != l_dns_alloc( &local_cycle ) )
    {
        err("dns create alloc failed\n");
        return ERROR;
    }

	do 
	{
		if( OK != net_alloc( &local_cycle->c ) )
		{
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
	
	if( local_cycle )
	{
		l_dns_over( local_cycle );
	}
	return ERROR;
}


static void l_dns_stop( dns_cycle_t * cycle, status rc )
{
    if( cycle && cycle->c && cycle->c->fd )
    {
        event_opt( cycle->c->event, cycle->c->fd, EV_NONE );
    }
    
	if( rc != OK && rc != ERROR )
	{
		err("status not support\n");
		rc = ERROR;
	}
	cycle->dns_status = rc;
	if( cycle->cb )
	{
		cycle->cb( cycle->cb_data );
	}
}

inline static void l_dns_cycle_timeout( void * data )
{
	l_dns_stop( (dns_cycle_t *)data, ERROR );
}

status l_dns_response_process( dns_cycle_t * cycle )
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
	 find ipv4 answer
	 */
    p = meta->pos + sizeof(dns_header_t) + cycle->qname_len + sizeof(dns_question_t);
	for( ; p < meta->last; p++ )
	{
		if( state == STATE_NAME_START )
		{
			if( *p == 0xc0 )
			{
				//debug("cycle [%p] - state name point start [%02x]\n",cycle, *p );
				cycle->answer.name = p;
				state = STATE_NAME_START_NEXT;
				continue;
			}
			else
			{
				//debug("cycle [%p] - state name str [%02x]\n", cycle, * p);
				if( *p == '0' )
				{	
					state = STATE_RDATA_START;
					cur = 0;
					state_len = sizeof(dns_rdata_t);
					continue;
				}
			}
		}
		if( state == STATE_NAME_START_NEXT )
		{
			//debug("cycle [%p] - state name next [%02x]\n",cycle, *p );
			state = STATE_RDATA_START;
			cur = 0;
			state_len = sizeof(dns_rdata_t);
			continue;
		}
		if( state == STATE_RDATA_START )
		{
			cycle->answer.rdata = (dns_rdata_t * )p;
			state = STATE_RDATA;
		}
		if( state == STATE_RDATA )
		{
			cur++;
			//debug("cycle [%p] - state rdata [%02x]\n",cycle, *p );
			if( cur >= state_len )
			{
				state = STATE_DATA_START;
				cur = 0;
				state_len = ntohs(cycle->answer.rdata->data_len);
				continue;
			}
		}
		if( state == STATE_DATA_START )
		{
			cycle->answer.data = p;
			state = STATE_DATA;
		}
		if( state == STATE_DATA )
		{
			cur++;
			//debug("cycle [%p] - state data [%02x]\n", cycle, *p );
			if( cur >= state_len )
			{
				state = STATE_NAME_START;
				cur = 0;

				if( ntohs( cycle->answer.rdata->type ) == 1 )
				{
					return OK;
				}
			}
		}
	}
	return ERROR;
}

static char* l_dns_resp_response_code2str( uint32 num )
{
    if( num == 0 )
    {
        return "no error";
    }
    else if ( num == 1 )
    {
        return "req format error";
    }
    else if ( num == 2 )
    {
        return "server failure";
    }
    else if ( num == 3 )
    {
        return "query name error";
    }
    else if ( num == 4 )
    {
        return "no implemented";
    }
    else if ( num == 5 )
    {
        return "refused";
    }
    return "unknow";
}

status l_dns_response_recv( event_t * ev )
{
	connection_t * c = ev->data;
	dns_cycle_t * cycle = c->data;
    meta_t * meta = &cycle->dns_meta;
    status rc = 0;
	dns_header_t * header = NULL;
    size_t want_len = sizeof(dns_header_t) + cycle->qname_len + sizeof(dns_question_t);
	
    while( meta_len( meta->pos, meta->last) < want_len )
    {
        ssize_t size = udp_recvs( c, meta->last, meta_len( meta->last, meta->end ) );
        if( size <= 0 )
        {
            if( errno == EAGAIN )
            {
                // add timer for recv
                timer_set_data( &c->event->timer, (void*)cycle );
                timer_set_pt( &c->event->timer, l_dns_cycle_timeout );
                timer_add( &c->event->timer, DNS_TIMEOUT );
                return AGAIN;
            }
            err("dns recv response failed, errno [%d]\n", errno );
            l_dns_stop( cycle, ERROR );
            return ERROR;
        }
        meta->last += size;
    }
	timer_del( &c->event->timer );
    
	header = (dns_header_t*)meta->pos;
	if( ntohs(header->flag) & 0xf )
	{
		err("dns response error, flag's resp code %x, [%s]\n", ntohs(header->flag)&0xf, l_dns_resp_response_code2str(ntohs(header->flag)&0xf) );
		l_dns_stop( cycle, ERROR );
		return ERROR;
	}
    // make dns connection event invalidate
	rc = l_dns_response_process( cycle );
    l_dns_stop( cycle, rc );
	return rc;
}

status l_dns_request_send( event_t * ev )
{
	connection_t * c = ev->data;
	dns_cycle_t * cycle = c->data;
    meta_t * meta = &cycle->dns_meta;
	ssize_t rc = 0;

	while( meta_len( meta->pos, meta->last ) > 0 )
	{
		rc = udp_sends( c, meta->pos, meta_len( meta->pos, meta->last ) );
		if( rc <= 0 )
		{
			if( errno == EAGAIN )
			{
				// add timer for send
				timer_set_data( &c->event->timer, (void*)cycle );
				timer_set_pt( &c->event->timer, l_dns_cycle_timeout );
				timer_add( &c->event->timer, DNS_TIMEOUT );
				return AGAIN;
			}
			err("dns send request failed, errno [%d]\n", errno );
			l_dns_stop( cycle, ERROR );
			return ERROR;
		}
		meta->pos += rc;
	}
	timer_del( &c->event->timer );
	// clear meta buffer
	meta->pos = meta->last = meta->start;

	event_opt( ev, c->fd, EV_R );
	ev->write_pt 	= NULL;
	ev->read_pt 	= l_dns_response_recv;
	return ev->read_pt( ev );
}

uint32_t l_dns_request_qname_conv( unsigned char * qname, unsigned char * query )
{
    unsigned char * host = query;
    unsigned char * dns = qname;
    unsigned char * s = host;
    int32_t i = 0;
    size_t len = 0;
    
    enum
    {
        s_str = 0,
        s_point
    } state = 0;
    
    strcat( (char*)query, "." );
    while( host < query + l_strlen(query) )
    {
        if( state == s_str )
        {
            if( *host == '.' )
            {
                len = host-s;
                *dns++ = len;
                for( i = 0; i < len; i ++ )
                {
                    *(dns+i) = *(s+i);
                }
                dns += len;
                state = s_point;
            }
        }
        else if ( state == s_point )
        {
            s = host;
            state = s_str;
        }
        host++;
    }
    *dns++ = '\0';
    return meta_len( qname, dns );
}

status l_dns_request_prepare( event_t * ev )
{
    connection_t * c = ev->data;
    dns_cycle_t * cycle = c->data;
    dns_header_t * header   = NULL;
    unsigned char * qname   = NULL;
    dns_question_t * qinfo  = NULL;
    meta_t * meta = &cycle->dns_meta;

    header = (dns_header_t*)meta->last;
    header->id              = (unsigned short) htons( 0xffff );
    header->flag            = htons(0x100);
    header->question_count  = htons(1);
    header->answer_count    = 0;
    header->auth_count      = 0;
    header->add_count       = 0;
    meta->last += sizeof(dns_header_t);
	
    qname = meta->last;
    cycle->qname_len = l_dns_request_qname_conv( qname, cycle->query );
    meta->last += cycle->qname_len;
	
    qinfo = (dns_question_t*)meta->last;
    qinfo->qtype    = htons(1);
    qinfo->qclass   = htons(1);
    meta->last += sizeof(dns_question_t);

	event_opt( c->event, c->fd, EV_W );
	ev->write_pt = l_dns_request_send;
	return ev->write_pt( ev );
}

status l_dns_start( dns_cycle_t * cycle )
{
	connection_t * c = cycle->c;
	struct sockaddr_in addr;
	
	addr.sin_family 		= AF_INET;
	addr.sin_port 			= htons( 53 );
	addr.sin_addr.s_addr 	= inet_addr( l_dns_get_serv() );

    do
    {
        c->fd = socket(AF_INET, SOCK_DGRAM, 0 );
        if( -1 == c->fd )
        {
            err("dns socket open failed, errno [%d]\n", errno );
            break;
        }
        if( OK != net_socket_resueaddr( c->fd ) )
        {
            err("dns socket set reuseaddr failed\n" );
            break;
        }
        if( OK != net_socket_nbio( c->fd ) )
        {
            err("dns socket set nonblock failed\n" );
            break;
        }
        
        memcpy( &c->addr, &addr, sizeof(c->addr) );
        c->event->read_pt       = NULL;
        c->event->write_pt      = l_dns_request_prepare;
        return c->event->write_pt( c->event );
    } while(0);
    
    l_dns_stop( cycle, ERROR );
    return ERROR;
}

status l_dns_init( void )
{
    uint32 i = 0;
    if( this )
    {
        err("dns init this not empty\n");
        return ERROR;
    }
    this = l_safe_malloc( sizeof(private_dns_t) + sizeof(dns_cycle_t)*MAX_NET_CON );
    if( !this )
    {
        err("dns init alloc dns pool failed, [%d]\n", errno );
        return ERROR;
    }
    memset( this, 0, sizeof(private_dns_t) + sizeof(dns_cycle_t)*MAX_NET_CON );
    
    queue_init(&this->usable);
    queue_init(&this->use);
    
    for( i = 0; i < MAX_NET_CON; i ++ )
    {
        queue_insert( &this->usable, &this->pool[i].queue );
    }
    return OK;
}

status l_dns_end( void )
{
    if( this )
    {
        l_safe_free(this);
        this = NULL;
    }
    return OK;
}
