// chi_refmodel_dpi.cpp
// DPI-C shim that wraps CHIron's transaction state-machine library and
// exposes it to SystemVerilog UVM via the functions declared in
// chi_scoreboard.sv.
//
// Build instructions
// ------------------
// Compile alongside CHIron headers and link into your simulator's shared
// object.  Example (VCS):
//
//   g++ -std=c++20 -fPIC -shared -o chi_refmodel.so \
//       -I<chiron_root> \
//       -DCHI_ISSUE_EB_ENABLE \
//       chi_refmodel_dpi.cpp
//
//   vcs ... -sv_lib chi_refmodel ...
//
// Replace <chiron_root> with the path to the CHIron repository root.
//
// Key CHIron classes used
// -----------------------
//   CHI::Flits::REQ<config>   — typed flit for REQ channel
//   CHI::Flits::RSP<config>   — typed flit for RSP channel
//   CHI::Flits::DAT<config>   — typed flit for DAT channel
//   CHI::Flits::SNP<config>   — typed flit for SNP channel
//   CHI::Xact::Router<config> — classifies incoming flits, routes them to
//                               the correct Xaction state machine, and
//                               reports protocol violations.

#define CHI_ISSUE_EB_ENABLE

#include "chi/spec/chi_protocol_flits.hpp"
#include "chi/xact/chi_xact_base/chi_xact_base_topology.hpp"
#include "chi/xact/chi_xactions/chi_xactions_impl.hpp"

#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Flit configuration — must match chi_pkg.sv parameter values
// ---------------------------------------------------------------------------
using Config = CHI::FlitConfiguration<
    7,      // NodeIDWidth
    48,     // ReqAddrWidth
    4,      // ReqRSVDCWidth
    4,      // DatRSVDCWidth
    256,    // DataWidth
    false,  // DataCheckPresent
    false,  // PoisonPresent
    false   // MPAMPresent
>;

// ---------------------------------------------------------------------------
// Internal state
// ---------------------------------------------------------------------------
namespace {
    // Collection of in-flight transactions keyed by (srcid, txnid)
    struct RefModel {
        std::vector<std::shared_ptr<CHI::Xact::Xaction<Config>>> xactions;
        std::string last_error;
        int         last_error_code;

        RefModel() : last_error_code(0) {}

        void reset() {
            xactions.clear();
            last_error.clear();
            last_error_code = 0;
        }

        // Feed a raw 512-bit flit (packed as svBitVecVal array from DPI)
        // channel: 0=REQ, 1=RSP, 2=DAT, 3=SNP
        int feed(int channel, const uint8_t* flit_bytes, int flit_len_bits) {
            // In a full implementation this function would:
            //   1. Reconstruct the typed CHI flit from the raw bytes.
            //   2. Pass it to the Router to find the matching Xaction.
            //   3. Call Xaction::NextRSP / NextDAT etc. to advance state.
            //   4. Detect and report any protocol violations.
            //
            // The stub below validates the flit_len range and returns 0
            // (no error).  Replace with full CHIron Router integration as
            // described in docs/uvm_integration_guide.md.
            (void)flit_bytes;
            if (flit_len_bits <= 0 || flit_len_bits > 512) {
                last_error      = "Invalid flit_length: " + std::to_string(flit_len_bits);
                last_error_code = -1;
                return -1;
            }
            last_error_code = 0;
            return 0;
        }
    };

    static RefModel g_model;

    // Copy DPI bit[511:0] (passed as svBitVecVal*) to a byte array.
    // svBitVecVal is a uint32_t; 512 bits = 16 uint32_t words.
    void copy_flit(const svBitVecVal* sv_flit, uint8_t* out_bytes) {
        std::memcpy(out_bytes, sv_flit, 64); // 512 bits = 64 bytes
    }
}

// ---------------------------------------------------------------------------
// DPI-C exported functions (declared in chi_scoreboard.sv)
// ---------------------------------------------------------------------------
extern "C" {

void chi_rm_init() {
    g_model.reset();
}

void chi_rm_feed_req(const svBitVecVal* flit, int flit_len) {
    uint8_t bytes[64];
    copy_flit(flit, bytes);
    g_model.feed(0, bytes, flit_len);
}

void chi_rm_feed_rsp(const svBitVecVal* flit, int flit_len) {
    uint8_t bytes[64];
    copy_flit(flit, bytes);
    g_model.feed(1, bytes, flit_len);
}

void chi_rm_feed_dat(const svBitVecVal* flit, int flit_len) {
    uint8_t bytes[64];
    copy_flit(flit, bytes);
    g_model.feed(2, bytes, flit_len);
}

void chi_rm_feed_snp(const svBitVecVal* flit, int flit_len) {
    uint8_t bytes[64];
    copy_flit(flit, bytes);
    g_model.feed(3, bytes, flit_len);
}

int chi_rm_check() {
    return g_model.last_error_code;
}

const char* chi_rm_get_error_string() {
    return g_model.last_error.c_str();
}

} // extern "C"
