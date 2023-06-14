#include "common.h"
#include "dns.h"
#include "s5_local.h"
#include "s5_server.h"

#define S5_USER_NAME_MAX		16
#define S5_USER_PASSWD_MAX		16
#define S5_USER_AUTH_FILE_LEN  4096

typedef struct s5_user {
	char		name[S5_USER_NAME_MAX];
	char 		passwd[S5_USER_PASSWD_MAX];
	queue_t		queue;
} s5_user_t;

typedef struct 
{
    queue_t g_users;
    mem_page_t * g_user_mempage;

    queue_t         usable;
    queue_t         use;
    socks5_cycle_t  pool[0];
} g_s5_t;
static g_s5_t * g_s5_ctx = NULL;

status s5_alloc( socks5_cycle_t ** s5 )
{
    queue_t * q = NULL;
    socks5_cycle_t * n_s5 = NULL;

    if( queue_empty(&g_s5_ctx->usable) )
    {
        err("s5 alloc usable empty\n");
        return ERROR;
    }
    q = queue_head( &g_s5_ctx->usable );
    queue_remove(q);

    queue_insert_tail(&g_s5_ctx->use, q);
    n_s5 = ptr_get_struct(q, socks5_cycle_t, queue);
    *s5 = n_s5;
    return OK;
}


static status s5_serv_usr_user_find( char * name, s5_user_t ** user )
{
	queue_t * q;
	s5_user_t * t = NULL;

	for( q = queue_head( &g_s5_ctx->g_users ); q != queue_tail( &g_s5_ctx->g_users ); q = queue_next(q) ) {
		t = ptr_get_struct( q, s5_user_t, queue );
		if( t && l_strlen(t->name) == strlen(name) && memcmp( t->name, name, strlen(name) ) == 0 ) {
			if( user ) {
				*user = t;
			}
			return OK;
		}
	}
	return ERROR;
}

static status s5_serv_usr_user_add( char * name, char * passwd )
{
    s5_user_t * user = mem_page_alloc( g_s5_ctx->g_user_mempage, sizeof(s5_user_t) );
	if( !user ) {
		err("alloc new user\n");
		return ERROR;
	}
	memset( user, 0, sizeof(s5_user_t) );

	memcpy( user->name, name, strlen(name) );
	memcpy( user->passwd, passwd, strlen(passwd) );
	queue_insert_tail( &g_s5_ctx->g_users, &user->queue );

#if(1)
	// show all users
	queue_t * q;
	s5_user_t * t = NULL;
	for( q = queue_head( &g_s5_ctx->g_users ); q != queue_tail( &g_s5_ctx->g_users ); q = queue_next(q) ) {
		t = ptr_get_struct( q, s5_user_t, queue );
		debug("queue show [%s] --- [%s]\n", t->name, t->passwd );
	}
#endif
    return OK;
}


status s5_free( socks5_cycle_t * s5 )
{
	queue_t * q = &s5->queue;

    memset( &s5->invite, 0x0, sizeof(socks5_message_invite_t) );
    memset( &s5->advance, 0x0, sizeof(socks5_message_advance_t) );

    if( s5->down )
    {
        net_free( s5->down );
        s5->down     = NULL;
    }
    if( s5->up )
    {
        net_free( s5->up );
        s5->up       = NULL;
    }
    if( s5->dns_cycle )
    {
        dns_over( s5->dns_cycle );
        s5->dns_cycle = NULL;
    }

	s5->recv_down_err = 0;
	s5->recv_up_err = 0;
	
    queue_remove( q );
    queue_insert_tail(&g_s5_ctx->usable, q);
    return OK;
}

void s5_timeout( void * data )
{
    s5_free( (socks5_cycle_t *)data );
}


static status s5_traffic_recv( event_t * ev )
{
	connection_t * c = ev->data;
    socks5_cycle_t * s5 = c->data;
    connection_t * down = s5->down;
	connection_t * up = s5->up;
	
	ssize_t recvn = 0;
   
    timer_set_data( &ev->timer, (void*)s5 );
    timer_set_pt( &ev->timer, s5_timeout );
    timer_add( &ev->timer, SOCKS5_TIME_OUT );

	while( down->meta->end > down->meta->last )
	{
		recvn = down->recv( down, down->meta->last, down->meta->end - down->meta->last );
		if( recvn < 0 )
		{
			if( recvn == ERROR )
			{
			    err("s5 down recv error\n");
				s5->recv_down_err = 1;
			}
			break;
		}
		down->meta->last += recvn;
	}

	if( down->meta->pos == down->meta->last && s5->recv_down_err == 1 )
	{
	    err("s5 down error. meta clear. goto free\n");
	    event_opt( down->event, down->fd, EV_NONE );
        event_opt( up->event, up->fd, EV_NONE );
		s5_free(s5);
		return ERROR;
	}

	if( down->meta->last > down->meta->pos )
	{
		event_opt( down->event, down->fd, down->event->trigger_type_previously & ~EV_R );
		event_opt( up->event, up->fd, up->event->trigger_type_previously | EV_W );
		return up->event->write_pt( up->event );
	}
	return AGAIN;
}


static int s5_traffic_send( event_t * ev )
{	
	connection_t * c = ev->data;
    socks5_cycle_t * s5 = c->data;
    connection_t * down = s5->down;
	connection_t * up = s5->up;
	ssize_t sendn = 0;

	timer_set_data( &ev->timer, (void*)s5 );
    timer_set_pt( &ev->timer, s5_timeout );
    timer_add( &ev->timer, SOCKS5_TIME_OUT );

	while( down->meta->last > down->meta->pos )
	{
		sendn = up->send( up, down->meta->pos, down->meta->last - down->meta->pos );
		if( sendn < 0 )
		{
			if( sendn == ERROR )
			{
			    err("s5 up send error\n");
				s5_free(s5);
				return ERROR;
			}
			timer_add( &ev->timer, SOCKS5_TIME_OUT );
			return AGAIN;
		}
		down->meta->pos += sendn;
	}

	if( s5->recv_down_err == 1 )
	{
	    err("s5 free, down already error, close down and up event.\n");
        
        event_opt( down->event, down->fd, EV_NONE );
        event_opt( up->event, up->fd, EV_NONE );
		s5_free(s5);
		return ERROR;
	}

	down->meta->last = down->meta->pos = down->meta->start;

	event_opt( down->event, down->fd, down->event->trigger_type_previously | EV_R );
	event_opt( up->event, up->fd, up->event->trigger_type_previously & ~EV_W );
	return down->event->read_pt( down->event );
}


static status s5_traffic_back_recv( event_t * ev )
{
    connection_t * c = ev->data;
    socks5_cycle_t * s5 = c->data;
    connection_t * down = s5->down;
	connection_t * up = s5->up;
	
	ssize_t recvn = 0;
   
    timer_set_data( &ev->timer, (void*)s5 );
    timer_set_pt( &ev->timer, s5_timeout );
    timer_add( &ev->timer, SOCKS5_TIME_OUT );

	while( up->meta->end > up->meta->last )
	{
		recvn = up->recv( up, up->meta->last, up->meta->end - up->meta->last );
		if( recvn < 0 )
		{
			if( recvn == ERROR )
			{   
			    err("s5 up recv error\n");
				s5->recv_up_err = 1;
			}
			break;
		}
		up->meta->last += recvn;
	}

	if( up->meta->pos == up->meta->last && s5->recv_up_err == 1 )
	{
	    err("s5 up error. meta clear. goto free\n");
	    event_opt( down->event, down->fd, EV_NONE );
        event_opt( up->event, up->fd, EV_NONE );
		s5_free(s5);
		return ERROR;
	}

	if( up->meta->last > up->meta->pos )
	{
		event_opt( up->event, up->fd, up->event->trigger_type_previously & ~EV_R );
		event_opt( down->event, down->fd, down->event->trigger_type_previously | EV_W );
		return down->event->write_pt( down->event );
	}
	return AGAIN;
}

static int s5_traffic_back_send( event_t * ev )
{
	connection_t * c = ev->data;
    socks5_cycle_t * s5 = c->data;
    connection_t * down = s5->down;
	connection_t * up = s5->up;
	
	ssize_t sendn = 0;
	
	timer_set_data( &ev->timer, (void*)s5 );
    timer_set_pt( &ev->timer, s5_timeout );
    timer_add( &ev->timer, SOCKS5_TIME_OUT );

	while( up->meta->last > up->meta->pos )
	{
		sendn = down->send( down, up->meta->pos, up->meta->last - up->meta->pos );
		if( sendn < 0 )
		{
			if( sendn == ERROR )
			{
			    err("s5 down send error\n");
				s5_free(s5);
				return ERROR;
			}
			timer_add( &ev->timer, SOCKS5_TIME_OUT );
			return AGAIN;
		}
		up->meta->pos += sendn;
	}

	if( s5->recv_up_err == 1 )
	{
	    err("s5 free, up already error. close down and up event.\n");
        event_opt( down->event, down->fd, EV_NONE );
        event_opt( up->event, up->fd, EV_NONE );
		s5_free(s5);
		return ERROR;
	}

	up->meta->last = up->meta->pos = up->meta->start;

	event_opt( down->event, down->fd, down->event->trigger_type_previously & ~EV_W );
	event_opt( up->event, up->fd, up->event->trigger_type_previously | EV_R );
	return up->event->read_pt( up->event );
}


status socks5_traffic_transfer( event_t * ev )
{
	/*
	 when client mode, connection means upstream
	 when server mode, connection means downstream 
	*/

    connection_t * c = ev->data;
    socks5_cycle_t * s5 = c->data;
	connection_t * down = s5->down;
	connection_t * up = s5->up;
	
    s5->down->event->read_pt 	= s5_traffic_recv;
	s5->up->event->write_pt		= s5_traffic_send;
	
    s5->up->event->read_pt 		= s5_traffic_back_recv;
	s5->down->event->write_pt	= s5_traffic_back_send;

	// init down stream traffic buffer
	if( !down->page )
    {
        if( OK != mem_page_create(&down->page, L_PAGE_DEFAULT_SIZE) )
        {
            err("webser down page create failed\n");
            s5_free(s5);
            return ERROR;
        }
    }
    if( !down->meta )
    {
		if( OK != meta_alloc_form_mempage( down->page, 4096, &down->meta ) )
        {
            err("s5 alloc down meta failed\n");
            s5_free(s5);
            return ERROR;
        }
	}
	

	// init up stream traffic buffer
	if( !up->page )
    {
        if( OK != mem_page_create(&up->page, L_PAGE_DEFAULT_SIZE) )
        {
            err("webser up page create failed\n");
            s5_free(s5);
            return ERROR;
        }
    }
    if( !up->meta )
    {
		if( OK != meta_alloc_form_mempage( up->page, 4096, &up->meta ) )
        {
            err("s5 alloc up meta failed\n");
            s5_free(s5);
            return ERROR;
        }
	}

	// init cache buffer
	down->meta->pos = down->meta->last = down->meta->start;
	up->meta->pos = up->meta->last = up->meta->start;
	
    event_opt( s5->up->event, s5->up->fd, EV_R );	
    event_opt( s5->down->event, s5->down->fd, EV_R );
	
    return s5->down->event->read_pt( s5->down->event );
}

static status socks5_server_msg_s5_adv_resp( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    status rc = 0;
	meta_t * meta = down->meta;

	while( meta->last - meta->pos > 0 )
	{
		rc = down->send( down, meta->pos, meta->last - meta->pos );
		if( rc < 0 )
	    {
	        if( rc == ERROR )
	        {
	            err("s5 server adv resp send failed\n");
	            s5_free(s5);
	            return ERROR;
	        }
	        timer_set_data( &ev->timer, down );
	        timer_set_pt( &ev->timer, s5_timeout );
	        timer_add( &ev->timer, SOCKS5_TIME_OUT );
	        return AGAIN;
	    }
		meta->pos += rc;
	}
    timer_del( &ev->timer );
	debug("s5 [%p] serv adv resp send success\n", s5);
	
    ev->read_pt		= socks5_traffic_transfer;
    ev->write_pt	= NULL;
    return ev->read_pt( ev );
}

static status socks5_server_msg_s5_adv_resp_build( event_t * ev )
{
    connection_t * up = ev->data;
    socks5_cycle_t * s5 = up->data;
    connection_t * down = s5->down;
	meta_t * meta = down->meta;
   
    socks5_message_advance_response_t * response = NULL;

    meta->last = meta->pos = meta->start;
    response = ( socks5_message_advance_response_t* )meta->last;
    response->ver 		=	0x05;
    response->rep		=	0x00;
    response->rsv		=	0x00;
    response->atyp		=	0x01;
    response->bnd_addr	=	htons((uint16_t)up->addr.sin_addr.s_addr);
    response->bnd_port	=	htons(up->addr.sin_port);
    meta->last += sizeof(socks5_message_advance_response_t);

    // make up event invalidat
    event_opt( up->event, up->fd, EV_NONE );

    event_opt( down->event, down->fd, EV_W );
	debug("s5 [%p] serv adv resp build success\n");
	
    down->event->write_pt = socks5_server_msg_s5_adv_resp;
    return down->event->write_pt( down->event );
}

static status socks5_server_up_connect_check( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * s5 = up->data;

    if( OK != net_socket_check_status( up->fd ) )
    {
        err("s5 server connect remote failed\n" );
        s5_free( s5 );
        return ERROR;
    }
    net_socket_nodelay( up->fd );
    timer_del( &ev->timer );

    ev->read_pt		= NULL;
    ev->write_pt 	= socks5_server_msg_s5_adv_resp_build;
    return ev->write_pt( ev );
}

static status socks5_server_up_connect( event_t * ev )
{
    connection_t * up = ev->data;
    socks5_cycle_t * s5 = up->data;
    status rc = 0;

    rc = net_connect( s5->up, &s5->up->addr );
    if( rc == ERROR )
    {
        err("s5 server connect up failed\n");
        s5_free( s5 );
        return ERROR;
    }
    ev->read_pt 	= NULL;
    ev->write_pt 	= socks5_server_up_connect_check;
    event_opt( ev, up->fd, EV_W );
    if( rc == AGAIN )
    {
        timer_set_data( &ev->timer, s5 );
        timer_set_pt( &ev->timer, s5_timeout );
        timer_add( &ev->timer, SOCKS5_TIME_OUT );
        return AGAIN;
    }
    return ev->write_pt( ev );
}

static status socks5_server_down_try_read( event_t * ev  )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;

    if( OK != net_socket_check_status( down->fd ) )
    {
        err("s5 server check down fd status error\n");
        s5_free( s5 );
        return ERROR;
    }
    return OK;
}


static void socks5_server_up_addr_get_cb( void * data )
{
    socks5_cycle_t * s5 = data;
    dns_cycle_t * dns_cycle = s5->dns_cycle;
    char ipstr[128] = {0};

    if( dns_cycle )
    {
        if( OK == dns_cycle->dns_status )
        {
            snprintf( ipstr, sizeof(ipstr), "%d.%d.%d.%d",
                dns_cycle->answer.rdata->data[0],
                dns_cycle->answer.rdata->data[1],
                dns_cycle->answer.rdata->data[2],
                dns_cycle->answer.rdata->data[3]
            );
            
            s5->up->addr.sin_family		= AF_INET;
            //memcpy( &s5->up->addr.sin_addr, s5->advance.addr_port, sizeof(uint16_t) );
            s5->up->addr.sin_port 		= *(uint16_t*)s5->advance.addr_port;
            s5->up->addr.sin_addr.s_addr = inet_addr( ipstr );
            
            s5->up->event->read_pt 		= NULL;
            s5->up->event->write_pt		= socks5_server_up_connect;
            s5->up->event->write_pt( s5->up->event );
        }
        else
        {
            err("socks5 server dns resolv failed\n");
            s5_free( s5 );
        }
    }
}

static status socks5_server_up_addr_get( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    char ipstr[128] = {0}, portstr[128] = {0};
    status rc = 0;

    down->event->read_pt 	= socks5_server_down_try_read;
    down->event->write_pt 	= NULL;
    event_opt( down->event, down->fd, EV_R );

    if( OK != net_alloc( &s5->up ) )
    {
        err("s5 server up alloc failed\n" );
        s5_free( s5 );
        return ERROR;
    }
    s5->up->data = s5;

    if( s5->advance.atyp == S5_REQ_TYPE_IPV4 )
    {
        int local_port = 0;
		uint16_t addr_port = 0;
        // ip type address
        snprintf( ipstr, sizeof(ipstr), "%d.%d.%d.%d",
            		(unsigned char )s5->advance.addr_str[0],
            		(unsigned char )s5->advance.addr_str[1],
            		(unsigned char )s5->advance.addr_str[2],
            		(unsigned char )s5->advance.addr_str[3] );
		memcpy( &addr_port, s5->advance.addr_port, sizeof(uint16_t) ); 
        local_port = ntohs( addr_port );
        snprintf( portstr, sizeof(portstr), "%d", local_port );

        s5->up->addr.sin_family			= AF_INET;
        s5->up->addr.sin_port 			= htons( local_port );
        s5->up->addr.sin_addr.s_addr 	= inet_addr( ipstr );

        s5->up->event->read_pt 			= NULL;
        s5->up->event->write_pt 		= socks5_server_up_connect;
        return s5->up->event->write_pt( s5->up->event );
    }
    else if ( s5->advance.atyp == S5_REQ_TYPE_DOMAIN )
    {
        if( l_strlen( s5->advance.addr_str ) > DOMAIN_LENGTH )
        {
            err("s5 server req domain too long\n");
            s5_free( s5 );
            return ERROR;
        }
        // domain type address
        rc = dns_create( &s5->dns_cycle );
        if( rc == ERROR )
        {
            err("s5 server dns cycle create failed\n");
            s5_free( s5 );
            return ERROR;
        }
        memcpy( s5->dns_cycle->query, s5->advance.addr_str, sizeof(s5->dns_cycle->query)-1 );
        s5->dns_cycle->cb 		= socks5_server_up_addr_get_cb;
        s5->dns_cycle->cb_data 	= s5;
        return dns_start( s5->dns_cycle );
    }
    err("s5 server not support socks5 request atyp [%x]\n", s5->advance.atyp );
    s5_free( s5 );
    return ERROR;
}


static status socks5_server_msg_s5_adv_recv( event_t * ev )
{
    unsigned char * p = NULL;
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    meta_t * meta = down->meta;
    enum{
        ver = 0,
        cmd,
        rsv,
        atyp,
        dst_addr_ipv4,
        dst_addr_ipv6,
        dst_addr_domain_len,
        dst_addr_domain,
        dst_port,
        dst_port_end
    } state;

    /*
     s5 msg advance format
        char  char  char  char   ...    char*2
        VER | CMD | RSV | ATYP | ADDR | PORT
     */

    while( 1 )
    {
        if( meta->last - meta->pos <= 0 )
        {
            ssize_t rc = down->recv( down, meta->last, meta->end - meta->last );
            if( rc < 0 )
            {
                if( rc == ERROR )
                {
                    err("s5 server advance req recv failed\n");
                    s5_free( s5 );
                    return ERROR;
                }
                timer_set_data( &ev->timer, (void*)s5 );
                timer_set_pt( &ev->timer, s5_timeout );
                timer_add( &ev->timer, SOCKS5_TIME_OUT );
                return AGAIN;
            }
            meta->last += rc;
        }

        state = s5->advance.state;
        for( ; meta->pos < meta->last; meta->pos ++ )
        {
            p = meta->pos;
            if( state == ver )
            {
                /*
                    ver always 0x05
                */
                s5->advance.ver = *p;
                state = cmd;
                continue;
            }
            if( state == cmd )
            {
                /*
                    socks5 support cmd value
                    01				connect
                    02				bind
                    03				udp associate
                */
                s5->advance.cmd = *p;
                state = rsv;
                continue;
            }
            if( state == rsv )
            {
                // rsv means resverd
                s5->advance.rsv = *p;
                state = atyp;
                continue;
            }
            if( state == atyp )
            {
                s5->advance.atyp = *p;
                /*
                    atyp		type		length
                    0x01		ipv4		4
                    0x03		domain		first octet
                    0x04		ipv6		16
                */
                if( s5->advance.atyp == S5_REQ_TYPE_IPV4 ) {
                    state = dst_addr_ipv4;
                    s5->advance.addr_recv = 0;
                    continue;
                }
                else if ( s5->advance.atyp == S5_REQ_TYPE_IPV6 )
                {
                    state = dst_addr_ipv6;
                    s5->advance.addr_recv = 0;
                    continue;
                }
                else if ( s5->advance.atyp == S5_REQ_TYPE_DOMAIN )
                {
                    state = dst_addr_domain_len;
                    s5->advance.addr_recv 	= 0;
                    s5->advance.addr_len 	= 0;
                    continue;
                }
                err("s5 server request atyp [%d] not support\n", s5->advance.atyp );
                s5_free( s5 );
                return ERROR;
            }
            if( state == dst_addr_ipv4 )
            {
                s5->advance.addr_str[(int)s5->advance.addr_recv++] = *p;
                if( s5->advance.addr_recv == 4 )
                {
                    state = dst_port;
                    continue;
                }
            }
            if( state == dst_addr_ipv6 )
            {
                s5->advance.addr_str[(int)s5->advance.addr_recv++] = *p;
                if( s5->advance.addr_recv == 16 )
                {
                    state = dst_port;
                    continue;
                }
            }
            if( state == dst_addr_domain_len )
            {
                s5->advance.addr_len = l_max( *p, 0 );
                state = dst_addr_domain;
                continue;
            }
            if( state == dst_addr_domain )
            {
                s5->advance.addr_str[(int)s5->advance.addr_recv++] = *p;
                if( s5->advance.addr_recv == s5->advance.addr_len )
                {
                    state = dst_port;
                    continue;
                }
            }
            if( state == dst_port )
            {
                s5->advance.addr_port[0] = *p;
                state = dst_port_end;
                continue;
            }
            if( state == dst_port_end )
            {
                s5->advance.addr_port[1] = *p;

                if( s5->advance.cmd != 0x01 )
                {
                    err("s5 server advance req only support CMD `connect` 0x01, [%d]\n", *p );
                    s5_free( s5 );
                    return ERROR;
                }
                if( s5->advance.atyp == 0x04 )
                {
                    err("s5 server advance req not support ipv6 request, atype [%d]\n", s5->advance.atyp );
                    s5_free( s5 );
                    return ERROR;
                }
                s5->advance.state    = 0;
                timer_del( &ev->timer );
				debug("s5 [%p] serv adv recv req success\n", s5);
				
                ev->write_pt 	= NULL;
                ev->read_pt  	= socks5_server_up_addr_get;
                return ev->read_pt( ev );
            }
        }
        s5->advance.state = state;
    }
}

static status socks5_server_msg_s5_resp ( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    status rc = 0;
	meta_t * meta = down->meta;

	while( meta->last - meta->pos > 0 )
	{
		rc = down->send( down, meta->pos, meta->last - meta->pos );
		if( rc < 0 )
	    {
	        if( rc == ERROR )
	        {
	            err("s5 server invite resp send failed\n");
	            s5_free(s5);
	            return ERROR;
	        }
	        timer_set_data( &ev->timer, down );
	        timer_set_pt( &ev->timer, s5_timeout );
	        timer_add( &ev->timer, SOCKS5_TIME_OUT );
	        return AGAIN;
	    }
		meta->pos += rc;
	}
    timer_del( &ev->timer );
	meta->pos = meta->last = meta->start;

	debug("s5 [%p] serv invite resp send success\n", s5 );
	
    ev->read_pt		= socks5_server_msg_s5_adv_recv;
    ev->write_pt	= NULL;
    event_opt( ev, down->fd, EV_R );
    return ev->read_pt( ev );
}

static status socks5_server_msg_s5_recv( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    unsigned char * p = NULL;
	meta_t * meta = down->meta;
    /*
        s5 invite message req format
        1 byte		1 byte					1-255 byte
        version | authentication method num | auth methods
    */
    enum
    {
        ver = 0,
        nmethod,
        methods
    } state;

    state = s5->invite.state;
    while( 1 )
    {
        if( meta->last - meta->pos <= 0 )
        {
            ssize_t rc = down->recv( down, meta->last, meta->end - meta->last );
            if( rc < 0 )
            {
                if( rc == ERROR )
                {
                    err("s5 server invate recv failed\n");
                    s5_free( s5 );
                    return ERROR;
                }
                timer_set_data( &ev->timer, (void*)s5 );
                timer_set_pt( &ev->timer, s5_timeout );
                timer_add( &ev->timer, SOCKS5_TIME_OUT );
                return AGAIN;
            }
            meta->last += rc;
        }

        for( ; meta->pos < meta->last; meta->pos ++ )
        {
            p = meta->pos;
            if( state == ver )
            {
                s5->invite.ver = *p;
                state = nmethod;
                continue;
            }
            if( state == nmethod )
            {
                s5->invite.method_num = *p;
                s5->invite.method_n = 0;
                state = methods;
                continue;
            }
            if( state == methods )
            {
                s5->invite.method[(int)s5->invite.method_n++] = *p;
                if( s5->invite.method_n == s5->invite.method_num )
                {
                    // recv over
                    timer_del( &ev->timer );

					meta->pos = meta->last = meta->start;

					socsk5_message_invite_response_t * invite_resp = ( socsk5_message_invite_response_t* ) meta->pos;
					invite_resp->ver	 	= 0x05;
				    invite_resp->method 	= 0x00;
				    meta->last += sizeof(socsk5_message_invite_response_t);

					debug("s5 [%p] serv invite req recv success\n", s5 );
					
                    ev->read_pt 	= NULL;
				    ev->write_pt 	= socks5_server_msg_s5_resp;
				    event_opt( ev, down->fd, EV_W );
				    return ev->write_pt( ev );
                }
            }
        }
        s5->invite.state = state;
    }
}

static status socks5_server_msg_pri_resp( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    status rc;
	meta_t * meta = down->meta;
 
	while( meta->last - meta->pos > 0 )
	{
		rc = down->send( down, meta->pos, meta->last - meta->pos );
		if( rc < 0 )
	    {
	        if( rc == ERROR )
	        {
	            err("s5 server auth resp send failed\n");
	            s5_free(s5);
	            return ERROR;
	        }
	        timer_set_data( &ev->timer, down );
	        timer_set_pt( &ev->timer, s5_timeout );
	        timer_add( &ev->timer, SOCKS5_TIME_OUT );
	        return AGAIN;
	    }
		meta->pos += rc;
	}
    timer_del( &ev->timer );
	meta->pos = meta->last = meta->start;
	
	debug("s5 [%p] serv pri auth resp send success\n", s5 );
	ev->write_pt	= NULL;
	ev->read_pt  	= socks5_server_msg_s5_recv;
  	event_opt( ev, down->fd, EV_R );
    return ev->read_pt( ev );
}


static status socks5_server_msg_pri_recv( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    socks5_auth_header_t * head = NULL;
    ssize_t rc = 0;
    s5_user_t * user = NULL;
    unsigned char resp_status = 0;
	meta_t * meta = down->meta;

    while( ( meta->last - meta->pos ) < sizeof(socks5_auth_header_t) )
    {
        rc = down->recv( down, meta->last, meta->end - meta->last );
        if( rc < 0 )
        {
            if( ERROR == rc )
            {
                err("s5 server authorizaton check header recv failed\n");
                s5_free(s5);
                return ERROR;
            }
            timer_set_data( &ev->timer, s5 );
            timer_set_pt(&ev->timer, s5_timeout );
            timer_add( &ev->timer, SOCKS5_TIME_OUT );
            return AGAIN;
        }
        meta->last += rc;
    }
    timer_del( &ev->timer );

	// check auth data
    do
    {
        head = (socks5_auth_header_t*)meta->pos;
        if( S5_AUTH_MAGIC_NUM != head->magic )
        {
            err("s5 pri, magic [%x] incorrect, should be %x\n", head->magic, S5_AUTH_MAGIC_NUM );
			resp_status = S5_AUTH_STAT_MAGIC_FAIL;
        }

		if( S5_AUTH_TYPE_AUTH_REQ != head->message_type )
		{
			err("s5 pri, message type [%x] incorrect, \n", head->message_type, S5_AUTH_TYPE_AUTH_REQ );
			resp_status = S5_AUTH_STAT_TYPE_FAIL;
		}

		if( OK != s5_serv_usr_user_find( (char*)head->data.name, &user ) )
		{
		    err("s5 pri, user [%s] not found\n", head->data.name );
		    resp_status = S5_AUTH_STAT_NO_USER;
		}
		if( (l_strlen(head->data.passwd) != l_strlen(user->passwd)) ||
			(0 != memcmp( user->passwd, head->data.passwd, l_strlen(head->data.passwd)) ) 
		)
		{
		    err("s5 pri, user [%s] passwd incorrect\n", head->data.name );
		    resp_status = S5_AUTH_STAT_PASSWD_FAIL;
		}
        resp_status = S5_AUTH_STAT_SUCCESS;
	    head = (socks5_auth_header_t*)meta->pos;
	    head->magic             = S5_AUTH_MAGIC_NUM;
	    head->message_type      = S5_AUTH_TYPE_AUTH_RESP;
	    head->message_status    = resp_status;

		meta->pos = meta->start;
		meta->last = meta->pos + sizeof(socks5_auth_header_t);

		debug("s5 [%p] serv pri auth req recv success\n", s5);
	    ev->read_pt     = NULL;
	    ev->write_pt    = socks5_server_msg_pri_resp;
	    event_opt( ev, down->fd, EV_W );
		return ev->write_pt( ev );
    } while(0);

    s5_free( s5 );
    return ERROR;
}

static status socks5_server_start( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = NULL;

    if( OK != s5_alloc(&s5) )
    {
        err("s5 server alloc cycle failed\n");
        net_free( down );
        return ERROR;
    }
    s5->down 	= down;
    down->data  = s5;

	if( !down->page )
    {
        if( OK != mem_page_create(&down->page, L_PAGE_DEFAULT_SIZE) )
        {
            err("webser c page create failed\n");
            s5_free(s5);
            return ERROR;
        }
    }

    if( !down->meta )
    {
		if( OK != meta_alloc_form_mempage( down->page, 4096, &down->meta ) )
        {
            err("s5 alloc down meta failed\n");
            s5_free(s5);
            return ERROR;
        }
	}

    ev->read_pt	= socks5_server_msg_pri_recv;
    return ev->read_pt( ev );
}

status socks5_server_secret_start( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = NULL;
	
	if( OK != s5_alloc(&s5) )
    {
        err("s5 server http conv s5 alloc cycle failed\n");
		net_free(down);
		return ERROR;
    }
    s5->down 	= down;
    down->data  = s5;

	ev->read_pt = socks5_server_msg_pri_recv;
	return ev->read_pt( ev );
	
}

static status socks5_server_accept_cb_check( event_t * ev )
{
    connection_t * down = ev->data;

    if( !down->ssl->handshaked )
    {
        err(" downstream handshake error\n" );
        net_free( down );
        return ERROR;
    }
    timer_del( &ev->timer );

    down->recv 			= ssl_read;
    down->send 			= ssl_write;
    down->recv_chain 	= NULL;
    down->send_chain 	= ssl_write_chain;

    ev->read_pt 	= socks5_server_start;
    ev->write_pt 	= NULL;
    return ev->read_pt( ev );
}

status socks5_server_accept_cb( event_t * ev )
{
    connection_t * down = ev->data;
    status rc = 0;

    // s5 local connect s5 server must use tls connection
    do
    {
        rc = net_check_ssl_valid(down);
        if( OK != rc )
        {
            if( AGAIN == rc )
            {
                timer_set_data( &ev->timer, down );
                timer_set_pt( &ev->timer, net_timeout );
                timer_add( &ev->timer, SOCKS5_TIME_OUT );
                return AGAIN;
            }
            err("s5 server check net ssl failed\n");
            break;
        }
        
        if( OK != ssl_create_connection( down, L_SSL_SERVER ) )
        {
            err("s5 server down ssl create connection failed\n");
            break;
        }
        rc = ssl_handshake( down->ssl );
        if( rc < 0 )
        {
            if( ERROR == rc )
            {
                err("s5 server down ssl handshake failed\n");
                break;
            }
            down->ssl->cb = socks5_server_accept_cb_check;
            timer_set_data( &ev->timer, down );
            timer_set_pt( &ev->timer, net_timeout );
            timer_add( &ev->timer, SOCKS5_TIME_OUT );
            return AGAIN;
        }
        return socks5_server_accept_cb_check( ev );
    } while(0);

    net_free( down );
    return ERROR;
}


static status s5_serv_usr_db_decode( meta_t * meta )
{
    cJSON * socks5_user_database = NULL;

	cJSON * root = cJSON_Parse( (char*)meta->pos );
	if( root ) {
		socks5_user_database = cJSON_GetObjectItem( root, "socks5_user_database" );
		if( socks5_user_database ) {
			int i = 0;
			cJSON * obj = NULL;
			cJSON * username = NULL;
			cJSON * passwd = NULL;
			for( i = 0; i < cJSON_GetArraySize( socks5_user_database ); i ++ ) {
				obj = cJSON_GetArrayItem( socks5_user_database, i );
				if( obj ) {
					username = cJSON_GetObjectItem( obj, "username" );
					passwd = cJSON_GetObjectItem( obj, "passwd" );
					if( username && passwd ) {
						s5_serv_usr_user_add( cJSON_GetStringValue(username), cJSON_GetStringValue(passwd) );
					}
				}
			}
		}
		cJSON_Delete( root );
	}
    return OK;
}

static status s5_serv_db_file_load( meta_t * meta )
{
    ssize_t size = 0;
    
    int fd = open( (char*)config_get()->s5_serv_auth_path, O_RDONLY  );
    if( ERROR == fd ) {
        err("usmgr auth open file [%s] failed, errno [%d]\n", config_get()->s5_serv_auth_path,  errno );
        return ERROR;
    }
    size = read( fd, meta->pos, meta_len( meta->start, meta->end ) );
    close( fd );
    if( size == ERROR ) {
        err("usmgr auth read auth file failed\n");
        return ERROR;
    }
    meta->last += size;
    return OK;
}



static status s5_serv_usr_db_init( )
{
    meta_t * meta = NULL;
    status rc = ERROR;
    
    do {
        if( OK != meta_alloc( &meta, S5_USER_AUTH_FILE_LEN ) ) {
            err("usmgr auth databse alloc meta failed\n");
            break;
        }
        if( OK != s5_serv_db_file_load( meta ) ) {
            err("usmgr auth file load failed\n");
            break;
        }
        if( OK != s5_serv_usr_db_decode( meta ) ) {
            err("usmgr auth file decode failed\n");
            break;
        }
        rc = OK;
    }while(0);
    
    if( meta ){
        meta_free( meta );
    }
    return rc;
}

status socks5_server_init( void )
{
   	int i = 0;
    int ret = -1;

    if( g_s5_ctx ) {
        err("s5 ctx not empty\n");
        return ERROR;
    }

    do {
        g_s5_ctx = l_safe_malloc( sizeof(g_s5_t) + MAX_NET_CON*sizeof(socks5_cycle_t) );
        if( !g_s5_ctx ) {
            err("s5 init allo this failed, [%d]\n", g_s5_ctx );
            return ERROR;
        }
        memset( g_s5_ctx, 0, sizeof(g_s5_t) + MAX_NET_CON*sizeof(socks5_cycle_t) );
        queue_init(&g_s5_ctx->usable);
        queue_init(&g_s5_ctx->use);
        for( i = 0; i < MAX_NET_CON; i++ )
           queue_insert_tail( &g_s5_ctx->usable, &g_s5_ctx->pool[i].queue );
           
        /// s5 server mode or server screct mode
        if( config_get()->s5_mode > SOCKS5_CLIENT ) {
            queue_init( &g_s5_ctx->g_users );
            if( OK != mem_page_create( &g_s5_ctx->g_user_mempage, sizeof(s5_user_t) ) ) {
                err("s5 serv alloc user mem page\n");
                break;
            }
            if( OK != s5_serv_usr_db_init() )
            {
                err("s5 serv usr db init failed\n");
                break;
            }
        }
        ret = 0;
    } while(0);

    if( ret == -1 ) {
        if( g_s5_ctx ) {
            if( config_get()->s5_mode > SOCKS5_CLIENT ) {
                if( g_s5_ctx->g_user_mempage ) {
                    mem_page_free( g_s5_ctx->g_user_mempage );
                    g_s5_ctx->g_user_mempage = NULL;
                }
            }
            l_safe_free(g_s5_ctx);
            g_s5_ctx = NULL;
        }
    }
    return OK;
}

status socks5_server_end( void )
{
    if( g_s5_ctx ) {
        if( g_s5_ctx->g_user_mempage ) {
            mem_page_free( g_s5_ctx->g_user_mempage );
            g_s5_ctx->g_user_mempage = NULL;
        }
        l_safe_free(g_s5_ctx);
        g_s5_ctx = NULL;
    }
	return OK;
}

