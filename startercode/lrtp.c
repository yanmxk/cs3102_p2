/*
  CS3201 Coursework P2 : Lightweight Reliable Transport Protocol (LRTP).
  saleem, Jan2024, Feb2023.
  checked March 2025 (sjm55)
  revised March 2026 (sjm55)

  API for LRTP.
*/

#include <stdio.h> /* this should have 'void perror(const char *s);' */
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "lrtp.h"
#include "lrtp-packet.h"
#include "lrtp-common.h"
#include "lrtp-fsm.h"
#include "lrtp-pcb.h"

#include "byteorder64.h"
#include "d_print.h"

extern Lrtp_Pcb_t G_pcb; /* in lrtp-pcb.c */

/* CS3102: Add anything you need below here - modify this file as you need to */


/*
  Must be called before any other lrtp_zzz() API calls.

  For use by client and server process.
*/
void
lrtp_init()
{
  /*
    CS3012 : modify as required
  */

  reset_LrtpPcb();
}



/*
  port : local port number to be used for socket
  return : error - lrtp-common.h
           success - valid socket descriptor
  
  For use by server process.
*/
int
lrtp_start(uint16_t port)
{
  /*
    CS3012 : modify as required
  */

  return LRTP_SUCCESS;
}


/*
  sd : socket descriptor as previously provided by lrtp_start()
  return : error - lrtp-common.h
           success - sd, to indicate sd now also is "connected"
  
  For use by server process.
*/
int
lrtp_accept(int sd)
{
  /*
    CS3012 : modify as required
  */

  return LRTP_SUCCESS;
}


/*
  port : local and remote port number to be used for socket
  return : error - LRTP_ERROR
           success - valid socket descriptor
  
  For use by client process.
*/
int
lrtp_open(const char *fqdn, uint16_t port)
{
  /*
    CS3012 : modify as required
  */

  return LRTP_SUCCESS;
}


/*
  port : local and remote port number to be used for socket
  return : error - LRTP_ERROR
           success - LRTP_SUCCESS
  
  For use by client process.
*/
int
lrtp_close(int sd)
{
  /*
    CS3012 : modify as required
  */

  return LRTP_SUCCESS;
}


/*
  sd : socket descriptor
  data : buffer with bytestream to transmit
  data_size : number of bytes to transmit
  return : error - LRTP_ERROR
           success - number of bytes transmitted
*/
int
lrtp_tx(int sd, void *data, uint16_t data_size)
{
  /*
    CS3012 : modify as required
  */

  return data_size;
}


/*
  sd : socket descriptor
  data : buffer to store bytestream received
  data_size : size of buffer
  return : error - LRTP_ERROR
           success - number of bytes received
*/
int
lrtp_rx(int sd, void *data, uint16_t data_size)
{
  /*
    CS3012 : modify as required
  */

  return data_size;
}
