module QUIC.Parse
include QUIC.Spec.Base

module B = LowStar.Buffer
module U8 = FStar.UInt8
module U32 = FStar.UInt32
module HST = FStar.HyperStack.ST
module S = FStar.Seq
module U64 = FStar.UInt64


let header_len_bound = 16500 // FIXME: this should be in line with the parser kind

val header_len (h:header) : GTot (n: pos { n <= header_len_bound })

(*
=
  match h with
  | MShort spin phase cid ->
    1 + S.length cid + 1 + pn_len
  | MLong is_hs version dcil scil dcid scid plen ->
    let _ = assert_norm(max_cipher_length < pow2 62) in
    6 + add3 dcil + add3 scil + vlen plen + 1 + pn_len
*)

(*
  | Short _ _ cid -> sub3 (S.length cid)
  | Long _ _ dcil _ _ _ _ -> dcil
*)

val format_header: h:header -> GTot (lbytes (header_len h))

module BF = LowParse.BitFields

val format_header_is_short: h: header -> Lemma
  (MShort? h <==> BF.get_bitfield (U8.v (S.index (format_header h) 0)) 7 8 == 0)

val format_header_is_retry: h: header -> Lemma
  (is_retry h <==> (
    BF.get_bitfield (U8.v (S.index (format_header h) 0)) 7 8 == 1 /\
    BF.get_bitfield (U8.v (S.index (format_header h) 0)) 4 6 == 3
  ))

val format_header_pn_length: h: header -> Lemma
  (requires (~ (is_retry h)))
  (ensures (BF.get_bitfield (U8.v (S.index (format_header h) 0)) 0 2 == U32.v (pn_length h) - 1))

val pn_offset: (h: header { ~ (is_retry h) }) -> GTot (n: nat { 0 < n /\ n + U32.v (pn_length h) == header_len h }) // need to know that packet number is the last field of the format

val putative_pn_offset: (cid_len: nat) -> (x: bytes) -> GTot (option (y: nat {0 < y /\ y <= Seq.length x}))

val putative_pn_offset_frame
  (cid_len: nat)
  (x1 x2: bytes)
: Lemma
  (requires (match putative_pn_offset cid_len x1 with
  | None -> False
  | Some off ->
    off <= Seq.length x2 /\
    Seq.slice x1 1 off `Seq.equal` Seq.slice x2 1 off /\ (
    let f1 = Seq.index x1 0 in
    let f2 = Seq.index x2 0 in
    let is_short = BF.get_bitfield (U8.v f1) 7 8 = 0 in
    let number_of_protected_bits = if is_short then 5 else 4 in
    BF.get_bitfield (U8.v f1) number_of_protected_bits 8 == BF.get_bitfield (U8.v f2) number_of_protected_bits 8
  )))
  (ensures (match putative_pn_offset cid_len x1 with
  | None -> False
  | Some off -> putative_pn_offset cid_len x2 == Some (off <: nat)
  ))

val putative_pn_offset_correct
  (h: header {~ (is_retry h)})
  (cid_len: nat)
: Lemma
  (requires (MShort? h ==> cid_len == dcid_len h))
  (ensures (putative_pn_offset cid_len (format_header h) == Some (pn_offset h <: nat)))

noeq
type h_result =
| H_Success:
  h: header ->
  c: bytes ->
  h_result
| H_Failure

val parse_header: cid_len: nat { cid_len < 20 } -> last: nat { last + 1 < pow2 62 } -> b:bytes -> GTot (r: h_result {
  match r with
  | H_Failure -> True
  | H_Success h c ->
    (MShort? h ==> dcid_len h == cid_len) /\
    ((~ (is_retry h)) ==> in_window (U32.v (pn_length h) - 1) last (U64.v (packet_number h))) /\
    Seq.length c <= Seq.length b /\
    c == Seq.slice b (Seq.length b - Seq.length c) (Seq.length b)
})

val lemma_header_parsing_correct:
  h: header ->
  c: bytes ->
  cid_len: nat { cid_len < 20 } ->
  last: nat { last + 1 < pow2 62 } ->
  Lemma
  (requires (
    (MShort? h ==> cid_len == dcid_len h) /\
    ((~ (is_retry h)) ==> in_window (U32.v (pn_length h) - 1) last (U64.v (packet_number h)))
  ))
  (ensures (
    parse_header cid_len last S.(format_header h @| c)
    == H_Success h c))

// N.B. this is only true for a given DCID len
val lemma_header_parsing_safe: cid_len: nat -> last: nat -> b1:bytes -> b2:bytes -> Lemma
  (requires (
    cid_len < 20 /\
    last + 1 < pow2 62 /\
    parse_header cid_len last b1 == parse_header cid_len last b2
  ))
  (ensures parse_header cid_len last b1 == H_Failure \/ b1 = b2)

let lemma_header_parsing_post
  (cid_len: nat { cid_len < 20 })
  (last: nat { last + 1 < pow2 62 })
  (b: bytes)
: Lemma
  (match parse_header cid_len last b with
  | H_Failure -> True
  | H_Success h c ->
    (MShort? h ==> dcid_len h == cid_len) /\
    header_len h + Seq.length c == Seq.length b /\
    b == format_header h `Seq.append` c /\
    Seq.slice b 0 (header_len h) == format_header h /\
    c == Seq.slice b (header_len h) (Seq.length b)
  )
= match parse_header cid_len last b with
  | H_Failure -> ()
  | H_Success h c ->
    lemma_header_parsing_correct h c cid_len last ;
    lemma_header_parsing_safe cid_len last b (format_header h `S.append` c);
    assert (b `Seq.equal` (format_header h `Seq.append` c));
    assert (Seq.slice b 0 (header_len h) `Seq.equal` format_header h);
    assert (c `Seq.equal` Seq.slice b (header_len h) (Seq.length b))

module Impl = QUIC.Impl.Base

val read_header
  (packet: B.buffer U8.t)
  (packet_len: U32.t { let v = U32.v packet_len in v == B.length packet /\ v < 4294967280 })
  (cid_len: U32.t { U32.v cid_len < 20 } )
  (last: uint62_t { U64.v last + 1 < pow2 62 })
: HST.Stack (option (Impl.header & U32.t))
  (requires (fun h ->
    B.live h packet
  ))
  (ensures (fun h res h' ->
    B.modifies B.loc_none h h' /\
    begin
      let spec = parse_header (U32.v cid_len) (U64.v last) (B.as_seq h packet) in
      match res with
      | None -> H_Failure? spec
      | Some (x, len) ->
        H_Success? spec /\
        begin
          let H_Success hd _ = spec in
          Impl.header_live x h' /\
          U32.v len <= B.length packet /\
          B.loc_buffer (B.gsub packet 0ul len) `B.loc_includes` Impl.header_footprint x /\
          Impl.g_header x h' == hd /\
          U32.v len = header_len hd
        end
    end
  ))

val impl_header_length
  (x: Impl.header)
: HST.Stack U32.t
  (requires (fun h -> Impl.header_live x h))
  (ensures (fun h res h' ->
    B.modifies B.loc_none h h' /\
    U32.v res == header_len (Impl.g_header x h)
  ))

val write_header
  (dst: B.buffer U8.t)
  (x: Impl.header)
: HST.Stack unit
  (requires (fun h ->
    B.live h dst /\
    Impl.header_live x h /\
    B.length dst == header_len (Impl.g_header x h) /\
    Impl.header_footprint x `B.loc_disjoint` B.loc_buffer dst
  ))
  (ensures (fun h _ h' ->
    B.modifies (B.loc_buffer dst) h h' /\
    B.as_seq h' dst == format_header (Impl.g_header x h)
  ))

(*
val test : B.buffer U8.t -> HST.Stack U32.t (requires (fun _ -> False)) (ensures (fun _ _ _ -> True))
