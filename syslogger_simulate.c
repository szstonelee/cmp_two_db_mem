#include "postgres.h"

#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>


static FILE *syslogFile = NULL;
static FILE *csvlogFile = NULL;


/* --------------------------------
 *		logfile routines
 * --------------------------------
 */

/*
 * Write text to the currently open logfile
 *
 * This is exported so that elog.c can call it when MyBackendType is B_LOGGER.
 * This allows the syslogger process to record elog messages of its own,
 * even though its stderr does not point at the syslog pipe.
 */
void
write_syslogger_file(const char *buffer, int count, int destination)
{
	int			rc;
	FILE	   *logfile;

	/*
	 * If we're told to write to csvlogFile, but it's not open, dump the data
	 * to syslogFile (which is always open) instead.  This can happen if CSV
	 * output is enabled after postmaster start and we've been unable to open
	 * csvlogFile.  There are also race conditions during a parameter change
	 * whereby backends might send us CSV output before we open csvlogFile or
	 * after we close it.  Writing CSV-formatted output to the regular log
	 * file isn't great, but it beats dropping log output on the floor.
	 *
	 * Think not to improve this by trying to open csvlogFile on-the-fly.  Any
	 * failure in that would lead to recursion.
	 */
	logfile = (destination == LOG_DESTINATION_CSVLOG &&
			   csvlogFile != NULL) ? csvlogFile : syslogFile;

	if (logfile != NULL)
	{
		rc = fwrite(buffer, 1, count, logfile);

		/*
		* Try to report any failure.  We mustn't use ereport because it would
		* just recurse right back here, but write_stderr is OK: it will write
		* either to the postmaster's original stderr, or to /dev/null, but never
		* to our input pipe which would result in a different sort of looping.
		*/
		if (rc != count)
			write_stderr("could not write to log file: %s\n", strerror(errno));
	}
}
