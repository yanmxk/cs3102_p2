#ifndef __lrtp_packet_h__
#define __lrtp_packet_h__

/*
  CS3102 Coursework P2 : Simple, Reliable Transport Protocol (LRTP)
  saleem, Jan2024, Feb2023
  checked March 2025 (sjm55)
  revised March 2026 (sjm55)

  These are suggested definitions only.
  Please modify as required.
*/

#include "lrtp-common.h"

/* packet type values : bit field, but can be used as required */

#define LRTP_TYPE_req       ((uint8_t) 0x01)
#define LRTP_TYPE_ack       ((uint8_t) 0x02)

#define LRTP_TYPE_open         ((uint8_t) 0x10)
#define LRTP_TYPE_open_req     (LRTP_TYPE_open | LRTP_TYPE_req)
#define LRTP_TYPE_open_reqack  (LRTP_TYPE_open | LRTP_TYPE_req | LRTP_TYPE_ack)
#define LRTP_TYPE_open_ack     (LRTP_TYPE_open | LRTP_TYPE_ack)

#define LRTP_TYPE_close     ((uint8_t) 0x20)
#define LRTP_TYPE_close_req (LRTP_TYPE_close | LRTP_TYPE_req)
#define LRTP_TYPE_close_ack (LRTP_TYPE_close | LRTP_TYPE_ack)

#define LRTP_TYPE_data      ((uint8_t) 0x40)
#define LRTP_TYPE_data_req  (LRTP_TYPE_data | LRTP_TYPE_req)
#define LRTP_TYPE_data_ack  (LRTP_TYPE_data | LRTP_TYPE_ack)
typedef struct Lrtp_Header_s {
    uint8_t  type;
    uint8_t  _pad;
    uint16_t _pad2;
    uint32_t seq;
    uint16_t data_size;
    uint16_t _pad3;
} Lrtp_Header_t;  /* 12 bytes */

typedef struct Lrtp_Packet_s {
    Lrtp_Header_t hdr;
    uint8_t       payload[LRTP_MAX_DATA_SIZE];
} Lrtp_Packet_t;

#endif /* __lrtp_packet_h__ */
