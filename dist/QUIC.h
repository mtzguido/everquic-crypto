

#ifndef __QUIC_H
#define __QUIC_H

#include "EverQuic_EverCrypt.h"
#include "EverQuic.h"
#include "krml/internal/target.h"
#include "krml/internal/types.h"
#include "krml/lowstar_endianness.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

typedef krml_checked_int_t QUIC_nat62;

krml_checked_int_t QUIC_cipher_keysize(Spec_Agile_AEAD_alg a);

typedef uint8_t QUIC_u2;

typedef uint8_t QUIC_u4;

typedef uint64_t QUIC_u62;

typedef EverQuic_index QUIC_index;

typedef Prims_list__uint8_t *QUIC_traffic_secret;

typedef EverQuic_state_s *QUIC_state;

typedef void *QUIC_invariant;

EverCrypt_Error_error_code
QUIC_encrypt(
  EverQuic_index i,
  EverQuic_state_s *s,
  uint8_t *dst,
  uint64_t *dst_pn,
  EverQuic_header h,
  uint8_t *plain,
  uint32_t plain_len
);

typedef void *QUIC_decrypt_post;

EverCrypt_Error_error_code
QUIC_decrypt(
  EverQuic_state_s *uu___1,
  EverQuic_result *uu___2,
  uint8_t *uu___3,
  uint32_t uu___4,
  uint8_t uu___5
);


#define __QUIC_H_DEFINED
#endif
