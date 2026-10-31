#include "chi_lunatic_backend_axi.hpp"

#include "../../../chi_eb/spec/chi_eb_protocol_encoding.hpp"

#include <bit>
#include <limits>
#include <utility>

namespace CHI::Lunatic {

    AXIBackend::AXIBackend(Options options)
        : options(std::move(options))
    {
    }

    void AXIBackend::Reset()
    {
        writes.clear();
        reads.clear();
        aw.clear();
        ar.clear();
        b.clear();
        r.clear();
        completions.clear();
        errors.clear();
        nextSerial = 0;
    }

    void AXIBackend::Tick()
    {
        ProcessResponses();
    }

    size_t AXIBackend::FirstBeat(const Record& record) noexcept
    {
        return size_t(std::countr_zero(record.operation.beatMask));
    }

    size_t AXIBackend::BeatCount(const Record& record) noexcept
    {
        return size_t(std::popcount(record.operation.beatMask));
    }

    AXIAddress AXIBackend::MakeAddress(const AXIOperation& operation, uint32_t id) const
    {
        AXIAddress result;
        result.id = id;
        result.address = operation.line.address + size_t(std::countr_zero(operation.beatMask)) * BytesPerBeat(options);
        result.len = uint8_t(std::popcount(operation.beatMask) - 1);
        result.size = uint8_t(std::countr_zero(BytesPerBeat(options)));
        result.burst = AXIBurst::INCR;
        result.cache = operation.write ? AXIWriteCache(operation.memAttr) : AXIReadCache(operation.memAttr);
        result.qos = operation.qos;
        result.prot = operation.line.nonSecure ? 0b010 : 0;
        return result;
    }

    uint32_t AXIBackend::AllocateId(const std::map<uint32_t, Record>& records) const noexcept
    {
        const uint32_t limit = uint32_t(1) << options.axiIdWidth;
        for (uint32_t id = 0; id < limit; ++id)
            if (!records.contains(id))
                return id;
        return std::numeric_limits<uint32_t>::max();
    }

    bool AXIBackend::Submit(const AXIOperation& operation)
    {
        auto& records = operation.write ? writes : reads;
        auto& addresses = operation.write ? aw : ar;
        if (records.size() >= options.axiOutstandingLimit || addresses.size() >= options.outputQueueLimit)
            return false;

        const size_t beats = BeatsPerLine(options);
        const uint64_t validMask = (uint64_t(1) << beats) - 1;
        if (!operation.beatMask || (operation.beatMask & ~validMask))
            return false;
        const size_t first = size_t(std::countr_zero(operation.beatMask));
        const size_t count = size_t(std::popcount(operation.beatMask));
        const size_t last = size_t(std::countr_zero(operation.beatMask)) + count - 1;
        if (last >= beats || (operation.beatMask >> first) != ((uint64_t(1) << count) - 1))
            return false;

        const uint32_t id = AllocateId(records);
        if (id == std::numeric_limits<uint32_t>::max())
            return false;
        Record record;
        record.operation = operation;
        record.address = MakeAddress(operation, id);
        record.serial = nextSerial++;
        records.emplace(id, std::move(record));
        addresses.push_back(MakeAddress(operation, id));
        return true;
    }

    bool AXIBackend::HasInflight() const noexcept
    {
        return !writes.empty() || !reads.empty() || !aw.empty() || !ar.empty() || !b.empty() || !r.empty() ||
               !completions.empty() || !errors.empty();
    }

    size_t AXIBackend::Outstanding(bool write) const noexcept
    {
        return write ? writes.size() : reads.size();
    }

    std::optional<AXIAddress> AXIBackend::PeekAW() const
    {
        return aw.empty() ? std::nullopt : std::optional<AXIAddress>(aw.front());
    }

    std::optional<AXIAddress> AXIBackend::PopAW()
    {
        if (aw.empty())
            return std::nullopt;
        const AXIAddress result = aw.front();
        aw.pop_front();
        const auto it = writes.find(result.id);
        if (it != writes.end())
            it->second.addressEmitted = true;
        return result;
    }

    const AXIBackend::Record* AXIBackend::SelectedWrite() const noexcept
    {
        const Record* selected = nullptr;
        for (const auto& [id, record] : writes)
        {
            (void)id;
            if (!record.addressEmitted || record.nextWriteBeat >= BeatCount(record))
                continue;
            if (!selected || record.serial < selected->serial)
                selected = &record;
        }
        return selected;
    }

    AXIBackend::Record* AXIBackend::SelectedWrite() noexcept
    {
        return const_cast<Record*>(static_cast<const AXIBackend&>(*this).SelectedWrite());
    }

    AXIWriteData AXIBackend::WriteBeat(const Record& record) const
    {
        AXIWriteData result;
        const size_t bytes = BytesPerBeat(options);
        const size_t offset = (FirstBeat(record) + record.nextWriteBeat) * bytes;
        for (size_t index = 0; index < bytes; ++index)
        {
            result.data[index] = record.operation.data[offset + index];
            result.strobes[index] = record.operation.enables[offset + index];
        }
        result.last = record.nextWriteBeat + 1 == BeatCount(record);
        return result;
    }

    std::optional<AXIWriteData> AXIBackend::PeekW() const
    {
        const Record* record = SelectedWrite();
        return record ? std::optional<AXIWriteData>(WriteBeat(*record)) : std::nullopt;
    }

    std::optional<AXIWriteData> AXIBackend::PopW()
    {
        Record* record = SelectedWrite();
        if (!record)
            return std::nullopt;
        const AXIWriteData result = WriteBeat(*record);
        ++record->nextWriteBeat;
        return result;
    }

    std::optional<AXIAddress> AXIBackend::PeekAR() const
    {
        return ar.empty() ? std::nullopt : std::optional<AXIAddress>(ar.front());
    }

    std::optional<AXIAddress> AXIBackend::PopAR()
    {
        if (ar.empty())
            return std::nullopt;
        const AXIAddress result = ar.front();
        ar.pop_front();
        const auto it = reads.find(result.id);
        if (it != reads.end())
            it->second.addressEmitted = true;
        return result;
    }

    bool AXIBackend::PushB(const AXIWriteResponse& response)
    {
        if (b.size() >= options.inputQueueLimit)
            return false;
        b.push_back(response);
        return true;
    }

    bool AXIBackend::PushR(const AXIReadData& response)
    {
        if (r.size() >= options.inputQueueLimit)
            return false;
        r.push_back(response);
        return true;
    }

    std::optional<AXICompletion> AXIBackend::PeekCompletion() const
    {
        return completions.empty() ? std::nullopt : std::optional<AXICompletion>(completions.front());
    }

    std::optional<AXICompletion> AXIBackend::PopCompletion()
    {
        if (completions.empty())
            return std::nullopt;
        const AXICompletion result = completions.front();
        completions.pop_front();
        auto& records = result.write ? writes : reads;
        for (auto it = records.begin(); it != records.end(); ++it)
        {
            if (it->second.operation.transaction != result.transaction)
                continue;
            records.erase(it);
            break;
        }
        return result;
    }

    std::optional<std::string> AXIBackend::PopError()
    {
        if (errors.empty())
            return std::nullopt;
        std::string result = std::move(errors.front());
        errors.pop_front();
        return result;
    }

    bool AXIBackend::Complete(Record& record, bool write)
    {
        if (record.completionQueued)
            return false;
        record.completionQueued = true;
        AXICompletion completion;
        completion.transaction = record.operation.transaction;
        completion.line = record.operation.line;
        completion.data = record.readData;
        completion.beatMask = record.operation.beatMask;
        completion.response = record.response;
        completion.write = write;
        completion.protocolError = record.protocolError;
        completions.push_back(std::move(completion));
        return true;
    }

    void AXIBackend::ProcessResponses()
    {
        while (!b.empty())
        {
            const AXIWriteResponse response = b.front();
            b.pop_front();
            ProcessB(response);
        }
        while (!r.empty())
        {
            const AXIReadData response = r.front();
            r.pop_front();
            ProcessR(response);
        }
    }

    void AXIBackend::ProcessB(const AXIWriteResponse& response)
    {
        const auto it = writes.find(response.id);
        if (it == writes.end() || it->second.completionQueued)
        {
            errors.push_back("unexpected AXI write response ID");
            return;
        }
        Record& record = it->second;
        if ((!record.addressEmitted || record.nextWriteBeat != BeatCount(record)) && record.protocolError.empty())
            record.protocolError = "AXI write response arrived before request or WLAST acceptance";
        if (response.resp == AXIResp::EXOKAY)
            record.protocolError = "AXI write returned EXOKAY to a non-exclusive request";
        else if (response.resp != AXIResp::OKAY)
            record.response = response.resp;
        Complete(record, true);
    }

    void AXIBackend::ProcessR(const AXIReadData& response)
    {
        const auto it = reads.find(response.id);
        if (it == reads.end() || it->second.completionQueued)
        {
            errors.push_back("unexpected AXI read response ID");
            return;
        }
        Record& record = it->second;
        if (!record.addressEmitted && record.protocolError.empty())
            record.protocolError = "AXI read data arrived before AR acceptance";
        const size_t expected = BeatCount(record);
        if (record.receivedBeats >= expected)
        {
            record.protocolError = "extra AXI read data beat";
            Complete(record, false);
            return;
        }

        const size_t bytes = BytesPerBeat(options);
        const size_t offset = (FirstBeat(record) + record.receivedBeats) * bytes;
        for (size_t index = 0; index < bytes; ++index)
            record.readData[offset + index] = response.data[index];
        if (response.resp == AXIResp::EXOKAY)
            record.protocolError = "AXI read returned EXOKAY to a non-exclusive request";
        else if (response.resp != AXIResp::OKAY)
            record.response = response.resp;
        ++record.receivedBeats;

        if (response.last && record.receivedBeats != expected)
            record.protocolError = "early AXI RLAST";
        else if (!response.last && record.receivedBeats == expected)
            record.protocolError = "missing AXI RLAST";
        if (response.last || record.receivedBeats == expected)
            Complete(record, false);
    }

    uint8_t AXIReadCache(uint8_t memAttr) noexcept
    {
        namespace MemAttrs = CHI::Eb::MemAttrs;
        if (MemAttrs::ExtractDevice(memAttr))
            return MemAttrs::ExtractEWA(memAttr) ? 0b0001 : 0b0000;
        if (!MemAttrs::ExtractCacheable(memAttr))
            return MemAttrs::ExtractEWA(memAttr) ? 0b0011 : 0b0010;
        return 0b1011 | (MemAttrs::ExtractAllocate(memAttr) ? 0b0100 : 0);
    }

    uint8_t AXIWriteCache(uint8_t memAttr) noexcept
    {
        namespace MemAttrs = CHI::Eb::MemAttrs;
        if (MemAttrs::ExtractDevice(memAttr))
            return MemAttrs::ExtractEWA(memAttr) ? 0b0001 : 0b0000;
        if (!MemAttrs::ExtractCacheable(memAttr))
            return MemAttrs::ExtractEWA(memAttr) ? 0b0011 : 0b0010;
        return 0b0111 | (MemAttrs::ExtractAllocate(memAttr) ? 0b1000 : 0);
    }

}
