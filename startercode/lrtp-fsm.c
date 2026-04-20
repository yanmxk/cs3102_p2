
/*
  CS3102 Coursework P2 : Lightweight Reliable Transport Protocol (LRTP)

  saleem, Jan2024, Feb2023
  checked March 2025 (sjm55)
  checked March 2026 (sjm55)
*/

#include <stdio.h>

#include "lrtp-fsm.h"

/*
  Adaptive RTO calculation based on RTT measurements (similar to TCP).
  
  Uses the standard formulas:
  - RTTVAR = 3/4 * RTTVAR + 1/4 * |SRTT - RTT|
  - SRTT = 7/8 * SRTT + 1/8 * RTT
  - RTO = SRTT + 4 * RTTVAR
  
  For the first RTT sample (srtt == 0), initialize SRTT and RTTVAR differently.
  
  Arguments:
  - rtt: Current RTT sample in microseconds
  - srtt: Pointer to smoothed RTT (will be updated)
  - rttvar: Pointer to RTT variance (will be updated)
  
  Returns:
  - The computed RTO value in microseconds
*/
uint32_t
lrtp_calculate_adaptive_rto(uint32_t rtt, uint32_t *srtt, uint32_t *rttvar)
{
  if (*srtt == 0) {
    /* First RTT sample: initialize SRTT and RTTVAR */
    *srtt = rtt;
    *rttvar = rtt / 2;
  } else {
    /* Update RTTVAR: 3/4 * RTTVAR + 1/4 * |SRTT - RTT| */
    uint32_t diff = (*srtt > rtt) ? (*srtt - rtt) : (rtt - *srtt);
    *rttvar = (3 * (*rttvar) + diff) / 4;
    
    /* Update SRTT: 7/8 * SRTT + 1/8 * RTT */
    *srtt = (7 * (*srtt) + rtt) / 8;
  }
  
  /* Calculate RTO: SRTT + 4 * RTTVAR, with minimum of 1s and maximum of 60s (RFC 6298) */
  uint32_t rto = *srtt + 4 * (*rttvar);
  
  /* Clamp RTO between 1 second (1000000 us) and 60 seconds (60000000 us) */
  if (rto < 1000000) {
    rto = 1000000; /* Minimum 1 second */
  } else if (rto > 60000000) {
    rto = 60000000; /* Maximum 60 seconds (optional per RFC 6928) */
  }
  
  return rto;
}
