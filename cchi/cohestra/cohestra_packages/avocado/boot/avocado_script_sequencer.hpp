#pragma once

//
// avocado_script_sequencer.hpp — Avocado AXI-master script sequencer for
// CHIron Cohestra V3.
//
// Replays an Avocado boot script (the linearized configuration write
// stream, see avocado_boot_script.h) through an AXI master, one entry at a
// time:
//
//     write32 -> wait B (resp must be OKAY) -> [read32 -> wait R, compare]
//
// This is the Cohestra-native equivalent of a boot CPU running
// configuration firmware: instead of an instruction-set simulator
// executing firmware that funnels config-space stores onto the RN-I AXI
// port, the sequencer issues the same single-beat 32-bit AXI accesses
// directly from the host. Completion of the script is the analog of the
// firmware's good-trap handshake: gate CHI stimulators / DUT-reset
// release on Done().
//
// The class is a duck-typed template so it can be driven both by
// Cohestra::AXIMaster (the harness) and by a test mock. Required master
// surface (subset of Cohestra::AXIMaster):
//
//     using WriteFuture = std::shared_ptr<FutureNow-like>;
//     using ReadFuture  = std::shared_ptr<FutureNow-like>;
//     WriteFuture Write32(uint64_t addr, uint32_t value);
//     ReadFuture  Read32 (uint64_t addr);
//     size_t      GetDataBytes() const;
//
// Future surface (subset of Cohestra::AXI::FutureNow): IsRejected(),
// IsNow(), Bind(callback) — with Bind running immediately for completed
// futures. Transaction surfaces used:
//     write: t.response.Bundle.Resp   (== 0 <=> AXI OKAY)
//     read:  t.data[i].Bundle.Data    (std::vector<uint64_t> beats)
//            t.data[i].Bundle.Resp
//
// Sequencing discipline: one entry in flight — the write->read-back of the
// same address must not be pipelined (AXI gives no cross-ID ordering).
// Call Tick() exactly once per cycle, BEFORE the master's
// TickPreHandshake(time) of the following cycle fires the futures (the
// Cohestra V3 boot-stage loop ticks boot sequencers ahead of the AXI
// masters within the same cycle, which satisfies this).
//
// Header-only; the explicit instantiation for Cohestra::AXIMaster lives in
// avocado_script_sequencer.cpp.
//

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "avocado_boot_script.h"


namespace Cohestra {

    template<typename MasterT>
    class AvocadoScriptSequencer {
    public:
        using WriteFuture = typename MasterT::WriteFuture;
        using ReadFuture  = typename MasterT::ReadFuture;

        enum class State : uint8_t {
            IssueWrite,     // dispatch the current entry's write
            WaitWrite,      // wait for the write response (B)
            WaitRead,       // wait for the read-back response (R)
            WaitMore,       // live mode: script not yet finalized, caught up
            Done,           // all entries completed (== GOOD trap)
            Failed,         // rejection, error response, or verify mismatch
        };

    private:
        MasterT*                                        master;
        std::shared_ptr<std::vector<AvocadoBootEntry>>  ownedScript;
        std::shared_ptr<std::vector<AvocadoBootEntry>>  script;
        bool                                            finalized;
        std::size_t                                     pc       = 0;
        State                                           state    = State::IssueWrite;
        bool                                            respBad  = false;
        bool                                            dataBad  = false;
        WriteFuture                                     wf;
        ReadFuture                                      rf;

    public:
        // Static-script mode: replays the given entries; the stream is
        // complete from the start.
        AvocadoScriptSequencer(MasterT& master,
                               const AvocadoBootEntry* script, std::size_t count) noexcept
            : master       (&master)
            , ownedScript  (std::make_shared<std::vector<AvocadoBootEntry>>(script, script + count))
            , script       (ownedScript)
            , finalized    (true)
        { }

        // Live mode: entries are appended to the shared vector while the
        // sequencer runs; call Finalize() when the producer is done.
        AvocadoScriptSequencer(MasterT& master,
                               std::shared_ptr<std::vector<AvocadoBootEntry>> liveScript) noexcept
            : master       (&master)
            , script       (std::move(liveScript))
            , finalized    (false)
        { }

    public:
        // Mark the live-mode script complete (no more entries will arrive).
        void Finalize() noexcept { finalized = true; }

        void Reset() noexcept
        {
            pc       = 0;
            state    = State::IssueWrite;
            respBad  = false;
            dataBad  = false;
            wf.reset();
            rf.reset();
        }

        // Advance one cycle. Returns true once State::Done is reached.
        bool Tick() noexcept
        {
            switch (state) {
            case State::IssueWrite: {
                if (pc >= script->size()) {
                    state = finalized ? State::Done : State::WaitMore;
                    break;
                }
                const AvocadoBootEntry& e = (*script)[pc];
                wf     = master->Write32(e.addr, e.value);
                state  = State::WaitWrite;
                break;
            }
            case State::WaitWrite: {
                if (wf->IsRejected()) { state = State::Failed; break; }
                if (!wf->IsNow())     break;
                respBad = false;
                wf->Bind([this](const auto& t) {
                    // AXI Resp::OKAY == 0
                    if (static_cast<uint8_t>(t.response.Bundle.Resp) != 0)
                        respBad = true;
                });
                if (respBad) { state = State::Failed; break; }
                if ((*script)[pc].verify) {
                    rf    = master->Read32((*script)[pc].addr);
                    state = State::WaitRead;
                } else {
                    Advance();
                }
                break;
            }
            case State::WaitRead: {
                if (rf->IsRejected()) { state = State::Failed; break; }
                if (!rf->IsNow())     break;
                respBad = false;
                dataBad = false;
                rf->Bind([this](const auto& t) {
                    if (t.data.empty()) { dataBad = true; return; }
                    if (static_cast<uint8_t>(t.data[0].Bundle.Resp) != 0) {
                        respBad = true;
                        return;
                    }
                    const AvocadoBootEntry& e = (*script)[pc];
                    // AXI returns narrow-read data on the addressed byte
                    // lanes of the bus window — compare at
                    // addr % dataBytes (do NOT use leading lanes).
                    const std::size_t off =
                        static_cast<std::size_t>(e.addr) % master->GetDataBytes();
                    for (std::size_t i = 0; i < 4; i++) {
                        if (ByteAt(t.data[0].Bundle.Data, off + i) !=
                            static_cast<uint8_t>(e.value >> (i * 8))) {
                            dataBad = true;
                            return;
                        }
                    }
                });
                if (respBad || dataBad) { state = State::Failed; break; }
                Advance();
                break;
            }
            case State::WaitMore: {
                if (pc < script->size())
                    state = State::IssueWrite;
                else if (finalized)
                    state = State::Done;
                break;
            }
            case State::Done:
            case State::Failed:
                break;
            }
            return state == State::Done;
        }

    public:
        bool        Done()      const noexcept { return state == State::Done;   }
        bool        Failed()    const noexcept { return state == State::Failed; }
        std::size_t Progress()  const noexcept { return pc;                     }
        std::size_t Total()     const noexcept { return script->size();         }
        State       GetState()  const noexcept { return state;                  }

        const char* StateName() const noexcept
        {
            switch (state) {
            case State::IssueWrite: return "issue-write";
            case State::WaitWrite:  return "wait-write";
            case State::WaitRead:   return "wait-read";
            case State::WaitMore:   return "wait-more";
            case State::Done:       return "done";
            case State::Failed:     return "failed";
            }
            return "?";
        }

    private:
        void Advance() noexcept
        {
            pc++;
            state = State::IssueWrite;
        }

        static uint8_t ByteAt(const std::vector<uint64_t>& data, std::size_t byte) noexcept
        {
            return static_cast<uint8_t>(data[byte >> 3] >> ((byte & 7) * 8));
        }
    };
}
