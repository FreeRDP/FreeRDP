/*
 * Copyright © 2012 Collabora, Ltd.
 * Copyright © 2014 David FORT <contact@hardening-consulting.com>
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/*
 * This file is an adaptation of src/wayland-os.h from the wayland project and
 * shared/os-compatiblity.h from the weston project.
 *
 * Functions have been renamed just to prevent name clashes.
 */

#ifndef __UWAC_OS_H
#define __UWAC_OS_H

#include <sys/socket.h>

int uwac_os_socket_cloexec(int domain, int type, int protocol);

int uwac_os_dupfd_cloexec(int fd, long minfd);

ssize_t uwac_os_recvmsg_cloexec(int sockfd, struct msghdr *msg, int flags);

int uwac_os_epoll_create_cloexec(void);

int uwac_create_anonymous_file(off_t size);
#endif /* __UWAC_OS_H */
