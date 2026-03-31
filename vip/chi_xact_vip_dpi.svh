/*
 * CHIron VIP — SystemVerilog DPI-C Import Declarations
 *
 * Include this file in every SV module or package that uses CHIron:
 *
 *   `include "chi_xact_vip_dpi.svh"
 *
 * All DPI-C functions correspond to the C++ API in chi_xact_vip_dpi.hpp.
 * Handles (opaque pointers) are represented as SystemVerilog chandle.
 * Flit payloads are passed as bit[511:0]; only the lowest flitBits bits
 * are significant.
 *
 * Denial codes are int (0 = accepted, non-zero = protocol violation).
 */


/* -------------------------------------------------------------------------
 * Node type constants
 * ---------------------------------------------------------------------- */
`define CHIRONVIP_NODE_TYPE_RN_F    1
`define CHIRONVIP_NODE_TYPE_RN_D    2
`define CHIRONVIP_NODE_TYPE_RN_I    3
`define CHIRONVIP_NODE_TYPE_HN_F    5
`define CHIRONVIP_NODE_TYPE_HN_I    7
`define CHIRONVIP_NODE_TYPE_SN_F    9
`define CHIRONVIP_NODE_TYPE_SN_I    11
`define CHIRONVIP_NODE_TYPE_MN      12


/* -------------------------------------------------------------------------
 * Denial code sentinel
 * ---------------------------------------------------------------------- */
`define CHIRONVIP_ACCEPTED          0


/* -------------------------------------------------------------------------
 * CacheState bitmask constants
 *   bit 0: UC   bit 1: UCE   bit 2: UD   bit 3: UDP
 *   bit 4: SC   bit 5: SD    bit 6: I
 * ---------------------------------------------------------------------- */
`define CHIRONVIP_CACHE_STATE_NONE  8'h00
`define CHIRONVIP_CACHE_STATE_UC    8'h01
`define CHIRONVIP_CACHE_STATE_UCE   8'h02
`define CHIRONVIP_CACHE_STATE_UD    8'h04
`define CHIRONVIP_CACHE_STATE_UDP   8'h08
`define CHIRONVIP_CACHE_STATE_SC    8'h10
`define CHIRONVIP_CACHE_STATE_SD    8'h20
`define CHIRONVIP_CACHE_STATE_I     8'h40
`define CHIRONVIP_CACHE_STATE_ALL   8'h7F


/* -------------------------------------------------------------------------
 * Flit bit-width constants for the default configuration
 * (CHI Issue E.b, 7-bit NodeID, 48-bit addr, 256-bit data, no extras)
 *
 * These match the REQ_t::WIDTH / SNP_t::WIDTH / RSP_t::WIDTH / DAT_t::WIDTH
 * values compiled into chi_xact_vip_dpi.so.  Override if you built the
 * library with non-default macros.
 * ---------------------------------------------------------------------- */
`ifndef CHIRONVIP_REQ_FLIT_BITS
`define CHIRONVIP_REQ_FLIT_BITS     135
`endif

`ifndef CHIRONVIP_SNP_FLIT_BITS
`define CHIRONVIP_SNP_FLIT_BITS     96
`endif

`ifndef CHIRONVIP_RSP_FLIT_BITS
`define CHIRONVIP_RSP_FLIT_BITS     65
`endif

`ifndef CHIRONVIP_DAT_FLIT_BITS
`define CHIRONVIP_DAT_FLIT_BITS     370
`endif


/* =========================================================================
 * Global context
 * ========================================================================= */

import "DPI-C" function chandle CHIronVIP_CreateGlobal ();

import "DPI-C" function void CHIronVIP_DestroyGlobal (
    input chandle   global
);

import "DPI-C" function void CHIronVIP_Global_AddTopoNode (
    input chandle   global,
    input int       nodeId,
    input int       nodeType,
    input string    name
);

import "DPI-C" function void CHIronVIP_Global_SetFieldCheckEnable (
    input chandle   global,
    input int       enable
);


/* =========================================================================
 * RNFJoint — Requester Node transaction model
 * ========================================================================= */

import "DPI-C" function chandle CHIronVIP_CreateRNFJoint ();

import "DPI-C" function void CHIronVIP_DestroyRNFJoint (
    input chandle   joint
);

import "DPI-C" function int CHIronVIP_RNF_NextTXREQ (
    input  chandle      joint,
    input  chandle      global,
    input  longint      time,
    input  bit [511:0]  flit,
    input  int          flitBits
);

import "DPI-C" function int CHIronVIP_RNF_NextRXSNP (
    input  chandle      joint,
    input  chandle      global,
    input  longint      time,
    input  bit [511:0]  flit,
    input  int          flitBits
);

import "DPI-C" function int CHIronVIP_RNF_NextTXRSP (
    input  chandle      joint,
    input  chandle      global,
    input  longint      time,
    input  bit [511:0]  flit,
    input  int          flitBits
);

import "DPI-C" function int CHIronVIP_RNF_NextRXRSP (
    input  chandle      joint,
    input  chandle      global,
    input  longint      time,
    input  bit [511:0]  flit,
    input  int          flitBits
);

import "DPI-C" function int CHIronVIP_RNF_NextTXDAT (
    input  chandle      joint,
    input  chandle      global,
    input  longint      time,
    input  bit [511:0]  flit,
    input  int          flitBits
);

import "DPI-C" function int CHIronVIP_RNF_NextRXDAT (
    input  chandle      joint,
    input  chandle      global,
    input  longint      time,
    input  bit [511:0]  flit,
    input  int          flitBits
);

import "DPI-C" function int CHIronVIP_RNF_GetInflightCount (
    input  chandle      joint,
    input  chandle      global
);


/* =========================================================================
 * SNFJoint — Home / Subordinate Node transaction model
 * ========================================================================= */

import "DPI-C" function chandle CHIronVIP_CreateSNFJoint ();

import "DPI-C" function void CHIronVIP_DestroySNFJoint (
    input chandle   joint
);

import "DPI-C" function int CHIronVIP_SNF_NextRXREQ (
    input  chandle      joint,
    input  chandle      global,
    input  longint      time,
    input  bit [511:0]  flit,
    input  int          flitBits
);

import "DPI-C" function int CHIronVIP_SNF_NextTXRSP (
    input  chandle      joint,
    input  chandle      global,
    input  longint      time,
    input  bit [511:0]  flit,
    input  int          flitBits
);

import "DPI-C" function int CHIronVIP_SNF_NextTXDAT (
    input  chandle      joint,
    input  chandle      global,
    input  longint      time,
    input  bit [511:0]  flit,
    input  int          flitBits
);

import "DPI-C" function int CHIronVIP_SNF_NextRXDAT (
    input  chandle      joint,
    input  chandle      global,
    input  longint      time,
    input  bit [511:0]  flit,
    input  int          flitBits
);

import "DPI-C" function int CHIronVIP_SNF_GetInflightCount (
    input  chandle      joint,
    input  chandle      global
);


/* =========================================================================
 * RNCacheStateMap — per-address cache state tracker for RN-F
 * ========================================================================= */

import "DPI-C" function chandle CHIronVIP_CreateRNCacheStateMap ();

import "DPI-C" function void CHIronVIP_DestroyRNCacheStateMap (
    input chandle   stateMap
);

import "DPI-C" function void CHIronVIP_StateMap_SetSilentEviction (
    input chandle   stateMap,
    input int       enable
);

import "DPI-C" function void CHIronVIP_StateMap_SetSilentSharing (
    input chandle   stateMap,
    input int       enable
);

import "DPI-C" function void CHIronVIP_StateMap_SetSilentStore (
    input chandle   stateMap,
    input int       enable
);

import "DPI-C" function void CHIronVIP_StateMap_SetSilentInvalidation (
    input chandle   stateMap,
    input int       enable
);

/*
 * Returns the current cache state as a CHIRONVIP_CACHE_STATE_* bitmask.
 * Returns CHIRONVIP_CACHE_STATE_I (0x40) when the address is not in the map.
 */
import "DPI-C" function int CHIronVIP_StateMap_GetState (
    input chandle   stateMap,
    input longint   addr
);

import "DPI-C" function void CHIronVIP_StateMap_SetState (
    input chandle   stateMap,
    input longint   addr,
    input int       cacheState
);

import "DPI-C" function int CHIronVIP_StateMap_NextTXREQ (
    input  chandle      stateMap,
    input  longint      addr,
    input  bit [511:0]  flit,
    input  int          flitBits
);

import "DPI-C" function int CHIronVIP_StateMap_NextRXSNP (
    input  chandle      stateMap,
    input  longint      addr,
    input  bit [511:0]  flit,
    input  int          flitBits
);

/*
 * Apply a final cache-state transition for a completed transaction.
 * Equivalent to SetState when called without a xaction reference.
 * Returns CHIRONVIP_ACCEPTED (0) always.
 */
import "DPI-C" function int CHIronVIP_StateMap_Transfer (
    input chandle   stateMap,
    input longint   addr,
    input int       cacheState
);


/* =========================================================================
 * Utility
 * ========================================================================= */

/*
 * Returns a human-readable name for a denial code.
 * The returned string is valid for the lifetime of the simulation.
 */
import "DPI-C" function string CHIronVIP_GetDenialName (
    input int       denialCode
);
