
/*
  CS3102 Coursework P2 : Simple, Reliable Transport Protocol (LRTP)
  saleem, Jan2024, Feb2023
  checked March 2025 (sjm55)
  revised March 2026 (sjm55)

  These are suggested definitions only.
  Please modify as required.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <arpa/inet.h>

#include "lrtp-common.h"
#include "lrtp-pcb.h"
#include "lrtp-fsm.h"
#include "lrtp.h"

/* Only one connection at a time required, so use a single PCB structure. */
/* (Would need to change this for supporting multiple, simultaneous connections.) */

Lrtp_Pcb_t G_pcb;

const char *G_state_str_aux[] = {
   /* same order as in LRTP_state_e */
  "error",
  "closed",
  "listening",
  "opening_i",
  "opening_r",
  "connected",
  "closing_i",
  "closing_r"
};

const char **G_state_str = &(G_state_str_aux[1]); /* so -1 will work for "error" */

#define LRTP_STATE_STR(s_) \
  (s_ >= LRTP_state_error && s_ <= LRTP_state_closing_r ? G_state_str[(int) s_] : "undefined")


void
reset_LrtpPcb()
{
  G_pcb.sd = 0;
  G_pcb.port = 0;
  memset((void *) &G_pcb.local, 0, sizeof(G_pcb.local));
  memset((void *) &G_pcb.remote, 0, sizeof(G_pcb.remote));
  G_pcb.state = LRTP_state_closed;
  G_pcb.seq_tx = 1; // next number to use in data_req
  G_pcb.seq_rx = 1; // next number expected in data_ack
  G_pcb.re_tx = 0;
  G_pcb.rtt = 0;
  G_pcb.rto = LRTP_RTO_FIXED; /* use this to start with */
  
  /* Adaptive RTO initialization */
  G_pcb.srtt = 0; /* smoothed RTT, 0 means not yet measured */
  G_pcb.rttvar = 0; /* RTT variance */
  G_pcb.tx_timestamp = 0; /* timestamp of sent packet */

#define RESETu64(r64_)  G_pcb.r64_ = 0

  RESETu64(start_time);
  RESETu64(finish_time);

  RESETu64(open_req_rx);
  RESETu64(open_req_dup_rx);
  RESETu64(open_reqack_rx);
  RESETu64(open_reqack_dup_rx);
  RESETu64(open_ack_rx);
  RESETu64(open_ack_dup_rx);

  RESETu64(open_req_tx);
  RESETu64(open_req_re_tx);
  RESETu64(open_reqack_tx);
  RESETu64(open_reqack_re_tx);
  RESETu64(open_ack_tx);
  RESETu64(open_ack_re_tx);

  RESETu64(data_req_rx);
  RESETu64(data_req_bytes_rx);
  RESETu64(data_ack_rx);
  RESETu64(data_req_dup_rx);
  RESETu64(data_req_bytes_dup_rx);
  RESETu64(data_ack_dup_rx);

  RESETu64(data_req_tx);
  RESETu64(data_req_bytes_tx);
  RESETu64(data_ack_tx);
  RESETu64(data_req_re_tx);
  RESETu64(data_req_bytes_re_tx);
  RESETu64(data_ack_re_tx);

  RESETu64(close_req_rx);
  RESETu64(close_req_dup_rx);
  RESETu64(close_ack_rx);
  RESETu64(close_ack_dup_rx);

  RESETu64(close_req_tx);
  RESETu64(close_req_re_tx);
  RESETu64(close_ack_tx);
  RESETu64(close_ack_re_tx);
}

/* debugging */

void
LrtpPcb_local(char *s)  // s must point to a space of >= 22 bytes
{ sprintf(s, "%s:%d", inet_ntoa(G_pcb.local.sin_addr), ntohs(G_pcb.local.sin_port)); }

void
LrtpPcb_remote(char *s)  // s must point to a space of >= 22 bytes
{ sprintf(s, "%s:%d", inet_ntoa(G_pcb.remote.sin_addr), ntohs(G_pcb.remote.sin_port)); }

void
LrtpPcb_report()
{
    /* flow duration in microseconds [us] */
  uint64_t duration = 0;

  if (G_pcb.start_time > 0) {

    uint64_t finish_time = G_pcb.finish_time > 0 ? G_pcb.finish_time : lrtp_timestamp();
    duration = finish_time - G_pcb.start_time;

  #define TIME_STR_SIZE 64 // should be enough!

    char time_str[TIME_STR_SIZE];
    
    if (lrtp_time_str(G_pcb.start_time, time_str) > 0)
      printf("start_time: %s\n", time_str);
    else printf("start_time: %"PRIu64".%06"PRIu64"s\n",
                LRTP_TIMESTAMP_SECONDS(G_pcb.start_time),
                LRTP_TIMESTAMP_MICROSECONDS(G_pcb.finish_time));

    if (lrtp_time_str(finish_time, time_str) > 0)
      printf("%s: %s\n", G_pcb.finish_time > 0 ? "finish_time" : "(now)", time_str);
    else printf("%s: %"PRIu64".%06"PRIu64"s\n",
                G_pcb.finish_time > 0 ? "finish_time" : "(now)",
                LRTP_TIMESTAMP_SECONDS(finish_time),
                LRTP_TIMESTAMP_MICROSECONDS(finish_time));

    printf("(duration): %"PRIu64".%06"PRIu64"s\n",
           LRTP_TIMESTAMP_SECONDS(duration), LRTP_TIMESTAMP_MICROSECONDS(duration));

  }

  else {
    printf("start_time: 0 (correct start_time not set)\n");
  }

  /*
    PCB state
  */
  printf("sd: %d\n", G_pcb.sd);
  printf("port: %u\n", G_pcb.port);
  printf("local: %s %d\n", inet_ntoa(G_pcb.local.sin_addr), ntohs(G_pcb.local.sin_port));
  printf("remote: %s %d\n", inet_ntoa(G_pcb.remote.sin_addr), ntohs(G_pcb.remote.sin_port));

  printf("state: %d (%s)\n", G_pcb.state, LRTP_STATE_STR(G_pcb.state));

  printf("seq_tx: %u\n", G_pcb.seq_tx);
  printf("seq_rx: %u\n", G_pcb.seq_rx);
  printf("rtt: %u us\n", G_pcb.rtt);
  printf("rto: %u us\n", G_pcb.rto);

#define PRINTFu64(p64_)  printf("%s: %"PRIu64"\n", #p64_, G_pcb.p64_)

  PRINTFu64(open_req_rx);
  PRINTFu64(open_req_dup_rx);
  PRINTFu64(open_reqack_rx);
  PRINTFu64(open_reqack_dup_rx);
  PRINTFu64(open_ack_rx);
  PRINTFu64(open_ack_dup_rx);

  PRINTFu64(open_req_tx);
  PRINTFu64(open_req_re_tx);
  PRINTFu64(open_reqack_tx);
  PRINTFu64(open_reqack_re_tx);
  PRINTFu64(open_ack_tx);
  PRINTFu64(open_ack_re_tx);

  PRINTFu64(data_req_rx);
  PRINTFu64(data_req_bytes_rx);
  PRINTFu64(data_ack_rx);
  PRINTFu64(data_req_dup_rx);
  PRINTFu64(data_req_bytes_dup_rx);
  PRINTFu64(data_ack_dup_rx);

  PRINTFu64(data_req_tx);
  PRINTFu64(data_req_bytes_tx);
  PRINTFu64(data_ack_tx);
  PRINTFu64(data_req_re_tx);
  PRINTFu64(data_req_bytes_re_tx);
  PRINTFu64(data_ack_re_tx);

  PRINTFu64(close_req_rx);
  PRINTFu64(close_req_dup_rx);
  PRINTFu64(close_ack_rx);
  PRINTFu64(close_ack_dup_rx);

  PRINTFu64(close_req_tx);
  PRINTFu64(close_req_re_tx);
  PRINTFu64(close_ack_tx);
  PRINTFu64(close_ack_re_tx);

  /*
    Evaluate data-rate in bits/s (bps) (approximate)
  */
  if (duration > 0) {

    double rate = 0.0;

#define MEGA 1000000.0
#define KILO 1000.0
    /* receive (rx) data rate */
    if (G_pcb.data_req_bytes_rx > 0) {
      printf("data mean rx_rate:"); // clock tick is 1000000.0 -- microseconds
      rate = (double) G_pcb.data_req_bytes_rx * 8 / ((double) duration / 1000000.0); // bps
      double rx_rate;

      /* pretty-print : Mbps */
      if (rate > MEGA) {
        rx_rate = rate / MEGA; /* Mbps */
        printf(" %.02lf Mbps", rx_rate);
      }

      /* pretty-print : Kbps */
      else if (rate > KILO) {
        rx_rate = rate / KILO; /* Kbps */
        printf(" %.02lf Kbps", rx_rate);
      }

      printf(" - %"PRIu64" bps\n", (uint64_t) rate);
    }

    /* transmission (tx) data rate */
    if (G_pcb.data_req_bytes_tx > 0) {

      printf("data mean tx_rate:");
      rate = (double) G_pcb.data_req_bytes_tx * 8 / ((double) duration / 1000000.0); // bps
      double tx_rate;

      /* pretty-print : Mbps */
      if (rate > MEGA) {
        tx_rate = rate / MEGA; /* Mbps */
        printf(" %.02lf Mbps", tx_rate);
      }

      /* pretty-print : Kbps */
      else if (rate > KILO) {
        tx_rate = rate / KILO; /* Kbps */
        printf(" %.02lf Kbps", tx_rate);
      }

      printf(" - %"PRIu64" bps\n", (uint64_t) rate);
    }

  } // if (duration > 0)

}
