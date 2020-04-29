module QUIC.Spec.Base

module FB = FStar.Bytes
module U62 = QUIC.UInt62
module U64 = FStar.UInt64
module U32 = FStar.UInt32
module U8 = FStar.UInt8
module S = FStar.Seq

type byte = FStar.UInt8.t
type bytes = S.seq byte
type lbytes (n:nat) = b:bytes{S.length b = n}

inline_for_extraction
noextract
let bitfield
  (sz: nat { sz <= 8 })
: Tot eqtype
= (x: U8.t { U8.v x < pow2 sz })

type payload_and_pn_length_t = (payload_and_pn_length: U62.t { U64.v payload_and_pn_length >= 20 })

let header_len_bound = 16500 // FIXME: this should be in line with the parser kind

inline_for_extraction
let short_dcid_len_t = (short_dcid_len: U32.t { U32.v short_dcid_len <= 20 })

inline_for_extraction
noextract
let token_max_len = 16383 // arbitrary bound

inline_for_extraction
let vlbytes (min: nat) (max: nat) =
  (x: FB.bytes { min <= FB.length x /\ FB.length x <= max })
