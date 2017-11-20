/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * SSH Agent Virtual Channel Extension
 *
 * Copyright 2012-2013 Jay Sorg
 * Copyright 2012-2013 Laxmikant Rashinkar
 * Copyright 2017 Ben Cohen
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Portions are from OpenSSH, under the following license:
 *
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * The authentication agent program.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * Copyright (c) 2000, 2001 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * xrdp-ssh-agent.c: program to forward ssh-agent protocol from xrdp session
 *
 * This performs the equivalent function of ssh-agent on a server you connect
 * to via ssh, but the ssh-agent protocol is over an RDP dynamic virtual
 * channel and not an SSH channel.
 *
 * This will print out variables to set in your environment (specifically,
 * $SSH_AUTH_SOCK) for ssh clients to find the agent's socket, then it will
 * run in the background.  This is suitable to run just as you would run the
 * normal ssh-agent, e.g. in your Xsession or /etc/xrdp/startwm.sh.
 *
 * Your RDP client needs to be running a compatible client-side plugin
 * that can see a local ssh-agent.
 *
 * usage (from within an xrdp session):
 *     xrdp-ssh-agent
 *
 * build instructions:
 *     gcc xrdp-ssh-agent.c -o xrdp-ssh-agent -L./.libs -lxrdpapi -Wall
 *
 * protocol specification:
 *     Forward data verbatim over RDP dynamic virtual channel named "sshagent"
 *     between a ssh client on the xrdp server and the real ssh-agent where
 *     the RDP client is running.  Each connection by a separate client to
 *     xrdp-ssh-agent gets a separate DVC invocation.
 */

#if defined(HAVE_CONFIG_H)
#include <config.h>
#endif

#ifdef __WIN32__
#include <mstsapi.h>
#endif

#include <freerdp/channels/wtsvc.h>


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/resource.h>

#define _PATH_DEVNULL  "/dev/null"

char socket_name[PATH_MAX];
char socket_dir[PATH_MAX];
static int sa_uds_fd = -1;
static int is_going = 1;


/* Make a template filename for mk[sd]temp() */
/* This is from mktemp_proto() in misc.c from openssh */
void
mktemp_proto(char* s, size_t len)
{
	const char* tmpdir;
	int r;

	if ((tmpdir = getenv("TMPDIR")) != NULL)
	{
		r = snprintf(s, len, "%s/ssh-XXXXXXXXXXXX", tmpdir);

		if (r > 0 && (size_t)r < len)
			return;
	}

	r = snprintf(s, len, "/tmp/ssh-XXXXXXXXXXXX");

	if (r < 0 || (size_t)r >= len)
	{
		fprintf(stderr, "%s: template string too short", __func__);
		exit(1);
	}
}


/* This uses parts of main() in ssh-agent.c from openssh */
static void
setup_ssh_agent(struct sockaddr_un* addr)
{
	int rc;
	/* Create private directory for agent socket */
	mktemp_proto(socket_dir, sizeof(socket_dir));

	if (mkdtemp(socket_dir) == NULL)
	{
		perror("mkdtemp: private socket dir");
		exit(1);
	}

	snprintf(socket_name, sizeof socket_name, "%s/agent.%ld", socket_dir,
	         (long)getpid());
	/* Create unix domain socket */
	unlink(socket_name);
	sa_uds_fd = socket(AF_UNIX, SOCK_STREAM, 0);

	if (sa_uds_fd == -1)
	{
		fprintf(stderr, "sshagent: socket creation failed");
		exit(2);
	}

	memset(addr, 0, sizeof(struct sockaddr_un));
	addr->sun_family = AF_UNIX;
	strncpy(addr->sun_path, socket_name, sizeof(addr->sun_path));
	addr->sun_path[sizeof(addr->sun_path) - 1] = 0;
	/* Create with privileges rw------- so other users can't access the UDS */
	mode_t umask_sav = umask(0177);
	rc = bind(sa_uds_fd, (struct sockaddr*)addr, sizeof(struct sockaddr_un));

	if (rc != 0)
	{
		fprintf(stderr, "sshagent: bind failed");
		close(sa_uds_fd);
		unlink(socket_name);
		exit(3);
	}

	umask(umask_sav);
	rc = listen(sa_uds_fd, /* backlog = */ 5);

	if (rc != 0)
	{
		fprintf(stderr, "listen failed\n");
		close(sa_uds_fd);
		unlink(socket_name);
		exit(1);
	}

	/* Now fork: the child becomes the ssh-agent daemon and the parent prints
	 * out the pid and socket name. */
	pid_t pid = fork();

	if (pid == -1)
	{
		perror("fork");
		exit(1);
	}
	else if (pid != 0)
	{
		/* Parent */
		close(sa_uds_fd);
		printf("SSH_AUTH_SOCK=%s; export SSH_AUTH_SOCK;\n", socket_name);
		printf("SSH_AGENT_PID=%d; export SSH_AGENT_PID;\n", pid);
		printf("echo Agent pid %d;\n", pid);
		exit(0);
	}

	/* Child */

	if (setsid() == -1)
	{
		fprintf(stderr, "setsid failed");
		exit(1);
	}

	(void)chdir("/");
	int devnullfd;

	if ((devnullfd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1)
	{
		/* XXX might close listen socket */
		(void)dup2(devnullfd, STDIN_FILENO);
		(void)dup2(devnullfd, STDOUT_FILENO);
		(void)dup2(devnullfd, STDERR_FILENO);

		if (devnullfd > 2)
			close(devnullfd);
	}

	/* deny core dumps, since memory contains unencrypted private keys */
	struct rlimit rlim;
	rlim.rlim_cur = rlim.rlim_max = 0;

	if (setrlimit(RLIMIT_CORE, &rlim) < 0)
	{
		fprintf(stderr, "setrlimit RLIMIT_CORE: %s", strerror(errno));
		exit(1);
	}
}


static void
handle_connection(int client_fd)
{
	int     rdp_fd = -1;
	int     rc;
	void* channel = WTSVirtualChannelOpenEx(WTS_CURRENT_SESSION,
	                                        "SSHAGENT",
	                                        WTS_CHANNEL_OPTION_DYNAMIC_PRI_MED);

	if (channel == NULL)
	{
		fprintf(stderr, "WTSVirtualChannelOpenEx() failed\n");
	}

	unsigned int retlen;
	int* retdata;
	rc = WTSVirtualChannelQuery(channel,
	                            WTSVirtualFileHandle,
	                            (void**)&retdata,
	                            &retlen);

	if (!rc)
	{
		fprintf(stderr, "WTSVirtualChannelQuery() failed\n");
	}

	if (retlen != sizeof(rdp_fd))
	{
		fprintf(stderr, "WTSVirtualChannelQuery() returned wrong length %d\n",
		        retlen);
	}

	rdp_fd = *retdata;
	int client_going = 1;

	while (client_going)
	{
		/* Wait for data from RDP or the client */
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(client_fd, &readfds);
		FD_SET(rdp_fd, &readfds);
		select(FD_SETSIZE, &readfds, NULL, NULL, NULL);

		if (FD_ISSET(rdp_fd, &readfds))
		{
			/* Read from RDP and write to the client */
			char buffer[4096];
			unsigned int bytes_to_write;
			rc = WTSVirtualChannelRead(channel,
			                           /* TimeOut = */ 5000,
			                           buffer,
			                           sizeof(buffer),
			                           &bytes_to_write);

			if (rc == 1)
			{
				char* pos = buffer;
				errno = 0;

				while (bytes_to_write > 0)
				{
					int bytes_written = send(client_fd, pos, bytes_to_write, 0);

					if (bytes_written > 0)
					{
						bytes_to_write -= bytes_written;
						pos += bytes_written;
					}
					else if (bytes_written == 0)
					{
						fprintf(stderr, "send() returned 0!\n");
					}
					else if (errno != EINTR)
					{
						/* Error */
						fprintf(stderr, "Error %d on recv\n", errno);
						client_going = 0;
					}
				}
			}
			else
			{
				/* Error */
				fprintf(stderr, "WTSVirtualChannelRead() failed: %d\n", errno);
				client_going = 0;
			}
		}

		if (FD_ISSET(client_fd, &readfds))
		{
			/* Read from the client and write to RDP */
			char buffer[4096];
			ssize_t bytes_to_write = recv(client_fd, buffer, sizeof(buffer), 0);

			if (bytes_to_write > 0)
			{
				char* pos = buffer;

				while (bytes_to_write > 0)
				{
					unsigned int bytes_written;
					int rc = WTSVirtualChannelWrite(channel,
					                                pos,
					                                bytes_to_write,
					                                &bytes_written);

					if (rc == 0)
					{
						fprintf(stderr, "WTSVirtualChannelWrite() failed: %d\n",
						        errno);
						client_going = 0;
					}
					else
					{
						bytes_to_write -= bytes_written;
						pos += bytes_written;
					}
				}
			}
			else if (bytes_to_write == 0)
			{
				/* Client has closed connection */
				client_going = 0;
			}
			else
			{
				/* Error */
				fprintf(stderr, "Error %d on recv\n", errno);
				client_going = 0;
			}
		}
	}

	WTSVirtualChannelClose(channel);
}


int
main(int argc, char** argv)
{
	/* Setup the Unix domain socket and daemon process */
	struct sockaddr_un addr;
	setup_ssh_agent(&addr);

	/* Wait for a client to connect to the socket */
	while (is_going)
	{
		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sa_uds_fd, &readfds);
		select(FD_SETSIZE, &readfds, NULL, NULL, NULL);

		/* If something connected then get it...
		 * (You can test this using "socat - UNIX-CONNECT:<udspath>".) */
		if (FD_ISSET(sa_uds_fd, &readfds))
		{
			socklen_t addrsize = sizeof(addr);
			int client_fd = accept(sa_uds_fd,
			                       (struct sockaddr*)&addr,
			                       &addrsize);
			handle_connection(client_fd);
			close(client_fd);
		}
	}

	close(sa_uds_fd);
	unlink(socket_name);
	return 0;
}

/* vim: set sw=4:ts=4:et: */
