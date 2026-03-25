#include "postgres.h"

#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

#include "proc.h"
#include "ilist.h"
#include "latch.h"
#include "lock.h"


/* Pointers to shared-memory structures */
PROC_HDR   *ProcGlobal = NULL;

/* Pointer to this process's PGPROC struct, if any */
PGPROC	   *MyProc = NULL;