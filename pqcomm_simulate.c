#include "postgres.h"

#ifdef HAVE_POLL_H
#include <poll.h>
#endif
#include <signal.h>
#include <fcntl.h>
#include <grp.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <netdb.h>
#include <netinet/in.h>
#ifdef HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif
#include <utime.h>
#ifdef _MSC_VER					/* mstcpip.h is missing on mingw */
#include <mstcpip.h>
#endif

#include "libpq.h"
#include "pg_bswap.h"
#include "miscadmin.h"

static bool PqCommBusy;			/* busy sending data to the client */

static char *PqSendBuffer;
static int	PqSendBufferSize;	/* Size send buffer */
static int	PqSendPointer;		/* Next index to store a byte in PqSendBuffer */
static int	PqSendStart;		/* Next index to send a byte in PqSendBuffer */

/* Internal functions */
static void socket_comm_reset(void);
//static void socket_close(int code, Datum arg);
static void socket_set_nonblocking(bool nonblocking);
static int	socket_flush(void);
static int	socket_flush_if_writable(void);
static bool socket_is_send_pending(void);
static int	socket_putmessage(char msgtype, const char *s, size_t len);
static void socket_putmessage_noblock(char msgtype, const char *s, size_t len);
static int	internal_putbytes(const char *s, size_t len);
static int	internal_flush(void);

static const PQcommMethods PqCommSocketMethods = {
	socket_comm_reset,
	socket_flush,
	socket_flush_if_writable,
	socket_is_send_pending,
	socket_putmessage,
	socket_putmessage_noblock
};
const PQcommMethods *PqCommMethods = &PqCommSocketMethods;



/* --------------------------------
 *		pq_putmessage_noblock	- like pq_putmessage, but never blocks
 *
 *		If the output buffer is too small to hold the message, the buffer
 *		is enlarged.
 */
static void
socket_putmessage_noblock(char msgtype, const char *s, size_t len)
{
	int			res PG_USED_FOR_ASSERTS_ONLY;
	int			required;

	/*
	 * Ensure we have enough space in the output buffer for the message header
	 * as well as the message itself.
	 */
	required = PqSendPointer + 1 + 4 + len;
	if (required > PqSendBufferSize)
	{
		PqSendBuffer = repalloc(PqSendBuffer, required);
		PqSendBufferSize = required;
	}
	res = pq_putmessage(msgtype, s, len);
	Assert(res == 0);			/* should not fail when the message fits in
								 * buffer */
}

/* --------------------------------
 *		socket_comm_reset - reset libpq during error recovery
 *
 * This is called from error recovery at the outer idle loop.  It's
 * just to get us out of trouble if we somehow manage to elog() from
 * inside a pqcomm.c routine (which ideally will never happen, but...)
 * --------------------------------
 */
static void
socket_comm_reset(void)
{
	/* Do not throw away pending data, but do reset the busy flag */
	PqCommBusy = false;
}

/* --------------------------------
 *		socket_flush		- flush pending output
 *
 *		returns 0 if OK, EOF if trouble
 * --------------------------------
 */
static int
socket_flush(void)
{
	int			res;

	/* No-op if reentrant call */
	if (PqCommBusy)
		return 0;
	PqCommBusy = true;
	socket_set_nonblocking(false);
	res = internal_flush();
	PqCommBusy = false;
	return res;
}

/* --------------------------------
 *		pq_flush_if_writable - flush pending output if writable without blocking
 *
 * Returns 0 if OK, or EOF if trouble.
 * --------------------------------
 */
static int
socket_flush_if_writable(void)
{
	int			res;

	/* Quick exit if nothing to do */
	if (PqSendPointer == PqSendStart)
		return 0;

	/* No-op if reentrant call */
	if (PqCommBusy)
		return 0;

	/* Temporarily put the socket into non-blocking mode */
	socket_set_nonblocking(true);

	PqCommBusy = true;
	res = internal_flush();
	PqCommBusy = false;
	return res;
}

/* --------------------------------
 *	socket_is_send_pending	- is there any pending data in the output buffer?
 * --------------------------------
 */
static bool
socket_is_send_pending(void)
{
	return (PqSendStart < PqSendPointer);
}

/* --------------------------------
 *		socket_putmessage - send a normal message (suppressed in COPY OUT mode)
 *
 *		msgtype is a message type code to place before the message body.
 *
 *		len is the length of the message body data at *s.  A message length
 *		word (equal to len+4 because it counts itself too) is inserted by this
 *		routine.
 *
 *		We suppress messages generated while pqcomm.c is busy.  This
 *		avoids any possibility of messages being inserted within other
 *		messages.  The only known trouble case arises if SIGQUIT occurs
 *		during a pqcomm.c routine --- quickdie() will try to send a warning
 *		message, and the most reasonable approach seems to be to drop it.
 *
 *		returns 0 if OK, EOF if trouble
 * --------------------------------
 */
static int
socket_putmessage(char msgtype, const char *s, size_t len)
{
	uint32		n32;

	Assert(msgtype != 0);

	if (PqCommBusy)
		return 0;
	PqCommBusy = true;
	if (internal_putbytes(&msgtype, 1))
		goto fail;

	n32 = pg_hton32((uint32) (len + 4));
	if (internal_putbytes((char *) &n32, 4))
		goto fail;

	if (internal_putbytes(s, len))
		goto fail;
	PqCommBusy = false;
	return 0;

fail:
	PqCommBusy = false;
	return EOF;
}

/* --------------------------------
 *			  socket_set_nonblocking - set socket blocking/non-blocking
 *
 * Sets the socket non-blocking if nonblocking is true, or sets it
 * blocking otherwise.
 * --------------------------------
 */
static void
socket_set_nonblocking(bool nonblocking)
{
	if (MyProcPort == NULL)
		ereport(ERROR,
				(errcode(ERRCODE_CONNECTION_DOES_NOT_EXIST),
				 errmsg("there is no client connection")));

	MyProcPort->noblock = nonblocking;
}

static int
internal_putbytes(const char *s, size_t len)
{
	size_t		amount;

	while (len > 0)
	{
		/* If buffer is full, then flush it out */
		if (PqSendPointer >= PqSendBufferSize)
		{
			socket_set_nonblocking(false);
			if (internal_flush())
				return EOF;
		}
		amount = PqSendBufferSize - PqSendPointer;
		if (amount > len)
			amount = len;
		memcpy(PqSendBuffer + PqSendPointer, s, amount);
		PqSendPointer += amount;
		s += amount;
		len -= amount;
	}
	return 0;
}

/* --------------------------------
 *		internal_flush - flush pending output
 *
 * Returns 0 if OK (meaning everything was sent, or operation would block
 * and the socket is in non-blocking mode), or EOF if trouble.
 * --------------------------------
 */
static int
internal_flush(void)
{
	static int	last_reported_send_errno = 0;

	char	   *bufptr = PqSendBuffer + PqSendStart;
	char	   *bufend = PqSendBuffer + PqSendPointer;

	while (bufptr < bufend)
	{
		int			r;

		r = secure_write(MyProcPort, bufptr, bufend - bufptr);

		if (r <= 0)
		{
			if (errno == EINTR)
				continue;		/* Ok if we were interrupted */

			/*
			 * Ok if no data writable without blocking, and the socket is in
			 * non-blocking mode.
			 */
			if (errno == EAGAIN ||
				errno == EWOULDBLOCK)
			{
				return 0;
			}

			/*
			 * Careful: an ereport() that tries to write to the client would
			 * cause recursion to here, leading to stack overflow and core
			 * dump!  This message must go *only* to the postmaster log.
			 *
			 * If a client disconnects while we're in the midst of output, we
			 * might write quite a bit of data before we get to a safe query
			 * abort point.  So, suppress duplicate log messages.
			 */
			if (errno != last_reported_send_errno)
			{
				last_reported_send_errno = errno;
				ereport(COMMERROR,
						(errcode_for_socket_access(),
						 errmsg("could not send data to client: %m")));
			}

			/*
			 * We drop the buffered data anyway so that processing can
			 * continue, even though we'll probably quit soon. We also set a
			 * flag that'll cause the next CHECK_FOR_INTERRUPTS to terminate
			 * the connection.
			 */
			PqSendStart = PqSendPointer = 0;
			ClientConnectionLost = 1;
			InterruptPending = 1;
			return EOF;
		}

		last_reported_send_errno = 0;	/* reset after any successful send */
		bufptr += r;
		PqSendStart += r;
	}

	PqSendStart = PqSendPointer = 0;
	return 0;
}

/* --------------------------------
 *		socket_close - shutdown libpq at backend exit
 *
 * This is the one pg_on_exit_callback in place during BackendInitialize().
 * That function's unusual signal handling constrains that this callback be
 * safe to run at any instant.
 * --------------------------------
 */
#if 0
static void
socket_close(int code, Datum arg)
{
	/* Nothing to do in a standalone backend, where MyProcPort is NULL. */
	if (MyProcPort != NULL)
	{
#ifdef ENABLE_GSS
		/*
		 * Shutdown GSSAPI layer.  This section does nothing when interrupting
		 * BackendInitialize(), because pg_GSS_recvauth() makes first use of
		 * "ctx" and "cred".
		 *
		 * Note that we don't bother to free MyProcPort->gss, since we're
		 * about to exit anyway.
		 */
		if (MyProcPort->gss)
		{
			OM_uint32	min_s;

			if (MyProcPort->gss->ctx != GSS_C_NO_CONTEXT)
				gss_delete_sec_context(&min_s, &MyProcPort->gss->ctx, NULL);

			if (MyProcPort->gss->cred != GSS_C_NO_CREDENTIAL)
				gss_release_cred(&min_s, &MyProcPort->gss->cred);
		}
#endif							/* ENABLE_GSS */

		/*
		 * Cleanly shut down SSL layer.  Nowhere else does a postmaster child
		 * call this, so this is safe when interrupting BackendInitialize().
		 */
		secure_close(MyProcPort);

		/*
		 * Formerly we did an explicit close() here, but it seems better to
		 * leave the socket open until the process dies.  This allows clients
		 * to perform a "synchronous close" if they care --- wait till the
		 * transport layer reports connection closure, and you can be sure the
		 * backend has exited.
		 *
		 * We do set sock to PGINVALID_SOCKET to prevent any further I/O,
		 * though.
		 */
		MyProcPort->sock = PGINVALID_SOCKET;
	}
}
#endif

/* --------------------------------
 *		pq_putmessage_v2 - send a message in protocol version 2
 *
 *		msgtype is a message type code to place before the message body.
 *
 *		We no longer support protocol version 2, but we have kept this
 *		function so that if a client tries to connect with protocol version 2,
 *		as a courtesy we can still send the "unsupported protocol version"
 *		error to the client in the old format.
 *
 *		Like in pq_putmessage(), we suppress messages generated while
 *		pqcomm.c is busy.
 *
 *		returns 0 if OK, EOF if trouble
 * --------------------------------
 */
int
pq_putmessage_v2(char msgtype, const char *s, size_t len)
{
	Assert(msgtype != 0);

	if (PqCommBusy)
		return 0;
	PqCommBusy = true;
	if (internal_putbytes(&msgtype, 1))
		goto fail;

	if (internal_putbytes(s, len))
		goto fail;
	PqCommBusy = false;
	return 0;

fail:
	PqCommBusy = false;
	return EOF;
}
