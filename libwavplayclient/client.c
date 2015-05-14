/* $Header$
 * Warren W. Gay VE3WWG		Tue Feb 25 22:43:40 1997
 *
 * CLIENT RELATED FUNCTIONS:
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
 * 2003/09/07 (hof) changed sys_errlist[errno] to g_strerror(errno)
 *
 * Revision 1.2  1999/12/04 00:01:20  wwg
 * Implement wavplay-1.4 release changes
 *
 * Revision 1.1.1.1  1999/11/21 19:50:56  wwg
 * Import wavplay-1.3 into CVS
 *
 * Revision 1.1  1997/04/14 00:11:03  wwg
 * Initial revision
 */
static const char rcsid[] = "@(#)client.c $Revision$";

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <malloc.h>
#include <string.h>
#include <memory.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <assert.h>
#ifdef __CYGWIN__
#include <sys/soundcard.h>
#else
#include <linux/soundcard.h>
#endif

#include <glib.h>

#include "wavplay.h"
#include "client.h"

pid_t svrPID = (pid_t)-1;					/* Forked process ID of server */
int svrIPC = -1;						/* IPC ID of message queue */
static ErrFunc v_erf;						/* Error reporting function */

/*
 * Error reporting function for this source module:
 */
static void
err(const char *format,...) {
	va_list ap;

	if ( v_erf == NULL )
		return;				/* Only report error if we have function */
	va_start(ap,format);
	v_erf(format,ap);			/* Use caller's supplied function */
	va_end(ap);
}

/*
 * This function is called at exit()
 */
static void
close_msgq(void) {
	int termstat;						/* Process termination status */
	pid_t PID;						/* PID from waitpid() */
	time_t t0, t1;						/* Times for timeout */

	if ( svrIPC >= 0 )
		tosvr_bye(0,NULL);				/* Tell server to exit politely */
	else if ( svrPID != (pid_t) -1L )
		kill(svrPID,SIGTERM);				/* It has no msg queue anyhow */

	/*
	 * Leave some time for the server to exit:
	 */
	if ( svrPID != (pid_t) -1L ) {				/* Did we start a fork? */
		time(&t0);					/* Start timer */

		do	{					/* Loop while server runs */
			if ( (PID = waitpid(-1,&termstat,WNOHANG)) == svrPID ) {
				svrPID = (pid_t) -1L;		/* Note the termination */
				break;				/* Server ended */
			}
			time(&t1);
			sleep(1);				/* Give up CPU */
		} while ( PID < 1 && t1 - t0 < 4 );		/* Eventually timeout */
	}

	if ( svrIPC >= 0 )
		MsgClose(svrIPC);				/* Remove message queue */

	if ( svrPID != (pid_t) -1L )
		kill(svrPID,SIGTERM);				/* Stab it again */
}

/*
 * Fork the server, and get it started:
 */
int
tosvr_start(ErrFunc erf) {
	int e;					/* Saved errno value */
	char buf[200];				/* Argv[0] for exec'd process */
	SVRMSG msg;				/* Server message from wavplay */
	pid_t PID;				/* Process ID of terminated process */
	int termstat;				/* Termination status of the process */
	time_t t0, t;				/* Start time, current time */

	v_erf = erf;				/* Error reporting function */

	/*
	 * Create a private message queue for the client<-->server
	 * communications.
	 */
	if ( (svrIPC = MsgCreate()) < 0 ) {
		err("%s: creating message queue",g_strerror(errno));
		return -1;
	}

	sprintf(buf,"WAVSVR=%d",svrIPC);	/* Pass this as argv[0] */

	/*
	 * Create a fork, that will then exec wavplay as a server:
	 */
	if ( (svrPID = fork()) == (pid_t)0L ) {
		/*
		 * Child process must now start the wavplay command as server:
		 */
		if ( cmdopt_x )
			execl(env_WAVPLAYPATH,buf,"-x",NULL);	/* Debug On */
		else	execl(env_WAVPLAYPATH,buf,NULL);	/* No debug yet */

		/* Returns only if error occurs */
		fprintf(stderr,"%s: exec of %s\n",g_strerror(errno),env_WAVPLAYPATH);
		exit(2);

	} else if ( svrPID < 0 ) {
		e = errno;
		err("%s: forking the server process",g_strerror(errno));
		MsgClose(svrIPC);
		errno = e;			/* Restore error code */
		return -1;
	}

	atexit(close_msgq);			/* Invoke close_msgq() at exit() time */

	time(&t0);				/* Start clock */

	while ( 1 ) {
		if ( !MsgFromServer(svrIPC,&msg,IPC_NOWAIT) ) {
			/*
			 * We have a message from the server:
			 */
			if ( msg.msg_type == ToClnt_Ready )
				return 0;	/* We connected to server OK! */
			fprintf(stderr,"Bad server message: %d\n",(int)msg.msg_type);

		} else	{
			while ( (PID = waitpid(-1,&termstat,WNOHANG)) == -1 && errno == EINTR )
				;	/* Repeat interrupted system call */
			if ( PID > 0 ) {
				/*
				 * We have terminated process!
				 */
				err("Server process ID %ld terminated: %s",PID,ProcTerm(termstat));
				return -1;

			} else	sleep(1);
		}

		time(&t);
		if ( t - t0 > 5 )
			break;
	}

	err("Timeout: starting server.");
	kill(svrPID,SIGTERM);
	return -1;
}

/*
 * Send simple command to server:
 */
int
tosvr_cmd(MSGTYP cmd,int flags,ErrFunc erf) {
	SVRMSG msg;
	int rc;

	v_erf = erf;				/* Error reporting function */

	msg.msg_type = cmd;
	msg.bytes = 0;				/* Simple commands have no other data */

	if ( (rc = MsgSend(svrIPC,&msg,flags,MSGNO_SRVR)) != 0 && erf != NULL )
		err("%s: Sending server message '%s'",
			msg_name(cmd),
			g_strerror(errno));
	return rc;				/* Zero indicates success */
}

/*
 * Send a pathname to the server:
 */
int
tosvr_path(const char *pathname,int flags,ErrFunc erf) {
	SVRMSG msg;
	int z;

	v_erf = erf;				/* Error reporting function */

	msg.msg_type = ToSvr_Path;
	strncpy(msg.u.tosvr_path.path,pathname,sizeof msg.u.tosvr_path)
		[sizeof msg.u.tosvr_path.path - 1] = 0;
	msg.bytes = strlen(msg.u.tosvr_path.path);

	if ( (z = MsgToServer(svrIPC,&msg,flags)) != 0 && erf != NULL )
		err("%s: Sending server message 'path'",g_strerror(errno));
	return z;
}

/*
 * Tell server about new data bits per sample setting:
 */
int
tosvr_bits(int flags,ErrFunc erf,int bits) {
	SVRMSG msg;
	int z;

	v_erf = erf;				/* Error reporting function */

	msg.msg_type = ToSvr_Bits;
	msg.u.tosvr_bits.DataBits = bits;
	msg.bytes = sizeof msg.u.tosvr_bits;

	if ( (z = MsgToServer(svrIPC,&msg,flags)) != 0 && erf != NULL )
		err("%s: Sending server message 'bits'",g_strerror(errno));

	return z;
}

/*
 * Tell server to start at a new sample number:
 */
int
tosvr_start_sample(int flags, ErrFunc erf, UInt32 sample) {
       SVRMSG msg;
       int z;

       v_erf = erf;                            /* Error reporting function */

       msg.msg_type = ToSvr_StartSample;
       msg.u.tosvr_start_sample.StartSample = sample;
       msg.bytes = sizeof msg.u.tosvr_start_sample;

       if ( (z = MsgToServer(svrIPC,&msg,flags)) != 0 && erf != NULL )
               err("%s: Sending server message 'start_sample'",g_strerror(errno));

       return z;
}
/*
 * Tell server to use new sampling rate:
 */
int
tosvr_sampling_rate(int flags,ErrFunc erf,UInt32 sampling_rate) {
	SVRMSG msg;
	int z;

	v_erf = erf;				/* Error reporting function */

	msg.msg_type = ToSvr_SamplingRate;
	msg.u.tosvr_sampling_rate.SamplingRate = sampling_rate;
	msg.bytes = sizeof msg.u.tosvr_sampling_rate;

	if ( (z = MsgToServer(svrIPC,&msg,flags)) != 0 && erf != NULL )
		err("%s: Sending server message 'sampling_rate'",g_strerror(errno));

	return z;
}

/*
 * Tell server about Mono/Stereo preference:
 */
int
tosvr_chan(int flags,ErrFunc erf,Chan chan) {
	SVRMSG msg;
	int z;

	v_erf = erf;				/* Error reporting function */

	msg.msg_type = ToSvr_Chan;
	msg.u.tosvr_chan.Channels = chan;
	msg.bytes = sizeof msg.u.tosvr_chan;

	if ( (z = MsgToServer(svrIPC,&msg,flags)) != 0 && erf != NULL )
		err("%s: Sending server message 'tosvr_chan'",g_strerror(errno));

	return z;
}

/*
 * Tell server to start recording:
 */
int
tosvr_record(int flags,ErrFunc erf,Chan Channels,UInt32 SamplingRate,UInt16 DataBits) {
	SVRMSG msg;
	int z;

	v_erf = erf;				/* Error reporting function */

	msg.msg_type = ToSvr_Record;
	msg.u.tosvr_record.Channels = Channels;
	msg.u.tosvr_record.SamplingRate = SamplingRate;
	msg.u.tosvr_record.DataBits = DataBits;
	msg.bytes = sizeof msg.u.tosvr_record;

	if ( (z = MsgToServer(svrIPC,&msg,flags)) != 0 && erf != NULL )
		err("%s: Sending server message 'tosvr_record'",g_strerror(errno));

	return z;
}

/*
 * Tell server about our current debug level setting:
 */
int
tosvr_debug(int flags,ErrFunc erf,int bDebugMode) {
	SVRMSG msg;
	int z;

	v_erf = erf;				/* Error reporting function */

	msg.msg_type = ToSvr_Debug;
	msg.u.tosvr_debug.bDebugMode = bDebugMode;
	msg.bytes = sizeof msg.u.tosvr_debug;

	if ( (z = MsgToServer(svrIPC,&msg,flags)) != 0 && erf != NULL )
		err("%s: Sending server message 'tosvr_debug'",g_strerror(errno));

	return z;
}

/* $Source$ */
