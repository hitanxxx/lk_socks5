#include "l_base.h"

ssize_t udp_recvs( connection_t * c, char * start, uint32 len )
{
	ssize_t rc;
	socklen_t socklen = sizeof(c->addr);

	do 
	{
		rc = recvfrom( c->fd, start, len, 0, (struct sockaddr*)&c->addr, &socklen );
		if( rc < 0 )
		{
			if( (errno == EAGAIN) || (errno == EWOULDBLOCK) )
			{
				return AGAIN;
			}
			else if ( errno == EINTR )
			{
				continue;
			}
			err("udp recv failed, [%d]\n", errno );
			return ERROR;
		}
		else if ( rc == 0 )
		{
			err("peer closed\n");
			return ERROR;
		}
		return rc;
	}while(0);
}

ssize_t recvs( connection_t * c, char * start, uint32 len )
{
	ssize_t rc;

	do 
	{
		rc = recv( c->fd, start, len, 0 );
		if( rc < 0 )
		{
			if( (errno == EAGAIN) || (errno == EWOULDBLOCK) )
			{
				return AGAIN;
			}
			else if ( errno == EINTR )
			{
				continue;
			}
			err("recv failed, [%d]\n", errno );
			return ERROR;
		}
		else if ( rc == 0 )
		{
			err("peer closed\n");
			return ERROR;
		}
		return rc;
	} while(0);
}

ssize_t udp_sends( connection_t * c, char * start, uint32 len )
{
	ssize_t rc;
	socklen_t socklen = sizeof(struct sockaddr);

	do
	{
		rc = sendto( c->fd, start, len, 0, (struct sockaddr*)&c->addr, socklen );
		if( rc < 0 )
		{
			if( (errno == EAGAIN) || (errno == EWOULDBLOCK))
			{
				return AGAIN;
			}
			else if ( errno == EINTR )
			{
				continue;
			}
			err("udp send failed, [%d]\n", errno );
			return ERROR;
		}
		return rc;
	} while(0);
}

ssize_t sends( connection_t * c, char * start, uint32 len )
{
	ssize_t rc;

	/*
	the program should check the data to send, 
	the length of the data is never equal to 0, 
	so the return value cannot be 0
	*/

	do 
	{
		rc = send( c->fd, start, len, 0 );
		if( rc < 0 )
		{
			if( (errno == EAGAIN) || (errno == EWOULDBLOCK)) 
			{
				return AGAIN;
			}
			else if ( errno == EINTR )
			{
				continue;
			}
			err("send failed, [%d]\n", errno );
			return ERROR;
		}
		return rc;
	} while(0);
}

inline static status send_chains_meta_need_send( meta_t * meta )
{
	return ( (meta_len( meta->pos, meta->last ) > 0) ? OK : ERROR );
}

status send_chains( connection_t * c, meta_t * head )
{
	meta_t * cur = NULL;
	status rc = 0;

	while(1)
	{
		for( cur = head; cur; cur = cur->next )
		{
			if( OK == send_chains_meta_need_send( cur ) )
			{
				break;
			}
		}
		if( cur == NULL )
		{
			return DONE;
		}
		
		rc = sends( c, cur->pos, meta_len( cur->pos, cur->last ) );
		if( rc < 0 )
		{
			return rc;
		}
		cur->pos += rc;
	}
}


