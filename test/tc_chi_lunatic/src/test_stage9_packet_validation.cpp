#include "driver.hpp"

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    using TestDriver = Driver<config>;

    template<typename config>
    using REQ = typename TestDriver<config>::REQ;

    template<typename config>
    using DAT = typename TestDriver<config>::DAT;

    void CheckRejectedRequest(TestDriver<Config256>& driver, REQ<Config256> request)
    {
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        driver.Until([&] { return static_cast<bool>(driver.node.PeekTXRSP() || driver.node.PeekTXDAT()); }, 40);
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
        const auto requestClass = ClassifyRequest(request.Opcode());
        if (IsReadRequest(request.Opcode()) && request.Size() <= 6 && request.Order() != 1)
        {
            Request geometryRequest{};
            geometryRequest.opcode = request.Opcode();
            geometryRequest.address = request.Addr();
            geometryRequest.size = request.Size();
            geometryRequest.memAttr = request.MemAttr();
            geometryRequest.endian = request.Endian();
            Options options;
            const uint64_t mask = TransferBeatMask(geometryRequest, options);
            if (request.Order() != 0)
                LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::ReadReceipt).RespErr() == CHI::Eb::RespErrs::OK);
            DataBufferId dbid = 0;
            const bool separated = driver.node.GetOptions().separatedReadResponse;
            if (separated)
            {
                const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::RespSepData);
                LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
                dbid = response.DBID();
            }
            for (size_t beat = 0; beat < 4; ++beat)
            {
                if (!(mask & (uint64_t(1) << beat)))
                    continue;
                const auto data = driver.NextDAT(separated ? CHI::Eb::Opcodes::DAT::DataSepResp
                                                           : CHI::Eb::Opcodes::DAT::CompData);
                LUNATIC_CHECK(data.RespErr() == CHI::Eb::RespErrs::NDERR);
                LUNATIC_CHECK(data.TgtID() == request.SrcID() && data.TxnID() == request.TxnID());
                dbid = data.DBID();
            }
            if (request.ExpCompAck())
                driver.CompAck(request.SrcID(), dbid);
        }
        else if (requestClass == RequestClass::NonCoherentWrite && request.Size() <= 6)
        {
            const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
            ByteEnables enables{};
            enables.fill(1);
            driver.WriteData(request.SrcID(), request.TxnID(), dbid, LineData{}, enables,
                CHI::Eb::Opcodes::DAT::NonCopyBackWrData, 0, (request.Addr() >> 4) & 3);
            LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
        }
        else if (requestClass == RequestClass::CopybackWrite && request.Size() <= 6)
        {
            const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
            LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
            ByteEnables enables{};
            enables.fill(1);
            driver.WriteData(request.SrcID(), request.TxnID(), response.DBID(), LineData{}, enables,
                CHI::Eb::Opcodes::DAT::CopyBackWrData, CHI::Eb::Resps::CopyBackWrData_UD_PD,
                (request.Addr() >> 4) & 3);
        }
        else if ((requestClass == RequestClass::CMO || requestClass == RequestClass::CleanUnique ||
                  requestClass == RequestClass::MakeUnique || requestClass == RequestClass::Evict) &&
                 request.Size() <= 6)
        {
            const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
            LUNATIC_CHECK(response.RespErr() == CHI::Eb::RespErrs::NDERR);
            if (requestClass == RequestClass::CleanUnique || requestClass == RequestClass::MakeUnique ||
                request.ExpCompAck())
                driver.CompAck(request.SrcID(), response.DBID());
        }
        else if (requestClass == RequestClass::Atomic)
        {
            Request geometryRequest{};
            geometryRequest.opcode = request.Opcode();
            geometryRequest.address = request.Addr();
            geometryRequest.size = request.Size();
            AtomicGeometry geometry;
            if (GetAtomicGeometry(geometryRequest, geometry))
            {
                const DataBufferId dbid = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp).DBID();
                Options options;
                const uint64_t mask = AtomicOperandBeatMask(geometryRequest, options);
                constexpr size_t bytes = Config256::dataWidth / 8;
                for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
                {
                    if (!(mask & (uint64_t(1) << beat)))
                        continue;
                    DAT<Config256> data{};
                    data.SrcID() = request.SrcID();
                    data.TgtID() = request.TgtID();
                    data.TxnID() = dbid;
                    data.Opcode() = CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
                    data.DataID() = beat * bytes / 16;
                    data.CCID() = (request.Addr() >> 4) & 3;
                    for (size_t index = 0; index < bytes; ++index)
                    {
                        const size_t lineOffset = beat * bytes + index;
                        if (lineOffset >= geometry.windowOffset &&
                            lineOffset < geometry.windowOffset + geometry.windowBytes)
                            data.BE() = uint64_t(data.BE()) | (uint64_t(1) << index);
                    }
                    LUNATIC_CHECK(driver.node.PushRXDAT(data));
                }
                driver.Tick();
                if (CHI::Eb::Opcodes::REQ::AtomicStore::Is(request.Opcode()))
                    LUNATIC_CHECK(driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp).RespErr() == CHI::Eb::RespErrs::NDERR);
                else
                    LUNATIC_CHECK(driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).RespErr() == CHI::Eb::RespErrs::NDERR);
            }
            else
                driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        }
        else
            driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LUNATIC_CHECK(!driver.node.PeekTXREQ());
        LUNATIC_CHECK(!driver.node.PeekTXDAT());
        LUNATIC_CHECK(!driver.node.PeekTXSNP());
    }

    template<typename config>
    DAT<config> MakeWriteData(const typename TestDriver<config>::REQ& request,
                              DataBufferId dbid, size_t dataId, uint8_t value)
    {
        DAT<config> data{};
        data.SrcID() = request.SrcID();
        data.TgtID() = request.TgtID();
        data.TxnID() = dbid;
        data.Opcode() = IsCopybackRequest(request.Opcode())
            ? CHI::Eb::Opcodes::DAT::CopyBackWrData : CHI::Eb::Opcodes::DAT::NonCopyBackWrData;
        data.Resp() = IsCopybackRequest(request.Opcode())
            ? CHI::Eb::Resps::CopyBackWrData_UD_PD : CHI::Eb::Resps::Comp_I;
        data.DataID() = dataId;
        data.CCID() = (request.Addr() >> 4) & 3;
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t index = 0; index < bytes; ++index)
        {
            data.Data()[index / 8] |= uint64_t(value) << ((index % 8) * 8);
            data.BE() |= uint64_t(1) << index;
        }
        return data;
    }

    void ApplyWriteWindow(DAT<Config256>& data, uint64_t address, uint8_t size)
    {
        data.BE() = 0;
        for (auto& word : data.Data())
            word = 0;
        for (size_t index = 0; index < (size_t(1) << size); ++index)
        {
            const size_t byte = address % 16 + index;
            data.BE() = uint64_t(data.BE()) | (uint64_t(1) << byte);
            data.Data()[byte / 8] |= uint64_t(0x6b) << ((byte % 8) * 8);
        }
    }

    template<typename config>
    void CheckFullWriteBE(uint16_t opcode)
    {
        TestDriver<config> driver;
        auto request = driver.Request(opcode, 4, 1, 0x71030);
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto response = driver.NextRSP(IsCopybackRequest(opcode)
            ? CHI::Eb::Opcodes::RSP::CompDBIDResp : CHI::Eb::Opcodes::RSP::DBIDResp);
        constexpr size_t bytes = config::dataWidth / 8;
        constexpr size_t beats = LINE_SIZE / bytes;
        const size_t dataId = (request.Addr() % LINE_SIZE) / bytes * (bytes / 16);
        auto data = MakeWriteData<config>(request, response.DBID(), dataId, 0x5a);
        data.BE() = uint64_t(data.BE()) & ~uint64_t(1);
        LUNATIC_CHECK(driver.node.PushRXDAT(data));
        driver.Tick();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());

        for (size_t beat = 0; beat < beats; ++beat)
        {
            data = MakeWriteData<config>(request, response.DBID(), beat * bytes / 16, 0x5a);
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
        if (!IsCopybackRequest(opcode))
            driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
    }

    void CheckWriteDataMutation(uint16_t opcode, uint64_t address, uint8_t size,
                                const std::function<void(DAT<Config256>&)>& invalidate)
    {
        TestDriver<Config256> driver;
        auto request = driver.Request(opcode, 4, 1, address);
        request.Size() = size;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto response = driver.NextRSP(IsCopybackRequest(opcode)
            ? CHI::Eb::Opcodes::RSP::CompDBIDResp : CHI::Eb::Opcodes::RSP::DBIDResp);
        const size_t dataId = ((address >> 4) & 3) & ~size_t(1);
        auto data = MakeWriteData<Config256>(request, response.DBID(), dataId, 0x6b);
        if (size < 6)
            ApplyWriteWindow(data, address, size);
        invalidate(data);
        LUNATIC_CHECK(driver.node.PushRXDAT(data));
        driver.Tick();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());

        data = MakeWriteData<Config256>(request, response.DBID(), dataId, 0x6b);
        if (size < 6)
            ApplyWriteWindow(data, address, size);
        LUNATIC_CHECK(driver.node.PushRXDAT(data));
        if (size == 6)
        {
            data = MakeWriteData<Config256>(request, response.DBID(), dataId ^ 2, 0x6b);
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
        if (!IsCopybackRequest(opcode))
            driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
    }

    void CheckDeviceCancel()
    {
        TestDriver<Config256> driver;
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl, 4, 1, 0x71220);
        request.MemAttr() = CHI::Eb::MemAttrs::Device_nEWA;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::DBIDResp);
        auto cancel = MakeWriteData<Config256>(request, response.DBID(), 2, 0);
        cancel.Opcode() = CHI::Eb::Opcodes::DAT::WriteDataCancel;
        cancel.BE() = 0;
        LUNATIC_CHECK(driver.node.PushRXDAT(cancel));
        driver.Tick();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
    }

    template<typename config>
    DAT<config> MakeBackendData(const REQ<config>& request, size_t dataId, uint8_t value)
    {
        DAT<config> data{};
        data.SrcID() = 6;
        data.TgtID() = 2;
        data.HomeNID() = 2;
        data.TxnID() = request.TxnID();
        data.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
        data.Resp() = CHI::Eb::Resps::CompData_I;
        data.DataID() = dataId;
        data.CCID() = (request.Addr() >> 4) & 3;
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t index = 0; index < bytes; ++index)
            data.Data()[index / 8] |= uint64_t(value) << ((index % 8) * 8);
        return data;
    }

    template<typename config>
    void CheckBackendData(const std::function<void(DAT<config>&, DAT<config>&)>& invalidate)
    {
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        TestDriver<config> driver(options);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x71331);
        request.Size() = 4;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        constexpr size_t bytes = config::dataWidth / 8;
        const size_t dataId = (downstream.Addr() % LINE_SIZE) / bytes * (bytes / 16);
        auto first = MakeBackendData<config>(downstream, dataId, 0x7c);
        auto second = MakeBackendData<config>(downstream, dataId, 0x7c);
        invalidate(first, second);
        LUNATIC_CHECK(driver.node.PushRXDAT(first));
        if (uint8_t(first.Opcode()) != uint8_t(second.Opcode()) || first.DataID() != second.DataID() ||
            first.CCID() != second.CCID() || first.Resp() != second.Resp())
            LUNATIC_CHECK(driver.node.PushRXDAT(second));
        driver.Tick();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
        LUNATIC_CHECK(!driver.node.PeekTXDAT());

        const auto data = MakeBackendData<config>(downstream, dataId, 0x7c);
        LUNATIC_CHECK(driver.node.PushRXDAT(data));
        driver.Tick();
        const auto response = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        LUNATIC_CHECK(response.DataID() == dataId);
        LUNATIC_CHECK(uint8_t(response.Data()[0]) == 0x7c);
        driver.Quiesce();
    }

    template<typename config>
    void CheckBackendCommonResp()
    {
        if constexpr (config::dataWidth == 512)
            return;
        Options options;
        options.backend = BackendKind::CHI;
        options.subordinateId = 6;
        TestDriver<config> driver(options);
        LUNATIC_CHECK(driver.node.PushRXREQ(driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x71330)));
        const auto downstream = driver.NextREQ(CHI::Eb::Opcodes::REQ::ReadNoSnp);
        constexpr size_t bytes = config::dataWidth / 8;
        constexpr size_t beats = LINE_SIZE / bytes;
        const size_t firstId = (downstream.Addr() % LINE_SIZE) / bytes * (bytes / 16);
        const size_t secondId = ((downstream.Addr() % LINE_SIZE) / bytes + 1) % beats * (bytes / 16);
        LUNATIC_CHECK(driver.node.PushRXDAT(MakeBackendData<config>(downstream, firstId, 0x7c)));
        auto malformed = MakeBackendData<config>(downstream, secondId, 0x7c);
        malformed.Resp() = CHI::Eb::Resps::CompData_UC;
        LUNATIC_CHECK(driver.node.PushRXDAT(malformed));
        driver.Tick();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
        for (size_t beat = 1; beat < beats; ++beat)
        {
            const size_t dataId = ((downstream.Addr() % LINE_SIZE) / bytes + beat) % beats * (bytes / 16);
            LUNATIC_CHECK(driver.node.PushRXDAT(MakeBackendData<config>(downstream, dataId, 0x7c)));
        }
        driver.Tick();
        for (size_t beat = 0; beat < beats; ++beat)
            driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        driver.Quiesce();
    }

    template<typename config>
    void SeedUniqueProvider(TestDriver<config>& driver, const LineKey& line, NodeId source = 6)
    {
        driver.ReadData(source, 90, line.address + 0x30, CHI::Eb::Opcodes::REQ::ReadUnique, true);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.InspectRequester(line, source) == CacheState::UC);
        LUNATIC_CHECK(driver.node.GetErrors().empty());
    }

    LineData PacketPattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 7);
        return data;
    }

    template<typename config>
    DAT<config> MakeSnoopData(const typename TestDriver<config>::SNP& snoop, size_t dataId, bool partial = false)
    {
        DAT<config> data{};
        data.SrcID() = snoop.target;
        data.TgtID() = 2;
        data.TxnID() = snoop.flit.TxnID();
        data.Opcode() = partial ? CHI::Eb::Opcodes::DAT::SnpRespDataPtl : CHI::Eb::Opcodes::DAT::SnpRespData;
        data.Resp() = CHI::Eb::Resps::SnpRespData_SC;
        data.DataID() = dataId;
        data.CCID() = uint64_t(snoop.flit.Addr()) >> 1;
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t index = 0; index < bytes; ++index)
        {
            data.Data()[index / 8] = 0;
            data.BE() |= uint64_t(1) << index;
        }
        return data;
    }

    template<typename config>
    void ApplyLowerHalf(DAT<config>& data, const LineData& line, size_t beat)
    {
        constexpr size_t bytes = config::dataWidth / 8;
        data.BE() = 0;
        for (auto& word : data.Data())
            word = 0;
        for (size_t index = 0; index < bytes / 2; ++index)
        {
            data.Data()[index / 8] |= uint64_t(line[beat * bytes + index]) << ((index % 8) * 8);
            data.BE() |= uint64_t(1) << index;
        }
    }

    template<typename config>
    void SeedDirtyProvider(TestDriver<config>& driver, const LineKey& line, NodeId source)
    {
        driver.node.GetMemory().WriteFull(line, PacketPattern(37));
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteBackFull, source, 90, line.address + 0x30);
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
            LUNATIC_CHECK(driver.node.PushRXDAT(
                MakeWriteData<config>(request, response.DBID(), beat * bytes / 16, 0x5a)));
        driver.Tick();
        driver.Quiesce();
        driver.ReadData(source, 91, line.address + 0x30, CHI::Eb::Opcodes::REQ::ReadUnique, true);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.InspectRequester(line, source) == CacheState::UD);
    }

    template<typename config>
    LineData SeedPartialProvider(TestDriver<config>& driver, const LineKey& line)
    {
        driver.node.GetMemory().WriteFull(line, PacketPattern(19));
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::WriteBackPtl, 6, 90, line.address + 0x30);
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto response = driver.NextRSP(CHI::Eb::Opcodes::RSP::CompDBIDResp);
        const LineData provider = PacketPattern(51);
        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            auto data = MakeWriteData<config>(request, response.DBID(), beat * bytes / 16, 0);
            ApplyLowerHalf<config>(data, provider, beat);
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
        driver.Quiesce();
        driver.ReadData(6, 91, line.address + 0x30, CHI::Eb::Opcodes::REQ::ReadUnique, true);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.InspectRequester(line, 6) == CacheState::UDP);
        LUNATIC_CHECK(driver.node.GetErrors().empty());
        return driver.node.GetMemory().Read(line);
    }

    template<typename config>
    void CheckPartialSnoopData()
    {
        TestDriver<config> driver;
        const LineKey line = LineKey::FromAddress(0x71500, false);
        const LineData beforeSnoop = SeedPartialProvider(driver, line);
        const LineData update = PacketPattern(91);
        LUNATIC_CHECK(driver.node.PushRXREQ(
            driver.Request(CHI::Eb::Opcodes::REQ::CleanInvalid, 4, 1, line.address + 0x30)));
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
        auto malformed = MakeSnoopData<config>(snoop, (config::dataWidth / 8) / 16, true);
        malformed.Resp() = CHI::Eb::Resps::SnpRespDataPtl_I_PD;
        ApplyLowerHalf<config>(malformed, update, size_t(malformed.DataID()) * 16 / (config::dataWidth / 8));
        malformed.Data()[config::dataWidth / 64 - 1] |= uint64_t(1) << 56;
        LUNATIC_CHECK(driver.node.PushRXDAT(malformed));
        driver.Tick();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
        LUNATIC_CHECK(driver.node.InspectRequester(line, 6) == CacheState::UDP);
        LUNATIC_CHECK(driver.node.GetMemory().Read(line) == beforeSnoop);

        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            auto data = MakeSnoopData<config>(snoop, beat * bytes / 16, true);
            data.Resp() = CHI::Eb::Resps::SnpRespDataPtl_I_PD;
            ApplyLowerHalf<config>(data, update, beat);
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
        driver.NextRSP(CHI::Eb::Opcodes::RSP::Comp);
        driver.Quiesce();
        LineData expected = beforeSnoop;
        for (size_t index = 0; index < LINE_SIZE; index += bytes)
            for (size_t byte = 0; byte < bytes / 2; ++byte)
                expected[index + byte] = update[index + byte];
        LUNATIC_CHECK(driver.node.GetMemory().Read(line) == expected);
        LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    }

    template<typename config>
    void CheckForwardedSnoopCCID()
    {
        Options options;
        options.dctEnabled = true;
        TestDriver<config> driver(options);
        const LineKey line = LineKey::FromAddress(0x71600, false);
        SeedDirtyProvider(driver, line, 6);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, line.address + 0x30, true);
        request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpSharedFwd);
        auto malformed = MakeSnoopData<config>(snoop, (config::dataWidth / 8) / 16);
        malformed.Opcode() = CHI::Eb::Opcodes::DAT::SnpRespDataFwded;
        malformed.Resp() = CHI::Eb::Resps::SnpRespData_SC_PD_Fwded_SC;
        malformed.FwdState() = CHI::Eb::FwdStates::SC;
        malformed.CCID() = uint8_t(malformed.CCID()) ^ 1;
        LUNATIC_CHECK(driver.node.PushRXDAT(malformed));
        driver.Tick();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
        LUNATIC_CHECK(!driver.node.PeekTXDAT());

        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            auto data = MakeSnoopData<config>(snoop, beat * bytes / 16);
            data.Opcode() = CHI::Eb::Opcodes::DAT::SnpRespDataFwded;
            data.Resp() = CHI::Eb::Resps::SnpRespData_SC_PD_Fwded_SC;
            data.FwdState() = CHI::Eb::FwdStates::SC;
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
        driver.CompAck(4, snoop.flit.TxnID());
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    }

    template<typename config>
    void CheckVictimSnoopBE()
    {
        Options options;
        options.snoopFilterSets = 1;
        options.snoopFilterWays = 2;
        TestDriver<config> driver(options);
        const LineKey a = LineKey::FromAddress(0x71700, false);
        const LineKey b = LineKey::FromAddress(0x71740, false);
        const LineKey c = LineKey::FromAddress(0x71780, false);
        driver.node.GetEvents()->OnVictimSelection.Register(
            Gravity::MakeListener<VictimSelectionEvent>("packet-victim", 0, [&](VictimSelectionEvent& event) {
                if (event.domain == VictimDomain::SnoopFilter)
                    event.SelectVictim(1);
            }));
        SeedUniqueProvider(driver, a, 4);
        SeedDirtyProvider(driver, b, 5);
        LUNATIC_CHECK(driver.node.PushRXREQ(
            driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique, 6, 3, c.address + 0x30, true)));
        const auto victim = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpCleanInvalid);
        LUNATIC_CHECK(victim.target == 5);
        auto malformed = MakeSnoopData<config>(victim, (config::dataWidth / 8) / 16);
        malformed.Resp() = CHI::Eb::Resps::SnpRespData_I_PD;
        malformed.BE() = uint64_t(malformed.BE()) & ~uint64_t(1);
        LUNATIC_CHECK(driver.node.PushRXDAT(malformed));
        driver.Tick();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
        LUNATIC_CHECK(driver.node.InspectRequester(b, 5) == CacheState::UD);
        LUNATIC_CHECK(!driver.node.PeekTXDAT());

        constexpr size_t bytes = config::dataWidth / 8;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
        {
            auto data = MakeSnoopData<config>(victim, beat * bytes / 16);
            data.Resp() = CHI::Eb::Resps::SnpRespData_I_PD;
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
            dbid = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).DBID();
        driver.CompAck(6, dbid);
        driver.Quiesce();
        LUNATIC_CHECK(driver.node.InspectRequester(b, 5) == CacheState::I);
        LUNATIC_CHECK(driver.node.GetErrors().size() == 1);
    }

    template<typename config>
    void CheckSnoopData(const std::function<void(DAT<config>&)>& invalidate)
    {
        TestDriver<config> driver;
        const LineKey line = LineKey::FromAddress(0x71400, false);
        SeedUniqueProvider(driver, line);
        auto request = driver.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, line.address + 0x30, true);
        LUNATIC_CHECK(driver.node.PushRXREQ(request));
        const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
        auto malformed = MakeSnoopData<config>(snoop, (config::dataWidth / 8) / 16);
        invalidate(malformed);
        LUNATIC_CHECK(driver.node.PushRXDAT(malformed));
        driver.Tick();
        LUNATIC_CHECK(!driver.node.GetErrors().empty());
        LUNATIC_CHECK(driver.node.InspectRequester(line, 6) == CacheState::UC);
        LUNATIC_CHECK(!driver.node.PeekTXDAT());

        constexpr size_t beats = LINE_SIZE / (config::dataWidth / 8);
        for (size_t beat = 0; beat < beats; ++beat)
        {
            auto data = MakeSnoopData<config>(snoop, beat * (config::dataWidth / 8) / 16);
            LUNATIC_CHECK(driver.node.PushRXDAT(data));
        }
        driver.Tick();
        DataBufferId dbid = 0;
        for (size_t beat = 0; beat < beats; ++beat)
            dbid = driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData).DBID();
        driver.CompAck(4, dbid);
        driver.Quiesce();
    }

}

LUNATIC_TEST(Stage9PacketRequestAttributeValidation)
{
    TestDriver<Config256> factory;
    auto request = factory.Request(CHI::Eb::Opcodes::REQ::ReadShared, 4, 1, 0x71000);
    request.MemAttr() = CHI::Eb::MemAttrs::NonCacheable_Bufferable;
    CheckRejectedRequest(factory, request);

    TestDriver<Config256> driver;
    request = driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp, 4, 1, 0x71000);
    request.SnpAttr() = 1;
    CheckRejectedRequest(driver, request);

    TestDriver<Config256> makeInvalid;
    request = makeInvalid.Request(CHI::Eb::Opcodes::REQ::ReadOnceMakeInvalid, 4, 1, 0x71000);
    CheckRejectedRequest(makeInvalid, request);

    TestDriver<Config256> evictFull;
    request = evictFull.Request(CHI::Eb::Opcodes::REQ::WriteEvictFull, 4, 1, 0x71000);
    request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_NoAllocate;
    CheckRejectedRequest(evictFull, request);

    TestDriver<Config256> evict;
    request = evict.Request(CHI::Eb::Opcodes::REQ::Evict, 4, 1, 0x71000);
    request.MemAttr() = CHI::Eb::MemAttrs::WriteBack_Allocate;
    CheckRejectedRequest(evict, request);

    TestDriver<Config256> atomic;
    request = atomic.Request(CHI::Eb::Opcodes::REQ::AtomicStore::ADD, 4, 1, 0x71000);
    request.Size() = 0;
    request.MemAttr() = CHI::Eb::MemAttrs::Cacheable;
    CheckRejectedRequest(atomic, request);

    TestDriver<Config256> cmo;
    request = cmo.Request(CHI::Eb::Opcodes::REQ::CleanShared, 4, 1, 0x71000);
    request.MemAttr() = CHI::Eb::MemAttrs::Device_EWA;
    request.SnpAttr() = 1;
    CheckRejectedRequest(cmo, request);

    TestDriver<Config256> reserved;
    request = reserved.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 4, 1, 0x71000);
    request.MemAttr() = 0x0c;
    CheckRejectedRequest(reserved, request);
}

LUNATIC_TEST(Stage9PacketFullWriteByteEnableValidation)
{
    for (const uint16_t opcode : { CHI::Eb::Opcodes::REQ::WriteNoSnpFull,
             CHI::Eb::Opcodes::REQ::WriteUniqueFull, CHI::Eb::Opcodes::REQ::WriteBackFull })
    {
        CheckFullWriteBE<Config128>(opcode);
        CheckFullWriteBE<Config256>(opcode);
        CheckFullWriteBE<Config512>(opcode);
    }
}

LUNATIC_TEST(Stage9PacketWriteDisabledLaneAndCCIDValidation)
{
    CheckWriteDataMutation(CHI::Eb::Opcodes::REQ::WriteNoSnpPtl, 0x71123, 0,
        [](DAT<Config256>& data) { data.Data()[0] = 1; });
    CheckWriteDataMutation(CHI::Eb::Opcodes::REQ::WriteUniquePtl, 0x71130, 6,
        [](DAT<Config256>& data) { data.CCID() = uint8_t(data.CCID()) ^ 1; });
}

LUNATIC_TEST(Stage9PacketWriteDataCancelValidation)
{
    CheckDeviceCancel();
    CheckWriteDataMutation(CHI::Eb::Opcodes::REQ::WriteNoSnpFull, 0x71230, 6,
        [](DAT<Config256>& data) {
            data.Opcode() = CHI::Eb::Opcodes::DAT::WriteDataCancel;
            data.BE() = 0;
            for (auto& word : data.Data())
                word = 0;
        });
}

LUNATIC_TEST(Stage9PacketBackendReadMaskValidation)
{
    const auto invalidate = [](auto& first, auto& second) {
        first.DataID() = uint8_t(first.DataID()) ^ 2;
        second.DataID() = uint8_t(second.DataID()) ^ 2;
    };
    CheckBackendData<Config128>(invalidate);
    CheckBackendData<Config256>(invalidate);
    CheckBackendData<Config512>(invalidate);
}

LUNATIC_TEST(Stage9PacketBackendReadCCIDValidation)
{
    const auto invalidate = [](auto& first, auto& second) {
        first.CCID() = uint8_t(first.CCID()) ^ 1;
        second.CCID() = uint8_t(second.CCID()) ^ 1;
    };
    CheckBackendData<Config128>(invalidate);
    CheckBackendData<Config256>(invalidate);
    CheckBackendData<Config512>(invalidate);
}

LUNATIC_TEST(Stage9PacketBackendReadCommonRespValidation)
{
    CheckBackendCommonResp<Config128>();
    CheckBackendCommonResp<Config256>();
    CheckBackendCommonResp<Config512>();
}

LUNATIC_TEST(Stage9PacketSnoopFullBEValidation)
{
    const auto invalidate = [](auto& data) { data.BE() = uint64_t(data.BE()) & ~uint64_t(1); };
    CheckSnoopData<Config128>(invalidate);
    CheckSnoopData<Config256>(invalidate);
    CheckSnoopData<Config512>(invalidate);
}

LUNATIC_TEST(Stage9PacketSnoopCCIDValidation)
{
    const auto invalidate = [](auto& data) { data.CCID() = uint8_t(data.CCID()) ^ 1; };
    CheckSnoopData<Config128>(invalidate);
    CheckSnoopData<Config256>(invalidate);
    CheckSnoopData<Config512>(invalidate);
}

LUNATIC_TEST(Stage9PacketSnoopDisabledLaneValidation)
{
    const auto invalidate = [](auto& data) {
        data.BE() = uint64_t(data.BE()) & ~uint64_t(1);
        data.Data()[0] = 1;
    };
    CheckSnoopData<Config128>(invalidate);
    CheckSnoopData<Config256>(invalidate);
    CheckSnoopData<Config512>(invalidate);
}

LUNATIC_TEST(Stage9PacketPartialSnoopValidation)
{
    CheckPartialSnoopData<Config128>();
    CheckPartialSnoopData<Config256>();
    CheckPartialSnoopData<Config512>();
}

LUNATIC_TEST(Stage9PacketForwardedSnoopCCIDValidation)
{
    CheckForwardedSnoopCCID<Config128>();
    CheckForwardedSnoopCCID<Config256>();
    CheckForwardedSnoopCCID<Config512>();
}

LUNATIC_TEST(Stage9PacketVictimSnoopValidation)
{
    CheckVictimSnoopBE<Config128>();
    CheckVictimSnoopBE<Config256>();
    CheckVictimSnoopBE<Config512>();
}
