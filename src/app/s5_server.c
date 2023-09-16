#include "common.h"
#include "dns.h"
#include "s5_local.h"
#include "s5_server.h"

#define S5_USER_NAME_MAX		16
#define S5_USER_PASSWD_MAX		16
#define S5_USER_AUTH_FILE_LEN  (4*1024)

typedef struct s5_user {
	char		auth[32];
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

    if( queue_empty(&g_s5_ctx->usable) ) {
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


static status s5_serv_usr_obj_find( char * auth );


status s5_free( socks5_cycle_t * s5 )
{
	queue_t * q = &s5->queue;

    memset( &s5->phase1, 0x0, sizeof(s5_rfc_phase1_req_t) );
    memset( &s5->phase2, 0x0, sizeof(s5_rfc_phase2_req_t) );

    if( s5->down ) {
        net_free( s5->down );
        s5->down = NULL;
    }
    if( s5->up ) {
        net_free( s5->up );
        s5->up = NULL;
    }
    if( s5->dns_cycle ) {
        dns_over( s5->dns_cycle );
        s5->dns_cycle = NULL;
    }

    s5->state = 0;
	s5->recv_down_err = 0;
	s5->recv_up_err = 0;
	
    queue_remove( q );
    queue_insert_tail(&g_s5_ctx->usable, q);
    return OK;
}

void s5_timeout_cb( void * data )
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
    timer_set_pt( &ev->timer, s5_timeout_cb );
    timer_add( &ev->timer, S5_TIMEOUT );

    /// try to recv down stream data unit meta full
	while( down->meta->end > down->meta->last ) {
		recvn = down->recv( down, down->meta->last, down->meta->end - down->meta->last );
		if( recvn < 0 ) {
			if( recvn == ERROR ) {
			    err("s5 down recv error\n");
				s5->recv_down_err = 1;
			}
            /// again
			break;
		}
		down->meta->last += recvn;
	}

    /// if meta is empty and down recv error happend, then goto stop the s5 transport
	if( down->meta->pos == down->meta->last && s5->recv_down_err == 1 ) {
	    err("s5 down error. meta clear. goto free\n");
	    event_opt( down->event, down->fd, EV_NONE );
        event_opt( up->event, up->fd, EV_NONE );
		s5_free(s5);
		return ERROR;
	}
    /// if meta remain data, goto send the data to up stream
	if( down->meta->last > down->meta->pos ) {
		event_opt( down->event, down->fd, down->event->opt & ~EV_R );
		event_opt( up->event, up->fd, up->event->opt|EV_W );
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
    timer_set_pt( &ev->timer, s5_timeout_cb );
    timer_add( &ev->timer, S5_TIMEOUT );

	while( down->meta->last > down->meta->pos ) {
		sendn = up->send( up, down->meta->pos, down->meta->last - down->meta->pos );
		if( sendn < 0 ) {
			if( sendn == ERROR ) {
			    err("s5 up send error\n");
				s5_free(s5);
				return ERROR;
			}
			timer_add( &ev->timer, S5_TIMEOUT );
			return AGAIN;
		}
		down->meta->pos += sendn;
	}

	if( s5->recv_down_err == 1 ) {
	    err("s5 free, down already error, close down and up event.\n");
        
        event_opt( down->event, down->fd, EV_NONE );
        event_opt( up->event, up->fd, EV_NONE );
		s5_free(s5);
		return ERROR;
	}

	down->meta->last = down->meta->pos = down->meta->start;

	event_opt( down->event, down->fd, down->event->opt|EV_R );
	event_opt( up->event, up->fd, up->event->opt & ~EV_W );
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
    timer_set_pt( &ev->timer, s5_timeout_cb );
    timer_add( &ev->timer, S5_TIMEOUT );

	while( up->meta->end > up->meta->last ) {
		recvn = up->recv( up, up->meta->last, up->meta->end - up->meta->last );
		if( recvn < 0 ) {
			if( recvn == ERROR ) {   
			    err("s5 up recv error\n");
				s5->recv_up_err = 1;
			}
			/// again
			break;
		}
		up->meta->last += recvn;
	}

	if( up->meta->pos == up->meta->last && s5->recv_up_err == 1 ) {
	    err("s5 up error. meta clear. goto free\n");
	    event_opt( down->event, down->fd, EV_NONE );
        event_opt( up->event, up->fd, EV_NONE );
		s5_free(s5);
		return ERROR;
	}

	if( up->meta->last > up->meta->pos ) {
		event_opt( up->event, up->fd, up->event->opt & ~EV_R );
		event_opt( down->event, down->fd, down->event->opt|EV_W );
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
    timer_set_pt( &ev->timer, s5_timeout_cb );
    timer_add( &ev->timer, S5_TIMEOUT );

	while( up->meta->last > up->meta->pos ) {
		sendn = down->send( down, up->meta->pos, up->meta->last - up->meta->pos );
		if( sendn < 0 ) {
			if( sendn == ERROR ) {
			    err("s5 down send error\n");
				s5_free(s5);
				return ERROR;
			}
			timer_add( &ev->timer, S5_TIMEOUT );
			return AGAIN;
		}
		up->meta->pos += sendn;
	}

	if( s5->recv_up_err == 1 ) {
	    err("s5 free, up already error. close down and up event.\n");
        event_opt( down->event, down->fd, EV_NONE );
        event_opt( up->event, up->fd, EV_NONE );
		s5_free(s5);
		return ERROR;
	}

	up->meta->last = up->meta->pos = up->meta->start;

	event_opt( down->event, down->fd, down->event->opt & ~EV_W );
	event_opt( up->event, up->fd, up->event->opt|EV_R );
	return up->event->read_pt( up->event );
}


status s5_traffic_process( event_t * ev )
{
	/*
	 when client mode, connection means upstream
	 when server mode, connection means downstream 
	*/

    connection_t * c = ev->data;
    socks5_cycle_t * s5 = c->data;
	connection_t * down = s5->down;
	connection_t * up = s5->up;

    s5->down->event->read_pt = s5_traffic_recv;
	s5->up->event->write_pt	= s5_traffic_send;
	
    s5->up->event->read_pt = s5_traffic_back_recv;
	s5->down->event->write_pt = s5_traffic_back_send;

	// init down stream traffic buffer
	if( !down->page ) {
        if( OK != mem_page_create(&down->page, L_PAGE_DEFAULT_SIZE) ) {
            err("webser down page create failed\n");
            s5_free(s5);
            return ERROR;
        }
    }
    if( !down->meta ) {
		if( OK != meta_alloc_form_mempage( down->page, 4096, &down->meta ) ) {
            err("s5 alloc down meta failed\n");
            s5_free(s5);
            return ERROR;
        }
	}
	

	// init up stream traffic buffer
	if( !up->page ) {
        if( OK != mem_page_create(&up->page, L_PAGE_DEFAULT_SIZE) ) {
            err("webser up page create failed\n");
            s5_free(s5);
            return ERROR;
        }
    }
    if( !up->meta ) {
		if( OK != meta_alloc_form_mempage( up->page, 4096, &up->meta ) ) {
            err("s5 alloc up meta failed\n");
            s5_free(s5);
            return ERROR;
        }
	}

	// init cache buffer
	down->meta->pos = down->meta->last = down->meta->start;
	up->meta->pos = up->meta->last = up->meta->start;
	/// set down stream to read, up stream to read
    event_opt( s5->up->event, s5->up->fd, EV_R );	
    event_opt( s5->down->event, s5->down->fd, EV_R );
	
    return s5->down->event->read_pt( s5->down->event );
}

static status s5_server_rfc_phase2_send( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    status rc = 0;
	meta_t * meta = down->meta;

	while( meta->last - meta->pos > 0 ) {
		rc = down->send( down, meta->pos, meta->last - meta->pos );
		if( rc < 0 ) {
	        if( rc == ERROR ) {
	            err("s5 server adv resp send failed\n");
	            s5_free(s5);
	            return ERROR;
	        }
	        timer_set_data( &ev->timer, down );
	        timer_set_pt( &ev->timer, s5_timeout_cb );
	        timer_add( &ev->timer, S5_TIMEOUT );
	        return AGAIN;
	    }
		meta->pos += rc;
	}
    timer_del( &ev->timer );
	/// send phase2 response to down stream finish
    ev->read_pt = s5_traffic_process;
    ev->write_pt = NULL;
    return ev->read_pt( ev );
}

static status s5_server_rfc_phase2_resp_build( event_t * ev )
{
    connection_t * up = ev->data;
    socks5_cycle_t * s5 = up->data;
    connection_t * down = s5->down;
	meta_t * meta = down->meta;
   
    /// clear the meta
    meta->last = meta->pos = meta->start;
    /// fix the meta with phase2 reponse 
    s5_rfc_phase2_resp_t * resp = ( s5_rfc_phase2_resp_t* )meta->last;
    resp->ver = 0x05;
    resp->rep = 0x00;
    resp->rsv = 0x00;
    resp->atyp = 0x01;
    resp->bnd_addr = htons((uint16_t)up->addr.sin_addr.s_addr);
    resp->bnd_port = htons(up->addr.sin_port);
    meta->last += sizeof(s5_rfc_phase2_resp_t);
    // make up event invalidat
    event_opt( up->event, up->fd, EV_NONE );
    /// set down to write, ready to send phase2 response
    event_opt( down->event, down->fd, EV_W );
	
    down->event->write_pt = s5_server_rfc_phase2_send;
    return down->event->write_pt( down->event );
}

static status s5_server_connect_check( event_t * ev )
{
    connection_t* up = ev->data;
    socks5_cycle_t * s5 = up->data;

    if( OK != net_socket_check_status( up->fd ) ) {
        err("s5 server connect remote failed\n" );
        s5_free( s5 );
        return ERROR;
    }
    net_socket_nodelay( up->fd );
    timer_del( &ev->timer );

    ev->read_pt		= NULL;
    ev->write_pt 	= s5_server_rfc_phase2_resp_build;
    return ev->write_pt( ev );
}

static status s5_server_connect( event_t * ev )
{
    connection_t * up = ev->data;
    socks5_cycle_t * s5 = up->data;
    status rc = 0;

    rc = net_connect( s5->up, &s5->up->addr );
    if( rc == ERROR ) {
        err("s5 server connect up failed\n");
        s5_free( s5 );
        return ERROR;
    }
    ev->read_pt = NULL;
    ev->write_pt = s5_server_connect_check;
    event_opt( ev, up->fd, EV_W );
    if( rc == AGAIN ) {
        timer_set_data( &ev->timer, s5 );
        timer_set_pt( &ev->timer, s5_timeout_cb );
        timer_add( &ev->timer, S5_TIMEOUT );
        return AGAIN;
    }
    return ev->write_pt( ev );
}

static status s5_server_try_read( event_t * ev  )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;

    if( OK != net_socket_check_status( down->fd ) ) {
        err("s5 server check down fd status error\n");
        s5_free( s5 );
        return ERROR;
    }
    return OK;
}


static void s5_server_address_get_cb( void * data )
{
    socks5_cycle_t * s5 = data;
    dns_cycle_t * dns_cycle = s5->dns_cycle;
    char ipstr[128] = {0};

    if( dns_cycle ) {
        if( OK == dns_cycle->dns_status ) {
            uint16_t addr_port = 0;
        
            snprintf( ipstr, sizeof(ipstr), "%d.%d.%d.%d",
                dns_cycle->answer.rdata_data[0],
                dns_cycle->answer.rdata_data[1],
                dns_cycle->answer.rdata_data[2],
                dns_cycle->answer.rdata_data[3] );
            memcpy( &addr_port, s5->phase2.dst_port, sizeof(uint16_t) ); 
            
            s5->up->addr.sin_family	= AF_INET;
            s5->up->addr.sin_port = addr_port;
            s5->up->addr.sin_addr.s_addr = inet_addr( ipstr );
            
            s5->up->event->read_pt = NULL;
            s5->up->event->write_pt = s5_server_connect;
            s5->up->event->write_pt( s5->up->event );
        } else {
            err("socks5 server dns resolv failed\n");
            s5_free(s5);
        }
    }
}

static status s5_server_address_get( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    char ipstr[128] = {0};
    status rc = 0;

    down->event->read_pt = s5_server_try_read;
    down->event->write_pt = NULL;
    event_opt( down->event, down->fd, EV_R );

    /// alloc upstream connection
    if( OK != net_alloc( &s5->up ) ) {
        err("s5 server up alloc failed\n" );
        s5_free(s5);
        return ERROR;
    }
    s5->up->data = s5;

    if( s5->phase2.atyp == S5_RFC_IPV4 ) {
        /// ipv4 type request, goto convert ipv4 address
		uint16_t addr_port = 0;

        snprintf( ipstr, sizeof(ipstr), "%d.%d.%d.%d",
    		(unsigned char )s5->phase2.dst_addr[0],
    		(unsigned char )s5->phase2.dst_addr[1],
    		(unsigned char )s5->phase2.dst_addr[2],
    		(unsigned char )s5->phase2.dst_addr[3] );
		memcpy( &addr_port, s5->phase2.dst_port, sizeof(uint16_t) );
		
        s5->up->addr.sin_family	= AF_INET;
        s5->up->addr.sin_port = addr_port;
        s5->up->addr.sin_addr.s_addr = inet_addr( ipstr );

        s5->up->event->read_pt = NULL;
        s5->up->event->write_pt = s5_server_connect;
        return s5->up->event->write_pt( s5->up->event );
        
    }  else if ( s5->phase2.atyp == S5_RFC_DOMAIN ) {
        /// domain type request, goto dns resolve the domain
        if( OK == dns_record_find( (char*)s5->phase2.dst_addr, ipstr ) ) {
            /// dns cache find success, use dns cache  
    		uint16_t addr_port = 0;
            
            snprintf( ipstr, sizeof(ipstr), "%d.%d.%d.%d",
        		(unsigned char )ipstr[0],
        		(unsigned char )ipstr[1],
        		(unsigned char )ipstr[2],
        		(unsigned char )ipstr[3] );
    		memcpy( &addr_port, s5->phase2.dst_port, sizeof(uint16_t) );
          
            s5->up->addr.sin_family = AF_INET;
            s5->up->addr.sin_port = addr_port;
            s5->up->addr.sin_addr.s_addr = inet_addr( ipstr );

            s5->up->event->read_pt = NULL;
            s5->up->event->write_pt = s5_server_connect;
            return s5->up->event->write_pt( s5->up->event );
        
        } else {
            /// dns cache find failed, goto dns query  
            rc = dns_create( &s5->dns_cycle );
            if( rc == ERROR ) {
                err("s5 server dns cycle create failed\n");
                s5_free(s5);
                return ERROR;
            }
            strncpy( (char*)s5->dns_cycle->query, (char*)s5->phase2.dst_addr, sizeof(s5->dns_cycle->query) );
            s5->dns_cycle->cb = s5_server_address_get_cb;
            s5->dns_cycle->cb_data = s5;
            return dns_start( s5->dns_cycle );
        }
    }

    err("s5 server phase2 atyp [0x%x]. not support\n", s5->phase2.atyp );
    s5_free(s5);
    return ERROR;
}


static status s5_server_rfc_phase2_recv( event_t * ev )
{
    unsigned char * p = NULL;
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    meta_t * meta = down->meta;
    enum {
        s_ver = 0,
        s_cmd,
        s_rsv,
        s_atyp,
        s_dst_addr_ipv4,
        s_dst_addr_ipv6,
        s_dst_addr_domain_len,
        s_dst_addr_domain,
        s_dst_port,
        s_dst_port_end
    };

    /*
     s5 msg phase2 format
        char  char  char  char   ...    char*2
        VER | CMD | RSV | ATYP | ADDR | PORT
     */

    while( 1 ) {
        if( meta->last - meta->pos <= 0 ) {
            ssize_t rc = down->recv( down, meta->last, meta->end - meta->last );
            if( rc < 0 ) {
                if( rc == ERROR ) {
                    err("s5 server rfc phase2 recv failed\n");
                    s5_free(s5);
                    return ERROR;
                }
                timer_set_data( &ev->timer, (void*)s5 );
                timer_set_pt( &ev->timer, s5_timeout_cb );
                timer_add( &ev->timer, S5_TIMEOUT );
                return AGAIN;
            }
            meta->last += rc;
        }

        for( ; meta->pos < meta->last; meta->pos ++ ) {
            p = meta->pos;
            if( s5->state == s_ver ) {
                /// ver is fixed. 0x05
                s5->phase2.ver = *p;
                s5->state = s_cmd;
                continue;
            }
            if( s5->state == s_cmd ) {
                /*
                    socks5 support cmd value
                    01				connect
                    02				bind
                    03				udp associate
                */
                s5->phase2.cmd = *p;
                s5->state = s_rsv;
                continue;
            }
            if( s5->state == s_rsv ) {
                // rsv means resverd
                s5->phase2.rsv = *p;
                s5->state = s_atyp;
                continue;
            }
            if( s5->state == s_atyp ) {
                s5->phase2.atyp = *p;
                /*
                    atyp		type		length
                    0x01		ipv4		4
                    0x03		domain		first octet of domain part
                    0x04		ipv6		16
                */
                if( s5->phase2.atyp == S5_RFC_IPV4 ) {
                    s5->state = s_dst_addr_ipv4;
                    s5->phase2.dst_addr_n = 4;
                    s5->phase2.dst_addr_cnt = 0;
                    continue;
                } else if ( s5->phase2.atyp == S5_RFC_IPV6 ) {
                    s5->state = s_dst_addr_ipv6;
                    s5->phase2.dst_addr_n = 16;
                    s5->phase2.dst_addr_cnt = 0;
                    continue;
                } else if ( s5->phase2.atyp == S5_RFC_DOMAIN ) {
                    /// atpy domain -> dst addr domain len -> dst addr domain
                    s5->state = s_dst_addr_domain_len;
                    s5->phase2.dst_addr_n = 0;
                    s5->phase2.dst_addr_cnt = 0;
                    continue;
                }
                err("s5 server request atyp [%d] not support\n", s5->phase2.atyp );
                s5_free(s5);
                return ERROR;
            }
            if( s5->state == s_dst_addr_ipv4 ) {
                s5->phase2.dst_addr[(int)s5->phase2.dst_addr_cnt++] = *p;
                if( s5->phase2.dst_addr_cnt == 4 ) {
                    s5->state = s_dst_port;
                    continue;
                }
            }
            if( s5->state == s_dst_addr_ipv6 ) {
                s5->phase2.dst_addr[(int)s5->phase2.dst_addr_cnt++] = *p;
                if( s5->phase2.dst_addr_cnt == 16 ) {
                    s5->state = s_dst_port;
                    continue;
                }
            }
            if( s5->state == s_dst_addr_domain_len ) {
                s5->phase2.dst_addr_n = *p;
                s5->state = s_dst_addr_domain;
                if( s5->phase2.dst_addr_n <= 0 ) {
                    err("s5 server phase2 dst domain len [%d] <= 0. error\n", s5->phase2.dst_addr_n );
                    s5_free(s5);
                    return ERROR;
                }
                if( s5->phase2.dst_addr_n > DOMAIN_LENGTH ) {
                    err("s5 server phase2 dst domain len [%d] > 255. error\n", s5->phase2.dst_addr_n );
                    s5_free(s5);
                    return ERROR;
                }
                continue;
            }
            if( s5->state == s_dst_addr_domain ) {
                s5->phase2.dst_addr[(int)s5->phase2.dst_addr_cnt++] = *p;
                if( s5->phase2.dst_addr_cnt == s5->phase2.dst_addr_n ) {
                    s5->state = s_dst_port;
                    continue;
                }
            }
            if( s5->state == s_dst_port ) {
                s5->phase2.dst_port[0] = *p;
                s5->state = s_dst_port_end;
                continue;
            }
            if( s5->state == s_dst_port_end ) {
                s5->phase2.dst_port[1] = *p;

                /// phase2 request recv finish
                /// reset state  
                s5->state = 0;
                timer_del( &ev->timer );
				
                /// !!! meta can't reset in here, becasue connect need

                do {
                    if( s5->phase2.ver != 0x05 ) {
                        err("s5 server phase2 ver is not '0x05', is [0x%x]\n", s5->phase2.ver );
                        break;
                    }
                    /// only support CONNECT 0x01 request
                    if( s5->phase2.cmd != 0x01 ) {
                        err("s5 server phase2 cmd is not `CONNECT` 0x01, is [0x%x]\n", s5->phase2.cmd );
                        break;
                    }
                    /// not support IPV6 request
                    if( s5->phase2.atyp != S5_RFC_IPV4 && s5->phase2.atyp != S5_RFC_DOMAIN ) {
                        err("s5 server phase2 atyp only support '0x1'(IPV4), '0x3'(DOMAIN), now is [0x%x]\n", s5->phase2.atyp );
                        break;
                    }

                    ev->write_pt = NULL;
                    ev->read_pt = s5_server_address_get;
                    return ev->read_pt( ev );

                } while(0);

				s5_free( s5 );
                return ERROR;
            }
        }
    }
}

static status s5_server_rfc_phase1_send( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    status rc = 0;
	meta_t * meta = down->meta;

	while( meta->last - meta->pos > 0 ) {
		rc = down->send( down, meta->pos, meta->last - meta->pos );
		if( rc < 0 ) {
	        if( rc == ERROR ) {
	            err("s5 server rfc phase1 resp send failed\n");
	            s5_free(s5);
	            return ERROR;
	        }
	        timer_set_data( &ev->timer, down );
	        timer_set_pt( &ev->timer, s5_timeout_cb );
	        timer_add( &ev->timer, S5_TIMEOUT );
	        return AGAIN;
	    }
		meta->pos += rc;
	}
    timer_del( &ev->timer );
    /// send phase1 response finish, reset the meta
	meta->pos = meta->last = meta->start;
	
	/// goto recv phase2 request
    ev->read_pt	= s5_server_rfc_phase2_recv;
    ev->write_pt = NULL;
    event_opt( ev, down->fd, EV_R );
    return ev->read_pt( ev );
}

static status s5_server_rfc_phase1_recv( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    unsigned char * p = NULL;
	meta_t * meta = down->meta;
    /*
        s5 phase1 message req format
        1 byte		1 byte	 1-255 byte
        version | nmethods | methods
    */
    enum {
        s_ver = 0,
        s_nmethod,
        s_methods
    };

    while( 1 ) {

        /// try to recv uint meta full or error again
        if( meta->last - meta->pos <= 0 ) {
            ssize_t rc = down->recv( down, meta->last, meta->end - meta->last );
            if( rc < 0 ) {
                if( rc == ERROR ) {
                    err("s5 server rfc phase1 recv failed\n");
                    s5_free( s5 );
                    return ERROR;
                }
                timer_set_data( &ev->timer, (void*)s5 );
                timer_set_pt( &ev->timer, s5_timeout_cb );
                timer_add( &ev->timer, S5_TIMEOUT );
                return AGAIN;
            }
            meta->last += rc;
        }

        for( ; meta->pos < meta->last; meta->pos ++ ) {
            p = meta->pos;
            if( s5->state == s_ver ) {
                s5->phase1.ver = *p;
                s5->state = s_nmethod;
                continue;
            }
            if( s5->state == s_nmethod ) {
                s5->phase1.methods_n = *p;
                s5->phase1.methods_cnt = 0;
                s5->state = s_methods;
                continue;
            }
            if( s5->state == s_methods ) {
                s5->phase1.methods[(int)s5->phase1.methods_cnt++] = *p;
                if( s5->phase1.methods_n == s5->phase1.methods_cnt ) {
                    /// rfc2918 socks5 protocol phase1 request packet recv finish
                    timer_del( &ev->timer );
                    /// reset the meta
					meta->pos = meta->last = meta->start;
                    /// reset the state 
                    s5->state = 0;
                    /// build the phase1 response
					s5_rfc_phase1_resp_t * resp = ( s5_rfc_phase1_resp_t* ) meta->pos;
					resp->ver = 0x05;
				    resp->method = 0x00;
				    meta->last += sizeof(s5_rfc_phase1_resp_t);
				    
					/// goto send phase1 response
                    ev->read_pt = NULL;
				    ev->write_pt = s5_server_rfc_phase1_send;
				    event_opt( ev, down->fd, EV_W );
				    return ev->write_pt( ev );
                }
            }
        }
    }
}

static status s5_server_auth_send( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    status rc;
	meta_t * meta = down->meta;
 
	while( meta->last - meta->pos > 0 ) {
		rc = down->send( down, meta->pos, meta->last - meta->pos );
		if( rc < 0 ) {
	        if( rc == ERROR ) {
	            err("s5 server auth resp send failed\n");
	            s5_free(s5);
	            return ERROR;
	        }
	        timer_set_data( &ev->timer, down );
	        timer_set_pt( &ev->timer, s5_timeout_cb );
	        timer_add( &ev->timer, S5_TIMEOUT );
	        return AGAIN;
	    }
		meta->pos += rc;
	}
    timer_del( &ev->timer );
    /// send auth resp finish, reset the meta
	meta->pos = meta->last = meta->start;
    /// goto recv the packet addording to the RFC1928 socks5 protocol
	
	ev->write_pt = NULL;
	ev->read_pt = s5_server_rfc_phase1_recv;
  	event_opt( ev, down->fd, EV_R );
    return ev->read_pt( ev );
}

static status s5_server_auth_recv_payload( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    meta_t * meta = down->meta;
    int err_code = S5_ERR_SUCCESS;
    ssize_t rc = 0;
    
    while( (meta->last - meta->pos) < sizeof(s5_auth_data_t) ) {
        rc = down->recv( down, meta->last, meta->end - meta->last );
        if( rc < 0 ) {
            if( ERROR == rc ) {
                err("s5 server authorizaton check header recv failed\n");
                s5_free(s5);
                return ERROR;
            }
            timer_set_data( &ev->timer, s5 );
            timer_set_pt(&ev->timer, s5_timeout_cb );
            timer_add( &ev->timer, S5_TIMEOUT );
            return AGAIN;
        }
        meta->last += rc;
    }
    timer_del( &ev->timer );

    do {
        s5_auth_data_t * payload = (s5_auth_data_t*) meta->pos;

        if(  OK != s5_serv_usr_obj_find( (char*)payload->auth ) ) {
            /// if auth user not find
		    err("s5 pri, auth not found\n", payload->auth );
		    err_code = S5_ERR_AUTH;
		}
    } while(0);

    if( err_code != S5_ERR_SUCCESS ) {
        err("s5 auth check payload failed\n");
        s5_free( s5 );
        return ERROR;
    }
    /// reset the meta 
    meta->pos = meta->last = meta->start;
    /// fix the meta with s5_auth_info_t for response
    s5_auth_info_t * header = (s5_auth_info_t*)meta->pos;
    header->magic = S5_AUTH_MAGIC_NUM;
    header->typ = S5_MSG_LOGIN_RESP;
    header->code = err_code;
    meta->last += sizeof(s5_auth_info_t);

    /// goto send auth resp
    
    ev->read_pt = NULL;
    ev->write_pt = s5_server_auth_send;
    event_opt( ev, down->fd, EV_W );
    return ev->write_pt( ev );
}

static status s5_server_auth_recv_header( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    ssize_t rc = 0;
    unsigned char err_code = S5_ERR_SUCCESS;
	meta_t * meta = down->meta;

    /// at least recv ad s5 auth header. then goto check the header
    while( ( meta->last - meta->pos ) < sizeof(s5_auth_info_t) ) {
        rc = down->recv( down, meta->last, meta->end - meta->last );
        if( rc < 0 ) {
            if( ERROR == rc ) {
                err("s5 server authorizaton check header recv failed\n");
                s5_free(s5);
                return ERROR;
            }
            timer_set_data( &ev->timer, s5 );
            timer_set_pt(&ev->timer, s5_timeout_cb );
            timer_add( &ev->timer, S5_TIMEOUT );
            return AGAIN;
        }
        meta->last += rc;
    }
    timer_del( &ev->timer );

	// check the auth header
    do {
        s5_auth_info_t * header = (s5_auth_info_t*)meta->pos;
        if( header->magic != S5_AUTH_MAGIC_NUM ) {
            err("s5 pri, magic [0x%x] incorrect, should be S5_AUTH_MAGIC_NUM [0x%x]\n", header->magic, S5_AUTH_MAGIC_NUM );
			err_code = S5_ERR_MAGIC;
        }

		if( header->typ != S5_MSG_LOGIN_REQ ) {
			err("s5 pri, msg type [0x%x] incorrect, should be S5_MSG_LOGIN_REQ [0x%x]\n", header->typ, S5_MSG_LOGIN_REQ );
			err_code = S5_ERR_TYPE;
		}
    } while(0);

    if( err_code != S5_ERR_SUCCESS ) {
        err("s5 auth check header failed\n");
        s5_free( s5 );
        return ERROR;
    }
    /// change the meta pos 
    meta->pos += sizeof(s5_auth_info_t);
    /// goto recv the auth payload and check payload
    ev->read_pt = s5_server_auth_recv_payload;
    return ev->read_pt( ev );
}

static status s5_server_start( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = NULL;

    if( OK != s5_alloc(&s5) ) {
        err("s5 server alloc cycle failed\n");
        net_free( down );
        return ERROR;
    }
    s5->down 	= down;
    down->data  = s5;

	if( !down->page ) {
        if( OK != mem_page_create(&down->page, L_PAGE_DEFAULT_SIZE) ) {
            err("webser c page create failed\n");
            s5_free(s5);
            return ERROR;
        }
    }

    if( !down->meta ) {
		if( OK != meta_alloc_form_mempage( down->page, 4096, &down->meta ) ) {
            err("s5 alloc down meta failed\n");
            s5_free(s5);
            return ERROR;
        }
	}

    ev->read_pt	= s5_server_auth_recv_header;
    return ev->read_pt( ev );
}

status s5_server_transport( event_t * ev )
{
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = NULL;
	
	if( OK != s5_alloc(&s5) ) {
        err("s5 server http conv s5 alloc cycle failed\n");
		net_free(down);
		return ERROR;
    }
    s5->down = down;
    down->data = s5;

	ev->read_pt = s5_server_auth_recv_header;
	return ev->read_pt( ev );
	
}

static status s5_server_accept_cb_check( event_t * ev )
{
    connection_t * down = ev->data;

    if( !down->ssl->handshaked ) {
        err(" downstream handshake error\n" );
        net_free( down );
        return ERROR;
    }
    timer_del( &ev->timer );

    down->recv = ssl_read;
    down->send = ssl_write;
    down->recv_chain = NULL;
    down->send_chain = ssl_write_chain;

    ev->read_pt = s5_server_start;
    ev->write_pt = NULL;
    return ev->read_pt( ev );
}

status s5_server_accept_cb( event_t * ev )
{
    connection_t * down = ev->data;
    status rc = 0;

    /// s5 server use ssl connection force 
    do {
        rc = net_check_ssl_valid(down);
        if( OK != rc ) {
            if( AGAIN == rc ) {
                timer_set_data( &ev->timer, down );
                timer_set_pt( &ev->timer, net_timeout );
                timer_add( &ev->timer, S5_TIMEOUT );
                return AGAIN;
            }
            err("s5 server check net ssl failed\n");
            break;
        }
        
        if( OK != ssl_create_connection( down, L_SSL_SERVER ) ) {
            err("s5 server down ssl create connection failed\n");
            break;
        }
        rc = ssl_handshake( down->ssl );
        if( rc < 0 ) {
            if( ERROR == rc ) {
                err("s5 server down ssl handshake failed\n");
                break;
            }
            down->ssl->cb = s5_server_accept_cb_check;
            timer_set_data( &ev->timer, down );
            timer_set_pt( &ev->timer, net_timeout );
            timer_add( &ev->timer, S5_TIMEOUT );
            return AGAIN;
        }
        return s5_server_accept_cb_check( ev );
    } while(0);

    net_free( down );
    return ERROR;
}

static status s5_serv_usr_obj_find( char * auth )
{
	queue_t * q;
	s5_user_t * t = NULL;

	for( q = queue_head( &g_s5_ctx->g_users ); q != queue_tail( &g_s5_ctx->g_users ); q = queue_next(q) ) {
		t = ptr_get_struct( q, s5_user_t, queue );
		if( t && l_strlen(t->auth) == strlen(auth) && memcmp( t->auth, auth, strlen(auth) ) == 0 ) {
			return OK;
		}
	}
	return ERROR;
}


static status s5_serv_usr_obj_add( char * auth )
{
    s5_user_t * user = mem_page_alloc( g_s5_ctx->g_user_mempage, sizeof(s5_user_t) );
	if( !user ) {
		err("alloc new user\n");
		return ERROR;
	}
	memset( user, 0, sizeof(s5_user_t) );

	memcpy( user->auth, auth, strlen(auth) );
	queue_insert_tail( &g_s5_ctx->g_users, &user->queue );

#if(1)
	// show all users
	queue_t * q;
	s5_user_t * t = NULL;
	for( q = queue_head( &g_s5_ctx->g_users ); q != queue_tail( &g_s5_ctx->g_users ); q = queue_next(q) ) {
		t = ptr_get_struct( q, s5_user_t, queue );
		debug("queue show ---> [%s]\n", t->auth );
	}
#endif
    return OK;
}

static status s5_serv_usr_db_parse( meta_t * meta )
{
	cJSON * root = cJSON_Parse( (char*)meta->pos );
	if(root) {
        /// traversal the array 
        int i = 0;
        for( i = 0; i < cJSON_GetArraySize(root); i ++ ) {
            cJSON * arrobj = cJSON_GetArrayItem( root, i );
            /// add arrobj into db
            s5_serv_usr_obj_add( cJSON_GetStringValue(arrobj) );
        }
		cJSON_Delete(root);
	}
    return OK;
}

static status s5_serv_usr_db_file_read( meta_t * meta )
{
    ssize_t size = 0;
    
    int fd = open( (char*)config_get()->s5_serv_auth_path, O_RDONLY  );
    if( ERROR == fd ) {
        err("usmgr auth open file [%s] failed, errno [%d]\n", config_get()->s5_serv_auth_path, errno );
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
        if( OK != s5_serv_usr_db_file_read( meta ) ) {
            err("usmgr auth file load failed\n");
            break;
        }
        if( OK != s5_serv_usr_db_parse( meta ) ) {
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

        queue_init(&g_s5_ctx->usable);
        queue_init(&g_s5_ctx->use);
        for( i = 0; i < MAX_NET_CON; i++ )
           queue_insert_tail( &g_s5_ctx->usable, &g_s5_ctx->pool[i].queue );
           
        if( config_get()->s5_mode > SOCKS5_CLIENT ) {
            /// s5 server mode or server screct mode                
            queue_init( &g_s5_ctx->g_users );
            if( OK != mem_page_create( &g_s5_ctx->g_user_mempage, sizeof(s5_user_t) ) ) {
                err("s5 serv alloc user mem page\n");
                break;
            }
            if( OK != s5_serv_usr_db_init() ) {
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

