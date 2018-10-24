/**
 * FreeRDP: A Remote Desktop Protocol Client
 * DirectFB Graphical Objects
 *
 * Copyright 2014 Killer{R} <support@killprog.com>
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

#include <linux/vt.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include "df_vt.h"

static volatile boolean vt_active = false;
static short relsig, acqsig;
static struct sigaction sa_relsig, sa_acqsig;
static uint8 vt_sig_usecnt = 0;


static int vt_tty_fd = -1;
static int vt_mine = -1;
static uint8 vt_tty_usecnt = 0;

INLINE static int get_active_vt()
{
	struct vt_stat vts;
	if (ioctl(vt_tty_fd, VT_GETSTATE, &vts)==-1)
		return -1;

	return vts.v_active;
}

static void df_sigaction_pre(int sig)
{
	if (sig==relsig)
	{
		vt_active = false;
		printf("df_sigaction: VT released\n");
	}
}

static void df_sigaction_post(int sig)
{
	if (sig==acqsig)
	{
		vt_active = true;
		printf("df_sigaction: VT acquired\n");
	}
}

static void df_sigaction_term_release(int sig, siginfo_t *si, void *p)
{
	df_sigaction_pre(sig);
	if (sa_relsig.sa_flags & SA_SIGINFO)
		sa_relsig.sa_sigaction(sig, si, p);
	else
		sa_relsig.sa_handler(sig);
	df_sigaction_post(sig);
}

static void df_sigaction_term_acquire(int sig, siginfo_t *si, void *p)
{
	df_sigaction_pre(sig);
	if (sa_acqsig.sa_flags & SA_SIGINFO)
		sa_acqsig.sa_sigaction(sig, si, p);
	else
		sa_acqsig.sa_handler(sig);
	df_sigaction_post(sig);
}

void df_vt_register()
{
	struct vt_mode   vtm;
	struct sigaction sa;
	char sz[32];
	int fd, i;

	if (vt_sig_usecnt)
	{
		++vt_tty_usecnt;
		++vt_sig_usecnt;
		assert(vt_sig_usecnt==0 || vt_tty_usecnt==0 || vt_tty_usecnt==1);
		return;
	}

	if (vt_tty_usecnt==0)
	{
		for (i = 0; i<12; ++i)
		{
			sprintf(sz, "/dev/tty%d", i);
			vt_tty_fd = open(sz, O_RDWR | O_NOCTTY);
			if (vt_tty_fd!=-1)
			{
				vt_mine = get_active_vt();
				if (vt_mine != -1)
				{
					printf("Mine VT is %d\n", vt_mine);
					break;
				}
				close(vt_tty_fd);
				vt_tty_fd = -1;
			}
		}
		if (vt_tty_fd == -1)
		{
			fprintf(stderr, "Failed to open TTY\n");
			return;
		}

		++vt_tty_usecnt;
		assert(vt_tty_usecnt);
	}

	sprintf(sz, "/dev/tty%d", vt_mine);
	fd = open( sz, O_RDWR | O_NOCTTY );
	if (fd==-1)
		return;

	i = ioctl( fd, VT_GETMODE, &vtm);
	close(fd);

	if (i<0)
		return;

	relsig = vtm.relsig;
	acqsig = vtm.acqsig;
	if (!relsig || !acqsig)
	{
//		printf("Please enable DirectFB vt-switching option to make VT switching work well\n");
		return;
	}

	if (!sigaction(relsig, NULL, &sa_relsig) < 0)
	{
		perror("get VT release signal\n");
		return;
	}

	if (!sigaction(acqsig, NULL, &sa_acqsig) < 0)
	{
		perror("get VT acquire signal\n");
		return;
	}

	memcpy(&sa, &sa_relsig, sizeof(sa));
	sa.sa_flags |= SA_SIGINFO;
	sa.sa_sigaction  = df_sigaction_term_release;
	if (sigaction( relsig, &sa, &sa_relsig) < 0)
	{
		perror("set VT release signal\n");
		return;
	}

	memcpy(&sa, &sa_acqsig, sizeof(sa));
	sa.sa_flags |= SA_SIGINFO;
	sa.sa_sigaction  = df_sigaction_term_acquire;
	if (sigaction( acqsig, &sa, &sa_acqsig) < 0)
	{
		perror("set VT acquire signal\n");
		sigaction( relsig, &sa_relsig, NULL);
		return;
	}

	++vt_sig_usecnt;
	assert(vt_sig_usecnt);
	vt_active = true;
}

void df_vt_deregister()
{
	if (vt_sig_usecnt)
	{
		if (!--vt_sig_usecnt)
		{
			sigaction( relsig, &sa_relsig, NULL );
			sigaction( acqsig, &sa_acqsig, NULL);
		}
	}

	if (vt_tty_usecnt)
	{
		if (!--vt_tty_usecnt)
		{
			close(vt_tty_fd);
			vt_tty_fd = -1;
		}
	}
}

boolean df_vt_is_disactivated_slow()
{
	if (vt_sig_usecnt)
		return !vt_active;

	return (vt_tty_usecnt && vt_mine != get_active_vt());
}

boolean df_vt_is_disactivated_fast_unreliable()
{
	return (vt_sig_usecnt && !vt_active);
}
