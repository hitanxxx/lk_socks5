#include "common.h"




ssize_t recvs( connection_t * c, unsigned char * buffer, uint32 len )
{
    ssize_t rc;

    sys_assert( len > 0 );
    sys_assert( buffer != NULL );
    sys_assert( c != NULL );
    
    while(1)
    {
        rc = recv( c->fd, buffer, len, 0 );

        if( rc <= 0 )
        {
            if( rc ==0 )
                return ERROR;
            else
            {
                if( (errno == EAGAIN) || (errno == EWOULDBLOCK) )
                    return AGAIN;
                else if ( errno == EINTR )
                    continue;
                else
                    return ERROR;
            }
        }
        return rc;
    };
}



ssize_t sends( connection_t * c, unsigned char * buffer, uint32 len )
{
    ssize_t rc;
    
    sys_assert( len > 0 );
    sys_assert( buffer != NULL );
    sys_assert( len > 0 );

    while(1)
    {
        rc = send( c->fd, buffer, len, 0 );
        if( rc < 0 )
        {
            if( (errno == EAGAIN) || (errno == EWOULDBLOCK) )
                return AGAIN;
            else if ( errno == EINTR )
                continue;
            else
                return ERROR;
        }
        return rc;
    };
}

inline static status meta_need_send( meta_t * meta )
{
    return( meta->last > meta->pos ) ? OK : ERROR;
}

status send_chains( connection_t * c, meta_t * head )
{
    meta_t * cur = NULL;
    ssize_t size = 0;
    
    sys_assert( c != NULL );
    sys_assert( head != NULL );

    while(1)
    {
        cur = head;
        while( cur )
        {
            if( OK == meta_need_send(cur) )
                break;
            cur = cur->next;
        }

        if( cur == NULL )
            return DONE;

        size = sends( c, cur->pos, meta_len( cur->pos, cur->last ) );
        if( size < 0 )
        {
            if( AGAIN == size )
                return AGAIN;
            
            return ERROR;
        }
        cur->pos += size;
    };
}

ssize_t udp_recvs( connection_t * c, unsigned char * buffer, uint32 len )
{
    ssize_t rc;
    socklen_t socklen = sizeof(struct sockaddr);

    sys_assert( c != NULL );
    sys_assert( buffer != NULL );
    sys_assert( len > 0 );

    while(1)
    {
        rc = recvfrom( c->fd, buffer, len, 0, (struct sockaddr*)&c->addr, &socklen );
        if( rc <= 0 )
        {
            if( rc == 0 )
                return ERROR;
            else 
            {
                if( (errno == EAGAIN) || (errno == EWOULDBLOCK) )
                    return AGAIN;
                else if ( errno == EINTR )
                    continue;
                else 
                    return ERROR;
            }
        }
        return rc;
    };
}


ssize_t udp_sends( connection_t * c, unsigned char * buffer, uint32 len )
{
    ssize_t rc;
    socklen_t socklen = sizeof(struct sockaddr);

    sys_assert( c != NULL);
    sys_assert( buffer != NULL);
    sys_assert( len > 0 );

    while(1)
    {
        rc = sendto( c->fd, buffer, len, 0, (struct sockaddr*)&c->addr, socklen );
        if( rc < 0 )
        {
            if( (errno == EAGAIN) || (errno == EWOULDBLOCK))
                return AGAIN;
            else if ( errno == EINTR )
                continue;
            else
                return ERROR;
        }
        return rc;
    };
}

