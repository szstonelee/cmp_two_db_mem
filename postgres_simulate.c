#include "postgres.h"

#include "dest.h"

CommandDest whereToSendOutput = DestDebug;

bool		ClientAuthInProgress = false;	/* T during new-client
											 * authentication */

const char *debug_query_string = NULL; /* client-supplied query string */

bool		redirection_done = false;	/* stderr redirected for syslogger? */
