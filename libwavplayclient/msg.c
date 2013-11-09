/* $Header$
 * Warren W. Gay VE3WWG		Tue Feb 25 22:45:09 1997
 *
 * MESSAGE QUEUE FUNCTIONS:
 *
 * 	X LessTif WAV Play :
 *
 * 	Copyright (C) 1997  Warren W. Gay VE3WWG
 *
 * This  program is free software; you can redistribute it and/or modify it
 * under the  terms  of  the GNU General Public License as published by the
 * Free Software Foundation version 2 of the License.
 *
 * This  program  is  distributed  in  the hope that it will be useful, but
 * WITHOUT   ANY   WARRANTY;   without   even  the   implied   warranty  of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details (see enclosed file COPYING).
 *
 * You  should have received a copy of the GNU General Public License along
 * with this  program; if not, see Foundation, Inc.,
 * 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Send correspondance to:
 *
 * 	Warren W. Gay VE3WWG
 *
 * Email:
 *	ve3wwg@yahoo.com
 *	wgay@mackenziefinancial.com
 *
 * Revision 1.2  1999/12/04 00:01:20  wwg
 * Implement wavplay-1.4 release changes
 *
 * Revision 1.1.1.1  1999/11/21 19:50:56  wwg
 * Import wavplay-1.3 into CVS
 *
 * Revision 1.1  1997/04/14 00:20:55  wwg
 * Initial revision
 *
 */
static const char rcsid[] = "@(#)msg.c $Revision$";

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <memory.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/ioctl.h>
#include <assert.h>
#ifdef __CYGWIN__
#include <sys/soundcard.h>
#else
#include <linux/soundcard.h>
#endif
#include "wavplay.h"

/*
 * Create a private message queue:
 */
int
MsgCreate(void) {
	int ipcid;

	if ( (ipcid = msgget(IPC_PRIVATE,IPC_CREAT|0600)) < 0 )
		return -1;			/* Failed: check errno */
	return ipcid;				/* Success: ipcid */
}

/*
 * Close (remove) a message queue:
 */
int
MsgClose(int ipcid) {
	return msgctl(ipcid,IPC_RMID,NULL);
}

/*
 * Send a Client/Server message:
 *	flags:	0		blocks on write
 *		IPC_NOWAIT	no blocking on write
 * Returns 0 if success, else -1
 */
int
MsgSend(int ipcid,SVRMSG *msg,int flags,long msgtype) {
	UInt16 hdrlen;					/* Length of the message header */
	UInt16 len;					/* Byte length of char mtext[] */
	int z;						/* Status return code */

	msg->type = msgtype;				/* 1=client, 2=server */

	hdrlen = ((char *) &msg->u - (char *)msg)	/* Get offset to the start of the union */
		- sizeof msg->type;			/* The message type does not get included */
	len = hdrlen + msg->bytes;			/* The final message length */

	while ( (z = msgsnd(ipcid,(struct msgbuf *)msg,len,flags)) < 0 && errno == EINTR )
		;				/* Repeat interrupted system calls */

	if ( cmdopt_x )
		fprintf(stderr,"%5d => Msg %s (%u bytes/%u) : %s\n",
			getpid(),
			msg_name(msg->msg_type),
			(unsigned)msg->bytes,
			(unsigned)len,
			z >= 0 ? "Sent" : "Not-sent");

	return z >= 0 ? 0 : -1;			/* Returns 0 if successful, else check errno */
}

/*
 * Receive a Client/Server Message:
 *	flags:	0		blocks on read
 *		IPC_NOWAIT	no blocking on read
 * Returns 0 if success, else -1
 */
int
MsgRecv(int ipcid,SVRMSG *msg,int flags,long msgtype) {
	int z;

	while ( (z = msgrcv(ipcid,(struct msgbuf *)msg,sizeof *msg-sizeof(long),msgtype,flags)) < 0 && errno == EINTR )
		; /* Repeat interrupted system calls */

	if ( cmdopt_x && (flags == 0 || z >= 0) )
		fprintf(stderr,"%5d <= Msg %s (%u bytes/%u) : %s\n",
			getpid(),
			msg_name(msg->msg_type),
			(unsigned)msg->bytes,
			(unsigned)z,
			z >= 0 ? "Recvd" : "Not-recvd");
	return z >= 0 ? 0 : -1;
}

/*
 * Return a string name for the enumerated message type:
 */
char *
msg_name(MSGTYP mtyp) {
	int x = (int) mtyp;			/* Message type as an (int) */
	static char *msg_names[] = {
		"ToClnt_Fatal",			/* Fatal server error */
		"ToClnt_Ready",			/* Tell client that server is ready */
		"ToSvr_Bye",			/* Client tells server to exit */
		"ToSvr_Path",			/* Client tells server a pathname */
		"ToClnt_Path",			/* Server acks pathname change */
		"ToClnt_Stat",			/* Server tells client stat info about pathname */
		"ToClnt_WavInfo",		/* Server responds with WAV header info */
		"ToSvr_Play",			/* Client tells server to play */
		"ToSvr_Pause",			/* Tell server to pause */
		"ToSvr_Stop",			/* Tell server to stop */
		"ToSvr_Bits",			/* Tell server new bits setting */
		"ToClnt_Bits",			/* Tell client current bits setting */
		"ToClnt_Settings",		/* Tell client current server settings */
		"ToSvr_SamplingRate",		/* Tell server new overriding sampling rate */
		"ToSvr_Restore",		/* Tell server to cancel overrides */
		"ToSvr_Chan",			/* Tell server new mono/stereo setting */
		"ToSvr_Record",			/* Tell server to start recording */
		"ToSvr_Debug",			/* Tell server debug mode setting */
		"ToClnt_ErrMsg",		/* Tell client an error message from server */
		"ToSvr_SemReset",		/* Tell server to reset its locking semaphores */
                "ToSvr_StartSample",            /* Tell server where to start playback from */
                "ToClnt_PlayState",             /* Tell client state of playback */
                "ToClnt_RecState"               /* Tell client state of recording */
	};
	static char buf[16];

	if ( x < 0 || x >= (int) MSGTYP_Last ) {
		sprintf(buf,"msgtyp=%d",x);
		return buf;			/* Wild message type */
	}

	return msg_names[x];			/* Proper message type */
}

/* $Source$ */
