#include "sequence.hpp"

#include <algorithm>
#include <vector>

using namespace XactTest;

namespace {

    using Config128 = FlitConfiguration<7, 48, 0, 0, 128, false, false, false>;
    using Config256 = FlitConfiguration<7, 48, 0, 0, 256, false, false, false>;
    using Config512 = FlitConfiguration<7, 48, 0, 0, 512, false, false, false>;

    template<typename config>
    class RNCancelFixture {
    public:
        Xact::Global<config>   global;
        Xact::RNFJoint<config> joint;

        RNCancelFixture()
        {
            global.TOPOLOGY.SetNode(2, Xact::NodeType::HN_F);
            global.TOPOLOGY.SetNode(4, Xact::NodeType::RN_F);
        }
    };

    template<typename config>
    class SNCancelFixture {
    public:
        Xact::Global<config>   global;
        Xact::SNFJoint<config> joint;

        SNCancelFixture()
        {
            global.TOPOLOGY.SetNode(2, Xact::NodeType::HN_F);
            global.TOPOLOGY.SetNode(4, Xact::NodeType::RN_F);
            global.TOPOLOGY.SetNode(6, Xact::NodeType::SN_F);
        }
    };

    bool IsNoSnp(uint16_t opcode)
    {
        return opcode == Opcodes::REQ::WriteNoSnpPtl || opcode == Opcodes::REQ::WriteNoSnpFull;
    }

    template<typename config>
    Flits::REQ<config> CancelRequest(uint16_t opcode, uint8_t size, uint64_t address,
                                     bool home, bool expCompAck = false, bool dwt = false)
    {
        Flits::REQ<config> request{};
        request.SrcID() = home ? 2 : 4;
        request.TgtID() = home ? 6 : 2;
        request.TxnID() = 1;
        request.Opcode() = opcode;
        request.Addr() = address;
        request.Size() = size;
        request.AllowRetry() = 1;
        request.ExpCompAck() = expCompAck;
        request.SnpAttr() = home ? (dwt ? 1 : 0) : (IsNoSnp(opcode) ? 0 : 1);
        request.MemAttr() = IsNoSnp(opcode) ? MemAttrs::NonCacheable_Bufferable
                                            : MemAttrs::WriteBack_NoAllocate;
        request.DoDWT() = home && dwt;
        request.ReturnNID() = 2;
        request.ReturnTxnID() = 0;
        return request;
    }

    template<typename config>
    Flits::RSP<config> WriteResponse(bool home, uint16_t opcode, uint16_t txnId, uint16_t dbid = 77)
    {
        Flits::RSP<config> response{};
        response.SrcID() = home ? 6 : 2;
        response.TgtID() = home ? 2 : 4;
        response.TxnID() = txnId;
        response.Opcode() = opcode;
        response.DBID() = dbid;
        return response;
    }

    template<typename config>
    Flits::RSP<config> CompAckResponse()
    {
        Flits::RSP<config> response{};
        response.SrcID() = 4;
        response.TgtID() = 2;
        response.TxnID() = 77;
        response.Opcode() = Opcodes::RSP::CompAck;
        return response;
    }

    template<typename config>
    void SetFullBE(Flits::DAT<config>& data)
    {
        data.BE() = static_cast<typename Flits::DAT<config>::be_t>(~uint64_t(0));
    }

    template<typename config>
    Flits::DAT<config> WriteData(bool home, uint16_t opcode, uint8_t dataId, uint64_t address)
    {
        Flits::DAT<config> data{};
        data.SrcID() = home ? 2 : 4;
        data.TgtID() = home ? 6 : 2;
        data.TxnID() = 77;
        data.Opcode() = opcode;
        data.Resp() = 0;
        data.DataID() = dataId;
        data.CCID() = (address >> 4) & 3;
        data.BE() = 0;
        return data;
    }

    template<typename config>
    void StartRN(RNCancelFixture<config>& fixture, uint16_t opcode, uint8_t size, uint64_t address,
                 bool expCompAck = false)
    {
        XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1,
            CancelRequest<config>(opcode, size, address, false, expCompAck)));
        XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 2,
            WriteResponse<config>(false, Opcodes::RSP::DBIDResp, 1)));
    }

    template<typename config>
    void StartSN(SNCancelFixture<config>& fixture, uint8_t size, uint64_t address, bool dwt = false)
    {
        auto request = CancelRequest<config>(Opcodes::REQ::WriteNoSnpPtl, size, address, true, false, dwt);
        if (dwt)
            request.TxnID() = 0;
        XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1, request));
        XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2,
            WriteResponse<config>(true, Opcodes::RSP::DBIDResp, dwt ? 0 : 1)));
    }

    template<typename config>
    void RunRNCancel(uint8_t size, uint64_t address, std::vector<uint8_t> dataIds,
                     bool reverse, bool earlyComp = false)
    {
        RNCancelFixture<config> fixture;
        StartRN(fixture, size == 6 ? Opcodes::REQ::WriteNoSnpFull : Opcodes::REQ::WriteNoSnpPtl,
                size, address);
        if (reverse)
            std::reverse(dataIds.begin(), dataIds.end());
        uint64_t time = 3;
        for (const uint8_t dataId : dataIds)
        {
            XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, time++,
                WriteData<config>(false, Opcodes::DAT::WriteDataCancel, dataId, address)));
            if (earlyComp && time == 4)
                XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, time++,
                    WriteResponse<config>(false, Opcodes::RSP::Comp, 1)));
        }
        if (!earlyComp)
            XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, time,
                WriteResponse<config>(false, Opcodes::RSP::Comp, 1)));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    template<typename config>
    void RunSNCancel(uint8_t size, uint64_t address, std::vector<uint8_t> dataIds,
                     bool reverse, bool earlyComp = false, bool dwt = false)
    {
        SNCancelFixture<config> fixture;
        StartSN(fixture, size, address, dwt);
        if (reverse)
            std::reverse(dataIds.begin(), dataIds.end());
        uint64_t time = 3;
        for (const uint8_t dataId : dataIds)
        {
            XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, time++,
                WriteData<config>(true, Opcodes::DAT::WriteDataCancel, dataId, address)));
            if (earlyComp && time == 4)
                XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, time++,
                    WriteResponse<config>(true, Opcodes::RSP::Comp, dwt ? 0 : 1)));
        }
        if (!earlyComp)
            XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, time,
                WriteResponse<config>(true, Opcodes::RSP::Comp, dwt ? 0 : 1)));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    template<typename config>
    void CheckRNDuplicate()
    {
        constexpr uint8_t size = config::dataWidth == 256 ? 6 : 5;
        constexpr uint64_t address = config::dataWidth == 256 ? 0x10000 : 0x78;
        constexpr uint8_t firstDataId = config::dataWidth == 256 ? 0 : 2;
        constexpr uint8_t secondDataId = config::dataWidth == 256 ? 2 : 3;
        RNCancelFixture<config> fixture;
        StartRN(fixture, Opcodes::REQ::WriteNoSnpPtl, size, address);
        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 3,
            WriteData<config>(false, Opcodes::DAT::WriteDataCancel, firstDataId, address)));
        XACT_CHECK(fixture.joint.NextTXDAT(fixture.global, 4,
            WriteData<config>(false, Opcodes::DAT::WriteDataCancel, firstDataId, address)) ==
            XactDenial::DENIED_DUPLICATED_DATAID);
        XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 5,
            WriteData<config>(false, Opcodes::DAT::WriteDataCancel, secondDataId, address)));
        XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 6,
            WriteResponse<config>(false, Opcodes::RSP::Comp, 1)));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    template<typename config>
    void CheckSNDuplicate()
    {
        constexpr uint8_t size = config::dataWidth == 256 ? 6 : 5;
        constexpr uint64_t address = config::dataWidth == 256 ? 0x10000 : 0x78;
        constexpr uint8_t firstDataId = config::dataWidth == 256 ? 0 : 2;
        constexpr uint8_t secondDataId = config::dataWidth == 256 ? 2 : 3;
        SNCancelFixture<config> fixture;
        StartSN(fixture, size, address);
        XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 3,
            WriteData<config>(true, Opcodes::DAT::WriteDataCancel, firstDataId, address)));
        XACT_CHECK(fixture.joint.NextRXDAT(fixture.global, 4,
            WriteData<config>(true, Opcodes::DAT::WriteDataCancel, firstDataId, address)) ==
            XactDenial::DENIED_DUPLICATED_DATAID);
        XACT_ACCEPT(fixture.joint.NextRXDAT(fixture.global, 5,
            WriteData<config>(true, Opcodes::DAT::WriteDataCancel, secondDataId, address)));
        XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 6,
            WriteResponse<config>(true, Opcodes::RSP::Comp, 1)));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

}

XACT_TEST(Stage6OracleWriteCancelRNTwoPacketBothOrders128)
{
    RunRNCancel<Config128>(5, 0x78, { 2, 3 }, false);
    RunRNCancel<Config128>(5, 0x78, { 2, 3 }, true);
}

XACT_TEST(Stage6OracleWriteCancelRNPacketGeometryAllWidths)
{
    RunRNCancel<Config128>(4, 0x10010, { 1 }, false);
    RunRNCancel<Config128>(6, 0x10000, { 0, 1, 2, 3 }, false);
    RunRNCancel<Config128>(6, 0x10000, { 0, 1, 2, 3 }, true);
    RunRNCancel<Config256>(4, 0x10010, { 0 }, false);
    RunRNCancel<Config256>(5, 0x10020, { 2 }, false);
    RunRNCancel<Config256>(6, 0x10000, { 0, 2 }, false);
    RunRNCancel<Config256>(6, 0x10000, { 0, 2 }, true);
    RunRNCancel<Config512>(4, 0x10010, { 0 }, false);
    RunRNCancel<Config512>(5, 0x10020, { 0 }, false);
    RunRNCancel<Config512>(6, 0x10000, { 0 }, false);
}

XACT_TEST(Stage6OracleWriteCancelRNDuplicateRejected)
{
    CheckRNDuplicate<Config128>();
    CheckRNDuplicate<Config256>();
}

XACT_TEST(Stage6OracleWriteCancelRNOnePacketControl256)
{
    RunRNCancel<Config256>(5, 0x10020, { 2 }, false);
}

XACT_TEST(Stage6OracleWriteCancelRNOrdinaryDataControls)
{
    for (const uint16_t opcode : { Opcodes::DAT::NonCopyBackWrData, Opcodes::DAT::NCBWrDataCompAck })
    {
        RNCancelFixture<Config256> fixture;
        StartRN(fixture, Opcodes::REQ::WriteNoSnpFull, 6, 0x10000,
                opcode == Opcodes::DAT::NCBWrDataCompAck);
        for (const uint8_t dataId : { uint8_t(0), uint8_t(2) })
        {
            auto data = WriteData<Config256>(false, opcode, dataId, 0x10000);
            SetFullBE(data);
            XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 3 + dataId, data));
        }
        XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 8,
            WriteResponse<Config256>(false, Opcodes::RSP::Comp, 1)));
        XACT_CHECK(!fixture.joint.HasInflight());
    }

    RNCancelFixture<Config128> duplicate;
    StartRN(duplicate, Opcodes::REQ::WriteNoSnpFull, 6, 0x10000);
    auto first = WriteData<Config128>(false, Opcodes::DAT::NonCopyBackWrData, 0, 0x10000);
    SetFullBE(first);
    XACT_ACCEPT(duplicate.joint.NextTXDAT(duplicate.global, 3, first));
    XACT_CHECK(duplicate.joint.NextTXDAT(duplicate.global, 4, first) ==
        XactDenial::DENIED_DUPLICATED_DATAID);
    for (const uint8_t dataId : { uint8_t(1), uint8_t(2), uint8_t(3) })
    {
        auto data = WriteData<Config128>(false, Opcodes::DAT::NonCopyBackWrData, dataId, 0x10000);
        SetFullBE(data);
        XACT_ACCEPT(duplicate.joint.NextTXDAT(duplicate.global, 5 + dataId, data));
    }
    XACT_ACCEPT(duplicate.joint.NextRXRSP(duplicate.global, 9,
        WriteResponse<Config128>(false, Opcodes::RSP::Comp, 1)));
    XACT_CHECK(!duplicate.joint.HasInflight());

    RNCancelFixture<Config128> mixed;
    StartRN(mixed, Opcodes::REQ::WriteNoSnpFull, 6, 0x10000);
    auto ordinary = WriteData<Config128>(false, Opcodes::DAT::NonCopyBackWrData, 0, 0x10000);
    SetFullBE(ordinary);
    XACT_ACCEPT(mixed.joint.NextTXDAT(mixed.global, 3, ordinary));
    XACT_CHECK(mixed.joint.NextTXDAT(mixed.global, 4,
        WriteData<Config128>(false, Opcodes::DAT::WriteDataCancel, 1, 0x10000)) ==
        XactDenial::DENIED_WRITEDATACANCEL_AFTER_NCBWRDATA);
    for (const uint8_t dataId : { uint8_t(1), uint8_t(2), uint8_t(3) })
    {
        auto data = WriteData<Config128>(false, Opcodes::DAT::NonCopyBackWrData, dataId, 0x10000);
        SetFullBE(data);
        XACT_ACCEPT(mixed.joint.NextTXDAT(mixed.global, 5 + dataId, data));
    }
    XACT_ACCEPT(mixed.joint.NextRXRSP(mixed.global, 9,
        WriteResponse<Config128>(false, Opcodes::RSP::Comp, 1)));
    XACT_CHECK(!mixed.joint.HasInflight());
}

XACT_TEST(Stage6OracleWriteCancelRNCompAckAndEarlyComp)
{
    RNCancelFixture<Config128> fixture;
    StartRN(fixture, Opcodes::REQ::WriteNoSnpPtl, 5, 0x78, true);
    XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 3,
        WriteData<Config128>(false, Opcodes::DAT::WriteDataCancel, 2, 0x78)));
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 4,
        WriteResponse<Config128>(false, Opcodes::RSP::Comp, 1)));
    XACT_ACCEPT(fixture.joint.NextTXDAT(fixture.global, 5,
        WriteData<Config128>(false, Opcodes::DAT::WriteDataCancel, 3, 0x78)));
    XACT_CHECK(fixture.joint.HasInflight());
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 6,
        CompAckResponse<Config128>()));
    XACT_CHECK(!fixture.joint.HasInflight());

    RunRNCancel<Config128>(5, 0x78, { 2, 3 }, false, true);
}

XACT_TEST(Stage6OracleWriteCancelRNRetryAck)
{
    RNCancelFixture<Config256> fixture;
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 1,
        CancelRequest<Config256>(Opcodes::REQ::WriteNoSnpFull, 6, 0x10000, false)));
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 2,
        WriteResponse<Config256>(false, Opcodes::RSP::RetryAck, 1, 0)));
    XACT_ACCEPT(fixture.joint.NextRXRSP(fixture.global, 3,
        WriteResponse<Config256>(false, Opcodes::RSP::PCrdGrant, 0, 0)));
    Flits::REQ<Config256> creditReturn{};
    creditReturn.SrcID() = 4;
    creditReturn.TgtID() = 2;
    creditReturn.Opcode() = Opcodes::REQ::PCrdReturn;
    XACT_ACCEPT(fixture.joint.NextTXREQ(fixture.global, 4, creditReturn));
    XACT_CHECK(!fixture.joint.HasInflight());
}

XACT_TEST(Stage6OracleWriteCancelSNTwoPacketBothOrders128)
{
    RunSNCancel<Config128>(5, 0x78, { 2, 3 }, false);
    RunSNCancel<Config128>(5, 0x78, { 2, 3 }, true);
}

XACT_TEST(Stage6OracleWriteCancelSNGeometryAndDWTControls)
{
    RunSNCancel<Config256>(5, 0x10020, { 2 }, false);
    RunSNCancel<Config256>(6, 0x10000, { 0, 2 }, false);
    RunSNCancel<Config256>(6, 0x10000, { 0, 2 }, true);
    RunSNCancel<Config512>(6, 0x10000, { 0 }, false);
    RunSNCancel<Config128>(5, 0x78, { 2, 3 }, false, false, true);
}

XACT_TEST(Stage6OracleWriteCancelSNDuplicateRejected)
{
    CheckSNDuplicate<Config128>();
    CheckSNDuplicate<Config256>();
}

XACT_TEST(Stage6OracleWriteCancelSNEarlyCompAndRetry)
{
    RunSNCancel<Config128>(5, 0x78, { 2, 3 }, false, true);

    SNCancelFixture<Config256> fixture;
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 1,
        CancelRequest<Config256>(Opcodes::REQ::WriteNoSnpFull, 6, 0x10000, true)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 2,
        WriteResponse<Config256>(true, Opcodes::RSP::RetryAck, 1, 0)));
    XACT_ACCEPT(fixture.joint.NextTXRSP(fixture.global, 3,
        WriteResponse<Config256>(true, Opcodes::RSP::PCrdGrant, 0, 0)));
    Flits::REQ<Config256> creditReturn{};
    creditReturn.SrcID() = 2;
    creditReturn.TgtID() = 6;
    creditReturn.Opcode() = Opcodes::REQ::PCrdReturn;
    XACT_ACCEPT(fixture.joint.NextRXREQ(fixture.global, 4, creditReturn));
    XACT_CHECK(!fixture.joint.HasInflight());
}
