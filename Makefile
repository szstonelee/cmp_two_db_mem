CC := gcc
CFLAGS := -Wall -g -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -O2

CXX := g++
CXXFLAGS := -Wall -Wextra -pedantic -std=c++17 -O2

.DEFAULT_GOAL := all


BASE_SOURCES := mcxt.c snprintf.c strerror.c elog.c \
	postgres_simulate.c stringinfo.c globals.c \
	proc_simulate.c miscinit_simulate.c \
	postmaster_simulate.c psprintf.c pqformat.c \
	pqcomm_simulate.c be-secure_simulate.c localtime.c \
	guc_simulate.c strlcpy.c pgfnames.c \
	pgtz.c path.c thread.c fd_simulate.c dynahash.c \
	shmem.c lwlock.c posix_sema.c hashfn.c ipc.c \
	pgstrcasecmp.c mbutils_simulatre.c wchar.c \
	encnames.c proc_simulate.c s_lock.c pgsleep.c \
	lwlocknames.c strftime.c ps_status.c aset.c \
	assert.c syslogger_simulate.c

BASE_OBJS := $(BASE_SOURCES:.c=.o)

%.o: %.c
	$(CC) $(CFLAGS) -c $<

single_pg.o: single_pg.c
	$(CC) $(CFLAGS) -c $<
single_pg: single_pg.o $(BASE_OBJS)
	$(CC) -o $@ $^

single_cpp_malloc: single_cpp_malloc.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

single_cpp_new: single_cpp_new.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

single_cpp_swap_new: single_cpp_swap_new.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

single_pg_swap.o: single_pg_swap.c
	$(CC) $(CFLAGS) -c $<
single_pg_swap: single_pg_swap.o $(BASE_OBJS)
	$(CC) -o $@ $^

single_cpp_swap_new_with_big: single_cpp_swap_new_with_big.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

single_pg_swap_with_big.o: single_pg_swap_with_big.c
	$(CC) $(CFLAGS) -c $<
single_pg_swap_with_big: single_pg_swap_with_big.o $(BASE_OBJS)
	$(CC) -o $@ $^

all: single_pg single_cpp_malloc single_cpp_new single_cpp_swap_new \
	single_pg_swap single_cpp_swap_new_with_big single_pg_swap_with_big

clean:
	rm -f *.o
	rm -f single_pg
	rm -f single_cpp_malloc
	rm -f single_cpp_new
	rm -f single_cpp_swap_new
	rm -f single_pg_swap
	rm -f single_cpp_swap_new_with_big
	rm -f single_pg_swap_with_big

.PHONY: all clean