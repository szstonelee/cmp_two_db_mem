#include "postgres.h"

#include <sys/param.h>
#include <signal.h>
#include <time.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <unistd.h>
#include <grp.h>
#include <pwd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <utime.h>

#include "miscadmin.h"

BackendType MyBackendType;
