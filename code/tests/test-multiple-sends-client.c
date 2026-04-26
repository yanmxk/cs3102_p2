/*
  CS3102 Coursework P2 : LRTP Multiple Sends Test Client
  
  Tests multiple sequential data packets with varying sizes.
  Demonstrates RTO stability across diverse packet streams.
  
  Usage: ./test-multiple-sends-client <server_ip> [server_port]
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "lrtp.h"
#include "lrtp-pcb.h"
#include "lrtp-common.h"

extern Lrtp_Pcb_t G_pcb;

int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(stderr, "Usage: %s <server_ip> [server_port]\n", argv[0]);
    return 1;
  }

  const char *server_ip = argv[1];
  uint16_t server_port = 24536;

  if (argc > 2) {
    server_port = (uint16_t)atoi(argv[2]);
  }

  lrtp_init();

  printf("Multiple Sends Test Client\n");
  printf("Connecting to %s:%u\n", server_ip, server_port);

  int sd = lrtp_open(server_ip, server_port);
  if (sd < 0) {
    fprintf(stderr, "lrtp_open() failed\n");
    return 1;
  }

  printf("Connection established\n\n");

  // Test payloads of varying sizes
  struct {
    const char *data;
    int size;
    const char *label;
  } payloads[] = {
    {"Hello", 5, "Small (5)"},
    {"This is a medium sized message.", 31, "Medium (31)"},
    {"The quick brown fox jumps over the lazy dog. " \
     "The quick brown fox jumps over the lazy dog.", 92, "Large (92)"},
    {"\x00\x01\x02\x03\x04\x05\x06\x07", 8, "Binary (8)"},
    {"", 0, "Empty (0)"},
  };

  int num_payloads = sizeof(payloads) / sizeof(payloads[0]);

  printf("Payload  |   RTT (us)   |  SRTT (us)   | RTTVAR (us)  |   RTO (us)   | Label\n");
  printf("---------|--------------|--------------|--------------|--------------|-------------------\n");

  for (int i = 0; i < num_payloads; i++) {
    int n = lrtp_tx(sd, (uint8_t *)payloads[i].data, payloads[i].size);
    if (n < 0) {
      fprintf(stderr, "lrtp_tx() failed at payload %d\n", i + 1);
      break;
    }

    printf("%7d | %12u | %12u | %12u | %12u | %s\n",
           i + 1,
           G_pcb.rtt,
           G_pcb.srtt,
           G_pcb.rttvar,
           G_pcb.rto,
           payloads[i].label);
  }

  printf("\n");
  printf("Closing connection...\n");

  if (lrtp_close(sd) < 0) {
    fprintf(stderr, "lrtp_close() failed\n");
  }

  printf("Connection closed\n\n");

  printf("Final RTO Statistics:\n");
  printf("  Smoothed RTT: %u us (%.3f ms)\n", G_pcb.srtt, (double)G_pcb.srtt / 1000.0);
  printf("  RTT Variance: %u us (%.3f ms)\n", G_pcb.rttvar, (double)G_pcb.rttvar / 1000.0);
  printf("  RTO: %u us (%.3f s)\n", G_pcb.rto, (double)G_pcb.rto / 1000000.0);
  printf("  Data packets TX: %llu\n", G_pcb.data_req_tx);
  printf("  Data packets RX (ACK): %llu\n", G_pcb.data_ack_rx);
  printf("  Retransmissions: %llu\n", G_pcb.data_req_re_tx);

  LrtpPcb_report();

  return 0;
}
