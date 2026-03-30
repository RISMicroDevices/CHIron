# Transaction Layer

The transaction layer is the highest-level abstraction in CHIron. It groups raw protocol flits into coherent, named **transactions**, tracks cache-coherence state, and reports specification violations through a structured event-driven system.

---

## Transaction Types

CHIron models every CHI transaction category defined in Issue E. Each type is represented by an `XactionType` enum value and a concrete implementation class in `chi/xact/chi_xactions/`.

```mermaid
graph TD
    subgraph RN_INITIATED["RN-Initiated Transactions"]
        AR["AllocatingRead\ne.g. ReadUnique, ReadShared, ReadClean"]
        NAR["NonAllocatingRead\ne.g. ReadOnce, ReadNoSnp"]
        IW["ImmediateWrite\ne.g. WriteUniqueFull, WriteNoSnpFull"]
        WZ["WriteZero\ne.g. WriteUniqueZero"]
        CBW["CopyBackWrite\ne.g. WriteBackFull, WriteCleanFull"]
        AT["Atomic\ne.g. AtomicLoad, AtomicSwap, AtomicCompare"]
        IS["IndependentStash\ne.g. StashOnceUnique, StashOnceSepUnique"]
        DL["Dataless\ne.g. MakeInvalid, CleanShared, CleanInvalid"]
    end

    subgraph HN_MANAGED["HN-Managed / Home Transactions"]
        HR["HomeRead\n(home-side read service)"]
        HW["HomeWrite\n(home-side write service)"]
        HWZ["HomeWriteZero"]
        HD["HomeDataless"]
        HA["HomeAtomic"]
        HS["HomeSnoop\ne.g. SnpOnce, SnpUnique, SnpClean"]
        FS["ForwardSnoop\ne.g. SnpUniqueFwd, SnpSharedFwd"]
        DVM["DVMSnoop\n(DVM operations)"]
    end
```

### Transaction Type Table

| XactionType | Initiator | Data Transfer | Allocates |
|-------------|-----------|---------------|-----------|
| `AllocatingRead` | RN | Home → RN | ✔ |
| `NonAllocatingRead` | RN | Home → RN | ✘ |
| `ImmediateWrite` | RN | RN → Home | ✘ cache |
| `WriteZero` | RN | None (implicit zero) | ✘ |
| `CopyBackWrite` | RN | RN → Home | ✘ |
| `Atomic` | RN | Bidirectional | Optional |
| `IndependentStash` | RN | RN → Home (hint) | Optional |
| `Dataless` | RN | None | ✘ |
| `HomeRead` | HN | Home → SN | Internal |
| `HomeWrite` | HN | SN → Home | Internal |
| `HomeWriteZero` | HN | None | Internal |
| `HomeDataless` | HN | None | Internal |
| `HomeAtomic` | HN | Bidirectional | Internal |
| `HomeSnoop` | HN | Optional | N/A |
| `ForwardSnoop` | HN | RN-to-RN | N/A |
| `DVMSnoop` | MN/HN | None | N/A |

---

## The Xaction Class

Every in-flight CHI transaction is modelled by a `Xaction<config>` object (in `chi/xact/chi_xactions/chi_xactions_base.hpp`).

```mermaid
classDiagram
    class Xaction~config~ {
        +XactionType type
        +FiredRequestFlit first
        +XactDenialEnum firstDenial
        +vector~FiredResponseFlit~ subsequence
        +vector~SubsequenceKey~ subsequenceKeys
        +bool resent
        +shared_ptr~Xaction~ resentXaction
        +bool retried
        +shared_ptr~Xaction~ retriedXaction
        +Companion companion
        ──────────────────────────
        +GetType() XactionType
        +GetFirstFlit() REQ
        +GetSubsequence() vector
        +IsResent() bool
        +IsRetried() bool
        +IsComplete() bool
    }

    class SubsequenceKey {
        +XactDenialEnum denial
        +bool isRSP
        +opcode : RSPOpcode or DATOpcode
        +bool hasDBID
        ──────────────────────────
        +IsRSP() bool
        +IsDAT() bool
        +IsAccepted() bool
        +IsDenied() bool
    }

    class FiredRequestFlit~config~ {
        +uint64_t time
        +REQ~config~ flit
    }

    class FiredResponseFlit~config~ {
        +uint64_t time
        +bool isRSP
        +RSP or DAT flit
    }

    class Companion {
        +void* userdata
        +Attach(key, value)
        +Get(key) value
    }

    Xaction~config~ "1" --> "*" SubsequenceKey
    Xaction~config~ "1" --> "1" FiredRequestFlit~config~
    Xaction~config~ "1" --> "*" FiredResponseFlit~config~
    Xaction~config~ "1" --> "1" Companion
    Xaction~config~ --> Xaction~config~ : resentXaction / retriedXaction
```

### Transaction Lifecycle

```mermaid
stateDiagram-v2
    [*] --> Fired : REQ flit observed (NextTXREQ / NextRXREQ)
    Fired --> Accepted : valid request
    Fired --> Denied : protocol violation
    Accepted --> WaitingRetry : AllowRetry=1 and PCrdReturn received
    WaitingRetry --> Resent : RetryAck + PCrdGrant; new REQ flit
    Resent --> InProgress : retry accepted
    Accepted --> InProgress : normal flow
    InProgress --> Complete : CompAck / final RSP or DAT
    InProgress --> Denied : unexpected or invalid response
    Complete --> [*]
    Denied --> [*]
```

---

## Cache State Tracking

`RNCacheStateMap<config>` maintains the per-address cache-line state for every Requester Node visible to the `Joint`.

### Cache States

CHIron encodes the seven CHI cache states as a compact bitset (7 bits, one per state):

```mermaid
stateDiagram-v2
    direction LR
    I : I (Invalid)
    SC : SC (Shared Clean)
    SD : SD (Shared Dirty)
    UC : UC (Unique Clean)
    UCE : UCE (Unique Clean Empty)
    UD : UD (Unique Dirty)
    UDP : UDP (Unique Dirty Partial)

    I --> SC : AllocatingRead → SharedClean
    I --> UC : AllocatingRead → UniqueClean
    I --> UD : AllocatingRead → UniqueDirty
    SC --> I : Evict / CleanInvalid / SnpInvalidate
    SC --> UC : ReadUnique upgrade
    UC --> UD : Local write
    UD --> SC : WriteBack → SharedClean
    UD --> I : WriteBack + evict
    SD --> I : WriteBack
```

### CacheState and CacheResp classes

```mermaid
classDiagram
    class CacheState {
        +UC  : 1 bit
        +UCE : 1 bit
        +UD  : 1 bit
        +UDP : 1 bit
        +SC  : 1 bit
        +SD  : 1 bit
        +I   : 1 bit
        ──────────────
        +operator|()  CacheResp
        +ToString()   string
        +FromSnpResp(Resp) CacheState
    }

    class CacheResp {
        +UC  : 1 bit
        +UCE : 1 bit
        +UD  : 1 bit
        +UDP : 1 bit
        +SC  : 1 bit
        +SD  : 1 bit
        +I   : 1 bit
        +PD  : 1 bit (PassDirty)
    }

    CacheState --> CacheResp : operator|
```

---

## Compile-Time State Transition Tables

State transitions are computed by `consteval` functions during compilation and stored in read-only tables. There are five major table groups, one per transaction category:

```mermaid
graph TD
    subgraph tables["chi/xact/chi_xact_state/"]
        READ["cst_xact_read.hpp\nReadNoSnp, ReadOnce,\nReadClean, ReadShared,\nReadUnique, …"]
        WRITE["cst_xact_write.hpp\nWriteUniqueFull,\nWriteBackFull,\nWriteCleanFull, …"]
        ATOM["cst_xact_atomic.hpp\nAtomicLoad, AtomicStore,\nAtomicSwap, AtomicCompare"]
        DLESS["cst_xact_dataless.hpp\nMakeInvalid,\nCleanShared, CleanInvalid"]
        SNOOP["cst_xact_snoop.hpp\n28 snoop types including\nSnpOnce, SnpUnique,\nSnpClean, SnpShared, …"]
        INTERM["cst_consteval_intermediates.hpp\nIntermediate state groups\n(DCT, nested transactions)"]
        RESP["cst_consteval_responses.hpp\nResponse state computation"]
    end

    INTERM --> READ & WRITE & ATOM & DLESS & SNOOP
    RESP --> READ & WRITE & ATOM & DLESS & SNOOP
```

Transition tables are indexed by `(initial_cache_state, opcode)` and return the set of `(valid_response, next_cache_state)` pairs. Invalid transitions are empty sets, detected as protocol violations at runtime.

---

## The Joint Coordinator

`Joint<config>` (in `chi/xact/chi_joint.hpp`) is the top-level system object that ingests individual flits, matches them to transactions, validates sequences, and fires events.

```mermaid
graph TD
    subgraph inputs["Flit Ingestion Methods"]
        TXREQ["NextTXREQ(glbl, time, reqFlit)"]
        RXREQ["NextRXREQ(glbl, time, reqFlit)"]
        TXSNP["NextTXSNP(glbl, time, snpFlit, tgtId)"]
        RXSNP["NextRXSNP(glbl, time, snpFlit, tgtId)"]
        TXRSP["NextTXRSP(glbl, time, rspFlit)"]
        RXRSP["NextRXRSP(glbl, time, rspFlit)"]
        TXDAT["NextTXDAT(glbl, time, datFlit)"]
        RXDAT["NextRXDAT(glbl, time, datFlit)"]
    end

    JOINT["Joint&lt;config&gt;\n─────────────────────\nTransaction map\nCache state map\nTopology / SAM\nEvent buses"]

    TXREQ & RXREQ & TXSNP & RXSNP --> JOINT
    TXRSP & RXRSP & TXDAT & RXDAT --> JOINT

    subgraph events["Published Events"]
        ACC["OnAccepted"]
        RET["OnRetry"]
        TIDALLOC["OnTxnIDAllocation"]
        TIDFREE["OnTxnIDFree"]
        DBIDALLOC["OnDBIDAllocation"]
        DBIDFREE["OnDBIDFree"]
        COMP["OnComplete"]
        DREQ["OnDeniedRequest"]
        DRESP["OnDeniedResponse"]
    end

    JOINT --> ACC & RET & TIDALLOC & TIDFREE
    JOINT --> DBIDALLOC & DBIDFREE & COMP
    JOINT --> DREQ & DRESP
```

### Joint Return Values

Every `Next*` method returns a `XactDenialEnum`. A value of `XactDenial::None` means the flit was accepted; any other value names the specific specification rule that was violated.

---

## Event-Driven Architecture

CHIron uses a fork of the [Gravity EventBus](../common/eventbus.hpp) that allows hybrid event inheritance. Each event bus is strongly typed — subscribers receive concrete event objects with full context.

```mermaid
classDiagram
    class JointDenialEventBase~config~ {
        +Joint~config~& joint
        +shared_ptr~Xaction~ xaction
        +XactDenialEnum denial
        +JointDenialSource source
        ──────────────────────
        +GetJoint()
        +GetXaction()
        +GetDenial()
    }

    class JointDeniedRequestEvent~config~ {
        +FiredRequestFlit~config~& firedFlit
    }

    class JointDeniedResponseEvent~config~ {
        +FiredResponseFlit~config~& firedFlit
    }

    class JointXactionEventBase~config~ {
        +Joint~config~& joint
        +shared_ptr~Xaction~ xaction
    }

    class JointXactionAcceptedEvent~config~
    class JointXactionRetryEvent~config~
    class JointXactionCompleteEvent~config~
    class JointXactionTxnIDAllocationEvent~config~
    class JointXactionTxnIDFreeEvent~config~
    class JointXactionDBIDAllocationEvent~config~
    class JointXactionDBIDFreeEvent~config~

    JointDenialEventBase~config~ <|-- JointDeniedRequestEvent~config~
    JointDenialEventBase~config~ <|-- JointDeniedResponseEvent~config~
    JointXactionEventBase~config~ <|-- JointXactionAcceptedEvent~config~
    JointXactionEventBase~config~ <|-- JointXactionRetryEvent~config~
    JointXactionEventBase~config~ <|-- JointXactionCompleteEvent~config~
    JointXactionEventBase~config~ <|-- JointXactionTxnIDAllocationEvent~config~
    JointXactionEventBase~config~ <|-- JointXactionTxnIDFreeEvent~config~
    JointXactionEventBase~config~ <|-- JointXactionDBIDAllocationEvent~config~
    JointXactionEventBase~config~ <|-- JointXactionDBIDFreeEvent~config~
```

### Subscribing to Events

```cpp
joint.OnDeniedRequest += [](JointDeniedRequestEvent<cfg>& ev) {
    std::cerr << "Denied: " << ev.GetDenial()->name
              << " on TxnID=" << ev.GetFiredFlit().flit.GetTxnID()
              << "\n";
};

joint.OnComplete += [](JointXactionCompleteEvent<cfg>& ev) {
    // ev.GetXaction() gives the completed Xaction object
};
```

---

## Topology and System Address Map

`GlobalTopology` and `SAM` (System Address Map) give `Joint` the information it needs to validate which node may send which request to which home.

```mermaid
graph TD
    subgraph TOPO["GlobalTopology\n(chi/xact/chi_xact_base/chi_xact_base_topology.hpp)"]
        NODES["Node Registry\nNodeID → NodeType\n(RN-F, HN-F, SN-F, …)"]
        SCOPE["XactScope\nRequester · Home · Subordinate"]
        CH["ChannelType\nREQ · RSP · DAT · SNP"]
    end

    subgraph SAM["System Address Map\n(chi/xact/chi_xact_base/chi_xact_base_sam.hpp)"]
        SAMSCOPE["SAM Scopes\nAddress ranges → HomeNode"]
    end

    TOPO --> JOINT["Joint&lt;config&gt;"]
    SAM  --> JOINT
```

---

## Transaction State Test Suite

`test/tc_chi_xact_state/` validates every cell of the state-transition tables. Tests are grouped into parts:

```mermaid
graph LR
    A["TCPt A\nInitial Reads\n(ReadNoSnp … ReadUnique)"]
    B["TCPt B\nInitial Dataless\n(MakeInvalid, CleanShared…)"]
    C["TCPt C\nInitial Writes"]
    D["TCPt D\nInitial Atomics"]
    E["TCPt E\nRead Responses\n(CompData variants)"]
    F["TCPt F\nDataless Responses"]
    G["TCPt G\nWrite Responses"]
    H["TCPt H\nAtomic Responses"]
    I["TCPt I\nSnoop Responses\n(28 snoop types)"]
    J["TCPt J\nForward Snoop Responses"]

    A & B & C & D --> INIT["Initial state\nvalidation"]
    E & F & G & H & I & J --> RESP["Response state\nvalidation"]
```

---

## Related Guides

| Guide | Description |
|-------|-------------|
| [Architecture Overview](architecture-overview.md) | Repository layout, layer model, design principles |
| [CHI Protocol Stack](chi-protocol-stack.md) | Nodes, channels, flits, interfaces, configuration parameters |
| [CLog & Tools](clog-and-tools.md) | Transaction log formats and CLI analysis tools |
