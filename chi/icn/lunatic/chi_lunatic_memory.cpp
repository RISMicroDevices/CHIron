#include "chi_lunatic_memory.hpp"

namespace CHI::Lunatic {

    LineData SparseMemory::Read(const LineKey& key) const
    {
        auto it = lines.find(key);
        return it == lines.end() ? LineData{} : it->second;
    }

    void SparseMemory::Write(const LineKey& key, const LineData& data, const ByteEnables& enables)
    {
        auto [it, inserted] = lines.try_emplace(key);
        (void)inserted;
        LineData& line = it->second;
        for (size_t index = 0; index < LINE_SIZE; ++index) {
            if (enables[index])
                line[index] = data[index];
        }
    }

    uint64_t SparseMemory::NextLatency(uint64_t fallback)
    {
        if (latencies.empty())
            return fallback;

        const uint64_t latency = latencies.front();
        latencies.pop_front();
        return latency;
    }

    void SparseMemory::WriteFull(const LineKey& key, const LineData& data)
    {
        lines.insert_or_assign(key, data);
    }

    bool SparseMemory::Contains(const LineKey& key) const noexcept
    {
        return lines.find(key) != lines.end();
    }

    void SparseMemory::Erase(const LineKey& key)
    {
        lines.erase(key);
    }

    void SparseMemory::Clear() noexcept
    {
        lines.clear();
        latencies.clear();
    }

}
