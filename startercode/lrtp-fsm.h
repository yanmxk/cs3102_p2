#ifndef __lrtp_fsm_h__
#define __lrtp_fsm_h__

/*
  CS3102 Coursework P2 : Lightweight Reliable Transport Protocol (LRTP)

  saleem, Jan2024, Feb2023
  checked March 2025 (sjm55)
  revised March 2026 (sjm55)
*/

/* CS3102: you can use the states defined below, or define your own */

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

/* CS3102: add anything else here if needed */

#endif /* __lrtp_fsm_h__ */
