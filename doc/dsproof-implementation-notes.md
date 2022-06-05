Double Spend Proofs - RADN Implementation Notes
===============================================

This document serves a two-fold purpose:

1. To give information to users of the RADN software about expected behavior
   of the DSProof feature which is not fully addressed in the current
   specification.

2. To provide additional notes about interpretation of the
   specification in instances where more clarity is deemed useful -
   for anyone trying to understand or implement the specification.

DSProof implementation behavior in RADN
---------------------------------------

We are providing here
some additional notes on the RADN implementation of this feature.

1. The DSProof functionality is enabled by default, this means
   DS proofs are created and relayed. Both creation and relay  can
   be disabled by setting `doublespendproof=0` in the configuration
   or passing `-doublespendproof=0` or `-disabledoublespendproof`
   on the command line.

2. DSProofs are issued even for double spent transactions whose
   direct ancestors are not confirmed.
   This includes transactions whose ancestry includes unspent
   P2SH inputs, or unspent transactions signed with ANYONECANPAY
   hash type.

3. If a transaction that is double spent is in the mempool (or UTXO set)
   and has descendants in the mempool, a DSProof is currently only
   issued for the double spent transaction itself.
   There is no notification or query mechanism yet to inform that the
   descendants have now been put at risk too due to the double spend.

4. The GUI wallet does not display any notice yet when a transaction
   pending to be confirmed is double spent.

5. There is a parameter for debug logging of DSProof functionality.
   It can be enabled by by adding `dsproof` to the debug flags.

6. Orphans:
   - DS proofs are stored as either orphans or non-orphans.
   - Under normal circumstances, orphans are proofs we have received for
     which the conflicting transaction (or UTXO) has not yet been seen,
     therefore the proof cannot be validated yet when it is seen.
   - Orphan proofs are never relayed (only non-orphans are).
   - There is a maximum number of orphans (default 65535) but in
     practice an extra 25% is allowed for performance reasons.
     The high water mark is 1.25 * max = 81918, if exceeded, the oldest
     orphans are removed until the subsystem is below the high water
     mark again.
   - There is an orphan expiry time which defaults to 90 seconds.
   - A periodic cleanup thread runs every 60 seconds, to reap expired
     orphans.
   - Orphans can become non-orphans when the necessary information to
     validate them, is received.
   - Non-orphans can also become orphans for up to 90 seconds (after which
     they are deleted) in the case where their associated transaction
     disappears from the mempool (due to e.g. being confirmed in a block).
     This allows us to keep proofs around for a time so that they may be
     re-applied in the unlikely event of a reorg, where a transaction may
     end up in the mempool again (in such a situation it is advantageous
     for the dsproof to not be lost).
   - Non-orphan proofs are not subject to automatic expiry -- they live
     as long as their associated transaction lives in the mempool.
   - Misbehaving peers that supplied orphan proofs which turn out to be fake
     (after a valid proof is received that shows the orphan was fake)
     will get punished (misbehaviour score increased by 10 points).

Notes on the DSProof specification
----------------------------------

1. A copy of the specification can be found at
   <https://upgradespecs.radiantblockchain.org/dsproof/>.

2. The sizes of the `FirstSpender` and `DoubleSpender` fields are variable.

3. The value of the  first `list-size` field in the spender record
   (the `Number-of-pushdata's`) is currently fixed to 1, and thus its
   encoding only occupies a single byte.
