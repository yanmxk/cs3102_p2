#ifndef __lrtp_common_h__
#define __lrtp_common_h__

/*
  CS3102 Coursework P2 : Lightweight Reliable Transport Protocol (LRTP)

  saleem, Jan2024, Feb2023
  checked March 2025 (sjm55)
  revised March 2026 (sjm55)
*/

#include <inttypes.h>

/* 1280 to fit comfortably within 1500 MTU. This avoids
   fragmentation/loss issues with UDP, even with a LRTP
   header added. */
#define LRTP_MAX_DATA_SIZE 1280

/*
  Return values from API.
*/
#define LRTP_SUCCESS 0

/* multiple error values (below) can be used as bit-mask, if needed */

/* LRTP_ERROR: general problem, e.g. a system call failure */
#define LRTP_ERROR (-1)

/* LRTP_ERROR_api: problem with API calls, e.g. API calls in the wrong order */
#define LRTP_ERROR_api (-2) 

/* LRTP_ERROR_fsm: problem with FSM state change, e.g. wrong packet incoming, 
   or API call in wrong order */
#define LRTP_ERROR_fsm (-4) 

/* LRTP_ERROR_data: @sender tx failed, @receiver ack failed. */
#define LRTP_ERROR_data (-8)

/* LRTP_ERROR_closed: connection is closed */
#define LRTP_ERROR_closed (-16)

/* LRTP_ERROR_protocol: problem with protocol exchange, e.g. unexpected packet type */
#define LRTP_ERROR_protocol (-32) 

uint64_t lrtp_timestamp(); // in microseconds
#define LRTP_TIMESTAMP_SECONDS(t_)      ( (uint64_t) t_ / 1000000)
#define LRTP_TIMESTAMP_MICROSECONDS(t_) ( (uint64_t) t_ % 1000000)
#define LRTP_TIMESTAMP_MILLISECONDS(t_) (((uint64_t) t_ % 1000000) / 1000)

/*
  A human-friendly version of t is provided in t_str, using localtime(3).
  
  Arguments:
  : t - a timestamp value as returned by lrtp_timestamp().
  : t_str - a string buffer of at least 64 bytes.

  Returns:
  : number of characters in the string (excluding the terminating '\0').
    t_str is updated, upon success, with a string, such as
      "2023-03-05_16:35:36  (1678034136.992466s)"
  : < 0 on error
  
*/
int lrtp_time_str(uint64_t t, char *t_str);

#endif /* __lrtp_common_h__ */
