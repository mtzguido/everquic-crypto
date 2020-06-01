module QUIC.Spec
include QUIC.Spec.Base

open Model.Indexing

module S = FStar.Seq
module HD = Spec.Hash.Definitions
module AEAD = Spec.Agile.AEAD
module Cipher = Spec.Agile.Cipher

// JP: should we allow inversion on either hash algorithm or AEAD algorithm?
#set-options "--max_fuel 0 --max_ifuel 0"

// Moved to Model.Indexing
type ha = ha
type ea = ea
type ca = ca

// Move from Hashing.Spec to Spec.Hash?
let keysized (a:ha) (l:nat) =
  l <= HD.max_input_length a /\ l + HD.block_length a < pow2 32
let hashable (a:ha) (l:nat) = l <= HD.max_input_length a

let header_len_bound = 16500 // FIXME: this should be in line with the parser kind

// AEAD plain and ciphertext. We want to guarantee that regardless
// of the header size (max is 54), the neader + ciphertext + tag fits in a buffer
// JP: perhaps cleaner with a separate lemma; any reason for putting this in a refinement?
let max_plain_length: n:nat {
  forall a. {:pattern AEAD.max_length a} n <= AEAD.max_length a
} =
  pow2 32 - header_len_bound - 16

let max_cipher_length : n:nat {
  forall a. {:pattern AEAD.max_length a \/ AEAD.tag_length a }
    n <= AEAD.max_length a + AEAD.tag_length a
} =
  pow2 32 - header_len_bound

type pbytes = b:bytes{let l = S.length b in 3 <= l /\ l < max_plain_length}
type pbytes' (is_retry: bool) = b:bytes{let l = S.length b in if is_retry then l == 0 else (3 <= l /\ l < max_plain_length)}
type cbytes = b:bytes{let l = S.length b in 19 <= l /\ l < max_cipher_length}
type cbytes' (is_retry: bool) = b: bytes { let l = S.length b in if is_retry then l == 0 else (19 <= l /\ l < max_cipher_length) }

let ae_keysize (a:ea) =
  Spec.Agile.Cipher.key_length (Spec.Agile.AEAD.cipher_alg_of_supported_alg a)

// Static byte sequences to be fed into secret derivation. Marked as inline, so
// that they can be used as arguments to gcmalloc_of_list for top-level arrays.
inline_for_extraction
val label_key: lbytes 3
inline_for_extraction
val label_iv: lbytes 2
inline_for_extraction
val label_hp: lbytes 2

val derive_secret:
  a: ha ->
  prk:Spec.Hash.Definitions.bytes_hash a ->
  label: bytes ->
  len: nat ->
  Pure (lbytes len)
  (requires len <= 255 /\
    S.length label <= 244 /\
    keysized a (S.length prk)
    )
  (ensures fun out -> True)

type nat2 = n:nat{n < 4}
type nat4 = n:nat{n < 16}
type nat32 = n:nat{n < pow2 32}
type nat62 = n:nat{n < pow2 62}

let add3 (n:nat4) : n:nat{n=0 \/ (n >= 4 /\ n <= 18)} = if n = 0 then 0 else 3+n
let sub3 (n:nat{n = 0 \/ (n >= 4 /\ n <= 18)}) : nat4 = if n = 0 then 0 else n-3
type qbytes (n:nat4) = lbytes (add3 n)

// JP: seems appropriate for this module...?
let _: squash (inversion header) = allow_inversion header

inline_for_extraction
val pn_sizemask_naive: pn_len:nat2 -> lbytes (pn_len + 1)

val block_of_sample: a:Spec.Agile.Cipher.cipher_alg -> k: Spec.Agile.Cipher.key a -> sample: lbytes 16 -> lbytes 16

// Header protection only
val header_encrypt: a:ea ->
  hpk: lbytes (ae_keysize a) ->
  h: header ->
  c: cbytes' (is_retry h) ->
  GTot packet

noeq
type h_result =
| H_Success:
  h: header ->
  cipher: cbytes' (is_retry h) ->
  rem: bytes ->
  h_result
| H_Failure

// JP: should we allow inversion on either hash algorithm or AEAD algorithm?
#push-options "--max_ifuel 1"

// Note that cid_len cannot be parsed from short headers
val header_decrypt: a:ea ->
  hpk: lbytes (ae_keysize a) ->
  cid_len: nat { cid_len <= 20 } ->
  last: nat { last + 1 < pow2 62 } ->
  p: packet ->
  GTot (r: h_result { match r with
  | H_Failure -> True
  | H_Success h c rem ->
    is_valid_header h cid_len last /\
    S.length rem <= S.length p /\
    rem `S.equal` S.slice p (S.length p - S.length rem) (S.length p)
  })

#pop-options

// TODO: add a prefix lemma on header_decrypt, if ever useful

module U32 = FStar.UInt32
module U64 = FStar.UInt64

// This is just functional correctness, but does not guarantee security:
// decryption can succeed on an input that is not the encryption
// of the same arguments (see QUIC.Spec.Old.*_malleable)
val lemma_header_encryption_correct:
  a:ea ->
  k:lbytes (ae_keysize a) ->
  h:header ->
  cid_len: nat { cid_len <= 20 /\ (MShort? h ==> cid_len == dcid_len h) } ->
  last: nat { last + 1 < pow2 62 /\ ((~ (is_retry h)) ==> in_window (U32.v (pn_length h) - 1) last (U64.v (packet_number h))) } ->
  c: cbytes' (is_retry h) { has_payload_length h ==> U64.v (payload_length h) == S.length c } ->
  Lemma (
    header_decrypt a k cid_len last (header_encrypt a k h c)
    == H_Success h c S.empty)


noeq
type result =
| Success: 
  h: header ->
  plain: pbytes' (is_retry h) ->
  remainder: bytes ->
  result
| Failure

val encrypt:
  a: ea ->
  k: AEAD.kv a ->
  static_iv: lbytes 12 ->
  hpk: lbytes (ae_keysize a) ->
  h: header ->
  plain: pbytes' (is_retry h) ->
  Ghost packet
  (requires has_payload_length h ==> U64.v (payload_length h) == S.length plain + AEAD.tag_length a)
  (ensures fun _ -> True)

/// decryption and correctness

#set-options "--max_fuel 0 --max_ifuel 1"

val decrypt:
  a: ea ->
  k: AEAD.kv a ->
  static_iv: lbytes 12 ->
  hpk: lbytes (ae_keysize a) ->
  last: nat{last+1 < pow2 62} ->
  cid_len: nat { cid_len <= 20 } ->
  packet: packet ->
  GTot (r: result {
    match r with
    | Failure -> True
    | Success h _ rem ->
      is_valid_header h cid_len last /\
      S.length rem <= Seq.length packet /\
      rem `S.equal` S.slice packet (S.length packet - S.length rem) (S.length packet)
  })

val lemma_encrypt_correct:
  a: ea ->
  k: AEAD.kv a ->
  siv: lbytes 12 ->
  hpk: lbytes (ae_keysize a) ->
  h: header ->
  cid_len: nat { cid_len <= 20 /\ (MShort? h ==> cid_len == dcid_len h) } ->
  last: nat{last+1 < pow2 62 } ->
  p: pbytes' (is_retry h)  { has_payload_length h ==> U64.v (payload_length h) == S.length p + AEAD.tag_length a } -> Lemma
  (requires (
    (~ (is_retry h)) ==> (
      in_window (U32.v (pn_length h) - 1) last (U64.v (packet_number h))
  )))
  (ensures (
    decrypt a k siv hpk last cid_len
      (encrypt a k siv hpk h p)
    == Success h p Seq.empty
  ))
