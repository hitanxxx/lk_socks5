#include "common.h"
#include "dns.h"
#include "s5_local.h"
#include "s5_server.h"

#define S5_USER_AUTH_FILE_LEN  (4*1024)

typedef struct 
{
    ezhash_t * auth_hash;
} g_s5_t;
static g_s5_t * g_s5_ctx = NULL;

status s5_alloc( socks5_cycle_t ** s5 )
{
    socks5_cycle_t * ns5 = mem_pool_alloc(sizeof(socks5_cycle_t));
    if(!ns5) {
        err("ns5 alloc failed\n");
        return ERROR;
    }
    *s5 = ns5;
    return OK;
}


status s5_free( socks5_cycle_t * s5 )
{
    memset( &s5->phase1, 0x0, sizeof(s5_rfc_phase1_req_t) );
    memset( &s5->phase2, 0x0, sizeof(s5_rfc_phase2_req_t) );

#ifndef S5_OVER_TLS
    if( s5->cipher_enc ) {
        sys_cipher_ctx_deinit(s5->cipher_enc);
        s5->cipher_enc = NULL;
    }
    if( s5->cipher_dec ) {
        sys_cipher_ctx_deinit(s5->cipher_dec);
        s5->cipher_dec = NULL;
    }
#endif

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

    mem_pool_free(s5);
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

    /// try recv if have space
    while( down->meta->end > down->meta->last ) {
        recvn = down->recv( down, down->meta->last, down->meta->end - down->meta->last );
        if( recvn < 0 ) {
            if( recvn == ERROR ) {
                err("s5 down recv error\n");
                s5->recv_down_err = 1;
            }
            /// means again
            break;
        }
#ifndef S5_OVER_TLS
        if( s5->typ == SOCKS5_CLIENT ) {
		    /// down -> up enc
            if( recvn != sys_cipher_conv( s5->cipher_enc, down->meta->last, recvn ) ) {
                err("s5 local cipher enc failed\n");
                s5_free(s5);
                return ERROR;
            }
        } else {
		    /// down->up dec
            if( recvn != sys_cipher_conv( s5->cipher_dec, down->meta->last, recvn ) ) {
                err("s5 server cipher dec failed\n");
                s5_free(s5);
                return ERROR;
            }
        }
#endif
        down->meta->last += recvn;
    }

    if( down->meta->last > down->meta->pos ) {
        event_opt( down->event, down->fd, down->event->opt & ~EV_R );
        event_opt( up->event, up->fd, up->event->opt|EV_W );
        return up->event->write_pt(up->event);
    } else {
        if( s5->recv_down_err == 1 ) {
            s5_free(s5);
            return ERROR;
        }
        return AGAIN;
    }
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
        err("s5 up send, down already error\n");
        s5_free(s5);
        return ERROR;
    } else {
        down->meta->last = down->meta->pos = down->meta->start;
        event_opt( up->event, up->fd, up->event->opt & ~EV_W );
        event_opt( down->event, down->fd, down->event->opt|EV_R );
        return down->event->read_pt( down->event );			
    }
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
            break;
        }
#ifndef S5_OVER_TLS
        if( s5->typ == SOCKS5_CLIENT ) {
	        /// up -> down dec
            if( recvn != sys_cipher_conv( s5->cipher_dec, up->meta->last, recvn ) ) {
                err("s5 local cipher dec failed\n");
                s5_free(s5);
                return ERROR;
            }   
        } else {
	        /// up -> down enc
            if( recvn != sys_cipher_conv( s5->cipher_enc, up->meta->last, recvn ) ) {
                err("s5 server cipher enc failed\n");
                s5_free(s5);
                return ERROR;
            }
        }
#endif
        up->meta->last += recvn;
    }

    if( up->meta->last > up->meta->pos ) {
        event_opt( up->event, up->fd, up->event->opt & ~EV_R );
        event_opt( down->event, down->fd, down->event->opt|EV_W );
        return down->event->write_pt( down->event );
    } else {
        if( s5->recv_up_err == 1 ) {
            s5_free(s5);
            return ERROR;
        }
        return AGAIN;
    }
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
        err("s5 down send, up already error\n");
        s5_free(s5);
        return ERROR;
    } else {
        up->meta->last = up->meta->pos = up->meta->start;		
        event_opt( down->event, down->fd, down->event->opt & ~EV_W );
        event_opt( up->event, up->fd, up->event->opt|EV_R );
        return up->event->read_pt( up->event );
    }
}


status s5_traffic_process( event_t * ev )
{
    connection_t * c = ev->data;
    socks5_cycle_t * s5 = c->data;
    connection_t * down = s5->down;
    connection_t * up = s5->up;

    // init down stream traffic buffer
    if( !down->meta ) {
        if( OK != meta_alloc( &down->meta, 8192 ) ) {
            err("s5 down meta alloc failed\n");
            s5_free(s5);
            return ERROR;
        }
    }
    
    // init up stream traffic buffer
    if( !up->meta ) {
        if( OK != meta_alloc( &up->meta, 8192 ) ) {
            err("s5 alloc up meta failed\n");
            s5_free(s5);
            return ERROR;
        }
    }
#ifndef S5_OVER_TLS
    if( !s5->cipher_enc ) {
        if( 0 != sys_cipher_ctx_init( &s5->cipher_enc, 0 ) ) {
            err("s5 server cipher enc init failed\n");
            s5_free(s5);
            return ERROR;
        }
    }
    if( !s5->cipher_dec ) {
        if( 0 != sys_cipher_ctx_init( &s5->cipher_dec, 1 ) ) {
            err("s5 server cipher dec init failed\n");
            s5_free(s5);
            return ERROR;
        }
    }
	
    int down_remain = down->meta->last - down->meta->pos;
    if( down_remain > 0 ) {
    	if( s5->typ == SOCKS5_CLIENT ) {
    		if( down_remain != sys_cipher_conv( s5->cipher_enc, down->meta->pos, down_remain ) ) {
                err("s5 client cipher enc remain failed\n");
                s5_free(s5);
                return ERROR;
            }
    	} else {
    		if( down_remain != sys_cipher_conv( s5->cipher_dec, down->meta->pos, down_remain ) ) {
                err("s5 server cipher dec remain failed\n");
                s5_free(s5);
                return ERROR;
            }
    	}	
    } 
#endif    
    /// only clear up meta in here. because s5 local run in here too.
    /// but local down mabey recv some data.
    up->meta->pos = up->meta->last = up->meta->start;

    /// set read/write callback funtion for transfer data between down and up
    /// down -> self -> up
    s5->down->event->read_pt = s5_traffic_recv;
    s5->up->event->write_pt	= s5_traffic_send;

    /// up -> self -> down 
    s5->up->event->read_pt = s5_traffic_back_recv;
    s5->down->event->write_pt = s5_traffic_back_send;

    /// default set readable
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
    
    meta->pos = meta->last = meta->start;  /// meta clear 
    /// goto process local server transfer data
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

    s5_rfc_phase2_resp_t * resp = ( s5_rfc_phase2_resp_t* )meta->last;
    resp->ver = 0x05;
    resp->rep = 0x00;
    resp->rsv = 0x00;
    resp->atyp = 0x01;
    resp->bnd_addr = htons((uint16_t)up->addr.sin_addr.s_addr);
    resp->bnd_port = htons(up->addr.sin_port);
    meta->last += sizeof(s5_rfc_phase2_resp_t);

#ifndef S5_OVER_TLS
    if( sizeof(s5_rfc_phase2_resp_t) != sys_cipher_conv( s5->cipher_enc, meta->pos, sizeof(s5_rfc_phase2_resp_t) ) ) {
        err("s5 server cipher enc data failed\n");
        s5_free(s5);
        return ERROR;
    }
#endif
    event_opt( up->event, up->fd, EV_NONE );
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
        s5_free(s5);
        return ERROR;
    }
    net_socket_nodelay( up->fd );
    timer_del( &ev->timer );

    ev->read_pt	= NULL;
    ev->write_pt = s5_server_rfc_phase2_resp_build;
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
            memcpy( &addr_port, s5->phase2.dst_port, sizeof(uint16_t) ); 
        
            snprintf( ipstr, sizeof(ipstr), "%d.%d.%d.%d",
                dns_cycle->answer.answer_addr[0],
                dns_cycle->answer.answer_addr[1],
                dns_cycle->answer.answer_addr[2],
                dns_cycle->answer.answer_addr[3] );
            
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

    /// s5 rfc phase2 resp not send yet. down just check connection
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
        memcpy( &addr_port, s5->phase2.dst_port, sizeof(uint16_t) );

        snprintf( ipstr, sizeof(ipstr), "%d.%d.%d.%d",
            (unsigned char )s5->phase2.dst_addr[0],
            (unsigned char )s5->phase2.dst_addr[1],
            (unsigned char )s5->phase2.dst_addr[2],
            (unsigned char )s5->phase2.dst_addr[3] );
        
        s5->up->addr.sin_family	= AF_INET;
        s5->up->addr.sin_port = addr_port;
        s5->up->addr.sin_addr.s_addr = inet_addr( ipstr );

        s5->up->event->read_pt = NULL;
        s5->up->event->write_pt = s5_server_connect;
        return s5->up->event->write_pt( s5->up->event );
        
    }  else if ( s5->phase2.atyp == S5_RFC_DOMAIN ) {
        /// domain type request, goto dns resolve the domain
        if( OK == dns_rec_find( (char*)s5->phase2.dst_addr, ipstr ) ) {
            /// dns cache find success, use dns cache  
            uint16_t addr_port = 0;
            memcpy( &addr_port, s5->phase2.dst_port, sizeof(uint16_t) );
            
            snprintf( ipstr, sizeof(ipstr), "%d.%d.%d.%d",
                (unsigned char )ipstr[0],
                (unsigned char )ipstr[1],
                (unsigned char )ipstr[2],
                (unsigned char )ipstr[3] );
        
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
        VER = 0,
        CMD,
        RSV,
        TYP,
        TYP_V4,
        TYP_V6,
        TYP_DOMAINN,
        TYP_DOMAIN,
        PORT,
        END
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
#ifndef S5_OVER_TLS
            if( rc != sys_cipher_conv( s5->cipher_dec, meta->last, rc ) ) {
                err("s5 server cipher dec data failed\n");
                s5_free(s5);
                return ERROR;
            }
#endif
            meta->last += rc;
        }

        for( ; meta->pos < meta->last; meta->pos ++ ) {
            p = meta->pos;
            if( s5->state == VER ) {
                /// ver is fixed. 0x05
                s5->phase2.ver = *p;
                s5->state = CMD;
                continue;
            }
            if( s5->state == CMD ) {
                /*
                    socks5 support cmd value
                    01				connect
                    02				bind
                    03				udp associate
                */
                s5->phase2.cmd = *p;
                s5->state = RSV;
                continue;
            }
            if( s5->state == RSV ) {    // RSV means resverd
                s5->phase2.rsv = *p;
                s5->state = TYP;
                continue;
            }
            if( s5->state == TYP ) {
                s5->phase2.atyp = *p;
                /*
                    atyp		type		length
                    0x01		ipv4		4
                    0x03		domain		first octet of domain part
                    0x04		ipv6		16
                */
                if( s5->phase2.atyp == S5_RFC_IPV4 ) {
                    s5->state = TYP_V4;
                    s5->phase2.dst_addr_n = 4;
                    s5->phase2.dst_addr_cnt = 0;
                    continue;
                } else if ( s5->phase2.atyp == S5_RFC_IPV6 ) {
                    s5->state = TYP_V6;
                    s5->phase2.dst_addr_n = 16;
                    s5->phase2.dst_addr_cnt = 0;
                    continue;
                } else if ( s5->phase2.atyp == S5_RFC_DOMAIN ) {
                    /// atpy domain -> dst addr domain len -> dst addr domain
                    s5->state = TYP_DOMAINN;
                    s5->phase2.dst_addr_n = 0;
                    s5->phase2.dst_addr_cnt = 0;
                    continue;
                }
                err("s5 server request atyp [%d] not support\n", s5->phase2.atyp );
                s5_free(s5);
                return ERROR;
            }
            if( s5->state == TYP_V4 ) {
                s5->phase2.dst_addr[(int)s5->phase2.dst_addr_cnt++] = *p;
                if( s5->phase2.dst_addr_cnt == 4 ) {
                    s5->state = PORT;
                    continue;
                }
            }
            if( s5->state == TYP_V6 ) {
                s5->phase2.dst_addr[(int)s5->phase2.dst_addr_cnt++] = *p;
                if( s5->phase2.dst_addr_cnt == 16 ) {
                    s5->state = PORT;
                    continue;
                }
            }
            if( s5->state == TYP_DOMAINN ) {
                s5->phase2.dst_addr_n = *p;
                s5->state = TYP_DOMAIN;
                if(s5->phase2.dst_addr_n < 0) s5->phase2.dst_addr_n = 0;
                if(s5->phase2.dst_addr_n > 255) s5->phase2.dst_addr_n = 255;
                continue;
            }
            if( s5->state == TYP_DOMAIN ) {
                s5->phase2.dst_addr[(int)s5->phase2.dst_addr_cnt++] = *p;
                if( s5->phase2.dst_addr_cnt == s5->phase2.dst_addr_n ) {
                    s5->state = PORT;
                    continue;
                }
            }
            if( s5->state == PORT ) {
                s5->phase2.dst_port[0] = *p;
                s5->state = END;
                continue;
            }
            if( s5->state == END ) {
                s5->phase2.dst_port[1] = *p;
                /// phase2 request recv finish
                timer_del( &ev->timer );
                /// reset state  
                s5->state = 0;
                
                meta->last = meta->pos = meta->start; /// meta clear 
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
            if( ev->opt != EV_W ) {
                event_opt( ev, down->fd, EV_W );
            }
            timer_set_data( &ev->timer, down );
            timer_set_pt( &ev->timer, s5_timeout_cb );
            timer_add( &ev->timer, S5_TIMEOUT );
            return AGAIN;
        }
        meta->pos += rc;
    }
    timer_del( &ev->timer );
    
    meta->pos = meta->last = meta->start; /// meta clear     
    /// goto recv phase2 request
    ev->read_pt	= s5_server_rfc_phase2_recv;
    ev->write_pt = NULL;
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
        1 byte	1 byte	    nmethods
        VERSION | METHODS | METHOD
    */
    enum {
        VERSION = 0,
        METHODN,
        METHOD
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
#ifndef S5_OVER_TLS
            if( rc != sys_cipher_conv( s5->cipher_dec, meta->last, rc ) ) {
                err("s5 server cipher dec data failed\n");
                s5_free(s5);
                return ERROR;
            }
#endif
            meta->last += rc;
        }

        for( ; meta->pos < meta->last; meta->pos ++ ) {
            p = meta->pos;
            if( s5->state == VERSION ) {
                s5->phase1.ver = *p;
                s5->state = METHODN;
                continue;
            }
            if( s5->state == METHODN ) {
                s5->phase1.methods_n = *p;
                s5->phase1.methods_cnt = 0;
                s5->state = METHOD;
                continue;
            }
            if( s5->state == METHOD ) {
                s5->phase1.methods[s5->phase1.methods_cnt++] = *p;
                if( s5->phase1.methods_n == s5->phase1.methods_cnt ) {
                    /// rfc2918 socks5 protocol phase1 request packet recv finish
                    timer_del( &ev->timer );

                    /// reset the state 
                    s5->state = 0;

                    meta->pos = meta->last = meta->start; /// meta clear 
                    /// build the phase1 response
                    s5_rfc_phase1_resp_t * resp = ( s5_rfc_phase1_resp_t* ) meta->pos;
                    resp->ver = 0x05;
                    resp->method = 0x00;
                    meta->last += sizeof(s5_rfc_phase1_resp_t);
#ifndef S5_OVER_TLS
                    if( sizeof(s5_rfc_phase1_resp_t) != sys_cipher_conv( s5->cipher_enc, meta->pos, sizeof(s5_rfc_phase1_resp_t) ) ) {
                        err("s5 server cipher enc data failed\n");
                        s5_free(s5);
                        return ERROR;
                    }
#endif
                    /// goto send phase1 response
                    ev->read_pt = NULL;
                    ev->write_pt = s5_server_rfc_phase1_send;
                    return ev->write_pt( ev );
                }
            }
        }
    }
}

static status s5_server_auth_recv( event_t * ev )
{
    /// porcess the private authroization between local and server  
    connection_t * down = ev->data;
    socks5_cycle_t * s5 = down->data;
    ssize_t rc = 0;
    meta_t * meta = down->meta;
    
    /// at least recv ad s5 auth header. then goto check the header
    while( ( meta->last - meta->pos ) < sizeof(s5_auth_t) ) {
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
#ifndef S5_OVER_TLS
        if( rc != sys_cipher_conv( s5->cipher_dec, meta->last, rc ) ) {
            err("s5 server cipher dec data failed\n");
            s5_free(s5);
            return ERROR;
        }
#endif
        meta->last += rc;
    }
    timer_del( &ev->timer );

    s5_auth_t * auth = (s5_auth_t*)meta->pos;
    if( ntohl( auth->magic ) != S5_AUTH_LOCAL_MAGIC ) {
        err("s5 pri, magic [0x%x] incorrect, should be S5_AUTH_LOCAL_MAGIC [0x%x]\n", auth->magic, S5_AUTH_LOCAL_MAGIC );
        s5_free( s5 );
        return ERROR;
    }
    if( NULL == ezhash_find( g_s5_ctx->auth_hash, (char*)auth->key ) ) {
        err("s5 auth find auth key failed. not found\n");
        s5_free(s5);
        return ERROR;
    }
    
    meta->pos = meta->last = meta->start;/// clear meta
    /// goto process rfc s5 phase1 
    ev->write_pt = NULL;
    ev->read_pt = s5_server_rfc_phase1_recv;
    event_opt( ev, down->fd, EV_R );
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
    s5->down = down;
    down->data = s5;

   
    if( !down->meta ) {
        if( OK != meta_alloc( &down->meta, 8192 ) ) {
            err("s5 alloc down meta failed\n");
            s5_free(s5);
            return ERROR;
        }
    }
#ifndef S5_OVER_TLS
    if( !s5->cipher_enc ) {
        if( 0 != sys_cipher_ctx_init( &s5->cipher_enc, 0 ) ) {
            err("s5 server cipher enc init failed\n");
            s5_free(s5);
            return ERROR;
        }
    }
    if( !s5->cipher_dec ) {
        if( 0 != sys_cipher_ctx_init( &s5->cipher_dec, 1 ) ) {
            err("s5 server cipher dec init failed\n");
            s5_free(s5);
            return ERROR;
        }
    }
#endif
    ev->read_pt	= s5_server_auth_recv;
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

    ev->read_pt = s5_server_auth_recv;
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

#ifndef S5_OVER_TLS
    ev->read_pt = s5_server_start;
    return ev->read_pt( ev );
#endif

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


static status s5_serv_usr_db_parse( meta_t * meta )
{
    cJSON * root = cJSON_Parse( (char*)meta->pos );
    if(root) {
        /// traversal the array 
        int i = 0;
        for( i = 0; i < cJSON_GetArraySize(root); i ++ ) {
            cJSON * arrobj = cJSON_GetArrayItem( root, i );
            if( 0 != ezhash_add( g_s5_ctx->auth_hash, cJSON_GetStringValue(arrobj), "0" ) ) {
                err("s5 serve user key [%s] add hash failed\n", cJSON_GetStringValue(arrobj) );
            }
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
    int ret = -1;

    if( g_s5_ctx ) {
        err("s5 ctx not empty\n");
        return ERROR;
    }

    do {
        g_s5_ctx = (g_s5_t*)mem_pool_alloc( sizeof(g_s5_t) );
        if( !g_s5_ctx ) {
            err("s5 ctx alloc failed\n" );
            return ERROR;
        }
        
        if( config_get()->s5_mode > SOCKS5_CLIENT ) {
            /// build a hash table to manager s5 users.     
            if( OK != ezhash_create( &g_s5_ctx->auth_hash, 64 ) ) {
                err("s5 serv user hash create failed\n");
                break;
            }
            /// init the hash table uses.
            if( OK != s5_serv_usr_db_init() ) {
                err("s5 serv usr db init failed\n");
                break;
            }
        } else {
            /// cli do nothing.
        }
        ret = 0;
    } while(0);

    if( ret == -1 ) {
        if( g_s5_ctx ) {
            if( config_get()->s5_mode > SOCKS5_CLIENT ) {
                ezhash_free(g_s5_ctx->auth_hash);
            }
            mem_pool_free(g_s5_ctx);
            g_s5_ctx = NULL;
        }
    }
    return OK;
}

status socks5_server_end( void )
{
    if( g_s5_ctx ) {
        if( g_s5_ctx->auth_hash ) {
            ezhash_free(g_s5_ctx->auth_hash);
            g_s5_ctx->auth_hash = NULL;
        }
        mem_pool_free((void*)g_s5_ctx);
        g_s5_ctx = NULL;
    }
    return OK;
}

