#ifndef __lrtp_fsm_h__
#define __lrtp_fsm_h__

#include <inttypes.h>

/*
  CS3102 Coursework P2 : Lightweight Reliable Transport Protocol (LRTP)

  saleem, Jan2024, Feb2023
  checked March 2025 (sjm55)
  revised March 2026 (sjm55)
*/

typedef enum LRTP_state_e {
  LRTP_state_error = -1, // -1 problem with state
  LRTP_state_closed,     //  0 connection closed
  LRTP_state_listening,  //  1 @server : listening for incoming connections
  LRTP_state_opening_i,  //  2 @client : sent request to start connection
  LRTP_state_opening_r,  //  3 @server : sent response to  connection
  LRTP_state_connected,  //  4 connected : connection established
  LRTP_state_closing_i,  //  5 closing initiated (@server or @client)
  LRTP_state_closing_r   //  6 closing responder (@server or @client)
} LRTP_state_t;

/* Adaptive RTO calculation based on RTT measurements (like TCP) */
uint32_t lrtp_calculate_adaptive_rto(uint32_t rtt, uint32_t *srtt, uint32_t *rttvar);

#endif /* __lrtp_fsm_h__ */
