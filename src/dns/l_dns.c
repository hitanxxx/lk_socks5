#include "l_base.h"
#include "l_dns.h"

inline static char * l_dns_get_serv(  )
{
	return "8.8.8.8";
}

status l_dns_free( dns_cycle_t * cycle )
{
	if( cycle )
	{
		if( cycle->c )
		{
			net_free( cycle->c );
			cycle->c = NULL;
		}
		cycle->cb = NULL;
		l_safe_free( cycle );
	}
	return OK;
}

status l_dns_create( dns_cycle_t ** dns_cycle )
{
	dns_cycle_t * local_cycle = NULL;

	local_cycle = ( dns_cycle_t * )l_safe_malloc( sizeof(dns_cycle_t) );
	if( !local_cycle )
	{
		err("dns malloc cycle failed, [%d]\n", errno );
		return ERROR;
	}
	memset( local_cycle, 0, sizeof(dns_cycle_t) );  

	do 
	{
		if( OK != net_alloc( &local_cycle->c ) )
		{
			err("dns alloc conn failed\n");
			break;
		}
		if( !local_cycle->c->meta )
		{
            // udp dns request and response packet len will be less than 1400
			if( OK != meta_alloc( &local_cycle->c->meta, 1400 ) )
			{
				err("dns conn alloc meta failed\n");
				break;
			}
		}
		local_cycle->c->data = local_cycle;
		*dns_cycle = local_cycle;
		return OK;
	} while(0);
	
	if( local_cycle )
	{
		l_dns_free( local_cycle );
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
	connection_t * c = cycle->c;
	unsigned char * p = NULL;
	int state_len = 0, cur = 0;
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
    p = (unsigned char*)c->meta->pos + sizeof(dns_header_t) + cycle->qname_len + sizeof(dns_question_t);
	for( ; p < (unsigned char*)c->meta->last; p++ )
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
    status rc = 0;
	dns_header_t * header = NULL;
	socklen_t len = sizeof(c->addr);
    size_t want_len = sizeof(dns_header_t) + cycle->qname_len + sizeof(dns_question_t);
	
    while( meta_len(c->meta->pos, c->meta->last) < want_len )
    {
        ssize_t size = recvfrom( c->fd, c->meta->last, meta_len( c->meta->last, c->meta->end ), 0, (struct sockaddr*)&c->addr, &len );
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
        c->meta->last += size;
    }
	timer_del( &c->event->timer );
    
	header = (dns_header_t*)c->meta->pos;
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
	ssize_t rc = 0;

	while( meta_len( c->meta->pos, c->meta->last ) > 0 )
	{
		rc = sendto( c->fd, c->meta->pos, meta_len( c->meta->pos, c->meta->last ), 0, (struct sockaddr*)&c->addr, sizeof(c->addr) );
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
		c->meta->pos += rc;
	}
	timer_del( &c->event->timer );
	// clear meta buffer
	c->meta->pos = c->meta->last = c->meta->start;

	event_opt( ev, c->fd, EV_R );
	ev->write_pt 	= NULL;
	ev->read_pt 	= l_dns_response_recv;
	return ev->read_pt( ev );
}

static uint32_t l_dns_request_qname_conv( unsigned char * qname, unsigned char * query )
{
	unsigned char *q = qname, * s = NULL, * p = NULL;
	int query_len = l_strlen((char*)query);	

	for( s = p = query; p < query+query_len; p ++ )
	{
		if( *p == '.' )
		{
			*q++ = p-s;
			for( ; s < p; s++ )
			{
				*q++ = *s;
			}
			p++;
			s = p;
		}
	}
	
	*q++ = p-s;
	for( ; s < p; s++ )
	{
		*q++ = *s;
	}
	*q++ = '\0';
	return meta_len( qname, q );
}


status l_dns_request_prepare( event_t * ev )
{
	connection_t * c = ev->data;
	dns_cycle_t * cycle = c->data;
	dns_header_t * header   = NULL;
	unsigned char * qname   = NULL;
	dns_question_t * qinfo  = NULL;

	header = (dns_header_t*)c->meta->last;
	header->id              = (unsigned short) htons( 0xffff );
	header->flag            = htons(0x100);
	header->question_count  = htons(1);
	header->answer_count    = 0;
	header->auth_count      = 0;
	header->add_count       = 0;
    c->meta->last += sizeof(dns_header_t);
	
    qname = c->meta->last;
	cycle->qname_len = l_dns_request_qname_conv( qname, cycle->query );
    c->meta->last += cycle->qname_len;
	
	qinfo = (dns_question_t*)(c->meta->last );
	qinfo->qtype    = htons(1);
	qinfo->qclass   = htons(1);
    c->meta->last += sizeof(dns_question_t);

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
        if( OK != l_socket_reuseaddr( c->fd ) )
        {
            err("dns socket set reuseaddr failed\n" );
            break;
        }
        if( OK != l_socket_nonblocking( c->fd ) )
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


