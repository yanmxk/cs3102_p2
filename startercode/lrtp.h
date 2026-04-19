#ifndef __lrtp_h__
#define __lrtp_h__

/*
  THIS FILE MUST NOT BE MODIFIED.

  CS3102 Coursework P2 : Lightweight Reliable Transport Protocol (LRTP).
  saleem, Jan2024, Feb2023.
  checked March 2025 (sjm55)
  revised March 2026 (sjm55)

  API for LRTP.

  This is a blocking / synchronous API.

  Please read the descriptions of the various API calls below, to understand the
  behaviour of each call.
*/

#include <inttypes.h>

/*
  Control for retransmissions within the LRTP protocol.

  LRTP_RTO_FIXED:
    re-transmission timeout (RTO), in in microseconds [us].

  LRTP_MAX_RETX:
    the maximum number of re-transmissions to try before assuming that
    transmission is no longer possible, e.g. there is an error, such as a
    network problem.

 */
#define LRTP_RTO_FIXED ((uint32_t) 500000) /* 0.5s in microseconds [us], arbitrary choice */ 
#define LRTP_MAX_RE_TX 3 /* maximum number of retransmissions, arbitrary choice */
/* */


/*
  lrtp_init()
  Must be called at the start of every program, server or client, to set-up
  and initialise the communication service offered by the API.
*/
void lrtp_init();
/* */


/*
  lrtp_start() is for the server : setting up listen for incoming requests
  only needs to be used once in a program to configure an endpoint on which
  to listen for incoming connection requests.

  Returns:
  : a socket descriptor, sd > 0, a C socket(2) descriptor.
  : < 0 on error (see lrtp-common.h for error values).
*/
int lrtp_start(uint16_t port);
/* */


/*
  lrtp_accept() is for the server : using the socket descriptor returned
  from strp_start(), this is used to accept incoming connections.
  
  This call should block until either:
  - a connection has been established; or
  - there is an error, such as a network problem.

  Arguments:
  : sd is the socket descriptor returned by lrtp_start().

  Returns:
  : sd to indicate that the connection has been processed, and so a
    connection now exists (only 1 connection at a time is required).
  : < 0 on error (see lrtp-common.h for error values).
*/
int lrtp_accept(int sd);
/* */


/*
  lrtp_open() is for a client to connect to a remote server. The port value is
  used for both the local and remote port number.

  This call should block until either:
  - a connection has been established; or
  - there is an error, such as a network problem.

  Arguments:
  : fqdn is the fully qualified domain name of the remote server (a "dot
    notation" IPv4 address string can also be used).
  : port is the port number to use - you should use getuid()


  Returns:
  : a socket descriptor, sd > 0, a C socket(2) descriptor.
  : < 0 on error (see lrtp-common.h for error values).
*/
int lrtp_open(const char *fqdn, uint16_t port);
/* */


/*
  lrtp_tx(), lrtp_rx(), and lrtp_close() are synchronous / blocking calls, and
  will return either when the operation is complete, or there is an error.
  They can be used by server or client, once a connection has been established.
*/

/*
  lrtp_tx() sends data_size bytes from the buffer data pointed to by data.

  This call should block until either:
  - the data has been transmitted successfully; or
  - there is an error, such as a network problem.

  Arguments:
  : sd is the socket descriptor returned by lrtp_accept() or lrtp_open().
  : data is a pointer to memory of at least the the number of bytes given
    by the value of data_size, and is the data that will be transmitted
    (the payload for your protocol).
  : data_size is the number of bytes to be transmitted.

  Returns:
  : number of bytes transmitted (== data_size for full success).
  : < 0 on error (see lrtp-common.h for error values).
*/
int lrtp_tx(int sd, void *data, uint16_t data_size);
/* */


/*
  lrtp_rx() receives maximum of data_size bytes into the buffer data pointed to by
  data.

  This call should block until either:
  - the data has been transmitted successfully; or
  - there is an error, such as a network problem.

  Arguments:
  : sd is the socket descriptor returned by lrtp_accept() or lrtp_open().
  : data is a pointer to memory of at least the the number of bytes given
    by the value of data_size, and is where the received data that will be
    copied to (the payload for your protocol).
  : data_size is the maximum number of bytes to be received.

  Returns:
  : number of bytes received (<= data_size).
  : < 0 on error (see lrtp-common.h for error values).
*/
int lrtp_rx(int sd, void *data, uint16_t data_size);
/* */


/*
  lrtp_close() terminates the connection, if required, and closes the underlying
  C socket. lrtp_close() can be called any time after lrtp_start() (@server) or
  lrtp_open() (@client) have been called successfully.

  This call should block until either:
  - the connection termination has been completed successfully; or
  - there is an error, such as a network problem.

  Arguments:
  : sd is the socket descriptor returned by lrtp_accept(), lrtp_accept() or
    lrtp_open().

  Returns:
  : LRTP_SUCCESS if this was completed correctly (see lrtp-common.h)..
  : < 0 on error (see lrtp-common.h for error values)
*/
int lrtp_close(int sd);
/* */

#endif /* __lrtp_h__ */
