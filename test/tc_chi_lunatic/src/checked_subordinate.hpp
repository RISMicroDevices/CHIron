#pragma once

#include "driver.hpp"
#include "oracle.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <vector>

namespace LunaticTest {

    using namespace CHI::Lunatic;

    template<typename config>
    class CheckedSubordinate {
        using Base = Driver<config>;
        using REQ = typename Base::REQ;
        using RSP = typename Base::RSP;
        using DAT = typename Base::DAT;

        std::map<LineKey, LineData> lines;
        std::vector<uint16_t>       opcodes;
        SNOracle<config>            oracle{ 2, 6 };
        bool                        retryWrite = false;
        bool                        grantFirst = false;
        bool                        combinedWriteResponse = false;

        void SendResponse(Base& driver, const REQ& request, uint16_t opcode)
        {
            RSP response{};
            response.SrcID() = 6;
            response.TgtID() = 2;
            response.TxnID() = request.TxnID();
            response.DBID() = opcode == CHI::Eb::Opcodes::RSP::RetryAck ? 0 : 77;
            response.Opcode() = opcode;
            response.Resp() = CHI::Eb::Resps::Comp_I;
            if (opcode == CHI::Eb::Opcodes::RSP::RetryAck)
                response.PCrdType() = request.PCrdType();
            oracle.Response(driver.now, response);
            LUNATIC_CHECK(driver.node.PushRXRSP(response));
            driver.Tick();
        }

        void SendRead(Base& driver, const REQ& request)
        {
            constexpr size_t bytes = config::dataWidth / 8;
            const LineKey line = LineKey::FromAddress(request.Addr(), request.NS());
            const LineData data = Read(line);
            const size_t transfer = std::min<size_t>(LINE_SIZE, size_t(1) << request.Size());
            const size_t first = ((request.Addr() & ~(transfer - 1)) % LINE_SIZE) / bytes;
            const size_t count = std::max<size_t>(1, (transfer + bytes - 1) / bytes);
            for (size_t packet = 0; packet < count; ++packet)
            {
                const size_t beat = first + packet;
                LUNATIC_CHECK(beat < LINE_SIZE / bytes);
                DAT response{};
                response.SrcID() = 6;
                response.TgtID() = 2;
                response.HomeNID() = 2;
                response.TxnID() = request.TxnID();
                response.Opcode() = CHI::Eb::Opcodes::DAT::CompData;
                response.Resp() = CHI::Eb::Resps::CompData_I;
                response.DataID() = beat * bytes / 16;
                response.CCID() = (request.Addr() >> 4) & 3;
                for (size_t index = 0; index < bytes; ++index)
                    response.Data()[index / 8] |= uint64_t(data[beat * bytes + index]) << ((index % 8) * 8);
                oracle.Data(driver.now, response);
                LUNATIC_CHECK(driver.node.PushRXDAT(response));
            }
            driver.Tick();
        }

        void HandleWrite(Base& driver, const REQ& request)
        {
            constexpr size_t bytes = config::dataWidth / 8;
            constexpr uint64_t fullEnables = bytes == 64 ? ~uint64_t(0) : (uint64_t(1) << bytes) - 1;
            if (retryWrite)
            {
                retryWrite = false;
                if (grantFirst)
                {
                    RSP grant{};
                    grant.SrcID() = 6;
                    grant.TgtID() = 2;
                    grant.Opcode() = CHI::Eb::Opcodes::RSP::PCrdGrant;
                    grant.PCrdType() = request.PCrdType();
                    oracle.Response(driver.now, grant);
                    LUNATIC_CHECK(driver.node.PushRXRSP(grant));
                    driver.Tick();
                }
                SendResponse(driver, request, CHI::Eb::Opcodes::RSP::RetryAck);
                if (!grantFirst)
                {
                    RSP grant{};
                    grant.SrcID() = 6;
                    grant.TgtID() = 2;
                    grant.Opcode() = CHI::Eb::Opcodes::RSP::PCrdGrant;
                    grant.PCrdType() = request.PCrdType();
                    oracle.Response(driver.now, grant);
                    LUNATIC_CHECK(driver.node.PushRXRSP(grant));
                    driver.Tick();
                }
                return;
            }

            SendResponse(driver, request, combinedWriteResponse
                ? CHI::Eb::Opcodes::RSP::CompDBIDResp : CHI::Eb::Opcodes::RSP::DBIDResp);
            const LineKey line = LineKey::FromAddress(request.Addr(), request.NS());
            LineData update = Read(line);
            for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
            {
                const auto data = driver.NextDAT(CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
                oracle.WriteData(driver.now, data);
                LUNATIC_CHECK(data.TgtID() == 6);
                LUNATIC_CHECK(data.TxnID() == 77);
                LUNATIC_CHECK(data.DBID() == 0);
                LUNATIC_CHECK(data.HomeNID() == 0);
                if (request.Opcode() == CHI::Eb::Opcodes::REQ::WriteNoSnpFull)
                    LUNATIC_CHECK(uint64_t(data.BE()) == fullEnables);
                const size_t offset = size_t(data.DataID()) * 16;
                for (size_t index = 0; index < bytes; ++index)
                    if ((uint64_t(data.BE()) >> index) & 1)
                        update[offset + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
            }
            lines[line] = update;
            if (!combinedWriteResponse)
                SendResponse(driver, request, CHI::Eb::Opcodes::RSP::Comp);
        }

    public:
        LineData Read(const LineKey& line) const
        {
            const auto it = lines.find(line);
            return it == lines.end() ? LineData{} : it->second;
        }

        void Write(const LineKey& line, const LineData& data) { lines[line] = data; }
        void RetryNextWrite(bool beforeRetryAck = false) { retryWrite = true; grantFirst = beforeRetryAck; }
        void UseCombinedWriteResponse(bool combined = true) { combinedWriteResponse = combined; }
        const std::vector<uint16_t>& Opcodes() const noexcept { return opcodes; }
        bool HasInflight() const noexcept { return oracle.HasInflight(); }

        void Step(Base& driver)
        {
            if (!driver.node.PeekTXREQ())
                return;
            const auto request = driver.node.PopTXREQ();
            LUNATIC_CHECK(request);
            oracle.Request(driver.now, *request);
            opcodes.push_back(request->Opcode());
            if (request->Opcode() == CHI::Eb::Opcodes::REQ::ReadNoSnp)
            {
                SendRead(driver, *request);
                return;
            }
            if (request->Opcode() == CHI::Eb::Opcodes::REQ::WriteNoSnpFull ||
                request->Opcode() == CHI::Eb::Opcodes::REQ::WriteNoSnpPtl)
            {
                HandleWrite(driver, *request);
                return;
            }
            throw std::runtime_error("unexpected checked-Subordinate opcode");
        }
    };

}
