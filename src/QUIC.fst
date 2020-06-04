module QUIC

module QSpec = QUIC.Spec
module QImpl = QUIC.State
module QImplBase = QUIC.Impl.Header.Base
module QModel = Model.QUIC

module I = Model.Indexing
module G = FStar.Ghost
module B = LowStar.Buffer
module S = FStar.Seq
module HS = FStar.HyperStack
module ST = FStar.HyperStack.ST

module U64 = FStar.UInt64
module U32 = FStar.UInt32
module U8 = FStar.UInt8

open FStar.HyperStack
open FStar.HyperStack.ST

open EverCrypt.Helpers
open EverCrypt.Error

let index =
  if I.model then QModel.id else QImpl.index

let mid (i:index{I.model}) = i <: QModel.id
let iid (i:index{not I.model}) = i <: QImpl.index

let alg (i:index) =
  if I.model then I.ae_id_ginfo (fst (mid i))
  else (iid i).QImpl.aead_alg

let halg (i:index) =
  if I.model then I.ae_id_ghash (fst (mid i))
  else (iid i).QImpl.hash_alg

let itraffic_secret (i:QModel.id) =
  Spec.Hash.Definitions.bytes_hash (I.ae_id_ghash (fst i))

module MH = Model.Helpers

let derived (#i:QModel.id) (#w:QModel.stream_writer i) (r:QModel.stream_reader w) (ts:itraffic_secret i) =
  if I.model && QModel.unsafe i then
    let ha = I.ae_id_hash (fst i) in
    let ea = I.ae_id_info (fst i) in
    let (k1, k2) = QModel.reader_leak r in
    MH.hide (QModel.writer_static_iv w) ==
      QSpec.derive_secret ha ts QSpec.label_iv 12 /\
    MH.hide k1 == QSpec.derive_secret ha ts
        QSpec.label_key (QSpec.cipher_keysize ea) /\
    MH.hide k2 == QUIC.Spec.derive_secret ha ts
        QUIC.Spec.label_key (QSpec.cipher_keysize ea)
  else True

noeq type mstate_t i =
| Ideal:
  writer: QModel.stream_writer i ->
  reader: QModel.stream_reader writer ->
  ts: itraffic_secret i{derived reader ts} -> // FIXME erased
  mstate_t i

let istate_t i = QImpl.state i

let state i =
  if I.model then mstate_t (mid i)
  else istate_t (iid i)

let mstate (#i:index{I.model}) (s:state i) = s <: mstate_t (mid i)
let istate (#i:index{not I.model}) (s:state i) = s <: istate_t (iid i)

let footprint #i h s =
  if I.model then
    QModel.rfootprint (mstate s).reader `B.loc_union` QModel.footprint (mstate s).writer
  else QImpl.footprint h (istate s)

let invariant #i h s =
  if I.model then
    let Ideal writer reader _ = mstate s in
    QModel.invariant writer h /\ QModel.rinvariant reader h /\
    B.loc_disjoint (QModel.rfootprint (mstate s).reader) (QModel.footprint (mstate s).writer)
  else QImpl.invariant h (istate s)

let g_traffic_secret #i s h =
  if I.model then (mstate s).ts
  else
    QImpl.g_traffic_secret (B.deref h (istate s))

let g_initial_packet_number #i s h =
  assert_norm (pow2 62 - 1 < pow2 64);
  if I.model then
    Lib.IntTypes.u64 (QModel.writer_offset #(mid i) (mstate s).writer)
  else
    QImpl.g_initial_packet_number (B.deref h (istate s))

let g_last_packet_number #i s h =
  assert_norm (pow2 62 - 1 < pow2 64);
  if I.model then
    Lib.IntTypes.u64 (UInt64.v (QModel.expected_pnT #(mid i) (mstate s).reader h)) // - 1 ?
  else
    QImpl.g_last_packet_number (B.deref h (istate s)) h

// TODO: reveal in the interface (just for good measure)
let frame_invariant #i l s h0 h1 =
  if I.model then
    let Ideal w r _ = mstate #(mid i) s in
    QModel.frame_invariant w h0 l h1;
    QModel.rframe_invariant r h0 l h1
  else
    QImpl.frame_invariant #(iid i) l s h0 h1

/// Ingredients we need for the mythical switch

/// First, a stateful equivalent of as_seq. Implementation doesn't need to be
/// efficient.

let rec as_seq #a (b: B.buffer a) (l: UInt32.t { l == B.len b }): Stack (S.seq a)
  (requires fun h0 ->
    B.live h0 b)
  (ensures fun h0 r h1 ->
    h0 == h1 /\
    B.as_seq h0 b `S.equal` r)
=
  let h0 = ST.get () in
  if l = 0ul then
    S.empty
  else
    let hd = B.index b 0ul in
    let l = l `U32.sub` 1ul in
    let b = B.sub b 1ul l in
    S.cons hd (as_seq b l)

#set-options "--fuel 0 --ifuel 0 --z3rlimit 200"

let encrypt #i s dst dst_pn h plain plain_len =
  if I.model then
    let i = i <: QModel.id in
    // A pure version of plain suitable for calling specs with. From here on,
    // this is a "magical" value that has no observable side-effects since it
    // belongs to spec-land.
    let plain_s = as_seq plain in

    // We can clear out the contents of the "real" buffer.
    B.fill plain (Lib.IntTypes.u8 0) plain_len;

    let _ = allow_inversion QImplBase.header in
    let _ = allow_inversion QImplBase.long_header_specifics in

    // Yet do a "fake" call that generates the same side-effects.
    push_frame ();
    let hash_alg: QSpec.ha = I.ae_id_hash (fst i) in
    let aead_alg = I.ae_id_info (fst i) in
    let dummy_traffic_secret = B.alloca (Lib.IntTypes.u8 0) (Hacl.Hash.Definitions.hash_len hash_alg) in
    let dummy_index: QImpl.index = { QImpl.hash_alg = hash_alg; QImpl.aead_alg = aead_alg } in
    let dummy_dst = B.alloca B.null 1ul in
    // This changes the side-effects between the two branches, which is
    // precisely what we're trying to avoid. We could allocate this on the stack
    // with QImpl.alloca (hence eliminating the heap allocation effect), but for
    // that we need EverCrypt.AEAD.alloca which was merged to master only two
    // days ago. So this will have to be fixed for the final version.
    let r = QImpl.create_in dummy_index HS.root dummy_dst (Lib.IntTypes.u64 0) dummy_traffic_secret in
    // This is just annoying because EverCrypt still doesn't have a C fallback
    // implementation for AES-GCM so UnsupportedAlgorithm errors may be thrown
    // for one of our chosen algorithms.
    // Assuming here a C implementation of AES-GCM will eventually happen and
    // EverCrypt will allow eliminating in the post-condition the
    // UnsupportedAlgorithm case provided the user passes in an aead_alg that is
    // one of the supported ones (i.e. not one of the CCM variants, which we do
    // not use here).
    assume (r <> UnsupportedAlgorithm);
    let dummy_s = LowStar.BufferOps.(!* dummy_dst) in
    let r = QImpl.encrypt #(G.hide dummy_index) dummy_s dst dst_pn h plain plain_len in
    pop_frame ();

    admit ();
    r
  else
    let s = s <: QImpl.state i in
    QImpl.encrypt #(G.hide (i <: QImpl.index)) s dst dst_pn h plain plain_len
