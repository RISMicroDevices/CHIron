#pragma once

#include "chi_lunatic_types.hpp"

#include <deque>
#include <map>

namespace CHI::Lunatic {

    class MemoryBackend {
    public:
        virtual ~MemoryBackend() = default;
        virtual LineData            Read(const LineKey& key) const = 0;
        virtual void                Write(const LineKey& key, const LineData& data, const ByteEnables& enables) = 0;
        virtual uint64_t            NextLatency(uint64_t fallback) = 0;
    };

    class SparseMemory final : public MemoryBackend {
        std::map<LineKey, LineData> lines;
        std::deque<uint64_t>        latencies;

    public:
        LineData                    Read(const LineKey& key) const override;
        void                        Write(const LineKey& key, const LineData& data, const ByteEnables& enables) override;
        uint64_t                    NextLatency(uint64_t fallback) override;

        void                        WriteFull(const LineKey& key, const LineData& data);
        bool                        Contains(const LineKey& key) const noexcept;
        void                        Erase(const LineKey& key);
        size_t                      Size() const noexcept { return lines.size(); }
        void                        ScriptLatency(uint64_t latency) { latencies.push_back(latency); }
        void                        Clear() noexcept;
    };

}
