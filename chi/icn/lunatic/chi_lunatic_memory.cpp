#include "chi_lunatic_memory.hpp"

namespace CHI::Lunatic {

    LineData SparseMemory::Read(const LineKey& key) const
    {
        return ReadImage(key).data;
    }

    LineImage SparseMemory::ReadImage(const LineKey& key) const
    {
        const auto it = lines.find(key);
        if (it != lines.end())
        {
            LineImage result = it->second;
            result.validBytes.fill(1);
            return result;
        }

        Options options;
        options.dataCheckEnabled = true;
        ByteEnables enables{};
        enables.fill(1);
        LineImage result;
        GenerateMetadata(result.poison, result.dataCheck, result.data, enables, options);
        result.validBytes = enables;
        return result;
    }

    void SparseMemory::Write(const LineKey& key, const LineData& data, const ByteEnables& enables)
    {
        Options options;
        options.dataCheckEnabled = true;
        LineImage update;
        update.data = data;
        GenerateMetadata(update.poison, update.dataCheck, data, enables, options);
        MergeLineImage(lines[key], update, enables, options);
    }

    void SparseMemory::WriteImage(const LineKey& key, const LineImage& data, const ByteEnables& enables,
                                  const Options& options)
    {
        MergeLineImage(lines[key], data, enables, options);
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
        ByteEnables enables{};
        enables.fill(1);
        Write(key, data, enables);
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
