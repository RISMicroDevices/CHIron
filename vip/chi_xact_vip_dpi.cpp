/*
 * CHIron VIP — DPI-C C++ Implementation
 *
 * Wraps CHI::Eb::Xact::RNFJoint, SNFJoint, and RNCacheStateMap for use from
 * SystemVerilog via DPI-C.
 *
 * Compile with C++20 (or later) and link into a shared library:
 *
 *   g++ -std=c++20 -O2 -shared -fPIC            \
 *       -I<CHIRON_ROOT>                          \
 *       -I<SIMULATOR_DPI_INCLUDE>               \
 *       -DCHI_NODEID_WIDTH=7                    \
 *       -DCHI_REQ_ADDR_WIDTH=48                 \
 *       -DCHI_DATA_WIDTH=256                     \
 *       -o libchironvip.so                       \
 *       chi_xact_vip_dpi.cpp
 */

#include "chi_xact_vip_dpi.hpp"

#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>


/* -------------------------------------------------------------------------
 * Compile-time flit configuration (overridable via -D macros)
 * ---------------------------------------------------------------------- */

#ifndef CHI_NODEID_WIDTH
#   define CHI_NODEID_WIDTH         7
#endif

#ifndef CHI_REQ_ADDR_WIDTH
#   define CHI_REQ_ADDR_WIDTH       48
#endif

#ifndef CHI_REQ_RSVDC_WIDTH
#   define CHI_REQ_RSVDC_WIDTH      0
#endif

#ifndef CHI_DAT_RSVDC_WIDTH
#   define CHI_DAT_RSVDC_WIDTH      0
#endif

#ifndef CHI_DATA_WIDTH
#   define CHI_DATA_WIDTH           256
#endif

#ifndef CHI_DATACHECK_PRESENT
#   define CHI_DATACHECK_PRESENT    false
#endif

#ifndef CHI_POISON_PRESENT
#   define CHI_POISON_PRESENT       false
#endif

#ifndef CHI_MPAM_PRESENT
#   define CHI_MPAM_PRESENT         false
#endif


/* -------------------------------------------------------------------------
 * CHI Issue E.b includes
 * ---------------------------------------------------------------------- */

#include "../chi_eb/xact/chi_eb_xact_base.hpp"      // IWYU pragma: keep
#include "../chi_eb/xact/chi_eb_joint.hpp"           // IWYU pragma: keep
#include "../chi_eb/xact/chi_eb_xact_state.hpp"      // IWYU pragma: keep
#include "../chi_eb/util/chi_eb_util_flit.hpp"       // IWYU pragma: keep


/* -------------------------------------------------------------------------
 * Instantiate concrete types for the chosen configuration
 * ---------------------------------------------------------------------- */

using namespace CHI::Eb;

using config = FlitConfiguration<
    CHI_NODEID_WIDTH,
    CHI_REQ_ADDR_WIDTH,
    CHI_REQ_RSVDC_WIDTH,
    CHI_DAT_RSVDC_WIDTH,
    CHI_DATA_WIDTH,
    CHI_DATACHECK_PRESENT,
    CHI_POISON_PRESENT,
    CHI_MPAM_PRESENT
>;

using REQ_t     = Flits::REQ<config>;
using SNP_t     = Flits::SNP<config>;
using RSP_t     = Flits::RSP<config>;
using DAT_t     = Flits::DAT<config>;
using Global_t  = Xact::Global<config>;
using RNFJoint_t        = Xact::RNFJoint<config>;
using SNFJoint_t        = Xact::SNFJoint<config>;
using RNCacheStateMap_t = Xact::RNCacheStateMap<config>;
using addr_t            = REQ_t::addr_t::value_type;


/* -------------------------------------------------------------------------
 * Internal helpers
 * ---------------------------------------------------------------------- */

// Convert a CHIron denial pointer to a plain int.
static inline int toInt(Xact::XactDenialEnum d) noexcept
{
    return d ? d->value : Xact::XactDenial::ACCEPTED.value;
}

// Lookup table: denial code value → name string.
// Built lazily on first call to CHIronVIP_GetDenialName.
static const std::unordered_map<int, const char*>& denialNames()
{
    static const std::unordered_map<int, const char*> table = {
        { Xact::XactDenial::ACCEPTED.value,                        "XACT_ACCEPTED"                        },
        { Xact::XactDenial::DENIED_NESTING_SNP.value,              "XACT_DENIED_NESTING_SNP"              },
        { Xact::XactDenial::DENIED_CHANNEL_TXREQ.value,            "XACT_DENIED_CHANNEL_TXREQ"            },
        { Xact::XactDenial::DENIED_CHANNEL_RXREQ.value,            "XACT_DENIED_CHANNEL_RXREQ"            },
        { Xact::XactDenial::DENIED_CHANNEL_TXSNP.value,            "XACT_DENIED_CHANNEL_TXSNP"            },
        { Xact::XactDenial::DENIED_CHANNEL_RXSNP.value,            "XACT_DENIED_CHANNEL_RXSNP"            },
        { Xact::XactDenial::DENIED_CHANNEL_TXRSP.value,            "XACT_DENIED_CHANNEL_TXRSP"            },
        { Xact::XactDenial::DENIED_CHANNEL_RXRSP.value,            "XACT_DENIED_CHANNEL_RXRSP"            },
        { Xact::XactDenial::DENIED_CHANNEL_TXDAT.value,            "XACT_DENIED_CHANNEL_TXDAT"            },
        { Xact::XactDenial::DENIED_CHANNEL_RXDAT.value,            "XACT_DENIED_CHANNEL_RXDAT"            },
        { Xact::XactDenial::DENIED_COMPLETED_RSP.value,            "XACT_DENIED_COMPLETED_RSP"            },
        { Xact::XactDenial::DENIED_COMPLETED_DAT.value,            "XACT_DENIED_COMPLETED_DAT"            },
        { Xact::XactDenial::DENIED_COMPLETED_SNP.value,            "XACT_DENIED_COMPLETED_SNP"            },
        { Xact::XactDenial::DENIED_REQ_TXNID_IN_USE.value,         "XACT_DENIED_REQ_TXNID_IN_USE"         },
        { Xact::XactDenial::DENIED_SNP_TXNID_IN_USE.value,         "XACT_DENIED_SNP_TXNID_IN_USE"         },
        { Xact::XactDenial::DENIED_RSP_TXNID_NOT_EXIST.value,      "XACT_DENIED_RSP_TXNID_NOT_EXIST"      },
        { Xact::XactDenial::DENIED_DAT_TXNID_NOT_EXIST.value,      "XACT_DENIED_DAT_TXNID_NOT_EXIST"      },
        { Xact::XactDenial::DENIED_RSP_DBID_IN_USE.value,          "XACT_DENIED_RSP_DBID_IN_USE"          },
        { Xact::XactDenial::DENIED_DAT_DBID_IN_USE.value,          "XACT_DENIED_DAT_DBID_IN_USE"          },
        { Xact::XactDenial::DENIED_RSP_DBID_NOT_EXIST.value,       "XACT_DENIED_RSP_DBID_NOT_EXIST"       },
        { Xact::XactDenial::DENIED_DAT_DBID_NOT_EXIST.value,       "XACT_DENIED_DAT_DBID_NOT_EXIST"       },
        { Xact::XactDenial::DENIED_REQ_OPCODE.value,               "XACT_DENIED_REQ_OPCODE"               },
        { Xact::XactDenial::DENIED_RSP_OPCODE.value,               "XACT_DENIED_RSP_OPCODE"               },
        { Xact::XactDenial::DENIED_DAT_OPCODE.value,               "XACT_DENIED_DAT_OPCODE"               },
        { Xact::XactDenial::DENIED_SNP_OPCODE.value,               "XACT_DENIED_SNP_OPCODE"               },
        { Xact::XactDenial::DENIED_REQ_OPCODE_NOT_SUPPORTED.value,  "XACT_DENIED_REQ_OPCODE_NOT_SUPPORTED" },
        { Xact::XactDenial::DENIED_RSP_OPCODE_NOT_SUPPORTED.value,  "XACT_DENIED_RSP_OPCODE_NOT_SUPPORTED" },
        { Xact::XactDenial::DENIED_DAT_OPCODE_NOT_SUPPORTED.value,  "XACT_DENIED_DAT_OPCODE_NOT_SUPPORTED" },
        { Xact::XactDenial::DENIED_SNP_OPCODE_NOT_SUPPORTED.value,  "XACT_DENIED_SNP_OPCODE_NOT_SUPPORTED" },
        { Xact::XactDenial::DENIED_STATE_INITIAL.value,             "XACT_DENIED_STATE_INITIAL"            },
        { Xact::XactDenial::DENIED_STATE_COMP.value,                "XACT_DENIED_STATE_COMP"               },
        { Xact::XactDenial::DENIED_STATE_COMPDATA.value,            "XACT_DENIED_STATE_COMPDATA"           },
        { Xact::XactDenial::DENIED_STATE_NESTED_TRANSFER.value,     "XACT_DENIED_STATE_NESTED_TRANSFER"    },
        { Xact::XactDenial::DENIED_UNSUPPORTED_FEATURE.value,       "XACT_DENIED_UNSUPPORTED_FEATURE"      },
        { Xact::XactDenial::DENIED_UNSUPPORTED_FEATURE_OPCODE.value,"XACT_DENIED_UNSUPPORTED_FEATURE_OPCODE"},
    };
    return table;
}


/* =========================================================================
 * Global context
 * ========================================================================= */

// Map CLog-style node type integer constants to CHIron NodeTypeEnum pointers.
static Xact::NodeTypeEnum toNodeType(uint32_t v) noexcept
{
    // CLog node-type values come from the CHI spec topology identifiers and
    // are also used as CHIRONVIP_NODE_TYPE_* constants (see clogdpi_b.svh).
    switch (v)
    {
        case 1:  return &Xact::NodeType::RN_F;
        case 2:  return &Xact::NodeType::RN_D;
        case 3:  return &Xact::NodeType::RN_I;
        case 5:  return &Xact::NodeType::HN_F;
        case 7:  return &Xact::NodeType::HN_I;
        case 9:  return &Xact::NodeType::SN_F;
        case 11: return &Xact::NodeType::SN_I;
        case 12: return &Xact::NodeType::MN;
        default: return &Xact::NodeType::RN_F; // fallback
    }
}

extern "C" void* CHIronVIP_CreateGlobal()
{
    return new Global_t();
}

extern "C" void CHIronVIP_DestroyGlobal(void* global)
{
    delete static_cast<Global_t*>(global);
}

extern "C" void CHIronVIP_Global_AddTopoNode(
    void*           global,
    uint32_t        nodeId,
    uint32_t        nodeType,
    const char*     name)
{
    auto* g = static_cast<Global_t*>(global);
    g->TOPOLOGY.SetNode(
        static_cast<uint16_t>(nodeId),
        toNodeType(nodeType),
        name ? std::string(name) : std::string("node") + std::to_string(nodeId));
}

extern "C" void CHIronVIP_Global_SetFieldCheckEnable(
    void*           global,
    int             enable)
{
    auto* g = static_cast<Global_t*>(global);
    g->CHECK_FIELD_MAPPING.enable = (enable != 0);
}


/* =========================================================================
 * RNFJoint
 * ========================================================================= */

extern "C" void* CHIronVIP_CreateRNFJoint()
{
    return new RNFJoint_t();
}

extern "C" void CHIronVIP_DestroyRNFJoint(void* joint)
{
    delete static_cast<RNFJoint_t*>(joint);
}

extern "C" int CHIronVIP_RNF_NextTXREQ(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits)
{
    REQ_t req{};
    if (!Flits::DeserializeREQ<config>(req, const_cast<uint32_t*>(flit), flitBits))
        return Xact::XactDenial::DENIED_REQ_OPCODE.value;

    auto* j = static_cast<RNFJoint_t*>(joint);
    auto* g = static_cast<Global_t*>(global);
    return toInt(j->NextTXREQ(*g, time, req, nullptr));
}

extern "C" int CHIronVIP_RNF_NextRXSNP(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits)
{
    SNP_t snp{};
    if (!Flits::DeserializeSNP<config>(snp, const_cast<uint32_t*>(flit), flitBits))
        return Xact::XactDenial::DENIED_SNP_OPCODE.value;

    auto* j = static_cast<RNFJoint_t*>(joint);
    auto* g = static_cast<Global_t*>(global);
    return toInt(j->NextRXSNP(*g, time, snp, nullptr));
}

extern "C" int CHIronVIP_RNF_NextTXRSP(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits)
{
    RSP_t rsp{};
    if (!Flits::DeserializeRSP<config>(rsp, const_cast<uint32_t*>(flit), flitBits))
        return Xact::XactDenial::DENIED_RSP_OPCODE.value;

    auto* j = static_cast<RNFJoint_t*>(joint);
    auto* g = static_cast<Global_t*>(global);
    return toInt(j->NextTXRSP(*g, time, rsp, nullptr));
}

extern "C" int CHIronVIP_RNF_NextRXRSP(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits)
{
    RSP_t rsp{};
    if (!Flits::DeserializeRSP<config>(rsp, const_cast<uint32_t*>(flit), flitBits))
        return Xact::XactDenial::DENIED_RSP_OPCODE.value;

    auto* j = static_cast<RNFJoint_t*>(joint);
    auto* g = static_cast<Global_t*>(global);
    return toInt(j->NextRXRSP(*g, time, rsp, nullptr));
}

extern "C" int CHIronVIP_RNF_NextTXDAT(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits)
{
    DAT_t dat{};
    if (!Flits::DeserializeDAT<config>(dat, const_cast<uint32_t*>(flit), flitBits))
        return Xact::XactDenial::DENIED_DAT_OPCODE.value;

    auto* j = static_cast<RNFJoint_t*>(joint);
    auto* g = static_cast<Global_t*>(global);
    return toInt(j->NextTXDAT(*g, time, dat, nullptr));
}

extern "C" int CHIronVIP_RNF_NextRXDAT(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits)
{
    DAT_t dat{};
    if (!Flits::DeserializeDAT<config>(dat, const_cast<uint32_t*>(flit), flitBits))
        return Xact::XactDenial::DENIED_DAT_OPCODE.value;

    auto* j = static_cast<RNFJoint_t*>(joint);
    auto* g = static_cast<Global_t*>(global);
    return toInt(j->NextRXDAT(*g, time, dat, nullptr));
}

extern "C" int CHIronVIP_RNF_GetInflightCount(
    void*               joint,
    void*               global)
{
    auto* j = static_cast<RNFJoint_t*>(joint);
    auto* g = static_cast<Global_t*>(global);
    std::vector<std::shared_ptr<Xact::Xaction<config>>> inflight;
    j->GetInflight(*g, inflight);
    return static_cast<int>(inflight.size());
}


/* =========================================================================
 * SNFJoint
 * ========================================================================= */

extern "C" void* CHIronVIP_CreateSNFJoint()
{
    return new SNFJoint_t();
}

extern "C" void CHIronVIP_DestroySNFJoint(void* joint)
{
    delete static_cast<SNFJoint_t*>(joint);
}

extern "C" int CHIronVIP_SNF_NextRXREQ(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits)
{
    REQ_t req{};
    if (!Flits::DeserializeREQ<config>(req, const_cast<uint32_t*>(flit), flitBits))
        return Xact::XactDenial::DENIED_REQ_OPCODE.value;

    auto* j = static_cast<SNFJoint_t*>(joint);
    auto* g = static_cast<Global_t*>(global);
    return toInt(j->NextRXREQ(*g, time, req, nullptr));
}

extern "C" int CHIronVIP_SNF_NextTXRSP(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits)
{
    RSP_t rsp{};
    if (!Flits::DeserializeRSP<config>(rsp, const_cast<uint32_t*>(flit), flitBits))
        return Xact::XactDenial::DENIED_RSP_OPCODE.value;

    auto* j = static_cast<SNFJoint_t*>(joint);
    auto* g = static_cast<Global_t*>(global);
    return toInt(j->NextTXRSP(*g, time, rsp, nullptr));
}

extern "C" int CHIronVIP_SNF_NextTXDAT(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits)
{
    DAT_t dat{};
    if (!Flits::DeserializeDAT<config>(dat, const_cast<uint32_t*>(flit), flitBits))
        return Xact::XactDenial::DENIED_DAT_OPCODE.value;

    auto* j = static_cast<SNFJoint_t*>(joint);
    auto* g = static_cast<Global_t*>(global);
    return toInt(j->NextTXDAT(*g, time, dat, nullptr));
}

extern "C" int CHIronVIP_SNF_NextRXDAT(
    void*               joint,
    void*               global,
    uint64_t            time,
    const uint32_t*     flit,
    uint32_t            flitBits)
{
    DAT_t dat{};
    if (!Flits::DeserializeDAT<config>(dat, const_cast<uint32_t*>(flit), flitBits))
        return Xact::XactDenial::DENIED_DAT_OPCODE.value;

    auto* j = static_cast<SNFJoint_t*>(joint);
    auto* g = static_cast<Global_t*>(global);
    return toInt(j->NextRXDAT(*g, time, dat, nullptr));
}

extern "C" int CHIronVIP_SNF_GetInflightCount(
    void*               joint,
    void*               global)
{
    auto* j = static_cast<SNFJoint_t*>(joint);
    auto* g = static_cast<Global_t*>(global);
    std::vector<std::shared_ptr<Xact::Xaction<config>>> inflight;
    j->GetInflight(*g, inflight);
    return static_cast<int>(inflight.size());
}


/* =========================================================================
 * RNCacheStateMap
 * ========================================================================= */

extern "C" void* CHIronVIP_CreateRNCacheStateMap()
{
    return new RNCacheStateMap_t();
}

extern "C" void CHIronVIP_DestroyRNCacheStateMap(void* stateMap)
{
    delete static_cast<RNCacheStateMap_t*>(stateMap);
}

extern "C" void CHIronVIP_StateMap_SetSilentEviction(void* stateMap, int enable)
{
    static_cast<RNCacheStateMap_t*>(stateMap)->SetSilentEviction(enable != 0);
}

extern "C" void CHIronVIP_StateMap_SetSilentSharing(void* stateMap, int enable)
{
    static_cast<RNCacheStateMap_t*>(stateMap)->SetSilentSharing(enable != 0);
}

extern "C" void CHIronVIP_StateMap_SetSilentStore(void* stateMap, int enable)
{
    static_cast<RNCacheStateMap_t*>(stateMap)->SetSilentStore(enable != 0);
}

extern "C" void CHIronVIP_StateMap_SetSilentInvalidation(void* stateMap, int enable)
{
    static_cast<RNCacheStateMap_t*>(stateMap)->SetSilentInvalidation(enable != 0);
}

extern "C" int CHIronVIP_StateMap_GetState(void* stateMap, uint64_t addr)
{
    auto* sm = static_cast<RNCacheStateMap_t*>(stateMap);
    return static_cast<int>(sm->Get(static_cast<addr_t>(addr)).i8);
}

extern "C" void CHIronVIP_StateMap_SetState(void* stateMap, uint64_t addr, int cacheState)
{
    auto* sm = static_cast<RNCacheStateMap_t*>(stateMap);
    sm->Set(static_cast<addr_t>(addr), Xact::CacheState(static_cast<uint8_t>(cacheState)));
}

extern "C" int CHIronVIP_StateMap_NextTXREQ(
    void*               stateMap,
    uint64_t            addr,
    const uint32_t*     flit,
    uint32_t            flitBits)
{
    REQ_t req{};
    if (!Flits::DeserializeREQ<config>(req, const_cast<uint32_t*>(flit), flitBits))
        return Xact::XactDenial::DENIED_REQ_OPCODE.value;

    auto* sm = static_cast<RNCacheStateMap_t*>(stateMap);
    return toInt(sm->NextTXREQ(static_cast<addr_t>(addr), req));
}

extern "C" int CHIronVIP_StateMap_NextRXSNP(
    void*               stateMap,
    uint64_t            addr,
    const uint32_t*     flit,
    uint32_t            flitBits)
{
    SNP_t snp{};
    if (!Flits::DeserializeSNP<config>(snp, const_cast<uint32_t*>(flit), flitBits))
        return Xact::XactDenial::DENIED_SNP_OPCODE.value;

    auto* sm = static_cast<RNCacheStateMap_t*>(stateMap);
    return toInt(sm->NextRXSNP(static_cast<addr_t>(addr), snp));
}

extern "C" int CHIronVIP_StateMap_Transfer(
    void*       stateMap,
    uint64_t    addr,
    int         cacheState)
{
    auto* sm = static_cast<RNCacheStateMap_t*>(stateMap);
    return toInt(sm->Transfer(
        static_cast<addr_t>(addr),
        Xact::CacheState(static_cast<uint8_t>(cacheState)),
        nullptr));
}


/* =========================================================================
 * Utility
 * ========================================================================= */

extern "C" const char* CHIronVIP_GetDenialName(int denialCode)
{
    const auto& table = denialNames();
    auto it = table.find(denialCode);
    if (it != table.end())
        return it->second;
    return "XACT_UNKNOWN";
}
