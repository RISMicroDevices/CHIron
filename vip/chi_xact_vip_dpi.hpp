#pragma once

/*
 * CHIron VIP — DPI-C C++ Header
 *
 * Exposes RNFJoint, SNFJoint, and RNCacheStateMap to SystemVerilog simulations
 * via the SystemVerilog Direct Programming Interface (DPI-C).
 *
 * All handles are opaque void* (mapped to SystemVerilog chandle).
 * Denial codes are int-sized (0 = XACT_ACCEPTED, non-zero = violation).
 * Cache states are uint8_t bitmasks (bit 0=UC, 1=UCE, 2=UD, 3=UDP, 4=SC, 5=SD, 6=I).
 *
 * Compile-time configuration macros (with defaults):
 *   CHI_NODEID_WIDTH       7
 *   CHI_REQ_ADDR_WIDTH    48
 *   CHI_REQ_RSVDC_WIDTH    0
 *   CHI_DAT_RSVDC_WIDTH    0
 *   CHI_DATA_WIDTH        256
 *   CHI_DATACHECK_PRESENT  false
 *   CHI_POISON_PRESENT     false
 *   CHI_MPAM_PRESENT       false
 */

#include <cstdint>


/*
 * Node type constants (mirror of CHI::Xact::NodeType).
 */
#define CHIRONVIP_NODE_TYPE_RN_F    1
#define CHIRONVIP_NODE_TYPE_RN_D    2
#define CHIRONVIP_NODE_TYPE_RN_I    3
#define CHIRONVIP_NODE_TYPE_HN_F    5
#define CHIRONVIP_NODE_TYPE_HN_I    7
#define CHIRONVIP_NODE_TYPE_SN_F    9
#define CHIRONVIP_NODE_TYPE_SN_I    11
#define CHIRONVIP_NODE_TYPE_MN      12

/*
 * Denial-code sentinel (0 = accepted).
 */
#define CHIRONVIP_ACCEPTED          0

/*
 * CacheState bitmask constants.
 *   bit 0: UC   bit 1: UCE   bit 2: UD   bit 3: UDP
 *   bit 4: SC   bit 5: SD    bit 6: I
 */
#define CHIRONVIP_CACHE_STATE_NONE  0x00
#define CHIRONVIP_CACHE_STATE_UC    0x01
#define CHIRONVIP_CACHE_STATE_UCE   0x02
#define CHIRONVIP_CACHE_STATE_UD    0x04
#define CHIRONVIP_CACHE_STATE_UDP   0x08
#define CHIRONVIP_CACHE_STATE_SC    0x10
#define CHIRONVIP_CACHE_STATE_SD    0x20
#define CHIRONVIP_CACHE_STATE_I     0x40
#define CHIRONVIP_CACHE_STATE_ALL   0x7F


/* =========================================================================
 * Global context (topology + checker settings)
 * ========================================================================= */

/*
 * Create a new Global context.  Must be called before any joint or state-map
 * operation.  Returns an opaque handle; the caller owns it.
 */
extern "C" void* CHIronVIP_CreateGlobal();

/*
 * Destroy a Global context previously returned by CHIronVIP_CreateGlobal.
 */
extern "C" void  CHIronVIP_DestroyGlobal(void* global);

/*
 * Register a node in the topology.
 *   nodeId   — CHI node ID
 *   nodeType — one of the CHIRONVIP_NODE_TYPE_* constants
 *   name     — human-readable label (may be NULL)
 */
extern "C" void  CHIronVIP_Global_AddTopoNode(
    void*           global,
    uint32_t        nodeId,
    uint32_t        nodeType,
    const char*     name);

/*
 * Enable or disable strict flit field-mapping checks.
 * Disable during initial bring-up to avoid false positives from
 * reserved-field mismatches.
 */
extern "C" void  CHIronVIP_Global_SetFieldCheckEnable(
    void*           global,
    int             enable);


/* =========================================================================
 * RNFJoint — Requester Node transaction model
 * ========================================================================= */

extern "C" void* CHIronVIP_CreateRNFJoint();
extern "C" void  CHIronVIP_DestroyRNFJoint(void* joint);

/*
 * Feed a flit observed on the named channel.  Returns an int denial code
 * (0 = ACCEPTED).  flit points to a packed bit-array; flitBits is the number
 * of significant bits (not the full 512 allocated in SV).
 */
extern "C" int CHIronVIP_RNF_NextTXREQ(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits);

extern "C" int CHIronVIP_RNF_NextRXSNP(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits);

extern "C" int CHIronVIP_RNF_NextTXRSP(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits);

extern "C" int CHIronVIP_RNF_NextRXRSP(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits);

extern "C" int CHIronVIP_RNF_NextTXDAT(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits);

extern "C" int CHIronVIP_RNF_NextRXDAT(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits);

/*
 * Return the number of transactions currently in-flight on this joint.
 */
extern "C" int CHIronVIP_RNF_GetInflightCount(
    void*               joint,
    void*               global);


/* =========================================================================
 * SNFJoint — Home / Subordinate Node transaction model
 * ========================================================================= */

extern "C" void* CHIronVIP_CreateSNFJoint();
extern "C" void  CHIronVIP_DestroySNFJoint(void* joint);

extern "C" int CHIronVIP_SNF_NextRXREQ(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits);

extern "C" int CHIronVIP_SNF_NextTXRSP(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits);

extern "C" int CHIronVIP_SNF_NextTXDAT(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits);

extern "C" int CHIronVIP_SNF_NextRXDAT(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits);

/*
 * Return the number of transactions currently in-flight on this joint.
 */
extern "C" int CHIronVIP_SNF_GetInflightCount(
    void*               joint,
    void*               global);


/* =========================================================================
 * RNCacheStateMap — per-address cache-state tracker for RN-F
 * ========================================================================= */

extern "C" void* CHIronVIP_CreateRNCacheStateMap();
extern "C" void  CHIronVIP_DestroyRNCacheStateMap(void* stateMap);

/*
 * Enable / disable silent state transitions.
 * All are disabled by default.
 */
extern "C" void CHIronVIP_StateMap_SetSilentEviction(
    void*   stateMap,
    int     enable);

extern "C" void CHIronVIP_StateMap_SetSilentSharing(
    void*   stateMap,
    int     enable);

extern "C" void CHIronVIP_StateMap_SetSilentStore(
    void*   stateMap,
    int     enable);

extern "C" void CHIronVIP_StateMap_SetSilentInvalidation(
    void*   stateMap,
    int     enable);

/*
 * Read / write the cached state for a 64-byte-aligned address.
 * State encoding: CHIRONVIP_CACHE_STATE_* bitmask.
 * GetState returns CHIRONVIP_CACHE_STATE_I (0x40) when the line is not in the map.
 */
extern "C" int  CHIronVIP_StateMap_GetState(
    void*       stateMap,
    uint64_t    addr);

extern "C" void CHIronVIP_StateMap_SetState(
    void*       stateMap,
    uint64_t    addr,
    int         cacheState);

/*
 * Validate a TXREQ flit against the current cache state.
 * Returns an int denial code (0 = ACCEPTED).
 */
extern "C" int CHIronVIP_StateMap_NextTXREQ(
    void*               stateMap,
    uint64_t            addr,
    const uint32_t*     flit,
    uint32_t            flitBits);

/*
 * Validate an RXSNP flit against the current cache state.
 * Returns an int denial code (0 = ACCEPTED).
 */
extern "C" int CHIronVIP_StateMap_NextRXSNP(
    void*               stateMap,
    uint64_t            addr,
    const uint32_t*     flit,
    uint32_t            flitBits);

/*
 * Apply a final cache-state transition after a transaction completes.
 *   addr       — 64-byte-aligned address of the affected cache line
 *   cacheState — new state as a CHIRONVIP_CACHE_STATE_* bitmask
 * When called without a joint (xaction = NULL or omitted), this is equivalent
 * to CHIronVIP_StateMap_SetState.
 * Always returns CHIRONVIP_ACCEPTED.
 */
extern "C" int CHIronVIP_StateMap_Transfer(
    void*       stateMap,
    uint64_t    addr,
    int         cacheState);


/* =========================================================================
 * Utility
 * ========================================================================= */

/*
 * Return a static string describing the denial code.
 * The returned pointer is valid for the lifetime of the process.
 * Returns "XACT_ACCEPTED" for code 0, "XACT_UNKNOWN" for unrecognised codes.
 */
extern "C" const char* CHIronVIP_GetDenialName(int denialCode);
