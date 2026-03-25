#include "postgres.h"

#include <assert.h>
#include <signal.h>
#include <time.h>

#include "port.h"
#include "memutils.h"

#define TIME_SECS 10
volatile bool timeout = false;

static void timer_handle(int sig);
static long get_elapsed_ms_time(struct timespec *start, struct timespec *end);
static void swap_head_tail(char *p, const Size len);

static void 
timer_handle(int sig)
{
    if (sig == SIGALRM)
        timeout = true;
}

static void 
swap_head_tail(char *p, const Size len) {
    char head = p[0];
    char tail = p[len-1];
    p[0] = tail;
    p[len-1] = head;
}

static long
get_elapsed_ms_time(struct timespec *start, struct timespec *end) 
{
    long seconds = end->tv_sec - start->tv_sec;
    long nanoseconds = end->tv_nsec - start->tv_nsec;
    
    if (nanoseconds < 0) 
    {
        seconds--;
        nanoseconds += 1000000000;
    }
    
    return seconds*1000 + nanoseconds/1000;
}

int main()
{
    MemoryContextInit();
    signal(SIGALRM, timer_handle);

    const Size len = 1024;
    long long count = 0;

    struct timespec start, end;   
        
    alarm(TIME_SECS);
    clock_gettime(CLOCK_MONOTONIC, &start);
    while (!timeout)
    {
        void *p = palloc(len);
        assert(p != NULL);
        swap_head_tail(p, len);
        pfree(p);
        ++count;
    }
    MemoryContextDelete(TopMemoryContext);
    clock_gettime(CLOCK_MONOTONIC, &end);

    long total_ms = get_elapsed_ms_time(&start, &end);
    printf("count = %lld, total time(ms) = %ld, per ms = %lld\n", 
        count, total_ms, count/(long long)total_ms);

    // MemoryContextStats(TopMemoryContext);

    return 0;
}