/*
  CS3102 Coursework P2 : LRTP Large Transfer Test Client
  
  Tests RTO adaptation over a longer transfer with larger payloads.
  Demonstrates how adaptive RTO adjusts to sustained network conditions.
  
  Usage: ./test-large-transfer-client <server_ip> [server_port] [payload_size]
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lrtp.h"
#include "lrtp-pcb.h"
#include "lrtp-common.h"

#define TOTAL_PACKETS 20
#define DEFAULT_PAYLOAD_SIZE 512

extern Lrtp_Pcb_t G_pcb;

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <server_ip> [server_port] [payload_size]\n", argv[0]);
    return 1;
  }

  const char *server_ip = argv[1];
  uint16_t server_port = 24536;
  int payload_size = DEFAULT_PAYLOAD_SIZE;

  if (argc > 2) {
    server_port = (uint16_t)atoi(argv[2]);
  }
  if (argc > 3) {
    payload_size = atoi(argv[3]);
    if (payload_size <= 0 || payload_size > 1024) {
      fprintf(stderr, "Payload size must be between 1 and 1024\n");
      return 1;
    }
  }

  lrtp_init();

  printf("Large Transfer Test Client\n");
  printf("Connecting to %s:%u\n", server_ip, server_port);
  printf("Payload size: %d bytes, Total packets: %d\n\n", payload_size, TOTAL_PACKETS);

  int sd = lrtp_open(server_ip, server_port);
  if (sd < 0) {
    fprintf(stderr, "lrtp_open() failed\n");
    return 1;
  }

  printf("Connection established\n\n");

  uint8_t *payload = (uint8_t *)malloc(payload_size);
  if (payload == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return 1;
  }

  /* Fill payload with pattern data */
  for (int i = 0; i < payload_size; i++) {
    payload[i] = (uint8_t)((i + 'A') % 256);
  }

  printf("Packet |   RTT (us)   |  SRTT (us)   | RTTVAR (us)  |   RTO (us)   |  RTO (s)\n");
  printf("--------|--------------|--------------|--------------|--------------|----------\n");

  for (int i = 0; i < TOTAL_PACKETS; i++) {
    int n = lrtp_tx(sd, payload, payload_size);
    if (n < 0) {
      fprintf(stderr, "lrtp_tx() failed at packet %d\n", i + 1);
      break;
    }

    printf("%7d | %12u | %12u | %12u | %12u | %8.3f\n",
           i + 1,
           G_pcb.rtt,
           G_pcb.srtt,
           G_pcb.rttvar,
           G_pcb.rto,
           (double)G_pcb.rto / 1000000.0);
  }

  printf("\n");
  printf("Closing connection...\n");

  if (lrtp_close(sd) < 0) {
    fprintf(stderr, "lrtp_close() failed\n");
  }

  printf("Connection closed\n\n");

  /* Print final RTO statistics */
  printf("Final RTO Statistics:\n");
  printf("  Smoothed RTT: %u us (%.3f ms)\n", G_pcb.srtt, (double)G_pcb.srtt / 1000.0);
  printf("  RTT Variance: %u us (%.3f ms)\n", G_pcb.rttvar, (double)G_pcb.rttvar / 1000.0);
  printf("  RTO: %u us (%.3f s)\n", G_pcb.rto, (double)G_pcb.rto / 1000000.0);
  printf("  Data packets TX: %llu\n", G_pcb.data_req_tx);
  printf("  Data packets RX (ACK): %llu\n", G_pcb.data_ack_rx);
  printf("  Retransmissions: %llu\n", G_pcb.data_req_re_tx);

  LrtpPcb_report();

  free(payload);
  return 0;
}
