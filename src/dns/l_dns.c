#include "l_base.h"
#include "l_dns.h"

static char g_dns_serv[64] = {0};

char * l_dns_get_serv(  )
{
	char dnsname[1024] = {0};
	int tmp = 0;
    int index = 0;

#if(1)
	return "8.8.8.8";
#endif

	if( strlen(g_dns_serv) > 0 )
	{
		return g_dns_serv;
	}
	else
	{
		FILE *fp = fopen( "/etc/resolv.conf", "r");
	    if (fp == NULL)
	    {
	        return NULL;
	    }

		memset(dnsname, '\0', sizeof(dnsname));
		while ((tmp = fgetc(fp)) != EOF)
	    {
	        dnsname[index++] = (char)tmp;
	        if (tmp == '\n')
	        {
	            dnsname[--index] = '\0';
	            index = 0;
	            if (strstr(dnsname, "nameserver") != 0)
	            {
					snprintf( g_dns_serv, sizeof(g_dns_serv), "%s", &dnsname[strlen("nameserver") + 1] );					
					break;
	            }
	            memset(dnsname, '\0', sizeof(dnsname));
	        }
	    }
	    fclose(fp);
	    fp = NULL;
	}
	
    return g_dns_serv;
}


status l_dns_create( dns_cycle_t ** dns_cycle )
{
	dns_cycle_t * local_cycle = NULL;

	local_cycle = ( dns_cycle_t * ) l_safe_malloc( sizeof(dns_cycle_t) );
	if( !local_cycle )
	{
		err("malloc dns cycle failed\n");
		return ERROR;
	}
	memset( local_cycle, 0, sizeof(dns_cycle_t) );  
	if( OK != net_alloc( &local_cycle->c ) )
	{
		err("alloc cycle's connection failed\n");
		l_safe_free( local_cycle );
		return ERROR;
	}
	if( !local_cycle->c->meta ) 
	{
		if( OK != meta_alloc( &local_cycle->c->meta, 1400 ) ) 
		{
			err(" dns connection meta alloc\n" );
			l_safe_free( local_cycle );
			return ERROR;
		}
	}
	local_cycle->c->data = local_cycle;
	*dns_cycle = local_cycle;
	return OK;
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

static void l_dns_stop( dns_cycle_t * cycle, status rc )
{
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

static void l_dns_cycle_timeout( void * data )
{
	dns_cycle_t * cycle = data;

	debug("dns timeout\n");
	l_dns_stop( cycle, ERROR);
}

status l_dns_response_process( dns_cycle_t * cycle )
{
	connection_t * c = cycle->c;
	dns_header_t * header = ( dns_header_t* )c->meta->pos;
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

#if(0)
	debug("The dns response info\n");
	debug("%d Questions\n", ntohs( header->question_count) );
	debug("%d Answers\n", ntohs( header->answer_count )) ;
	debug("%d Authoritative\n", ntohs( header->auth_count) );
	debug("%d Additional records\n", ntohs( header->add_count ));
#endif
	p = c->meta->pos + sizeof(dns_header_t) + cycle->qname_len + sizeof(dns_question_t);

	/*
	 find ipv4 answer
	 */
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

status l_dns_response_recv( event_t * ev )
{
	connection_t * c = ev->data;
	dns_cycle_t * cycle = c->data;
	int rc = 0;
	dns_header_t * header = NULL;
	socklen_t len = sizeof(c->addr);
	
	rc = recvfrom( c->fd, c->meta->last, meta_len( c->meta->last, c->meta->end ), 0, (struct sockaddr*)&c->addr, &len );
	if( rc <= 0 )	
	{
		if( errno == EAGAIN )
		{
			// add timer for recv
			timer_set_data( &c->event.timer, (void*)cycle );
			timer_set_pt( &c->event.timer, l_dns_cycle_timeout );
			timer_add( &c->event.timer, DNS_TIMEOUT );
			return AGAIN;
		}
		err("dns recv response failed, errno [%d]\n", errno );
		l_dns_stop( cycle, ERROR );
		return ERROR;
	}
	c->meta->last += rc;

	if( rc < sizeof(dns_header_t) )
	{
		err("dns response size < dns header.\n");
		l_dns_stop( cycle, ERROR );
		return ERROR;
	}
	header = (dns_header_t*)c->meta->pos;
	if( ntohs(header->flag) & 0xf )
	{
		err("dns response error, flag [%x] resp code %x\n", header->flag, header->flag & 0xf );
		l_dns_stop( cycle, ERROR );
		return ERROR;
	}
	if( rc < (sizeof(dns_header_t) + cycle->qname_len + sizeof(dns_question_t) ) )
	{
		debug("dns recv not complate\n");
		timer_set_data( &c->event.timer, (void*)cycle );
		timer_set_pt( &c->event.timer, l_dns_cycle_timeout );
		timer_add( &c->event.timer, DNS_TIMEOUT );
		return AGAIN;
	}
	timer_del( &c->event.timer );

	rc = l_dns_response_process( cycle );
	if( rc == OK )
	{
		l_dns_stop( cycle, OK );  
	}
	else
	{
		l_dns_stop( cycle, ERROR );
	}
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
				timer_set_data( &c->event.timer, (void*)cycle );
				timer_set_pt( &c->event.timer, l_dns_cycle_timeout );
				timer_add( &c->event.timer, DNS_TIMEOUT );
				return AGAIN;
			}
			err("dns send request failed, errno [%d]\n", errno );
			l_dns_stop( cycle, ERROR );
			return ERROR;
		}
		c->meta->pos += rc;
	}
	timer_del( &c->event.timer );
	// clear meta buffer
	c->meta->pos = c->meta->last = c->meta->start;

	event_opt( ev, c->fd, EV_R );
	ev->write_pt = NULL;
	ev->read_pt = l_dns_response_recv;
	return ev->read_pt( ev );
}

static int l_dns_request_qname_conv( unsigned char * qname, unsigned char * query )
{
	unsigned char *q = qname, * s = NULL, * p = NULL;
	int query_len = strlen(query);	

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
	return q - qname;
}


status l_dns_request_prepare( event_t * ev )
{
	connection_t * c = ev->data;
	dns_cycle_t * cycle = c->data;
	unsigned char * qname = NULL;
	dns_header_t * header = NULL;
	dns_question_t * qinfo;

	header = (dns_header_t*)c->meta->last;
	header->id = (unsigned short) htons( getpid() );
	header->flag = htons(0x100);
	header->question_count = htons(1);
	header->answer_count = 0;
	header->auth_count = 0;
	header->add_count = 0;
	
	qname = (unsigned char*)( c->meta->last + sizeof(dns_header_t));
	cycle->qname_len = l_dns_request_qname_conv( qname, cycle->query );
	
	qinfo = (dns_question_t*)(c->meta->last + sizeof(dns_header_t) + cycle->qname_len );
	qinfo->qtype = htons(1);
	qinfo->qclass = htons(1);
	
	c->meta->last = c->meta->pos + sizeof(dns_header_t) + cycle->qname_len + sizeof(dns_question_t);

	ev->write_pt = l_dns_request_send;
	return ev->write_pt( ev );
}

status l_dns_start( dns_cycle_t * cycle )
{
	connection_t * c = cycle->c;
	struct sockaddr_in addr;
	int rc = 0;
	
	addr.sin_family = AF_INET;
	addr.sin_port 	= htons( 53 );
	addr.sin_addr.s_addr = inet_addr( cycle->dns_serv );

	c->fd = socket(AF_INET, SOCK_DGRAM, 0 );
	if( ERROR == c->fd )
	{
		err("dns socket open failed, errno [%d]\n", errno );
		l_dns_stop( cycle, ERROR );
		return ERROR;
	}
	if( OK != l_socket_reuseaddr( c->fd ) ) {
		err(" reuseaddr failed\n" );
		l_dns_stop( cycle, ERROR );
		return ERROR;
	}
	if( OK != l_socket_nonblocking( c->fd ) ) {
		err(" nonblock failed\n" );
		l_dns_stop( cycle, ERROR );
		return ERROR;
	}
	
	memcpy( &c->addr, &addr, sizeof(c->addr) );
	c->event.read_pt = NULL;
	c->event.write_pt = l_dns_request_prepare;
	return c->event.write_pt( &c->event );
}


