/* $Header$
 * Warren W. Gay VE3WWG		Wed Feb 26 22:43:33 1997
 *
 * INTERPRET PROCESS TERMINATION:
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
 * Revision 1.1.1.1  1999/11/21 19:50:56  wwg
 * Import wavplay-1.3 into CVS
 *
 * Revision 1.1  1997/04/14 00:07:16  wwg
 * Initial revision
 *
 */
static const char rcsid[] = "@(#)procterm.c $Revision$";

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
 * Return an interpreted text description of a process termination event.
 */
char *
ProcTerm(int ProcStat) {
	int ExitCode, Signal;
	static char buf[80];

	if ( ProcStat == 0 || WIFEXITED(ProcStat) ) {
		if ( !ProcStat )
			ExitCode = 0;					/* exit(0) */
		else	ExitCode = WEXITSTATUS(ProcStat);		/* exit(code) */
		sprintf(buf,"exit(%d)",ExitCode);

	} else if ( WIFSIGNALED(ProcStat) ) {
		Signal = WTERMSIG(ProcStat);				/* Signalled or aborted */
		sprintf(buf,"signal(%d)%s",Signal,WCOREDUMP(ProcStat)
				? ", core file written"
				: "");

	} else if ( WIFSTOPPED(ProcStat) ) {
		Signal = WSTOPSIG(ProcStat);				/* Stopped process */
		sprintf(buf,"stopsig(%d)",Signal);

	} else	sprintf(buf,"Weird termination 0x%04X",ProcStat);	/* Something weird */

	return buf;
}

/* $Source$ */
