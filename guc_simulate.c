#include "postgres.h"

#include <ctype.h>
#include <float.h>
#include <math.h>
#include <limits.h>
#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#ifndef WIN32
#include <sys/mman.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_SYSLOG
#include <syslog.h>
#endif
#include <unistd.h>

char	   *application_name;
char	   *cluster_name = "";
int			log_min_error_statement = ERROR;
int			log_min_messages = WARNING;
int			client_min_messages = NOTICE;
char	   *backtrace_symbol_list;
char	   *backtrace_functions;
int			trace_recovery_messages = LOG;