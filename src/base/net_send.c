#include "common.h"

int recvs(con_t * c, unsigned char * buf, int bufn)
{
    int rc;
    sys_assert(bufn > 0);
    sys_assert(buf != NULL);
    sys_assert(c != NULL);

    for(;;) {
        rc = recv(c->fd, buf, bufn, 0);
        if(rc <= 0) {
            if(rc == 0) {
                err("peer closed\n");
                return -1;
            } else {
                if((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                    return -11;
                } else if (errno == EINTR) {
                    continue;
                } else {
                    err("rec failed. [%d] [%s]\n", errno, strerror(errno));
                    return -1;
                }
            }
        }
        return rc;
    };
}


int sends(con_t * c, unsigned char * buf, int bufn)
{
    int rc;
    
    sys_assert(bufn > 0);
    sys_assert(buf != NULL);
    sys_assert(c != NULL);

    for(;;) {
        rc = send(c->fd, buf, bufn, 0);
        if(rc < 0) {
            if((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                return -11;
            } else if (errno == EINTR) {
                continue;
            } else {
                err("send failed. [%d] [%s]\n", errno, strerror(errno));
                return -1;
            }
        }
        return rc;
    };
}

int send_chains(con_t * c, meta_t * head)
{
    sys_assert(c != NULL);
    sys_assert(head != NULL);
    
    for(;;) {
        meta_t * n = head;
        while(n) {
            if(meta_getlen(n) > 0) {
                break;
            }
            n = n->next;
        }
        if(!n) {
            return 1;
        }
        int sendn = sends(c, n->pos, meta_getlen(n));
        if(sendn < 0) {
            if(-11 == sendn) {
                return -11;
            }
            return -1;
        }
        n->pos += sendn;
    }
}

int udp_recvs(con_t * c, unsigned char * buf, int bufn)
{
    socklen_t socklen = sizeof(struct sockaddr);
    
    sys_assert(c != NULL);
    sys_assert(buf != NULL);
    sys_assert(bufn > 0);

    for(;;) {
        int recvd = recvfrom(c->fd, buf, bufn, 0, (struct sockaddr*)&c->addr, &socklen);
        if(recvd <= 0) {
            if(recvd == 0) {
                return -1;
            } else {
                if((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                    return -11;
                } else if (errno == EINTR) {
                    continue;
                } else {
                    return -1;
                }
            }
        }
        return recvd;
    };
}

int udp_sends(con_t * c, unsigned char * buf, int bufn)
{
    socklen_t socklen = sizeof(struct sockaddr);

    sys_assert(c != NULL);
    sys_assert(buf != NULL);
    sys_assert(bufn > 0);

    for(;;) {
        int sendn = sendto(c->fd, buf, bufn, 0, (struct sockaddr*)&c->addr, socklen);
        if(sendn < 0) {
            if((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                return -11;
            } else if (errno == EINTR) {
                continue;
            } else {
                return -1;
            }
        }
        return sendn;
    };
}

