#include "common.hpp"

#include "cchi/cohestra/cohestra_v3/interface_chi.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <variant>

using namespace Cohestra;
using namespace Cohestra::details;

#define MATRIX_CONTROLS(prefix) \
    uint8_t prefix##flitpend = 0; \
    uint8_t prefix##flitv = 0; \
    uint8_t prefix##lcrdv = 0

#define MATRIX_SCALAR_MEMBER(prefix, name) \
    uint64_t prefix##name = 0;

#define MATRIX_REQ_FIELDS(X, P) \
    X(P, QoS) X(P, TgtID) X(P, SrcID) X(P, TxnID) X(P, ReturnNID) \
    X(P, StashNID) X(P, StashNIDValid) X(P, Endian) X(P, ReturnTxnID) \
    X(P, StashLPIDValid) X(P, StashLPID) X(P, Opcode) X(P, Size) X(P, Addr) \
    X(P, NS) X(P, LikelyShared) X(P, AllowRetry) X(P, Order) X(P, PCrdType) \
    X(P, MemAttr) X(P, SnpAttr) X(P, LPID) X(P, Excl) X(P, SnoopMe) \
    X(P, ExpCompAck) X(P, TraceTag) X(P, SLCRepHint) X(P, Deep) X(P, DoDWT) \
    X(P, PGroupID) X(P, StashGroupID) X(P, TagGroupID) X(P, TagOp) X(P, MPAM) \
    X(P, RSVDC)

#define MATRIX_RSP_FIELDS(X, P) \
    X(P, QoS) X(P, TgtID) X(P, SrcID) X(P, TxnID) X(P, Opcode) X(P, RespErr) \
    X(P, Resp) X(P, FwdState) X(P, DataPull) X(P, CBusy) X(P, DBID) \
    X(P, PGroupID) X(P, StashGroupID) X(P, TagGroupID) X(P, PCrdType) \
    X(P, TagOp) X(P, TraceTag)

#define MATRIX_DAT_SCALAR_FIELDS(X, P) \
    X(P, QoS) X(P, TgtID) X(P, SrcID) X(P, TxnID) X(P, HomeNID) X(P, Opcode) \
    X(P, RespErr) X(P, Resp) X(P, FwdState) X(P, DataPull) X(P, DataSource) \
    X(P, CBusy) X(P, DBID) X(P, CCID) X(P, DataID) X(P, TagOp) X(P, Tag) \
    X(P, TU) X(P, TraceTag) X(P, RSVDC) X(P, BE) X(P, DataCheck) X(P, Poison)

#define MATRIX_SNP_FIELDS(X, P) \
    X(P, QoS) X(P, SrcID) X(P, TxnID) X(P, FwdNID) X(P, FwdTxnID) \
    X(P, StashLPID) X(P, StashLPIDValid) X(P, VMIDExt) X(P, Opcode) X(P, Addr) \
    X(P, NS) X(P, DoNotGoToSD) X(P, RetToSrc) X(P, TraceTag) X(P, MPAM)

#define MATRIX_REQ_MEMBERS(prefix) MATRIX_REQ_FIELDS(MATRIX_SCALAR_MEMBER, prefix)
#define MATRIX_RSP_MEMBERS(prefix) MATRIX_RSP_FIELDS(MATRIX_SCALAR_MEMBER, prefix)
#define MATRIX_SNP_MEMBERS(prefix) MATRIX_SNP_FIELDS(MATRIX_SCALAR_MEMBER, prefix)
#define MATRIX_DAT_MEMBERS(prefix) \
    MATRIX_DAT_SCALAR_FIELDS(MATRIX_SCALAR_MEMBER, prefix) \
    uint64_t prefix##Data[4] = {};

#define MATRIX_PACKED_CHANNEL(prefix, Flit) \
    MATRIX_CONTROLS(prefix); \
    uint32_t prefix##bits[(Flit::WIDTH + 31) / 32] = {}

#define MATRIX_PACKED_MONITOR(prefix, Flit) \
    MATRIX_CONTROLS(prefix); \
    uint32_t prefix##flit[(Flit::WIDTH + 31) / 32] = {}

namespace {

    struct SeparateMock {
        MATRIX_CONTROLS(chi_rn0_txreq_);
        MATRIX_CONTROLS(chi_rn0_txrsp_);
        MATRIX_CONTROLS(chi_rn0_txdat_);
        MATRIX_CONTROLS(chi_rn0_rxrsp_);
        MATRIX_CONTROLS(chi_rn0_rxdat_);
        MATRIX_CONTROLS(chi_rn0_rxsnp_);
        MATRIX_CONTROLS(chi_sn0_rxreq_);
        MATRIX_CONTROLS(chi_sn0_txrsp_);
        MATRIX_CONTROLS(chi_sn0_txdat_);
        MATRIX_CONTROLS(chi_sn0_rxdat_);

        MATRIX_REQ_MEMBERS(chi_rn0_txreq_bits_)
        MATRIX_RSP_MEMBERS(chi_rn0_txrsp_bits_)
        MATRIX_DAT_MEMBERS(chi_rn0_txdat_bits_)
        MATRIX_RSP_MEMBERS(chi_rn0_rxrsp_bits_)
        MATRIX_DAT_MEMBERS(chi_rn0_rxdat_bits_)
        MATRIX_SNP_MEMBERS(chi_rn0_rxsnp_bits_)
        MATRIX_REQ_MEMBERS(chi_sn0_rxreq_bits_)
        MATRIX_RSP_MEMBERS(chi_sn0_txrsp_bits_)
        MATRIX_DAT_MEMBERS(chi_sn0_txdat_bits_)
        MATRIX_DAT_MEMBERS(chi_sn0_rxdat_bits_)

        MATRIX_CONTROLS(mon_chi_rn0_txreq_);
        MATRIX_CONTROLS(mon_chi_rn0_txrsp_);
        MATRIX_CONTROLS(mon_chi_rn0_txdat_);
        MATRIX_CONTROLS(mon_chi_rn0_rxrsp_);
        MATRIX_CONTROLS(mon_chi_rn0_rxdat_);
        MATRIX_CONTROLS(mon_chi_rn0_rxsnp_);
        MATRIX_CONTROLS(mon_chi_sn0_rxreq_);
        MATRIX_CONTROLS(mon_chi_sn0_txrsp_);
        MATRIX_CONTROLS(mon_chi_sn0_txdat_);
        MATRIX_CONTROLS(mon_chi_sn0_rxdat_);

        MATRIX_REQ_MEMBERS(mon_chi_rn0_txreq_flit_)
        MATRIX_RSP_MEMBERS(mon_chi_rn0_txrsp_flit_)
        MATRIX_DAT_MEMBERS(mon_chi_rn0_txdat_flit_)
        MATRIX_RSP_MEMBERS(mon_chi_rn0_rxrsp_flit_)
        MATRIX_DAT_MEMBERS(mon_chi_rn0_rxdat_flit_)
        MATRIX_SNP_MEMBERS(mon_chi_rn0_rxsnp_flit_)
        MATRIX_REQ_MEMBERS(mon_chi_sn0_rxreq_flit_)
        MATRIX_RSP_MEMBERS(mon_chi_sn0_txrsp_flit_)
        MATRIX_DAT_MEMBERS(mon_chi_sn0_txdat_flit_)
        MATRIX_DAT_MEMBERS(mon_chi_sn0_rxdat_flit_)
    };

    struct PackedMock {
        MATRIX_PACKED_CHANNEL(chi_rn0_txreq_, Cohestra::CHI::REQFlit);
        MATRIX_PACKED_CHANNEL(chi_rn0_txrsp_, Cohestra::CHI::RSPFlit);
        MATRIX_PACKED_CHANNEL(chi_rn0_txdat_, Cohestra::CHI::DATFlit);
        MATRIX_PACKED_CHANNEL(chi_rn0_rxrsp_, Cohestra::CHI::RSPFlit);
        MATRIX_PACKED_CHANNEL(chi_rn0_rxdat_, Cohestra::CHI::DATFlit);
        MATRIX_PACKED_CHANNEL(chi_rn0_rxsnp_, Cohestra::CHI::SNPFlit);
        MATRIX_PACKED_CHANNEL(chi_sn0_rxreq_, Cohestra::CHI::REQFlit);
        MATRIX_PACKED_CHANNEL(chi_sn0_txrsp_, Cohestra::CHI::RSPFlit);
        MATRIX_PACKED_CHANNEL(chi_sn0_txdat_, Cohestra::CHI::DATFlit);
        MATRIX_PACKED_CHANNEL(chi_sn0_rxdat_, Cohestra::CHI::DATFlit);

        MATRIX_PACKED_MONITOR(mon_chi_rn0_txreq_, Cohestra::CHI::REQFlit);
        MATRIX_PACKED_MONITOR(mon_chi_rn0_txrsp_, Cohestra::CHI::RSPFlit);
        MATRIX_PACKED_MONITOR(mon_chi_rn0_txdat_, Cohestra::CHI::DATFlit);
        MATRIX_PACKED_MONITOR(mon_chi_rn0_rxrsp_, Cohestra::CHI::RSPFlit);
        MATRIX_PACKED_MONITOR(mon_chi_rn0_rxdat_, Cohestra::CHI::DATFlit);
        MATRIX_PACKED_MONITOR(mon_chi_rn0_rxsnp_, Cohestra::CHI::SNPFlit);
        MATRIX_PACKED_MONITOR(mon_chi_sn0_rxreq_, Cohestra::CHI::REQFlit);
        MATRIX_PACKED_MONITOR(mon_chi_sn0_txrsp_, Cohestra::CHI::RSPFlit);
        MATRIX_PACKED_MONITOR(mon_chi_sn0_txdat_, Cohestra::CHI::DATFlit);
        MATRIX_PACKED_MONITOR(mon_chi_sn0_rxdat_, Cohestra::CHI::DATFlit);
    };

    Cohestra::CHI::REQFlit MakeRequest(uint16_t txnID)
    {
        Cohestra::CHI::REQFlit flit{};
        flit.QoS() = 0x5;
        flit.TgtID() = 0x12;
        flit.SrcID() = 0x23;
        flit.TxnID() = txnID;
        flit.ReturnNID() = 0x34;
        flit.StashNIDValid() = 1;
        flit.ReturnTxnID() = 0x500 | (txnID & 0xff);
        flit.Opcode() = 0x45;
        flit.Size() = 6;
        flit.Addr() = 0x123456789000ULL | txnID;
        flit.NS() = 1;
        flit.LikelyShared() = 1;
        flit.AllowRetry() = 1;
        flit.Order() = 2;
        flit.PCrdType() = 0xa;
        flit.MemAttr() = 0xd;
        flit.SnpAttr() = 1;
        flit.TagGroupID() = 0xa7;
        flit.Excl() = 1;
        flit.ExpCompAck() = 1;
        flit.TagOp() = 2;
        flit.TraceTag() = 1;
        flit.RSVDC() = 0xb;
        return flit;
    }

    Cohestra::CHI::RSPFlit MakeResponse(uint16_t txnID)
    {
        Cohestra::CHI::RSPFlit flit{};
        flit.QoS() = 0x9;
        flit.TgtID() = 0x26;
        flit.SrcID() = 0x35;
        flit.TxnID() = txnID;
        flit.Opcode() = 0x12;
        flit.RespErr() = 2;
        flit.Resp() = 5;
        flit.FwdState() = 6;
        flit.CBusy() = 5;
        flit.DBID() = 0xa00 | (txnID & 0xff);
        flit.PCrdType() = 0xb;
        flit.TagOp() = 1;
        flit.TraceTag() = 1;
        return flit;
    }

    Cohestra::CHI::DATFlit MakeData(uint16_t txnID)
    {
        Cohestra::CHI::DATFlit flit{};
        flit.QoS() = 0xc;
        flit.TgtID() = 0x17;
        flit.SrcID() = 0x29;
        flit.TxnID() = txnID;
        flit.HomeNID() = 0x3b;
        flit.Opcode() = 0x15;
        flit.RespErr() = 1;
        flit.Resp() = 3;
        flit.DataSource() = 0xb;
        flit.CBusy() = 6;
        flit.DBID() = 0x600 | (txnID & 0xff);
        flit.CCID() = 2;
        flit.DataID() = 2;
        flit.TagOp() = 2;
        flit.Tag() = 0x5a;
        flit.TU() = 1;
        flit.TraceTag() = 1;
        flit.RSVDC() = 0xe;
        flit.BE() = 0xcc339669;
        flit.Data()[0] = 0x0123456789abcdefULL | txnID;
        flit.Data()[1] = 0x1032547698badcfeULL | (uint64_t(txnID) << 16);
        flit.Data()[2] = 0xfedcba9876543210ULL | (uint64_t(txnID) << 32);
        flit.Data()[3] = 0xefcdab8967452301ULL | (uint64_t(txnID) << 48);
        return flit;
    }

    Cohestra::CHI::SNPFlit MakeSnoop(uint16_t txnID)
    {
        Cohestra::CHI::SNPFlit flit{};
        flit.QoS() = 0x7;
        flit.SrcID() = 0x4d;
        flit.TxnID() = txnID;
        flit.FwdNID() = 0x5e;
        flit.FwdTxnID() = 0xd00 | (txnID & 0xff);
        flit.Opcode() = 0x15;
        flit.Addr() = 0x0fedcba98000ULL | txnID;
        flit.NS() = 1;
        flit.DoNotGoToSD() = 1;
        flit.RetToSrc() = 1;
        flit.TraceTag() = 1;
        return flit;
    }

#define MATRIX_CHECK_FLIT_FIELD(actual, name) \
    LUNATIC_CHECK(actual.name() == expected.name());

    void CheckFlit(const Cohestra::CHI::REQFlit& actual, const Cohestra::CHI::REQFlit& expected)
    {
        MATRIX_REQ_FIELDS(MATRIX_CHECK_FLIT_FIELD, actual)
    }

    void CheckFlit(const Cohestra::CHI::RSPFlit& actual, const Cohestra::CHI::RSPFlit& expected)
    {
        MATRIX_RSP_FIELDS(MATRIX_CHECK_FLIT_FIELD, actual)
    }

    void CheckFlit(const Cohestra::CHI::DATFlit& actual, const Cohestra::CHI::DATFlit& expected)
    {
        MATRIX_DAT_SCALAR_FIELDS(MATRIX_CHECK_FLIT_FIELD, actual)
        for (size_t index = 0; index < Cohestra::CHI::DATFlit::DATA_WIDTH / 64; ++index)
            LUNATIC_CHECK(actual.Data()[index] == expected.Data()[index]);
    }

    void CheckFlit(const Cohestra::CHI::SNPFlit& actual, const Cohestra::CHI::SNPFlit& expected)
    {
        MATRIX_SNP_FIELDS(MATRIX_CHECK_FLIT_FIELD, actual)
    }

    template<class Value>
    void CheckPinValue(uint64_t signal, const Value& value)
    {
        if constexpr (!std::is_same_v<std::remove_cvref_t<Value>, std::monostate>)
            LUNATIC_CHECK(signal == static_cast<uint64_t>(value));
    }

#define MATRIX_CHECK_PIN_FIELD(prefix, name) \
    CheckPinValue(mock.prefix##name, expected.name());

#define MATRIX_CHECK_REQ_PINS(prefix) MATRIX_REQ_FIELDS(MATRIX_CHECK_PIN_FIELD, prefix)
#define MATRIX_CHECK_RSP_PINS(prefix) MATRIX_RSP_FIELDS(MATRIX_CHECK_PIN_FIELD, prefix)
#define MATRIX_CHECK_SNP_PINS(prefix) MATRIX_SNP_FIELDS(MATRIX_CHECK_PIN_FIELD, prefix)
#define MATRIX_CHECK_DAT_PINS(prefix) \
    MATRIX_DAT_SCALAR_FIELDS(MATRIX_CHECK_PIN_FIELD, prefix) \
    for (size_t index = 0; index < Cohestra::CHI::DATFlit::DATA_WIDTH / 64; ++index) \
        LUNATIC_CHECK(mock.prefix##Data[index] == expected.Data()[index]);

    template<class Policy, class Flit>
    void CheckSeparateChannel(SeparateMock& mock, const Flit& expected)
    {
        static_assert(Policy::FlitWidth() == Flit::WIDTH);
        Flit actual{};
        LUNATIC_CHECK(!Policy::SampleFlit(mock, &actual));
        Policy::DriveFlit(mock, &expected, true);
        LUNATIC_CHECK(Policy::Traits::FlitV(mock) == 1);
        LUNATIC_CHECK(Policy::SampleFlit(mock, &actual));
        CheckFlit(actual, expected);
    }

    void PackFlit(const Cohestra::CHI::REQFlit& flit, uint32_t* bits)
    {
        V3CHIPackREQ(flit, bits);
    }

    void PackFlit(const Cohestra::CHI::RSPFlit& flit, uint32_t* bits)
    {
        V3CHIPackRSP(flit, bits);
    }

    void PackFlit(const Cohestra::CHI::DATFlit& flit, uint32_t* bits)
    {
        V3CHIPackDAT(flit, bits);
    }

    void PackFlit(const Cohestra::CHI::SNPFlit& flit, uint32_t* bits)
    {
        V3CHIPackSNP(flit, bits);
    }

    template<class Policy, class Flit>
    void CheckPackedChannel(PackedMock& mock, const Flit& expected)
    {
        using Traits = typename Policy::Traits;
        static_assert(Policy::FlitWidth() == Flit::WIDTH);
        Flit actual{};
        LUNATIC_CHECK(!Policy::SampleFlit(mock, &actual));
        Policy::DriveFlit(mock, &expected, true);
        LUNATIC_CHECK(Traits::FlitV(mock) == 1);

        std::array<uint32_t, (Flit::WIDTH + 31) / 32> expectedBits{};
        PackFlit(expected, expectedBits.data());
        LUNATIC_CHECK(std::equal(expectedBits.begin(), expectedBits.end(), Traits::Bits(mock)));
        LUNATIC_CHECK(Policy::SampleFlit(mock, &actual));
        CheckFlit(actual, expected);
    }

    template<class Policy, class Flit>
    void PopulateSeparateMonitor(SeparateMock& mock, const Flit& expected)
    {
        using Traits = typename Policy::Traits;
        if constexpr (std::is_same_v<Flit, Cohestra::CHI::REQFlit>)
            V3CHISeparateFieldDriveREQ<SeparateMock, Traits>(mock, expected);
        else if constexpr (std::is_same_v<Flit, Cohestra::CHI::RSPFlit>)
            V3CHISeparateFieldDriveRSP<SeparateMock, Traits>(mock, expected);
        else if constexpr (std::is_same_v<Flit, Cohestra::CHI::DATFlit>)
            V3CHISeparateFieldDriveDAT<SeparateMock, Traits>(mock, expected);
        else if constexpr (std::is_same_v<Flit, Cohestra::CHI::SNPFlit>)
            V3CHISeparateFieldDriveSNP<SeparateMock, Traits>(mock, expected);
        Traits::FlitV(mock) = 1;
        Traits::LCrdV(mock) = 1;
    }

    template<template<class, size_t, class> class Policy, class ChannelTag, class Flit>
    void CheckSeparateMonitor(SeparateMock& mock, const Flit& expected, uint64_t time)
    {
        using ChannelPolicy = Policy<SeparateMock, 0, ChannelTag>;
        CHICreditChannel<Flit> monitor;
        monitor.template MonitorObserve<SeparateMock, Policy, 0, ChannelTag>(mock, time - 1);
        LUNATIC_CHECK(!monitor.HasFiredObservation());

        PopulateSeparateMonitor<ChannelPolicy>(mock, expected);
        LUNATIC_CHECK(ChannelPolicy::SampleLCrdV(mock));
        monitor.template MonitorObserve<SeparateMock, Policy, 0, ChannelTag>(mock, time);
        LUNATIC_CHECK(monitor.HasFiredObservation());
        const auto fired = monitor.PopFiredObservation();
        LUNATIC_CHECK(fired.has_value());
        LUNATIC_CHECK(fired->Time == time);
        CheckFlit(fired->Flit, expected);
        LUNATIC_CHECK(!monitor.HasFiredObservation());
    }

    template<template<class, size_t, class> class Policy, class ChannelTag, class Flit>
    void CheckPackedMonitor(PackedMock& mock, const Flit& expected, uint64_t time)
    {
        using ChannelPolicy = Policy<PackedMock, 0, ChannelTag>;
        using Traits = typename ChannelPolicy::Traits;
        CHICreditChannel<Flit> monitor;
        monitor.template MonitorObserve<PackedMock, Policy, 0, ChannelTag>(mock, time - 1);
        LUNATIC_CHECK(!monitor.HasFiredObservation());

        PackFlit(expected, Traits::Bits(mock));
        Traits::FlitV(mock) = 1;
        Traits::LCrdV(mock) = 1;
        LUNATIC_CHECK(ChannelPolicy::SampleLCrdV(mock));
        monitor.template MonitorObserve<PackedMock, Policy, 0, ChannelTag>(mock, time);
        LUNATIC_CHECK(monitor.HasFiredObservation());
        const auto fired = monitor.PopFiredObservation();
        LUNATIC_CHECK(fired.has_value());
        LUNATIC_CHECK(fired->Time == time);
        CheckFlit(fired->Flit, expected);
        LUNATIC_CHECK(!monitor.HasFiredObservation());
    }

}

LUNATIC_TEST(Stage9CodecMatrixSeparateRNRequestsAndSnoops)
{
    SeparateMock mock;
    {
        const auto expected = MakeRequest(0x111);
        using Policy = V3CHISeparateFieldRNChannelPolicy<SeparateMock, 0, TXREQChannel>;
        CheckSeparateChannel<Policy>(mock, expected);
        MATRIX_CHECK_REQ_PINS(chi_rn0_txreq_bits_)
        LUNATIC_CHECK(mock.chi_sn0_rxreq_flitv == 0);
    }
    {
        const auto expected = MakeSnoop(0x222);
        using Policy = V3CHISeparateFieldRNChannelPolicy<SeparateMock, 0, RXSNPChannel>;
        CheckSeparateChannel<Policy>(mock, expected);
        MATRIX_CHECK_SNP_PINS(chi_rn0_rxsnp_bits_)
        LUNATIC_CHECK(mock.chi_rn0_txreq_bits_TxnID == 0x111);
    }
}

LUNATIC_TEST(Stage9CodecMatrixSeparateRNTX)
{
    SeparateMock mock;
    {
        const auto expected = MakeResponse(0x333);
        using Policy = V3CHISeparateFieldRNChannelPolicy<SeparateMock, 0, TXRSPChannel>;
        CheckSeparateChannel<Policy>(mock, expected);
        MATRIX_CHECK_RSP_PINS(chi_rn0_txrsp_bits_)
        LUNATIC_CHECK(mock.chi_rn0_rxrsp_flitv == 0);
        LUNATIC_CHECK(mock.chi_sn0_txrsp_flitv == 0);
    }
    {
        const auto expected = MakeData(0x444);
        using Policy = V3CHISeparateFieldRNChannelPolicy<SeparateMock, 0, TXDATChannel>;
        CheckSeparateChannel<Policy>(mock, expected);
        MATRIX_CHECK_DAT_PINS(chi_rn0_txdat_bits_)
        LUNATIC_CHECK(mock.chi_rn0_rxdat_flitv == 0);
        LUNATIC_CHECK(mock.chi_sn0_txdat_flitv == 0);
    }
}

LUNATIC_TEST(Stage9CodecMatrixSeparateSN)
{
    SeparateMock mock;
    {
        const auto expected = MakeRequest(0x555);
        using Policy = V3CHISeparateFieldSNChannelPolicy<SeparateMock, 0, RXREQChannel>;
        CheckSeparateChannel<Policy>(mock, expected);
        MATRIX_CHECK_REQ_PINS(chi_sn0_rxreq_bits_)
        LUNATIC_CHECK(mock.chi_rn0_txreq_flitv == 0);
    }
    {
        const auto expected = MakeResponse(0x666);
        using Policy = V3CHISeparateFieldSNChannelPolicy<SeparateMock, 0, TXRSPChannel>;
        CheckSeparateChannel<Policy>(mock, expected);
        MATRIX_CHECK_RSP_PINS(chi_sn0_txrsp_bits_)
        LUNATIC_CHECK(mock.chi_rn0_txrsp_flitv == 0);
    }
    {
        const auto expected = MakeData(0x777);
        using Policy = V3CHISeparateFieldSNChannelPolicy<SeparateMock, 0, TXDATChannel>;
        CheckSeparateChannel<Policy>(mock, expected);
        MATRIX_CHECK_DAT_PINS(chi_sn0_txdat_bits_)
        LUNATIC_CHECK(mock.chi_rn0_txdat_flitv == 0);
        LUNATIC_CHECK(mock.chi_sn0_rxdat_flitv == 0);
    }
}

LUNATIC_TEST(Stage9CodecMatrixPackedRN)
{
    PackedMock mock;
    {
        const auto expected = MakeRequest(0x181);
        CheckPackedChannel<V3CHIPackedRNChannelPolicy<PackedMock, 0, TXREQChannel>>(mock, expected);
        LUNATIC_CHECK(mock.chi_rn0_txreq_flitv == 1);
        LUNATIC_CHECK(mock.chi_sn0_rxreq_flitv == 0);
    }
    {
        const auto expected = MakeResponse(0x282);
        CheckPackedChannel<V3CHIPackedRNChannelPolicy<PackedMock, 0, TXRSPChannel>>(mock, expected);
        LUNATIC_CHECK(mock.chi_rn0_txrsp_flitv == 1);
        LUNATIC_CHECK(mock.chi_rn0_rxrsp_flitv == 0);
    }
    {
        const auto expected = MakeData(0x383);
        CheckPackedChannel<V3CHIPackedRNChannelPolicy<PackedMock, 0, TXDATChannel>>(mock, expected);
        LUNATIC_CHECK(mock.chi_rn0_txdat_flitv == 1);
        LUNATIC_CHECK(mock.chi_rn0_rxdat_flitv == 0);
    }
    {
        const auto expected = MakeResponse(0x484);
        CheckPackedChannel<V3CHIPackedRNChannelPolicy<PackedMock, 0, RXRSPChannel>>(mock, expected);
        LUNATIC_CHECK(mock.chi_rn0_rxrsp_flitv == 1);
        LUNATIC_CHECK(mock.chi_rn0_txrsp_bits[0] != 0);
    }
    {
        const auto expected = MakeData(0x585);
        CheckPackedChannel<V3CHIPackedRNChannelPolicy<PackedMock, 0, RXDATChannel>>(mock, expected);
        LUNATIC_CHECK(mock.chi_rn0_rxdat_flitv == 1);
        LUNATIC_CHECK(mock.chi_rn0_txdat_flitv == 1);
    }
    {
        const auto expected = MakeSnoop(0x686);
        CheckPackedChannel<V3CHIPackedRNChannelPolicy<PackedMock, 0, RXSNPChannel>>(mock, expected);
        LUNATIC_CHECK(mock.chi_rn0_rxsnp_flitv == 1);
        LUNATIC_CHECK(mock.chi_rn0_txreq_flitv == 1);
    }
}

LUNATIC_TEST(Stage9CodecMatrixPackedSN)
{
    PackedMock mock;
    {
        const auto expected = MakeRequest(0x787);
        CheckPackedChannel<V3CHIPackedSNChannelPolicy<PackedMock, 0, RXREQChannel>>(mock, expected);
        LUNATIC_CHECK(mock.chi_sn0_rxreq_flitv == 1);
        LUNATIC_CHECK(mock.chi_rn0_txreq_flitv == 0);
    }
    {
        const auto expected = MakeResponse(0x888);
        CheckPackedChannel<V3CHIPackedSNChannelPolicy<PackedMock, 0, TXRSPChannel>>(mock, expected);
        LUNATIC_CHECK(mock.chi_sn0_txrsp_flitv == 1);
        LUNATIC_CHECK(mock.chi_rn0_txrsp_flitv == 0);
    }
    {
        const auto expected = MakeData(0x989);
        CheckPackedChannel<V3CHIPackedSNChannelPolicy<PackedMock, 0, TXDATChannel>>(mock, expected);
        LUNATIC_CHECK(mock.chi_sn0_txdat_flitv == 1);
        LUNATIC_CHECK(mock.chi_sn0_rxdat_flitv == 0);
    }
    {
        const auto expected = MakeData(0xa8a);
        CheckPackedChannel<V3CHIPackedSNChannelPolicy<PackedMock, 0, RXDATChannel>>(mock, expected);
        LUNATIC_CHECK(mock.chi_sn0_rxdat_flitv == 1);
        LUNATIC_CHECK(mock.chi_sn0_txdat_flitv == 1);
    }
}

LUNATIC_TEST(Stage9CodecMatrixSeparateMonitors)
{
    SeparateMock mock;
    {
        const auto expected = MakeRequest(0xb01);
        CheckSeparateMonitor<V3CHISeparateFieldMonitorRNChannelPolicy, TXREQChannel>(mock, expected, 101);
        LUNATIC_CHECK(mock.mon_chi_rn0_txreq_flit_TxnID == expected.TxnID());
    }
    {
        const auto expected = MakeResponse(0xb02);
        CheckSeparateMonitor<V3CHISeparateFieldMonitorRNChannelPolicy, TXRSPChannel>(mock, expected, 102);
        LUNATIC_CHECK(mock.mon_chi_rn0_txrsp_flit_TxnID == expected.TxnID());
    }
    {
        const auto expected = MakeData(0xb03);
        CheckSeparateMonitor<V3CHISeparateFieldMonitorRNChannelPolicy, TXDATChannel>(mock, expected, 103);
        LUNATIC_CHECK(mock.mon_chi_rn0_txdat_flit_Data[3] == expected.Data()[3]);
    }
    {
        const auto expected = MakeResponse(0xb04);
        CheckSeparateMonitor<V3CHISeparateFieldMonitorRNChannelPolicy, RXRSPChannel>(mock, expected, 104);
        LUNATIC_CHECK(mock.mon_chi_rn0_rxrsp_flit_DBID == expected.DBID());
    }
    {
        const auto expected = MakeData(0xb05);
        CheckSeparateMonitor<V3CHISeparateFieldMonitorRNChannelPolicy, RXDATChannel>(mock, expected, 105);
        LUNATIC_CHECK(mock.mon_chi_rn0_rxdat_flit_Data[0] == expected.Data()[0]);
    }
    {
        const auto expected = MakeSnoop(0xb06);
        CheckSeparateMonitor<V3CHISeparateFieldMonitorRNChannelPolicy, RXSNPChannel>(mock, expected, 106);
        LUNATIC_CHECK(mock.mon_chi_rn0_rxsnp_flit_FwdTxnID == expected.FwdTxnID());
    }
    {
        const auto expected = MakeRequest(0xb07);
        CheckSeparateMonitor<V3CHISeparateFieldMonitorSNChannelPolicy, RXREQChannel>(mock, expected, 107);
        LUNATIC_CHECK(mock.mon_chi_sn0_rxreq_flit_ReturnTxnID == expected.ReturnTxnID());
    }
    {
        const auto expected = MakeResponse(0xb08);
        CheckSeparateMonitor<V3CHISeparateFieldMonitorSNChannelPolicy, TXRSPChannel>(mock, expected, 108);
        LUNATIC_CHECK(mock.mon_chi_sn0_txrsp_flit_DataPull == expected.DataPull());
    }
    {
        const auto expected = MakeData(0xb09);
        CheckSeparateMonitor<V3CHISeparateFieldMonitorSNChannelPolicy, TXDATChannel>(mock, expected, 109);
        LUNATIC_CHECK(mock.mon_chi_sn0_txdat_flit_DataSource == expected.DataSource());
    }
    {
        const auto expected = MakeData(0xb0a);
        CheckSeparateMonitor<V3CHISeparateFieldMonitorSNChannelPolicy, RXDATChannel>(mock, expected, 110);
        LUNATIC_CHECK(mock.mon_chi_sn0_rxdat_flit_BE == expected.BE());
    }
}

LUNATIC_TEST(Stage9CodecMatrixPackedMonitors)
{
    PackedMock mock;
    {
        const auto expected = MakeRequest(0xc11);
        CheckPackedMonitor<V3CHIPackedMonitorRNChannelPolicy, TXREQChannel>(mock, expected, 201);
        LUNATIC_CHECK(mock.mon_chi_rn0_txreq_flitv == 1);
    }
    {
        const auto expected = MakeResponse(0xc12);
        CheckPackedMonitor<V3CHIPackedMonitorRNChannelPolicy, TXRSPChannel>(mock, expected, 202);
        LUNATIC_CHECK(mock.mon_chi_rn0_txrsp_flitv == 1);
    }
    {
        const auto expected = MakeData(0xc13);
        CheckPackedMonitor<V3CHIPackedMonitorRNChannelPolicy, TXDATChannel>(mock, expected, 203);
        LUNATIC_CHECK(mock.mon_chi_rn0_txdat_flitv == 1);
    }
    {
        const auto expected = MakeResponse(0xc14);
        CheckPackedMonitor<V3CHIPackedMonitorRNChannelPolicy, RXRSPChannel>(mock, expected, 204);
        LUNATIC_CHECK(mock.mon_chi_rn0_rxrsp_flitv == 1);
    }
    {
        const auto expected = MakeData(0xc15);
        CheckPackedMonitor<V3CHIPackedMonitorRNChannelPolicy, RXDATChannel>(mock, expected, 205);
        LUNATIC_CHECK(mock.mon_chi_rn0_rxdat_flitv == 1);
    }
    {
        const auto expected = MakeSnoop(0xc16);
        CheckPackedMonitor<V3CHIPackedMonitorRNChannelPolicy, RXSNPChannel>(mock, expected, 206);
        LUNATIC_CHECK(mock.mon_chi_rn0_rxsnp_flitv == 1);
    }
    {
        const auto expected = MakeRequest(0xc17);
        CheckPackedMonitor<V3CHIPackedMonitorSNChannelPolicy, RXREQChannel>(mock, expected, 207);
        LUNATIC_CHECK(mock.mon_chi_sn0_rxreq_flitv == 1);
    }
    {
        const auto expected = MakeResponse(0xc18);
        CheckPackedMonitor<V3CHIPackedMonitorSNChannelPolicy, TXRSPChannel>(mock, expected, 208);
        LUNATIC_CHECK(mock.mon_chi_sn0_txrsp_flitv == 1);
    }
    {
        const auto expected = MakeData(0xc19);
        CheckPackedMonitor<V3CHIPackedMonitorSNChannelPolicy, TXDATChannel>(mock, expected, 209);
        LUNATIC_CHECK(mock.mon_chi_sn0_txdat_flitv == 1);
    }
    {
        const auto expected = MakeData(0xc1a);
        CheckPackedMonitor<V3CHIPackedMonitorSNChannelPolicy, RXDATChannel>(mock, expected, 210);
        LUNATIC_CHECK(mock.mon_chi_sn0_rxdat_flitv == 1);
    }
}

#undef MATRIX_CHECK_DAT_PINS
#undef MATRIX_CHECK_SNP_PINS
#undef MATRIX_CHECK_RSP_PINS
#undef MATRIX_CHECK_REQ_PINS
#undef MATRIX_CHECK_PIN_FIELD
#undef MATRIX_CHECK_FLIT_FIELD
#undef MATRIX_PACKED_MONITOR
#undef MATRIX_PACKED_CHANNEL
#undef MATRIX_DAT_MEMBERS
#undef MATRIX_SNP_MEMBERS
#undef MATRIX_RSP_MEMBERS
#undef MATRIX_REQ_MEMBERS
#undef MATRIX_SNP_FIELDS
#undef MATRIX_DAT_SCALAR_FIELDS
#undef MATRIX_RSP_FIELDS
#undef MATRIX_REQ_FIELDS
#undef MATRIX_SCALAR_MEMBER
#undef MATRIX_CONTROLS
