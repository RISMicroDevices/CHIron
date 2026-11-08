#include "common.hpp"

#include "cchi/cohestra/cohestra_v3/interface_chi.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace CHI::Lunatic;
using namespace Cohestra;
using namespace Cohestra::details;
using namespace LunaticTest;

namespace {

    template<typename Flit>
    struct CreditMock {
        using FlitType = Flit;

        uint8_t flitpend = 0;
        uint8_t flitv = 0;
        uint8_t lcrdv = 0;
        Flit    flit{};
    };

    template<class VT, size_t PORT, class ChannelTag>
    struct CreditPolicy {
        static void ClearValid(VT& mock) noexcept { mock.flitv = 0; }
        static void DrivePending(VT& mock, bool pending) noexcept { mock.flitpend = pending; }
        static void DriveFlit(VT& mock, const void* flit, bool valid) noexcept
        {
            mock.flit = *static_cast<const typename VT::FlitType*>(flit);
            mock.flitv = valid;
        }
        static bool SampleFlit(VT& mock, void* flit) noexcept
        {
            if (!mock.flitv)
                return false;
            *static_cast<typename VT::FlitType*>(flit) = mock.flit;
            return true;
        }
        static void ClearLCrdV(VT& mock) noexcept { mock.lcrdv = 0; }
        static void DriveLCrdV(VT& mock, bool valid) noexcept { mock.lcrdv = valid; }
        static bool SampleLCrdV(VT& mock) noexcept { return mock.lcrdv; }
    };

    template<typename Flit>
    std::vector<uint32_t> Packed(const Flit& flit)
    {
        std::vector<uint32_t> bits((Flit::WIDTH + 31) / 32);
        if constexpr (std::is_same_v<Flit, Cohestra::CHI::REQFlit>)
            V3CHIPackREQ(flit, bits.data());
        else if constexpr (std::is_same_v<Flit, Cohestra::CHI::RSPFlit>)
            V3CHIPackRSP(flit, bits.data());
        else if constexpr (std::is_same_v<Flit, Cohestra::CHI::DATFlit>)
            V3CHIPackDAT(flit, bits.data());
        else
            V3CHIPackSNP(flit, bits.data());
        return bits;
    }

    template<typename Flit>
    std::vector<Flit> MakeTraffic()
    {
        std::vector<Flit> flits(24);
        for (size_t index = 0; index < flits.size(); ++index)
        {
            auto& flit = flits[index];
            flit.QoS() = index % 16;
            flit.SrcID() = 1 + index % 3;
            if constexpr (!std::is_same_v<Flit, Cohestra::CHI::SNPFlit>)
                flit.TgtID() = 2 + index % 2;
            flit.TxnID() = 0x40 + index;
            flit.TraceTag() = index % 2;
            if constexpr (std::is_same_v<Flit, Cohestra::CHI::REQFlit>)
            {
                flit.Opcode() = ::CHI::Eb::Opcodes::REQ::ReadNoSnp;
                flit.Addr() = 0x80000000 + index * LINE_SIZE;
                flit.Size() = 6;
                flit.MemAttr() = ::CHI::Eb::MemAttrs::WriteBack_Allocate;
            }
            else if constexpr (std::is_same_v<Flit, Cohestra::CHI::RSPFlit>)
            {
                flit.Opcode() = ::CHI::Eb::Opcodes::RSP::CompDBIDResp;
                flit.Resp() = ::CHI::Eb::Resps::Comp_UC;
                flit.DBID() = 0x80 + index;
            }
            else if constexpr (std::is_same_v<Flit, Cohestra::CHI::DATFlit>)
            {
                flit.HomeNID() = 2;
                flit.Opcode() = ::CHI::Eb::Opcodes::DAT::CompData;
                flit.Resp() = ::CHI::Eb::Resps::CompData_UC;
                flit.DataID() = index % 2 * 2;
                flit.BE() = 0xffffffff;
                for (size_t lane = 0; lane < 4; ++lane)
                    flit.Data()[lane] = 0x1000000000000000ULL * lane + index * 0x101 + lane;
            }
            else
            {
                flit.Opcode() = ::CHI::Eb::Opcodes::SNP::SnpUnique;
                flit.Addr() = 0x80000000 + index * LINE_SIZE;
                flit.FwdNID() = 4;
                flit.FwdTxnID() = 0x90 + index;
            }
        }
        return flits;
    }

    struct CreditResult {
        size_t transfers = 0;
        size_t heldCredits = 0;
        size_t heldPops = 0;
        size_t stableChecks = 0;
        size_t cycles = 0;

        bool operator==(const CreditResult&) const = default;
    };

    template<typename Flit, typename ChannelTag>
    class CreditExchange {
        using Mock = CreditMock<Flit>;
        using Channel = CHICreditChannel<Flit>;

        std::vector<Flit>     expected;
        uint64_t              seed;
        Channel               sender;
        Channel               receiver;
        Mock                  mock;
        DeterministicRandom   creditRandom;
        DeterministicRandom   popRandom;
        std::vector<uint32_t> previousHead;
        size_t                received = 0;

        void Offer()
        {
            sender.SetBufferSize(expected.size());
            receiver.SetRxCreditCount(3);
            for (const auto& flit : expected)
                LUNATIC_CHECK(sender.Push(flit));
        }

    public:
        CreditResult result;

        CreditExchange(std::vector<Flit> expected, uint64_t seed)
            : expected(std::move(expected)), seed(seed), creditRandom(seed ^ 0x9e3779b9),
              popRandom(seed ^ 0xd1b54a35)
        {
            Offer();
        }

        bool Step(bool force = false)
        {
            ++result.cycles;
            sender.template Drive<Mock, CreditPolicy, 0, ChannelTag>(mock);
            if (const auto* head = sender.txBuffer.Peek())
            {
                const auto packed = Packed(*head);
                if (packed == previousHead)
                    ++result.stableChecks;
                previousHead = packed;
            }
            else
                previousHead.clear();

            receiver.template Observe<Mock, CreditPolicy, 0, ChannelTag>(mock, result.cycles);
            if (receiver.HasObservation() && (force || popRandom.Bounded(3) != 0))
            {
                const auto observed = receiver.PopObservation();
                LUNATIC_CHECK(observed);
                LUNATIC_CHECK(received < expected.size());
                LUNATIC_CHECK(Packed(*observed) == Packed(expected[received]));
                ++received;
                ++result.transfers;
            }
            else if (receiver.HasObservation())
                ++result.heldPops;

            if (receiver.rxPendingReturns > 0 && !force && creditRandom.Bounded(3) == 0)
            {
                CreditPolicy<Mock, 0, ChannelTag>::ClearLCrdV(mock);
                ++result.heldCredits;
            }
            else
                receiver.template ReturnCredits<Mock, CreditPolicy, 0, ChannelTag>(mock);
            sender.template Retire<Mock, CreditPolicy, 0, ChannelTag>(mock);
            LUNATIC_CHECK(sender.ValidateCreditAccounting());
            LUNATIC_CHECK(receiver.ValidateCreditAccounting());
            return received == expected.size() && sender.txBuffer.IsEmpty() && !sender.txDriven;
        }

        CreditResult Run()
        {
            for (size_t cycle = 0; cycle < 1024; ++cycle)
            {
                if (Step(cycle >= 768))
                {
                    for (size_t drain = 0; drain < 32 && !receiver.IsDrained(); ++drain)
                    {
                        LUNATIC_CHECK(!receiver.HasObservation());
                        receiver.template ReturnCredits<Mock, CreditPolicy, 0, ChannelTag>(mock);
                        sender.template Retire<Mock, CreditPolicy, 0, ChannelTag>(mock);
                    }
                    LUNATIC_CHECK(receiver.IsDrained());
                    LUNATIC_CHECK(sender.IsDrained());
                    LUNATIC_CHECK(sender.ValidateCreditAccounting());
                    LUNATIC_CHECK(receiver.ValidateCreditAccounting());
                    LUNATIC_CHECK(result.heldCredits != 0);
                    LUNATIC_CHECK(result.heldPops != 0);
                    LUNATIC_CHECK(result.stableChecks != 0);
                    return result;
                }
            }
            throw std::runtime_error("credit-channel timeout, seed=" + std::to_string(seed));
        }

        void Reset()
        {
            sender = Channel{};
            receiver = Channel{};
            mock = Mock{};
            creditRandom.Reset(seed ^ 0xa24baed5);
            popRandom.Reset(seed ^ 0x63f3a721);
            previousHead.clear();
            received = 0;
            result = CreditResult{};
            Offer();
            LUNATIC_CHECK(sender.ValidateCreditAccounting());
            LUNATIC_CHECK(receiver.ValidateCreditAccounting());
        }
    };

    template<typename Flit, typename ChannelTag>
    void VerifyIndependentChannels(uint64_t seed)
    {
        CreditExchange<Flit, ChannelTag> original(MakeTraffic<Flit>(), seed);
        const auto baseline = original.Run();
        CreditExchange<Flit, ChannelTag> replay(MakeTraffic<Flit>(), seed);
        LUNATIC_CHECK(replay.Run() == baseline);
        CreditExchange<Flit, ChannelTag> perturbed(MakeTraffic<Flit>(), seed ^ 0x5bd1e995);
        LUNATIC_CHECK(perturbed.Run().transfers == baseline.transfers);
    }

    template<typename Flit, typename ChannelTag>
    void VerifyResetGeneration(uint64_t seed)
    {
        CreditExchange<Flit, ChannelTag> exchange(MakeTraffic<Flit>(), seed);
        bool partial = false;
        for (size_t cycle = 0; cycle < 256 && !partial; ++cycle)
        {
            exchange.Step();
            partial = exchange.result.transfers >= 3 && exchange.result.transfers < 24;
        }
        LUNATIC_CHECK(partial);
        exchange.Reset();
        LUNATIC_CHECK(exchange.result.transfers == 0);
        const auto restarted = exchange.Run();
        LUNATIC_CHECK(restarted.transfers == 24);
    }

}

LUNATIC_TEST(Stage9IndependentCreditBackpressureMatrix)
{
    VerifyIndependentChannels<Cohestra::CHI::REQFlit, TXREQChannel>(0x1001);
    VerifyIndependentChannels<Cohestra::CHI::RSPFlit, RXRSPChannel>(0x2002);
    VerifyIndependentChannels<Cohestra::CHI::DATFlit, TXDATChannel>(0x3003);
    VerifyIndependentChannels<Cohestra::CHI::SNPFlit, RXSNPChannel>(0x4004);
}

LUNATIC_TEST(Stage9CreditResetStartsFreshGenerations)
{
    VerifyResetGeneration<Cohestra::CHI::REQFlit, TXREQChannel>(0x5005);
    VerifyResetGeneration<Cohestra::CHI::RSPFlit, RXRSPChannel>(0x6006);
    VerifyResetGeneration<Cohestra::CHI::DATFlit, TXDATChannel>(0x7007);
    VerifyResetGeneration<Cohestra::CHI::SNPFlit, RXSNPChannel>(0x8008);
}
