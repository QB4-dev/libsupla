/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "util.h"
#include "net.h"

#if (LIBSUPLA_ARCH == LIBSUPLA_ARCH_UNIX)

uint64_t supla_time_getmonotonictime_milliseconds(void)
{
    struct timespec current_time;
    clock_gettime(CLOCK_MONOTONIC, &current_time);
    return (uint64_t)((current_time.tv_sec * 1000) + (current_time.tv_nsec / 1000000));
}

#ifndef NOSSL
#include "../supla-common/supla-socket.h"

int supla_cloud_connect(supla_link_t *link, const char *host, int port, unsigned char ssl)
{
    *link = ssocket_client_init(host, port, ssl);
    return ssocket_client_connect(*link, NULL, NULL, 500);
}

int supla_cloud_send(supla_link_t link, void *buf, int count)
{
    return ssocket_write(link, NULL, buf, count);
}

int supla_cloud_recv(supla_link_t link, void *buf, int count)
{
    return ssocket_read(link, NULL, buf, count);
}

int supla_cloud_disconnect(supla_link_t *link)
{
    if (!link)
        return EINVAL;

    ssocket_free(*link);
    *link = 0;
    return 0;
}

#else

typedef struct {
    int sfd;
} socket_data_t;

int supla_cloud_connect(supla_link_t *link, const char *host, int port, unsigned char ssl)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp = NULL;
    int rc;

    socket_data_t *ssd = malloc(sizeof(socket_data_t));
    if (!ssd)
        return SUPLA_RESULT_FALSE;

    memset(ssd, 0, sizeof(socket_data_t));
    ssd->sfd = -1;

    *link = ssd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICSERV;

    // Address resolution.
    rc = getaddrinfo(host, NULL, &hints, &result);
    if (rc != 0) {
        free(ssd);
        return rc;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        ssd->sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (ssd->sfd == -1)
            continue;

        switch (rp->ai_family) {
        case AF_INET6:
            ((struct sockaddr_in6 *)(rp->ai_addr))->sin6_port = htons(port);
            break;
        case AF_INET:
            ((struct sockaddr_in *)(rp->ai_addr))->sin_port = htons(port);
            break;
        default:
            free(ssd);
            return SUPLA_RESULT_FALSE;
        }

        // Attempt to connect.
        rc = connect(ssd->sfd, rp->ai_addr, rp->ai_addrlen);
        if (rc != -1) {
            freeaddrinfo(result);
            return SUPLA_RESULT_TRUE;
        } else {
            if (errno == EINPROGRESS) {
                freeaddrinfo(result);
                return SUPLA_RESULT_TRUE;
            } else {
                close(ssd->sfd);
            }
        }
    }
    freeaddrinfo(result);
    free(ssd);
    return SUPLA_RESULT_FALSE;
}

int supla_cloud_send(supla_link_t link, void *buf, int count)
{
    socket_data_t *ssd = link;
    return send(ssd->sfd, buf, count, MSG_NOSIGNAL);
}

int supla_cloud_recv(supla_link_t link, void *buf, int count)
{
    socket_data_t *socket_data = link;
    return recv(socket_data->sfd, buf, count, MSG_DONTWAIT);
}

int supla_cloud_disconnect(supla_link_t *link)
{
    if (!link)
        return EINVAL;

    socket_data_t *ssd = *link;
    if (!ssd)
        return EINVAL;

    if (ssd->sfd != -1) {
        shutdown(ssd->sfd, SHUT_RDWR);
        close(ssd->sfd);
    }
    free(ssd);
    return 0;
}

#endif //NOSSL

#endif
