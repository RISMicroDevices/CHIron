#pragma once

#include "chi/icn/lunatic/chi_lunatic_backend_axi.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace LunaticTest {

    enum class AXIChannel : uint8_t {
        AW,
        W,
        AR,
        B,
        R
    };

    class AXIModel {
    public:
        struct CompletedWrite {
            CHI::Lunatic::AXIAddress                 address{};
            std::vector<CHI::Lunatic::AXIWriteData>  beats{};
            CHI::Lunatic::AXIResp                    response = CHI::Lunatic::AXIResp::OKAY;
        };

        struct CompletedRead {
            CHI::Lunatic::AXIAddress                address{};
            std::vector<CHI::Lunatic::AXIReadData>  beats{};
        };

        explicit AXIModel(size_t bytesPerBeat)
            : bytesPerBeat(bytesPerBeat)
        {
            if (bytesPerBeat != 16 && bytesPerBeat != 32 && bytesPerBeat != 64)
                throw std::invalid_argument("AXI model supports 128, 256, or 512-bit beats");
        }

        void Reset()
        {
            writes.clear();
            writeResponses.clear();
            readResponses.clear();
            completedWrites.clear();
            completedReads.clear();
            accessedReadAddresses.clear();
            writeResponseScripts.clear();
            readResponseScripts.clear();
            writeResponseOrder.clear();
            readResponseOrder.clear();
            errors.clear();
            activeReadId.reset();
            nextReadBeat = 0;
            stalls.fill(0);
        }

        void Stall(AXIChannel channel, size_t cycles)
        {
            stalls[static_cast<size_t>(channel)] = cycles;
        }

        void ScriptWriteResponse(CHI::Lunatic::AXIResp response)
        {
            writeResponseScripts.push_back(response);
        }

        void ScriptReadResponse(CHI::Lunatic::AXIResp response)
        {
            readResponseScripts.push_back(response);
        }

        void SetWriteResponseOrder(const std::vector<uint32_t>& ids)
        {
            writeResponseOrder.assign(ids.begin(), ids.end());
        }

        void SetReadResponseOrder(const std::vector<uint32_t>& ids)
        {
            readResponseOrder.assign(ids.begin(), ids.end());
        }

        void SetLine(uint64_t address, const CHI::Lunatic::LineData& data)
        {
            memory[LineAddress(address)] = data;
        }

        CHI::Lunatic::LineData ReadLine(uint64_t address) const
        {
            const auto it = memory.find(LineAddress(address));
            return it == memory.end() ? CHI::Lunatic::LineData{} : it->second;
        }

        void ClearMemory()
        {
            memory.clear();
        }

        bool Drain() const noexcept
        {
            return writes.empty() && writeResponses.empty() && readResponses.empty();
        }

        const std::map<uint64_t, CHI::Lunatic::LineData>& Memory() const noexcept { return memory; }
        const std::vector<CompletedWrite>& CompletedWrites() const noexcept { return completedWrites; }
        const std::vector<CompletedRead>& CompletedReads() const noexcept { return completedReads; }
        const std::vector<uint64_t>& AccessedReadAddresses() const noexcept { return accessedReadAddresses; }
        const std::vector<std::string>& Errors() const noexcept { return errors; }

        template<typename Endpoint>
        void Tick(Endpoint& endpoint)
        {
            if (Ready(AXIChannel::AW) && endpoint.PeekAW())
            {
                const auto expected = endpoint.PeekAW();
                const auto address = endpoint.PopAW();
                if (!address || address != expected)
                    errors.push_back("AXI AW handshake changed between Peek and Pop");
                else
                    AcceptAW(*address);
            }

            if (Ready(AXIChannel::W) && !writes.empty() && endpoint.PeekW())
            {
                const auto expected = endpoint.PeekW();
                const auto data = endpoint.PopW();
                if (!data || data != expected)
                    errors.push_back("AXI W handshake changed between Peek and Pop");
                else
                    AcceptW(*data);
            }

            if (Ready(AXIChannel::AR) && endpoint.PeekAR())
            {
                const auto expected = endpoint.PeekAR();
                const auto address = endpoint.PopAR();
                if (!address || address != expected)
                    errors.push_back("AXI AR handshake changed between Peek and Pop");
                else
                    AcceptAR(*address);
            }

            if (Ready(AXIChannel::B) && !writeResponses.empty())
            {
                const size_t index = SelectResponse(writeResponses, writeResponseOrder);
                const CHI::Lunatic::AXIWriteResponse response{
                    writeResponses[index].address.id,
                    writeResponses[index].response
                };
                if (endpoint.PushB(response))
                {
                    MarkResponseSent(response.id, writeResponseOrder);
                    completedWrites.push_back(std::move(writeResponses[index]));
                    writeResponses.erase(writeResponses.begin() + index);
                }
            }

            if (Ready(AXIChannel::R) && !readResponses.empty())
            {
                size_t index = 0;
                if (activeReadId)
                {
                    while (index < readResponses.size() && readResponses[index].address.id != *activeReadId)
                        ++index;
                    if (index == readResponses.size())
                    {
                        errors.push_back("AXI model lost its active read burst");
                        activeReadId.reset();
                        index = 0;
                    }
                }
                else
                {
                    index = SelectResponse(readResponses, readResponseOrder);
                    activeReadId = readResponses[index].address.id;
                }

                auto& burst = readResponses[index];
                if (nextReadBeat >= burst.beats.size())
                {
                    errors.push_back("AXI model read beat index exceeded its burst");
                }
                else if (endpoint.PushR(burst.beats[nextReadBeat]))
                {
                    ++nextReadBeat;
                    if (nextReadBeat == burst.beats.size())
                    {
                        MarkResponseSent(burst.address.id, readResponseOrder);
                        completedReads.push_back(std::move(burst));
                        readResponses.erase(readResponses.begin() + index);
                        activeReadId.reset();
                        nextReadBeat = 0;
                    }
                }
            }

            for (size_t& stall : stalls)
                if (stall)
                    --stall;
        }

    private:
        static uint64_t LineAddress(uint64_t address) noexcept
        {
            return address & ~uint64_t(CHI::Lunatic::LINE_SIZE - 1);
        }

        static size_t BurstLength(const CHI::Lunatic::AXIAddress& address) noexcept
        {
            return size_t(address.len) + 1;
        }

        uint64_t BeatAddress(const CHI::Lunatic::AXIAddress& address, size_t beat) const noexcept
        {
            if (address.burst == CHI::Lunatic::AXIBurst::FIXED || beat == 0)
                return address.address;
            const size_t transferBytes = size_t(1) << address.size;
            return (address.address & ~uint64_t(transferBytes - 1)) + beat * transferBytes;
        }

        size_t ActiveBytes(const CHI::Lunatic::AXIAddress& address, size_t beat) const noexcept
        {
            const size_t transferBytes = size_t(1) << address.size;
            return beat == 0 && address.burst != CHI::Lunatic::AXIBurst::FIXED
                ? transferBytes - address.address % transferBytes : transferBytes;
        }

        uint8_t ReadByte(uint64_t address) const
        {
            const auto it = memory.find(LineAddress(address));
            if (it == memory.end())
                return 0;
            return it->second[address % CHI::Lunatic::LINE_SIZE];
        }

        void AcceptAW(const CHI::Lunatic::AXIAddress& address)
        {
            if (address.burst == CHI::Lunatic::AXIBurst::WRAP)
                errors.push_back("AXI model does not support WRAP write bursts");
            CompletedWrite write;
            write.address = address;
            if (!writeResponseScripts.empty())
            {
                write.response = writeResponseScripts.front();
                writeResponseScripts.pop_front();
            }
            writes.push_back(std::move(write));
        }

        void AcceptW(const CHI::Lunatic::AXIWriteData& data)
        {
            auto& write = writes.front();
            const size_t beat = write.beats.size();
            const uint64_t base = BeatAddress(write.address, beat);
            const size_t lane = base % bytesPerBeat;
            const size_t activeBytes = ActiveBytes(write.address, beat);
            for (size_t index = 0; index < activeBytes; ++index)
            {
                if (!data.strobes[lane + index])
                    continue;
                const uint64_t address = base + index;
                memory[LineAddress(address)][address % CHI::Lunatic::LINE_SIZE] = data.data[lane + index];
            }
            write.beats.push_back(data);
            if (data.last)
            {
                if (write.beats.size() != BurstLength(write.address))
                    errors.push_back("AXI write burst ended at an unexpected beat");
                writeResponses.push_back(std::move(write));
                writes.pop_front();
            }
        }

        void AcceptAR(const CHI::Lunatic::AXIAddress& address)
        {
            if (address.burst == CHI::Lunatic::AXIBurst::WRAP)
                errors.push_back("AXI model does not support WRAP read bursts");
            CHI::Lunatic::AXIResp response = CHI::Lunatic::AXIResp::OKAY;
            if (!readResponseScripts.empty())
            {
                response = readResponseScripts.front();
                readResponseScripts.pop_front();
            }

            CompletedRead read;
            read.address = address;
            read.beats.resize(BurstLength(address));
            for (size_t beat = 0; beat < read.beats.size(); ++beat)
            {
                auto& data = read.beats[beat];
                data.id = address.id;
                data.resp = response;
                data.last = beat + 1 == read.beats.size();
                const uint64_t base = BeatAddress(address, beat);
                const size_t lane = base % bytesPerBeat;
                const size_t activeBytes = ActiveBytes(address, beat);
                for (size_t index = 0; index < activeBytes; ++index)
                {
                    accessedReadAddresses.push_back(base + index);
                    data.data[lane + index] = ReadByte(base + index);
                }
            }
            readResponses.push_back(std::move(read));
        }

        template<typename Range>
        static size_t SelectResponse(const Range& responses, const std::deque<uint32_t>& order)
        {
            for (const uint32_t id : order)
                for (size_t index = 0; index < responses.size(); ++index)
                    if (responses[index].address.id == id)
                        return index;
            return 0;
        }

        static void MarkResponseSent(uint32_t id, std::deque<uint32_t>& order)
        {
            const auto it = std::find(order.begin(), order.end(), id);
            if (it != order.end())
                order.erase(it);
        }

        bool Ready(AXIChannel channel) const noexcept
        {
            return stalls[static_cast<size_t>(channel)] == 0;
        }

        size_t                                      bytesPerBeat;
        std::map<uint64_t, CHI::Lunatic::LineData> memory;
        std::deque<CompletedWrite>                  writes;
        std::deque<CompletedWrite>                  writeResponses;
        std::deque<CompletedRead>                   readResponses;
        std::vector<CompletedWrite>                 completedWrites;
        std::vector<CompletedRead>                  completedReads;
        std::vector<uint64_t>                       accessedReadAddresses;
        std::deque<CHI::Lunatic::AXIResp>           writeResponseScripts;
        std::deque<CHI::Lunatic::AXIResp>           readResponseScripts;
        std::deque<uint32_t>                        writeResponseOrder;
        std::deque<uint32_t>                        readResponseOrder;
        std::vector<std::string>                    errors;
        std::optional<uint32_t>                     activeReadId;
        size_t                                      nextReadBeat = 0;
        std::array<size_t, 5>                       stalls{};
    };

}
