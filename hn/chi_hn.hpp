#pragma once

#ifndef CHI_HN__STANDALONE
#   include "chi_hn_header.hpp"
#endif

#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__HN_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__HN_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__HN_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__HN_EB
#endif


/*
    CHI Home Node (HN-F) — basic functional implementation.

    This module implements the CHI HN-F protocol role:
      - Receives REQ from RN  (RXREQ)
      - Sends SNP to RN       (TXSNP)  — coherency snoops
      - Sends RSP to RN       (TXRSP)  — Comp, CompDBIDResp, RetryAck, PCrdGrant
      - Sends DAT to RN       (TXDAT)  — CompData, DataSepResp
      - Receives RSP from RN  (RXRSP)  — CompAck, SnpResp, SnpRespFwded
      - Receives DAT from RN  (RXDAT)  — CopyBackWrData, NCBWrData, SnpRespData
      - Sends REQ to SN       (TXREQ)  — ReadNoSnp, WriteNoSnpFull/Ptl
      - Receives RSP from SN  (RXRSP)  — DBIDResp, Comp, CompDBIDResp, ReadReceipt
      - Receives DAT from SN  (RXDAT)  — CompData, DataSepResp

    Transaction subsequences implemented:
      ReadShared/ReadClean/ReadNotSharedDirty
        → snoop peer RN(s) when line is UC/UD/SD
        → fetch from SN when line is I or clean snoops produced no data
        → CompData[SC] to requester, update directory

      ReadUnique/MakeReadUnique
        → invalidate all sharers/owner via SnpUnique or SnpMakeInvalid
        → fetch from SN if still needed
        → CompData[UC] to requester, update directory

      ReadOnce/ReadNoSnp  (non-allocating)
        → ReadNoSnp to SN, forward CompData to requester (no directory update)

      WriteBackFull/WriteCleanFull/WriteEvictFull  (copyback writes)
        → CompDBIDResp[I] to RN, wait for CopyBackWrData
        → WriteNoSnpFull to SN for the dirty data, wait for Comp
        → Update directory (line → I)

      WriteNoSnpFull/WriteNoSnpPtl  (immediate writes, no cached copy)
        → CompDBIDResp to RN, wait for NonCopyBackWrData
        → WriteNoSnpFull to SN, wait for Comp

      WriteUniqueFull/WriteUniquePtl  (unique writes from coherent RN)
        → Invalidate any existing owner via SnpMakeInvalid (if line cached elsewhere)
        → CompDBIDResp to RN, wait for NonCopyBackWrData
        → WriteNoSnpFull to SN, wait for Comp
        → Update directory (line → I, data stored at SN)

      Evict  (SC line being dropped)
        → Comp[I] to RN immediately, remove from sharers

      CleanUnique/MakeUnique  (dataless upgrade to exclusive)
        → SnpMakeInvalid to all other holders, wait for SnpResp[I]
        → Comp[UC] to requester, update directory

      CleanShared  (clean back to SC from UD/SD)
        → SnpCleanShared to owner if UD/SD, wait for SnpResp
        → Comp to requester, update directory

      CleanInvalid/MakeInvalid  (dataless invalidate)
        → SnpCleanInvalid/SnpMakeInvalid to all holders, wait for SnpResp[I]
        → Comp[I] to requester, update directory → I

    Link interface:
        Input  : NextREQ(req), NextRSP(rsp), NextDAT(dat)
        Output : txSNP(snp, tgtId), txRSP(rsp), txDAT(dat), txREQ(req)
*/

namespace HN {

    // -------------------------------------------------------------------------
    // Cache directory state per cache line (as seen by HN-F)
    // -------------------------------------------------------------------------
    enum class CacheState {
        I,      // Invalid — no RN has a copy
        SC,     // Shared Clean — one or more RNs have SC; SN is up-to-date
        SD,     // Shared Dirty — sharers present but one carries dirty data
        UC,     // Unique Clean — one RN has UC; SN is up-to-date
        UD,     // Unique Dirty — one RN has UD; SN may be stale
    };


    // Per-cache-line entry in the HN directory
    template<FlitConfigurationConcept config>
    struct CacheLineEntry {
        using nodeid_t = typename Flits::REQ<config>::srcid_t;

        CacheState                  state   = CacheState::I;
        nodeid_t                    owner   = 0;    // valid for UC/UD/SD
        std::set<nodeid_t>          sharers;        // valid for SC/SD (all, including SD owner)
    };


    // -------------------------------------------------------------------------
    // In-flight transaction phase
    // -------------------------------------------------------------------------
    enum class TxnPhase {
        PendingSnoops,          // waiting for snoop responses (SnpResp / SnpRespData)
        PendingSNReadResp,      // waiting for CompData (or DataSepResp) from SN
        PendingWriteDataFromRN, // waiting for CopyBackWrData / NCBWrData from RN
        PendingSNWriteDBID,     // waiting for DBIDResp from SN (to then send write data)
        PendingSNWriteComp,     // waiting for Comp from SN after sending write data
        PendingCompAck,         // waiting for CompAck from RN (read with ExpCompAck=1)
        Complete,
    };


    // -------------------------------------------------------------------------
    // Per-transaction context
    // -------------------------------------------------------------------------
    template<FlitConfigurationConcept config>
    struct PendingTxn {
        using nodeid_t      = typename Flits::REQ<config>::srcid_t;
        using txnid_t       = typename Flits::REQ<config>::txnid_t;
        using addr_t        = typename Flits::REQ<config>::addr_t;
        using addr_value_t  = typename Flits::REQ<config>::addr_t::value_type;
        using opcode_t      = typename Flits::REQ<config>::opcode_t;
        using ssize_t       = typename Flits::REQ<config>::ssize_t;
        using resp_t        = typename Flits::RSP<config>::resp_t;
        using dbid_t        = typename Flits::RSP<config>::dbid_t;

        // ── Original RN request ──────────────────────────────────────────────
        nodeid_t    requesterID;
        txnid_t     requesterTxnID;
        addr_t      addr;
        opcode_t    opcode;
        ssize_t     size;
        bool        expCompAck;     // RN set ExpCompAck in the REQ

        // ── HN-allocated transaction state ───────────────────────────────────
        txnid_t     hnTxnID;        // used as: SNP TxnID, SN REQ TxnID, DBID for RN
        TxnPhase    phase;

        // ── Framework transaction tracker (RN's perspective) ─────────────────
        // Created by RNFJoint::NextTXREQ() when the RN issues the REQ.
        // Records the full protocol subsequence for protocol-level analysis.
        std::shared_ptr<Xact::Xaction<config>> xaction;

        // ── Snoop tracking ───────────────────────────────────────────────────
        int         pendingSnoopCount;      // number of outstanding snoop responses
        bool        gotDataFromSnoop;       // at least one SnpRespData received
        bool        dirtyDataFromSnoop;     // the snoop data was dirty (PassDirty set)

        // ── Data buffer ──────────────────────────────────────────────────────
        // Stores a copy of the most recent DAT flit received (from snoop or SN read).
        // Used when forwarding data to the final destination.
        Flits::DAT<config>  cachedDat;
        bool                dataValid;      // cachedDat contains valid data

        // ── SN write tracking ────────────────────────────────────────────────
        nodeid_t    snNodeID;           // SN to write dirty data to
        dbid_t      dbidFromSN;         // DBID received from SN (for write data)
        bool        needWriteToSN;      // must write data to SN (dirty data pending)
    };


    // -------------------------------------------------------------------------
    // HN-F Node
    // -------------------------------------------------------------------------
    template<FlitConfigurationConcept config>
    class Node {
    public:
        using nodeid_t      = typename Flits::REQ<config>::srcid_t;
        using txnid_t       = typename Flits::REQ<config>::txnid_t;
        using addr_t        = typename Flits::REQ<config>::addr_t;
        using addr_value_t  = typename Flits::REQ<config>::addr_t::value_type;
        using opcode_t      = typename Flits::REQ<config>::opcode_t;
        using snpopcode_t   = typename Flits::SNP<config>::opcode_t;
        using resp_t        = typename Flits::RSP<config>::resp_t;
        using dbid_t        = typename Flits::RSP<config>::dbid_t;
        using txn_t         = PendingTxn<config>;
        using dir_t         = CacheLineEntry<config>;

        // ── Configuration ────────────────────────────────────────────────────
        nodeid_t                                nodeID;
        std::function<nodeid_t(addr_t)>         getSNNodeID;    // map address → SN NodeID

        // ── CHI link output interface ─────────────────────────────────────────
        // Register callbacks to receive flits the HN transmits.
        std::function<void(const Flits::SNP<config>&, nodeid_t tgtId)>  txSNP;
        std::function<void(const Flits::RSP<config>&)>                  txRSP;
        std::function<void(const Flits::DAT<config>&)>                  txDAT;
        std::function<void(const Flits::REQ<config>&)>                  txREQ;

        // ── Framework global context ──────────────────────────────────────────
        // Topology / SAM-scope / field-check configuration shared across all
        // Xaction and RNCacheStateMap operations.
        Xact::Global<config>                    global;

    private:
        // ── Cache directory ──────────────────────────────────────────────────
        std::unordered_map<uint64_t, dir_t>     directory;

        // ── Per-RN framework trackers ─────────────────────────────────────────
        // Each connected RN gets its own Joint (creates/tracks Xaction objects
        // from the RN's protocol perspective) and its own RNCacheStateMap (tracks
        // what state that RN holds per cache-line address).
        std::unordered_map<uint64_t, Xact::RNFJoint<config>>        rnJoints;
        std::unordered_map<uint64_t, Xact::RNCacheStateMap<config>>  rnStateMaps;

        // ── Transaction table ─────────────────────────────────────────────────
        // Keyed by hnTxnID, which equals:
        //   • SNP.TxnID        (for outgoing snoops)
        //   • SN REQ.TxnID     (for outgoing SN reads/writes)
        //   • DBID sent to RN  (for CompDBIDResp/CompData)
        // This lets every inbound response be looked up by flit.TxnID() directly.
        std::unordered_map<uint64_t, txn_t>     transactions;

        // Reverse map: {srcID, txnID} → hnTxnID for the initial RN REQ.
        // Used to detect duplicate requests and clean up on completion.
        std::unordered_map<uint64_t, txnid_t>   rxReqMap;

        txnid_t     nextTxnID;

    public:
        explicit Node(nodeid_t id,
                      std::function<nodeid_t(addr_t)> snSelector = nullptr) noexcept;

        // ── CHI link input interface ──────────────────────────────────────────
        void NextREQ(const Flits::REQ<config>& req);
        void NextRSP(const Flits::RSP<config>& rsp);
        void NextDAT(const Flits::DAT<config>& dat);

    private:
        // ── TxnID allocation ─────────────────────────────────────────────────
        txnid_t AllocateTxnID() noexcept;

        // ── Directory helpers ─────────────────────────────────────────────────
        dir_t&  GetOrCreateEntry(addr_t addr) noexcept;
        dir_t*  FindEntry(addr_t addr) noexcept;
        void    RemoveEntry(addr_t addr) noexcept;

        // ── Reverse-map key helper ─────────────────────────────────────────────
        // Shift by the full TxnID field width to avoid collisions.
        static uint64_t RxReqKey(nodeid_t src, txnid_t txn) noexcept
        {
            return (static_cast<uint64_t>(src) << Flits::REQ<config>::TXNID_WIDTH)
                 | static_cast<uint64_t>(txn);
        }

        // ── Address conversion helper ─────────────────────────────────────────
        // Convert addr_t to the plain integer type used by RNCacheStateMap.
        static addr_value_t AddrValue(addr_t addr) noexcept
        { return static_cast<addr_value_t>(addr); }

        // ── Per-RN framework tracker accessors ────────────────────────────────
        Xact::RNFJoint<config>&         RNJoint    (nodeid_t rn) noexcept;
        Xact::RNCacheStateMap<config>&  RNStateMap (nodeid_t rn) noexcept;

        // ── Transaction lifecycle ─────────────────────────────────────────────
        txn_t&  CreateTransaction(const Flits::REQ<config>& req, txnid_t hnTxn);
        void    CompleteTransaction(txnid_t hnTxn) noexcept;

        // Called after each state change; decides the next action.
        void    AdvanceTransaction(txnid_t hnTxn);

        // ── REQ handling ──────────────────────────────────────────────────────
        void HandleRead       (const Flits::REQ<config>& req);
        void HandleWrite      (const Flits::REQ<config>& req);
        void HandleDataless   (const Flits::REQ<config>& req);

        // ── RSP handling ──────────────────────────────────────────────────────
        void HandleSnpResp    (const Flits::RSP<config>& rsp);
        void HandleCompAck    (const Flits::RSP<config>& rsp);
        void HandleSNDBIDResp (const Flits::RSP<config>& rsp);
        void HandleSNComp     (const Flits::RSP<config>& rsp);

        // ── DAT handling ──────────────────────────────────────────────────────
        void HandleSnpRespData  (const Flits::DAT<config>& dat);
        void HandleRNWriteData  (const Flits::DAT<config>& dat);
        void HandleSNCompData   (const Flits::DAT<config>& dat);

        // ── Data and response helpers ─────────────────────────────────────────
        // Copy data array from one DAT flit into another.
        static void CopyDatData(Flits::DAT<config>& dst, const Flits::DAT<config>& src) noexcept
        {
            constexpr size_t words = sizeof(typename Flits::DAT<config>::data_t) / sizeof(uint64_t);
            for (size_t i = 0; i < words; ++i)
                dst.Data()[i] = src.Data()[i];
        }

        // Returns true when the Resp field has the PassDirty bit set.
        static bool HasPassDirty(resp_t resp) noexcept
        { return (resp & Resps::PassDirty) != 0; }

        // Returns the Resp field stripped of the PassDirty bit.
        static resp_t RespWithoutPD(resp_t resp) noexcept
        {
            return static_cast<resp_t>(
                static_cast<uint64_t>(resp) & ~static_cast<uint64_t>(Resps::PassDirty));
        }


        // ── Protocol message generation ───────────────────────────────────────
        void IssueSnoop    (txn_t& txn, nodeid_t tgt, snpopcode_t op, bool retToSrc);
        void IssueSNRead   (txn_t& txn);
        void IssueSNWrite  (txn_t& txn);

        void SendComp         (txn_t& txn, resp_t resp);
        void SendCompDBIDResp (txn_t& txn, resp_t resp);
        void SendCompData     (txn_t& txn, resp_t resp);
        void SendRetryAck     (txn_t& txn);

        // ── Snoop opcode selection ─────────────────────────────────────────────
        // Returns the snoop opcode appropriate for downgrading / invalidating
        // a line for a given incoming REQ opcode, plus whether data is needed.
        struct SnoopParams { snpopcode_t opcode; bool retToSrc; };
        SnoopParams SnoopForRead    (opcode_t req, CacheState curState) const noexcept;
        SnoopParams SnoopForDataless(opcode_t req) const noexcept;
    };


    // =========================================================================
    // Implementation
    // =========================================================================

    template<FlitConfigurationConcept config>
    inline Node<config>::Node(nodeid_t id,
                               std::function<nodeid_t(addr_t)> snSelector) noexcept
        : nodeID    (id)
        , getSNNodeID(snSelector ? std::move(snSelector)
                                 : [](addr_t) -> nodeid_t { return 0; })
        , nextTxnID (0)
    {}


    // ── TxnID allocation ────────────────────────────────────────────────────

    template<FlitConfigurationConcept config>
    inline typename Node<config>::txnid_t
    Node<config>::AllocateTxnID() noexcept
    {
        // Wrap around within the TxnID field width
        constexpr uint64_t maxID = (uint64_t(1) << Flits::REQ<config>::TXNID_WIDTH) - 1;
        txnid_t id = nextTxnID;
        nextTxnID  = static_cast<txnid_t>((static_cast<uint64_t>(nextTxnID) + 1) & maxID);
        return id;
    }


    // ── Directory helpers ───────────────────────────────────────────────────

    template<FlitConfigurationConcept config>
    inline typename Node<config>::dir_t&
    Node<config>::GetOrCreateEntry(addr_t addr) noexcept
    {
        return directory[static_cast<uint64_t>(addr)];
    }

    template<FlitConfigurationConcept config>
    inline typename Node<config>::dir_t*
    Node<config>::FindEntry(addr_t addr) noexcept
    {
        auto it = directory.find(static_cast<uint64_t>(addr));
        return it != directory.end() ? &it->second : nullptr;
    }

    template<FlitConfigurationConcept config>
    inline void Node<config>::RemoveEntry(addr_t addr) noexcept
    {
        auto* e = FindEntry(addr);
        if (e && e->state == CacheState::I)
            directory.erase(static_cast<uint64_t>(addr));
    }


    // ── Per-RN tracker accessors ────────────────────────────────────────────

    template<FlitConfigurationConcept config>
    inline Xact::RNFJoint<config>&
    Node<config>::RNJoint(nodeid_t rn) noexcept
    {
        return rnJoints[static_cast<uint64_t>(rn)];
    }

    template<FlitConfigurationConcept config>
    inline Xact::RNCacheStateMap<config>&
    Node<config>::RNStateMap(nodeid_t rn) noexcept
    {
        return rnStateMaps[static_cast<uint64_t>(rn)];
    }


    // ── Transaction lifecycle ───────────────────────────────────────────────

    template<FlitConfigurationConcept config>
    inline typename Node<config>::txn_t&
    Node<config>::CreateTransaction(const Flits::REQ<config>& req, txnid_t hnTxn)
    {
        txn_t txn{};
        txn.requesterID     = req.SrcID();
        txn.requesterTxnID  = req.TxnID();
        txn.addr            = req.Addr();
        txn.opcode          = req.Opcode();
        txn.size            = req.Size();
        txn.expCompAck      = (req.ExpCompAck() != 0);
        txn.hnTxnID         = hnTxn;
        txn.phase           = TxnPhase::PendingSnoops; // default, overridden below
        txn.pendingSnoopCount = 0;
        txn.gotDataFromSnoop  = false;
        txn.dirtyDataFromSnoop = false;
        txn.dataValid          = false;
        txn.needWriteToSN      = false;
        txn.snNodeID           = getSNNodeID(req.Addr());
        txn.dbidFromSN         = 0;

        // Register with the per-RN framework trackers.
        // RNFJoint creates the right Xaction subtype (AllocatingRead, CopyBackWrite,
        // ImmediateWrite, Dataless, …) keyed by the RN's {SrcID, TxnID}.
        // RNCacheStateMap validates and records the initial cache-line state for
        // protocol-level state-machine tracking.
        RNJoint   (req.SrcID()).NextTXREQ(global, 0, req, &txn.xaction);
        RNStateMap(req.SrcID()).NextTXREQ(AddrValue(req.Addr()), req);

        return transactions[static_cast<uint64_t>(hnTxn)] = txn;
    }

    template<FlitConfigurationConcept config>
    inline void Node<config>::CompleteTransaction(txnid_t hnTxn) noexcept
    {
        auto it = transactions.find(static_cast<uint64_t>(hnTxn));
        if (it == transactions.end()) return;

        // Clean up rxReqMap
        auto& txn = it->second;
        rxReqMap.erase(RxReqKey(txn.requesterID, txn.requesterTxnID));
        transactions.erase(it);
    }


    // ── CHI link input interface ─────────────────────────────────────────────

    template<FlitConfigurationConcept config>
    inline void Node<config>::NextREQ(const Flits::REQ<config>& req)
    {
        namespace Op = Opcodes::REQ;

        // Reject duplicate in-flight requests from the same RN
        uint64_t rkey = RxReqKey(req.SrcID(), req.TxnID());
        if (rxReqMap.count(rkey))
            return; // duplicate — silently ignore (production code would assert)

        switch (req.Opcode())
        {
            // ── Reads ──────────────────────────────────────────────────────
            case Op::ReadShared:
            case Op::ReadClean:
            case Op::ReadNotSharedDirty:
            case Op::ReadPreferUnique:
            case Op::ReadUnique:
            case Op::MakeReadUnique:
            case Op::ReadOnce:
            case Op::ReadOnceCleanInvalid:
            case Op::ReadOnceMakeInvalid:
            case Op::ReadNoSnp:
            case Op::ReadNoSnpSep:
                HandleRead(req);
                break;

            // ── Copyback writes ────────────────────────────────────────────
            case Op::WriteBackFull:
            case Op::WriteBackPtl:
            case Op::WriteCleanFull:
            case Op::WriteEvictFull:
            case Op::WriteEvictOrEvict:
            // ── Immediate writes ───────────────────────────────────────────
            case Op::WriteNoSnpFull:
            case Op::WriteNoSnpPtl:
            case Op::WriteNoSnpZero:
            case Op::WriteUniqueFull:
            case Op::WriteUniquePtl:
            case Op::WriteUniqueZero:
            case Op::WriteUniqueFullStash:
            case Op::WriteUniquePtlStash:
                HandleWrite(req);
                break;

            // ── Dataless ───────────────────────────────────────────────────
            case Op::Evict:
            case Op::CleanUnique:
            case Op::MakeUnique:
            case Op::CleanShared:
            case Op::CleanSharedPersist:
            case Op::CleanSharedPersistSep:
            case Op::CleanInvalid:
            case Op::MakeInvalid:
                HandleDataless(req);
                break;

            default:
                break; // unrecognised or unsupported opcode
        }
    }

    template<FlitConfigurationConcept config>
    inline void Node<config>::NextRSP(const Flits::RSP<config>& rsp)
    {
        namespace Op = Opcodes::RSP;

        switch (rsp.Opcode())
        {
            case Op::SnpResp:
            case Op::SnpRespFwded:
                HandleSnpResp(rsp);
                break;

            case Op::CompAck:
                HandleCompAck(rsp);
                break;

            case Op::DBIDResp:
                HandleSNDBIDResp(rsp);
                break;

            case Op::CompDBIDResp:
                // SN replied with Comp+DBID at once: treat as DBIDResp then Comp
                HandleSNDBIDResp(rsp);
                // HandleSNComp will be called from AdvanceTransaction
                break;

            case Op::Comp:
                HandleSNComp(rsp);
                break;

            default:
                break;
        }
    }

    template<FlitConfigurationConcept config>
    inline void Node<config>::NextDAT(const Flits::DAT<config>& dat)
    {
        namespace Op = Opcodes::DAT;

        switch (dat.Opcode())
        {
            case Op::SnpRespData:
            case Op::SnpRespDataPtl:
            case Op::SnpRespDataFwded:
                HandleSnpRespData(dat);
                break;

            case Op::CopyBackWrData:
            case Op::NonCopyBackWrData:
            case Op::NCBWrDataCompAck:
                HandleRNWriteData(dat);
                break;

            case Op::CompData:
            case Op::DataSepResp:
                HandleSNCompData(dat);
                break;

            default:
                break;
        }
    }


    // =========================================================================
    // Request handlers
    // =========================================================================

    template<FlitConfigurationConcept config>
    inline void Node<config>::HandleRead(const Flits::REQ<config>& req)
    {
        namespace Op = Opcodes::REQ;

        txnid_t hnTxn = AllocateTxnID();
        rxReqMap[RxReqKey(req.SrcID(), req.TxnID())] = hnTxn;
        txn_t& txn = CreateTransaction(req, hnTxn);

        dir_t& entry = GetOrCreateEntry(req.Addr());

        bool isAllocating  = (req.Opcode() == Op::ReadShared
                           || req.Opcode() == Op::ReadClean
                           || req.Opcode() == Op::ReadNotSharedDirty
                           || req.Opcode() == Op::ReadPreferUnique
                           || req.Opcode() == Op::ReadUnique
                           || req.Opcode() == Op::MakeReadUnique);

        bool wantsUnique   = (req.Opcode() == Op::ReadUnique
                           || req.Opcode() == Op::MakeReadUnique);

        bool isNonAlloc    = !isAllocating;  // ReadOnce, ReadNoSnp, etc.

        if (isNonAlloc)
        {
            // Non-allocating reads: always go to SN, no directory update
            IssueSNRead(txn);
            txn.phase = TxnPhase::PendingSNReadResp;
            return;
        }

        // ── Coherent (allocating) reads ───────────────────────────────────────

        bool hasPeers = false;

        if (wantsUnique)
        {
            // Invalidate all holders except the requester
            for (nodeid_t sh : entry.sharers)
            {
                if (sh == req.SrcID()) continue;
                auto sp = SnoopForRead(req.Opcode(), entry.state);
                IssueSnoop(txn, sh, sp.opcode, sp.retToSrc);
                hasPeers = true;
            }
            if (entry.state == CacheState::UC || entry.state == CacheState::UD)
            {
                if (entry.owner != req.SrcID())
                {
                    auto sp = SnoopForRead(req.Opcode(), entry.state);
                    IssueSnoop(txn, entry.owner, sp.opcode, sp.retToSrc);
                    hasPeers = true;
                }
            }
        }
        else
        {
            // ReadShared / ReadClean / ReadNotSharedDirty:
            // Only need to snoop if a unique/dirty copy exists at another RN
            if ((entry.state == CacheState::UC || entry.state == CacheState::UD)
                && entry.owner != req.SrcID())
            {
                auto sp = SnoopForRead(req.Opcode(), entry.state);
                IssueSnoop(txn, entry.owner, sp.opcode, sp.retToSrc);
                hasPeers = true;
            }
            else if (entry.state == CacheState::SD)
            {
                // Snoop the SD owner to downgrade
                auto sp = SnoopForRead(req.Opcode(), entry.state);
                IssueSnoop(txn, entry.owner, sp.opcode, sp.retToSrc);
                hasPeers = true;
            }
        }

        if (hasPeers)
        {
            txn.phase = TxnPhase::PendingSnoops;
        }
        else
        {
            // No coherent peers — go directly to SN (or use existing SC data)
            if (entry.state == CacheState::I
             || entry.state == CacheState::SC)
            {
                IssueSNRead(txn);
                txn.phase = TxnPhase::PendingSNReadResp;
            }
            else
            {
                // Line is cached at the requester itself (requester has UC/UD)
                // Just update the directory and respond
                AdvanceTransaction(hnTxn);
            }
        }
    }


    template<FlitConfigurationConcept config>
    inline void Node<config>::HandleWrite(const Flits::REQ<config>& req)
    {
        namespace Op = Opcodes::REQ;

        txnid_t hnTxn = AllocateTxnID();
        rxReqMap[RxReqKey(req.SrcID(), req.TxnID())] = hnTxn;
        txn_t& txn = CreateTransaction(req, hnTxn);

        // For WriteZero variants: no data comes from RN, we may still need SN write
        bool isZeroWrite = (req.Opcode() == Op::WriteNoSnpZero
                         || req.Opcode() == Op::WriteUniqueZero);

        // Immediate writes (WriteUnique*): may need to invalidate existing holder
        bool isUniqueWrite = (req.Opcode() == Op::WriteUniqueFull
                           || req.Opcode() == Op::WriteUniquePtl
                           || req.Opcode() == Op::WriteUniqueFullStash
                           || req.Opcode() == Op::WriteUniquePtlStash
                           || req.Opcode() == Op::WriteUniqueZero);

        dir_t& entry = GetOrCreateEntry(req.Addr());

        // Invalidate any existing holder(s) before writing
        bool hasPeers = false;
        if (isUniqueWrite)
        {
            for (nodeid_t sh : entry.sharers)
            {
                if (sh == req.SrcID()) continue;
                IssueSnoop(txn, sh, Opcodes::SNP::SnpMakeInvalid, /*retToSrc=*/false);
                hasPeers = true;
            }
            if ((entry.state == CacheState::UC || entry.state == CacheState::UD)
                && entry.owner != req.SrcID())
            {
                IssueSnoop(txn, entry.owner, Opcodes::SNP::SnpMakeInvalid, /*retToSrc=*/false);
                hasPeers = true;
            }
        }

        txn.needWriteToSN = !isZeroWrite; // WriteZero sends zeroes directly to SN later

        if (hasPeers)
        {
            // Wait for invalidation snoops to complete before sending CompDBIDResp
            txn.phase = TxnPhase::PendingSnoops;
        }
        else
        {
            if (isZeroWrite)
            {
                // No RN data needed: write zeros directly to SN
                IssueSNWrite(txn);
                txn.needWriteToSN = false;
                txn.phase = TxnPhase::PendingSNWriteDBID;
            }
            else
            {
                // Request data from RN
                SendCompDBIDResp(txn, Resps::Comp::I);
                txn.phase = TxnPhase::PendingWriteDataFromRN;
            }
        }
    }


    template<FlitConfigurationConcept config>
    inline void Node<config>::HandleDataless(const Flits::REQ<config>& req)
    {
        namespace Op = Opcodes::REQ;

        txnid_t hnTxn = AllocateTxnID();
        rxReqMap[RxReqKey(req.SrcID(), req.TxnID())] = hnTxn;
        txn_t& txn = CreateTransaction(req, hnTxn);

        dir_t& entry = GetOrCreateEntry(req.Addr());

        if (req.Opcode() == Op::Evict)
        {
            // SC line being dropped — just update dir and respond
            entry.sharers.erase(req.SrcID());
            if (entry.sharers.empty())
                entry.state = CacheState::I;
            SendComp(txn, Resps::Comp::I);
            CompleteTransaction(hnTxn);
            RemoveEntry(req.Addr());
            return;
        }

        // Determine snoop type and who to snoop
        auto sp = SnoopForDataless(req.Opcode());

        bool hasPeers = false;

        if (req.Opcode() == Op::CleanUnique || req.Opcode() == Op::MakeUnique)
        {
            // Requester wants exclusive — invalidate all other holders
            for (nodeid_t sh : entry.sharers)
            {
                if (sh == req.SrcID()) continue;
                IssueSnoop(txn, sh, sp.opcode, sp.retToSrc);
                hasPeers = true;
            }
            if ((entry.state == CacheState::UC || entry.state == CacheState::UD)
                && entry.owner != req.SrcID())
            {
                IssueSnoop(txn, entry.owner, sp.opcode, sp.retToSrc);
                hasPeers = true;
            }
        }
        else
        {
            // CleanInvalid / MakeInvalid / CleanShared(Persist) — all holders
            for (nodeid_t sh : entry.sharers)
            {
                IssueSnoop(txn, sh, sp.opcode, sp.retToSrc);
                hasPeers = true;
            }
            if (entry.state == CacheState::UC || entry.state == CacheState::UD)
            {
                IssueSnoop(txn, entry.owner, sp.opcode, sp.retToSrc);
                hasPeers = true;
            }
        }

        if (hasPeers)
        {
            txn.phase = TxnPhase::PendingSnoops;
        }
        else
        {
            // No peers: complete immediately
            AdvanceTransaction(hnTxn);
        }
    }


    // =========================================================================
    // RSP handlers
    // =========================================================================

    template<FlitConfigurationConcept config>
    inline void Node<config>::HandleSnpResp(const Flits::RSP<config>& rsp)
    {
        // rsp.TxnID() == HN's SNP TxnID == hnTxnID
        auto it = transactions.find(static_cast<uint64_t>(rsp.TxnID()));
        if (it == transactions.end()) return;
        txn_t& txn = it->second;

        // Notify the snooped RN's joint and state map.
        // RNFJoint::NextTXRSP looks up the snoop xaction by TxnID and returns it;
        // RNCacheStateMap uses that xaction to apply the correct state transition.
        {
            std::shared_ptr<Xact::Xaction<config>> snpXact;
            RNJoint(rsp.SrcID()).NextTXRSP(global, 0, rsp, &snpXact);
            if (snpXact)
                RNStateMap(rsp.SrcID()).NextTXRSP(AddrValue(txn.addr), *snpXact, rsp);
        }

        // Update snoop filter for the responding RN
        dir_t& entry = GetOrCreateEntry(txn.addr);
        nodeid_t responder = rsp.SrcID();
        entry.sharers.erase(responder);
        if (entry.owner == responder)
        {
            entry.owner = 0;
            entry.state = CacheState::I;
        }

        --txn.pendingSnoopCount;
        if (txn.pendingSnoopCount <= 0)
            AdvanceTransaction(txn.hnTxnID);
    }


    template<FlitConfigurationConcept config>
    inline void Node<config>::HandleCompAck(const Flits::RSP<config>& rsp)
    {
        // rsp.TxnID() == DBID that HN sent == hnTxnID
        auto it = transactions.find(static_cast<uint64_t>(rsp.TxnID()));
        if (it == transactions.end()) return;

        // Notify the requester RN's joint — CompAck closes the TxnID slot.
        RNJoint(rsp.SrcID()).NextTXRSP(global, 0, rsp);

        CompleteTransaction(it->second.hnTxnID);
    }


    template<FlitConfigurationConcept config>
    inline void Node<config>::HandleSNDBIDResp(const Flits::RSP<config>& rsp)
    {
        // rsp.TxnID() == HN's outgoing REQ TxnID == hnTxnID
        auto it = transactions.find(static_cast<uint64_t>(rsp.TxnID()));
        if (it == transactions.end()) return;
        txn_t& txn = it->second;

        txn.dbidFromSN = rsp.DBID();

        // Send the write data to SN now that we have the DBID
        Flits::DAT<config> dat{};
        dat.Opcode()    = Opcodes::DAT::NonCopyBackWrData;
        dat.SrcID()     = nodeID;
        dat.TgtID()     = txn.snNodeID;
        dat.TxnID()     = txn.dbidFromSN;
        if (txn.dataValid)
            CopyDatData(dat, txn.cachedDat);
        // Data() is zero-initialised for WriteZero
        if (txDAT) txDAT(dat);

        txn.phase = TxnPhase::PendingSNWriteComp;

        // If SN sent CompDBIDResp, Comp is implied — complete now
        if (rsp.Opcode() == Opcodes::RSP::CompDBIDResp)
            HandleSNComp(rsp);
    }


    template<FlitConfigurationConcept config>
    inline void Node<config>::HandleSNComp(const Flits::RSP<config>& rsp)
    {
        auto it = transactions.find(static_cast<uint64_t>(rsp.TxnID()));
        if (it == transactions.end()) return;
        txn_t& txn = it->second;

        // Update directory: line is now clean at SN
        dir_t& entry = GetOrCreateEntry(txn.addr);
        entry.state   = CacheState::I;
        entry.owner   = 0;
        entry.sharers.clear();

        CompleteTransaction(txn.hnTxnID);
        RemoveEntry(txn.addr);
    }


    // =========================================================================
    // DAT handlers
    // =========================================================================

    template<FlitConfigurationConcept config>
    inline void Node<config>::HandleSnpRespData(const Flits::DAT<config>& dat)
    {
        // dat.TxnID() == HN's SNP TxnID == hnTxnID
        auto it = transactions.find(static_cast<uint64_t>(dat.TxnID()));
        if (it == transactions.end()) return;
        txn_t& txn = it->second;

        // Capture the data returned by the snooped RN
        txn.cachedDat         = dat;
        txn.dataValid         = true;
        txn.gotDataFromSnoop  = true;

        // Check PassDirty bit
        if (HasPassDirty(dat.Resp()))
        {
            txn.dirtyDataFromSnoop = true;
            txn.needWriteToSN      = true; // must write dirty data to SN
        }

        // Notify the snooped RN's joint and state map.
        {
            std::shared_ptr<Xact::Xaction<config>> snpXact;
            RNJoint(dat.SrcID()).NextTXDAT(global, 0, dat, &snpXact);
            if (snpXact)
                RNStateMap(dat.SrcID()).NextTXDAT(AddrValue(txn.addr), *snpXact, dat);
        }

        // Update directory for the responding RN
        dir_t& entry = GetOrCreateEntry(txn.addr);
        nodeid_t responder = dat.SrcID();
        entry.sharers.erase(responder);
        if (entry.owner == responder)
        {
            entry.owner = 0;
            entry.state = CacheState::I;
        }

        --txn.pendingSnoopCount;
        if (txn.pendingSnoopCount <= 0)
            AdvanceTransaction(txn.hnTxnID);
    }


    template<FlitConfigurationConcept config>
    inline void Node<config>::HandleRNWriteData(const Flits::DAT<config>& dat)
    {
        // dat.TxnID() == DBID sent to RN in CompDBIDResp == hnTxnID
        auto it = transactions.find(static_cast<uint64_t>(dat.TxnID()));
        if (it == transactions.end()) return;
        txn_t& txn = it->second;

        txn.cachedDat = dat;
        txn.dataValid = true;

        // If NCBWrDataCompAck, the RN has also acknowledged Comp — no separate CompAck
        if (dat.Opcode() == Opcodes::DAT::NCBWrDataCompAck)
            txn.expCompAck = false;

        // Notify the requester RN's joint and state map.
        // The requester's xaction carries the original REQ opcode context.
        {
            std::shared_ptr<Xact::Xaction<config>> reqXact;
            RNJoint(txn.requesterID).NextTXDAT(global, 0, dat, &reqXact);
            if (reqXact)
                RNStateMap(txn.requesterID).NextTXDAT(AddrValue(txn.addr), *reqXact, dat);
        }

        // Update directory: RN no longer holds the line (write-back / invalidation)
        dir_t& entry = GetOrCreateEntry(txn.addr);
        entry.sharers.erase(txn.requesterID);
        if (entry.owner == txn.requesterID)
        {
            entry.owner = 0;
            entry.state = CacheState::I;
        }

        if (txn.needWriteToSN)
        {
            // Forward data to SN
            IssueSNWrite(txn);
            txn.phase = TxnPhase::PendingSNWriteDBID;
        }
        else
        {
            // Write not needed (e.g. WriteEvict of clean line) — done
            RemoveEntry(txn.addr);
            CompleteTransaction(txn.hnTxnID);
        }
    }


    template<FlitConfigurationConcept config>
    inline void Node<config>::HandleSNCompData(const Flits::DAT<config>& dat)
    {
        // dat.TxnID() == HN's REQ TxnID == hnTxnID
        auto it = transactions.find(static_cast<uint64_t>(dat.TxnID()));
        if (it == transactions.end()) return;
        txn_t& txn = it->second;

        txn.cachedDat = dat;
        txn.dataValid = true;

        AdvanceTransaction(txn.hnTxnID);
    }


    // =========================================================================
    // Protocol message generation
    // =========================================================================

    template<FlitConfigurationConcept config>
    inline void Node<config>::IssueSnoop(txn_t& txn, nodeid_t tgt,
                                          snpopcode_t op, bool retToSrc)
    {
        Flits::SNP<config> snp{};
        snp.Opcode()    = op;
        snp.SrcID()     = nodeID;
        snp.TxnID()     = txn.hnTxnID;
        // SNP address = physical address with lower 3 bits stripped
        snp.Addr()      = static_cast<typename Flits::SNP<config>::addr_t>(txn.addr >> 3);
        snp.RetToSrc()  = retToSrc ? 1 : 0;
        snp.FwdNID()    = txn.requesterID;

        ++txn.pendingSnoopCount;

        // Register the outgoing snoop with the target RN's joint and state map.
        // RNFJoint::NextRXSNP creates the snoop Xaction (XactionHomeSnoop or
        // XactionForwardSnoop) that will be matched when the SnpResp/SnpRespData
        // arrives later via HandleSnpResp / HandleSnpRespData.
        RNJoint   (tgt).NextRXSNP(global, 0, snp, tgt);
        RNStateMap(tgt).NextRXSNP(AddrValue(txn.addr), snp);

        if (txSNP) txSNP(snp, tgt);
    }


    template<FlitConfigurationConcept config>
    inline void Node<config>::IssueSNRead(txn_t& txn)
    {
        Flits::REQ<config> req{};
        req.Opcode()     = Opcodes::REQ::ReadNoSnp;
        req.SrcID()      = nodeID;
        req.TgtID()      = txn.snNodeID;
        req.TxnID()      = txn.hnTxnID;
        req.Addr()       = txn.addr;
        req.Size()       = txn.size;
        req.AllowRetry() = 1;

        if (txREQ) txREQ(req);
    }


    template<FlitConfigurationConcept config>
    inline void Node<config>::IssueSNWrite(txn_t& txn)
    {
        Flits::REQ<config> req{};
        req.Opcode()     = Opcodes::REQ::WriteNoSnpFull;
        req.SrcID()      = nodeID;
        req.TgtID()      = txn.snNodeID;
        req.TxnID()      = txn.hnTxnID;
        req.Addr()       = txn.addr;
        req.Size()       = txn.size;
        req.AllowRetry() = 1;

        if (txREQ) txREQ(req);
    }


    template<FlitConfigurationConcept config>
    inline void Node<config>::SendComp(txn_t& txn, resp_t resp)
    {
        Flits::RSP<config> rsp{};
        rsp.Opcode() = Opcodes::RSP::Comp;
        rsp.SrcID()  = nodeID;
        rsp.TgtID()  = txn.requesterID;
        rsp.TxnID()  = txn.requesterTxnID;
        rsp.Resp()   = resp;

        if (txRSP) txRSP(rsp);

        // Notify the requester RN's joint and state map that it received a Comp.
        RNJoint(txn.requesterID).NextRXRSP(global, 0, rsp);
        if (txn.xaction)
            RNStateMap(txn.requesterID).NextRXRSP(AddrValue(txn.addr), *txn.xaction, rsp);
    }


    template<FlitConfigurationConcept config>
    inline void Node<config>::SendCompDBIDResp(txn_t& txn, resp_t resp)
    {
        Flits::RSP<config> rsp{};
        rsp.Opcode() = Opcodes::RSP::CompDBIDResp;
        rsp.SrcID()  = nodeID;
        rsp.TgtID()  = txn.requesterID;
        rsp.TxnID()  = txn.requesterTxnID;
        rsp.DBID()   = txn.hnTxnID;   // RN will use this as TxnID in write data / CompAck
        rsp.Resp()   = resp;

        if (txRSP) txRSP(rsp);

        // Notify the requester RN's joint and state map that it received a CompDBIDResp.
        RNJoint(txn.requesterID).NextRXRSP(global, 0, rsp);
        if (txn.xaction)
            RNStateMap(txn.requesterID).NextRXRSP(AddrValue(txn.addr), *txn.xaction, rsp);
    }


    template<FlitConfigurationConcept config>
    inline void Node<config>::SendCompData(txn_t& txn, resp_t resp)
    {
        Flits::DAT<config> dat{};
        dat.Opcode()  = Opcodes::DAT::CompData;
        dat.SrcID()   = nodeID;
        dat.TgtID()   = txn.requesterID;
        dat.TxnID()   = txn.requesterTxnID;
        dat.Resp()    = resp;
        dat.HomeNID() = nodeID;

        // Include a DBID if the response grants write permission (UC/UD)
        resp_t respBase = RespWithoutPD(resp);
        bool grantWrite = (respBase == Resps::UC || respBase == Resps::UD);
        if (grantWrite)
            dat.DBID() = txn.hnTxnID;

        // Copy data payload if available
        if (txn.dataValid)
            CopyDatData(dat, txn.cachedDat);

        if (txDAT) txDAT(dat);

        // Notify the requester RN's joint and state map that it received CompData.
        RNJoint(txn.requesterID).NextRXDAT(global, 0, dat);
        if (txn.xaction)
            RNStateMap(txn.requesterID).NextRXDAT(AddrValue(txn.addr), *txn.xaction, dat);
    }


    // =========================================================================
    // Snoop opcode selection
    // =========================================================================

    template<FlitConfigurationConcept config>
    inline typename Node<config>::SnoopParams
    Node<config>::SnoopForRead(opcode_t req, CacheState curState) const noexcept
    {
        namespace Op = Opcodes::REQ;
        namespace SOp = Opcodes::SNP;

        switch (req)
        {
            case Op::ReadShared:
            case Op::ReadClean:
            case Op::ReadNotSharedDirty:
            case Op::ReadPreferUnique:
                // Downgrade unique owner to shared; get data if dirty
                return { SOp::SnpShared, /*retToSrc=*/true };

            case Op::ReadUnique:
            case Op::MakeReadUnique:
                // Invalidate all holders; may need data from dirty owner
                if (curState == CacheState::UD || curState == CacheState::SD)
                    return { SOp::SnpUnique, /*retToSrc=*/true };
                return { SOp::SnpMakeInvalid, /*retToSrc=*/false };

            default:
                return { SOp::SnpShared, /*retToSrc=*/true };
        }
    }

    template<FlitConfigurationConcept config>
    inline typename Node<config>::SnoopParams
    Node<config>::SnoopForDataless(opcode_t req) const noexcept
    {
        namespace Op = Opcodes::REQ;
        namespace SOp = Opcodes::SNP;

        switch (req)
        {
            case Op::CleanUnique:
            case Op::MakeUnique:
                return { SOp::SnpMakeInvalid, /*retToSrc=*/false };

            case Op::CleanShared:
            case Op::CleanSharedPersist:
            case Op::CleanSharedPersistSep:
                return { SOp::SnpCleanShared, /*retToSrc=*/false };

            case Op::CleanInvalid:
                return { SOp::SnpCleanInvalid, /*retToSrc=*/false };

            case Op::MakeInvalid:
                return { SOp::SnpMakeInvalid, /*retToSrc=*/false };

            default:
                return { SOp::SnpMakeInvalid, /*retToSrc=*/false };
        }
    }


    // =========================================================================
    // AdvanceTransaction — state-machine driver
    // =========================================================================

    template<FlitConfigurationConcept config>
    inline void Node<config>::AdvanceTransaction(txnid_t hnTxn)
    {
        namespace Op = Opcodes::REQ;

        auto it = transactions.find(static_cast<uint64_t>(hnTxn));
        if (it == transactions.end()) return;
        txn_t& txn = it->second;

        dir_t& entry = GetOrCreateEntry(txn.addr);

        // ── Reads ──────────────────────────────────────────────────────────────
        bool isReadAlloc  = (txn.opcode == Op::ReadShared
                          || txn.opcode == Op::ReadClean
                          || txn.opcode == Op::ReadNotSharedDirty
                          || txn.opcode == Op::ReadPreferUnique);
        bool isReadUnique = (txn.opcode == Op::ReadUnique
                          || txn.opcode == Op::MakeReadUnique);
        bool isNonAlloc   = (txn.opcode == Op::ReadOnce
                          || txn.opcode == Op::ReadOnceCleanInvalid
                          || txn.opcode == Op::ReadOnceMakeInvalid
                          || txn.opcode == Op::ReadNoSnp
                          || txn.opcode == Op::ReadNoSnpSep);
        bool isRead       = isReadAlloc || isReadUnique || isNonAlloc;

        bool isDataless   = (txn.opcode == Op::CleanUnique
                          || txn.opcode == Op::MakeUnique
                          || txn.opcode == Op::CleanShared
                          || txn.opcode == Op::CleanSharedPersist
                          || txn.opcode == Op::CleanSharedPersistSep
                          || txn.opcode == Op::CleanInvalid
                          || txn.opcode == Op::MakeInvalid);

        bool isWrite      = !isRead && !isDataless;

        // After all snoops complete: decide what to do next
        if (txn.phase == TxnPhase::PendingSnoops)
        {
            if (isWrite)
            {
                // All invalidation snoops done — now accept write data from RN
                SendCompDBIDResp(txn, Resps::Comp::I);
                txn.phase = TxnPhase::PendingWriteDataFromRN;
                return;
            }

            if (isDataless)
            {
                // All snoops done — respond to requester
                resp_t resp = Resps::Comp::I;
                if (txn.opcode == Op::CleanUnique || txn.opcode == Op::MakeUnique)
                    resp = Resps::Comp::UC;

                // Update directory
                if (txn.opcode == Op::CleanUnique || txn.opcode == Op::MakeUnique)
                {
                    entry.state   = CacheState::UC;
                    entry.owner   = txn.requesterID;
                    entry.sharers.clear();
                    entry.sharers.insert(txn.requesterID);
                }
                else
                {
                    entry.state = CacheState::I;
                    entry.owner = 0;
                    entry.sharers.clear();
                }

                SendComp(txn, resp);
                CompleteTransaction(hnTxn);
                RemoveEntry(txn.addr);
                return;
            }

            // Read: snoops done — check if we got data
            if (txn.dataValid)
            {
                // Got data from snoop — skip SN read
                // (Fall through to data-available path below)
            }
            else if (entry.state == CacheState::I
                  || entry.state == CacheState::SC)
            {
                // Need to fetch from SN
                IssueSNRead(txn);
                txn.phase = TxnPhase::PendingSNReadResp;
                return;
            }
        }

        // ── Data available: send response to RN ───────────────────────────────
        if (isRead && txn.dataValid)
        {
            resp_t resp = Resps::Comp::SC; // default: shared clean

            if (isNonAlloc)
            {
                resp = Resps::Comp::UC; // ReadOnce always returns UC to requester
            }
            else if (isReadUnique)
            {
                resp = Resps::Comp::UC;
                // Update directory: requester has unique clean copy
                entry.state  = CacheState::UC;
                entry.owner  = txn.requesterID;
                entry.sharers.clear();
            }
            else // isReadAlloc (ReadShared etc.)
            {
                resp = Resps::Comp::SC;
                // Update directory: add requester as sharer
                if (entry.state == CacheState::I)
                {
                    entry.state = CacheState::SC;
                }
                else if (entry.state == CacheState::UC)
                {
                    // Owner downgraded by snoop to SC
                    entry.sharers.insert(entry.owner);
                    entry.state = CacheState::SC;
                    entry.owner = 0;
                }
                entry.sharers.insert(txn.requesterID);
            }

            // If dirty data came from snoop, write to SN before responding (or after)
            if (txn.needWriteToSN)
            {
                IssueSNWrite(txn);
                // We can still respond to RN with data now (data already captured)
                // The SN write happens concurrently; we don't wait for it here.
            }

            bool grantWrite = isReadUnique;
            SendCompData(txn, resp);

            if (!isNonAlloc && !isReadUnique)
                RemoveEntry(txn.addr); // clean SC state - could be compacted
            // (leave entry for isReadUnique so directory reflects UC state)

            if (txn.expCompAck)
            {
                txn.phase = TxnPhase::PendingCompAck;
            }
            else if (!txn.needWriteToSN)
            {
                CompleteTransaction(hnTxn);
            }
            // else: leave alive until SN write completes (HandleSNComp)
        }
    }

} // namespace HN


#endif // CHI issue guards
