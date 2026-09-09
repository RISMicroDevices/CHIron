#pragma once

#ifndef __CCHI__CCHI_PROF__METRICS
#define __CCHI__CCHI_PROF__METRICS

#include <cstdint>
#include <cstddef>
#include <bit>
#include <array>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <algorithm>


/*
*   CCHI Performance Profiling Infrastructure: metrics primitives.
*
*   This header provides the harness-independent metric types used by the CCHI
*   profilers (see cchi_prof_joint.hpp):
*
*   - Counter:    monotonic 64-bit event counter.
*   - Gauge:      last-value-wins level with min/max tracking (occupancy etc.).
*   - Histogram:  log2-bucketed distribution with exact count/sum/min/max and
*                 bucket-resolution percentile estimates (latency etc.).
*   - TimeSeries: fixed-window ring of per-window values, accumulating (rates)
*                 or gauge (level snapshots) mode.
*   - MetricSet:  named registry over the above, for report generation.
*
*   All types are allocation-stable after creation and their update paths are
*   O(1), so they can sit on synchronous per-flit event buses.
*/
namespace CCHI::Prof {

    // Monotonic 64-bit event counter.
    class Counter {
    private:
        uint64_t    value;

    public:
        inline Counter() noexcept;

    public:
        inline void         Inc() noexcept;
        inline void         Add(uint64_t n) noexcept;

        inline uint64_t     Get() const noexcept;

        inline void         Reset() noexcept;
    };


    // Last-value-wins level with min/max tracking.
    class Gauge {
    private:
        uint64_t    value;
        uint64_t    minValue;
        uint64_t    maxValue;
        bool        everSet;

    public:
        inline Gauge() noexcept;

    public:
        inline void         Set(uint64_t v) noexcept;
        inline void         Inc() noexcept;
        inline void         Dec() noexcept;

        inline uint64_t     Get() const noexcept;
        inline uint64_t     GetMin() const noexcept;    // 0 when never set
        inline uint64_t     GetMax() const noexcept;
        inline bool         EverSet() const noexcept;

        // Merges another gauge into this one: levels add, min/max combine.
        inline void         MergeFrom(const Gauge& other) noexcept;

        inline void         Reset() noexcept;
    };


    // Log2-bucketed distribution.
    // Bucket 0 holds the exact value 0; bucket i (1..64) holds [2^(i-1), 2^i).
    // Percentiles return the lower bound of the percentile bucket, i.e. the
    // estimate is accurate to within one bucket step (<= 2x for values > 0).
    class Histogram {
    public:
        static constexpr size_t     NUM_BUCKETS = 65;

    private:
        std::array<uint64_t, NUM_BUCKETS>   buckets;
        uint64_t                            count;
        uint64_t                            sum;
        uint64_t                            minValue;
        uint64_t                            maxValue;

    public:
        inline Histogram() noexcept;

    public:
        inline void                     Record(uint64_t value) noexcept;
        inline void                     Reset() noexcept;

    public:
        inline uint64_t                 GetCount() const noexcept;
        inline uint64_t                 GetSum() const noexcept;
        inline double                   GetMean() const noexcept;
        inline uint64_t                 GetMin() const noexcept;    // 0 when empty
        inline uint64_t                 GetMax() const noexcept;

        // p in [0, 100]; returns the lower bound of the percentile bucket.
        inline uint64_t                 GetPercentile(double p) const noexcept;

        // Merges another histogram into this one (same bucket layout).
        inline void                     MergeFrom(const Histogram& other) noexcept;

    public:
        inline static constexpr size_t  BucketIndexOf(uint64_t value) noexcept;
        inline static constexpr uint64_t BucketLowerBound(size_t index) noexcept;
        inline static constexpr uint64_t BucketUpperBound(size_t index) noexcept;   // exclusive

        inline uint64_t                 GetBucket(size_t index) const noexcept;
        inline const std::array<uint64_t, NUM_BUCKETS>&
                                        GetBuckets() const noexcept;
    };


    // Fixed-window ring of per-window values.
    // Accumulating mode (gauge=false): Add() sums values into the current window
    //   (flit rates, byte rates, ...).
    // Gauge mode (gauge=true): Set() keeps the last value per window
    //   (occupancy levels, ...).
    // The window index of a sample is (time / windowTicks). Sealed windows are
    // kept in a ring of fixed capacity; oldest windows are dropped on overflow.
    // Empty windows between samples are sealed as 0 (gap fill is capped at capacity).
    class TimeSeries {
    private:
        uint64_t                windowTicks;
        bool                    gaugeMode;
        size_t                  capacity;

        std::vector<uint64_t>   ring;       // sealed windows, chronological
        uint64_t                total;

        int64_t                 curWindow;  // -1 when no sample seen yet
        uint64_t                curValue;

    public:
        inline TimeSeries(uint64_t windowTicks, size_t capacity = 4096, bool gauge = false) noexcept;

    public:
        inline void                     Add(uint64_t time, uint64_t value = 1) noexcept;
        inline void                     Set(uint64_t time, uint64_t value) noexcept;

        inline void                     Reset() noexcept;

    public:
        inline uint64_t                 GetWindowTicks() const noexcept;
        inline bool                     IsGauge() const noexcept;

        inline size_t                   GetWindowCount() const noexcept;    // sealed windows only
        inline uint64_t                 GetWindow(size_t index) const noexcept;

        inline int64_t                  GetCurrentWindowIndex() const noexcept;
        inline uint64_t                 GetCurrentWindowValue() const noexcept;

        inline uint64_t                 GetTotal() const noexcept;          // sealed + current (accumulating)
        inline uint64_t                 GetMaxWindow() const noexcept;
        inline double                   GetMeanWindow() const noexcept;

    private:
        inline void                     SealTo(uint64_t windowIndex) noexcept;
        inline void                     Push(uint64_t value) noexcept;
    };


    // Named metric registry. Get-or-create semantics; metrics are owned by the set.
    class MetricSet {
    private:
        std::unordered_map<std::string, std::unique_ptr<Counter>>       counters;
        std::unordered_map<std::string, std::unique_ptr<Gauge>>         gauges;
        std::unordered_map<std::string, std::unique_ptr<Histogram>>     histograms;
        std::unordered_map<std::string, std::unique_ptr<TimeSeries>>    series;

    public:
        inline MetricSet() noexcept;

    public:
        inline Counter&                 C(const std::string& name);
        inline Gauge&                   G(const std::string& name);
        inline Histogram&               H(const std::string& name);
        inline TimeSeries&              S(const std::string& name, uint64_t windowTicks, bool gauge = false);

    public:
        inline bool                     HasCounter(const std::string& name) const noexcept;
        inline bool                     HasGauge(const std::string& name) const noexcept;
        inline bool                     HasHistogram(const std::string& name) const noexcept;
        inline bool                     HasSeries(const std::string& name) const noexcept;

        inline const Counter*           GetCounter(const std::string& name) const noexcept;
        inline const Gauge*             GetGauge(const std::string& name) const noexcept;
        inline const Histogram*         GetHistogram(const std::string& name) const noexcept;
        inline const TimeSeries*        GetSeries(const std::string& name) const noexcept;

    public:
        inline std::vector<std::string> ListCounters() const;
        inline std::vector<std::string> ListGauges() const;
        inline std::vector<std::string> ListHistograms() const;
        inline std::vector<std::string> ListSeries() const;

        // Merges another set into this one with an optional name prefix
        // (e.g. "node0."). Counters add, gauges/histograms merge, series are
        // skipped (window alignment between sets is not guaranteed).
        inline void                     MergeFrom(const MetricSet& other, const std::string& prefix = "");

        inline void                     Clear() noexcept;
    };
}



// Implementation of: class Counter
namespace CCHI::Prof {

    inline Counter::Counter() noexcept
        : value (0)
    { }

    inline void Counter::Inc() noexcept
    {
        value++;
    }

    inline void Counter::Add(uint64_t n) noexcept
    {
        value += n;
    }

    inline uint64_t Counter::Get() const noexcept
    {
        return value;
    }

    inline void Counter::Reset() noexcept
    {
        value = 0;
    }
}



// Implementation of: class Gauge
namespace CCHI::Prof {

    inline Gauge::Gauge() noexcept
        : value     (0)
        , minValue  (0)
        , maxValue  (0)
        , everSet   (false)
    { }

    inline void Gauge::Set(uint64_t v) noexcept
    {
        value = v;

        if (!everSet)
        {
            minValue = v;
            maxValue = v;
            everSet  = true;
        }
        else
        {
            minValue = std::min(minValue, v);
            maxValue = std::max(maxValue, v);
        }
    }

    inline void Gauge::Inc() noexcept
    {
        Set(everSet ? value + 1 : 1);
    }

    inline void Gauge::Dec() noexcept
    {
        Set(everSet && value > 0 ? value - 1 : 0);
    }

    inline uint64_t Gauge::Get() const noexcept
    {
        return value;
    }

    inline uint64_t Gauge::GetMin() const noexcept
    {
        return everSet ? minValue : 0;
    }

    inline uint64_t Gauge::GetMax() const noexcept
    {
        return maxValue;
    }

    inline bool Gauge::EverSet() const noexcept
    {
        return everSet;
    }

    inline void Gauge::Reset() noexcept
    {
        value    = 0;
        minValue = 0;
        maxValue = 0;
        everSet  = false;
    }

    inline void Gauge::MergeFrom(const Gauge& other) noexcept
    {
        if (!other.everSet)
            return;

        if (!everSet)
        {
            value    = other.value;
            minValue = other.minValue;
            maxValue = other.maxValue;
            everSet  = true;
            return;
        }

        value   += other.value;
        minValue = std::min(minValue, other.minValue);
        maxValue = std::max(maxValue, other.maxValue);
    }
}



// Implementation of: class Histogram
namespace CCHI::Prof {

    inline Histogram::Histogram() noexcept
        : count    (0)
        , sum      (0)
        , minValue (0)
        , maxValue (0)
    {
        buckets.fill(0);
    }

    inline void Histogram::Record(uint64_t value) noexcept
    {
        buckets[BucketIndexOf(value)]++;

        count++;
        sum += value;

        if (count == 1)
        {
            minValue = value;
            maxValue = value;
        }
        else
        {
            minValue = std::min(minValue, value);
            maxValue = std::max(maxValue, value);
        }
    }

    inline void Histogram::Reset() noexcept
    {
        buckets.fill(0);
        count    = 0;
        sum      = 0;
        minValue = 0;
        maxValue = 0;
    }

    inline uint64_t Histogram::GetCount() const noexcept
    {
        return count;
    }

    inline uint64_t Histogram::GetSum() const noexcept
    {
        return sum;
    }

    inline double Histogram::GetMean() const noexcept
    {
        return count ? double(sum) / double(count) : 0.0;
    }

    inline uint64_t Histogram::GetMin() const noexcept
    {
        return minValue;
    }

    inline uint64_t Histogram::GetMax() const noexcept
    {
        return maxValue;
    }

    inline uint64_t Histogram::GetPercentile(double p) const noexcept
    {
        if (!count)
            return 0;

        if (p <= 0.0)
            return minValue;

        if (p >= 100.0)
            return maxValue;

        uint64_t target = uint64_t((p / 100.0) * double(count));
        if (!target)
            target = 1;

        uint64_t cumulative = 0;
        for (size_t i = 0; i < NUM_BUCKETS; i++)
        {
            cumulative += buckets[i];
            if (cumulative >= target)
                return BucketLowerBound(i);
        }

        return maxValue;
    }

    inline constexpr size_t Histogram::BucketIndexOf(uint64_t value) noexcept
    {
        if (!value)
            return 0;

        return 64 - size_t(std::countl_zero(value));
    }

    inline constexpr uint64_t Histogram::BucketLowerBound(size_t index) noexcept
    {
        return index ? (uint64_t(1) << (index - 1)) : 0;
    }

    inline constexpr uint64_t Histogram::BucketUpperBound(size_t index) noexcept
    {
        return index >= NUM_BUCKETS - 1 ? ~uint64_t(0) : (uint64_t(1) << index);
    }

    inline uint64_t Histogram::GetBucket(size_t index) const noexcept
    {
        return index < NUM_BUCKETS ? buckets[index] : 0;
    }

    inline const std::array<uint64_t, Histogram::NUM_BUCKETS>& Histogram::GetBuckets() const noexcept
    {
        return buckets;
    }

    inline void Histogram::MergeFrom(const Histogram& other) noexcept
    {
        if (!other.count)
            return;

        const bool wasEmpty = (count == 0);

        for (size_t i = 0; i < NUM_BUCKETS; i++)
            buckets[i] += other.buckets[i];

        count += other.count;
        sum   += other.sum;

        minValue = wasEmpty ? other.minValue : std::min(minValue, other.minValue);
        maxValue = wasEmpty ? other.maxValue : std::max(maxValue, other.maxValue);
    }
}



// Implementation of: class TimeSeries
namespace CCHI::Prof {

    inline TimeSeries::TimeSeries(uint64_t windowTicks, size_t capacity, bool gauge) noexcept
        : windowTicks   (windowTicks ? windowTicks : 1)
        , gaugeMode     (gauge)
        , capacity      (capacity ? capacity : 1)
        , ring          ()
        , total         (0)
        , curWindow     (-1)
        , curValue      (0)
    { }

    inline void TimeSeries::Add(uint64_t time, uint64_t value) noexcept
    {
        uint64_t w = time / windowTicks;

        if (curWindow < 0)
        {
            curWindow = int64_t(w);
            curValue  = 0;
        }

        if (int64_t(w) != curWindow)
            SealTo(w);

        curValue += value;
    }

    inline void TimeSeries::Set(uint64_t time, uint64_t value) noexcept
    {
        uint64_t w = time / windowTicks;

        if (curWindow < 0)
        {
            curWindow = int64_t(w);
            curValue  = value;
            return;
        }

        if (int64_t(w) != curWindow)
            SealTo(w);

        curValue = value;
    }

    inline void TimeSeries::Reset() noexcept
    {
        ring.clear();
        total     = 0;
        curWindow = -1;
        curValue  = 0;
    }

    inline uint64_t TimeSeries::GetWindowTicks() const noexcept
    {
        return windowTicks;
    }

    inline bool TimeSeries::IsGauge() const noexcept
    {
        return gaugeMode;
    }

    inline size_t TimeSeries::GetWindowCount() const noexcept
    {
        return ring.size();
    }

    inline uint64_t TimeSeries::GetWindow(size_t index) const noexcept
    {
        return index < ring.size() ? ring[index] : 0;
    }

    inline int64_t TimeSeries::GetCurrentWindowIndex() const noexcept
    {
        return curWindow;
    }

    inline uint64_t TimeSeries::GetCurrentWindowValue() const noexcept
    {
        return curValue;
    }

    inline uint64_t TimeSeries::GetTotal() const noexcept
    {
        return total + (gaugeMode ? 0 : (curWindow >= 0 ? curValue : 0));
    }

    inline uint64_t TimeSeries::GetMaxWindow() const noexcept
    {
        uint64_t m = 0;
        for (uint64_t v : ring)
            m = std::max(m, v);
        if (curWindow >= 0)
            m = std::max(m, curValue);
        return m;
    }

    inline double TimeSeries::GetMeanWindow() const noexcept
    {
        size_t n = ring.size() + (curWindow >= 0 ? 1 : 0);
        if (!n)
            return 0.0;

        if (gaugeMode)
        {
            uint64_t gsum = total;  // note: in gauge mode total tracks sealed values as well
            if (curWindow >= 0)
                gsum += curValue;
            return double(gsum) / double(n);
        }

        return double(GetTotal()) / double(n);
    }

    inline void TimeSeries::SealTo(uint64_t windowIndex) noexcept
    {
        // Seal current window, then zero-fill skipped windows (capped at capacity).
        int64_t target = int64_t(windowIndex);

        while (curWindow < target)
        {
            Push(curValue);

            curWindow++;
            curValue = 0;

            // Cap gap fill: if the jump exceeds capacity, drop history instead.
            if (target - curWindow > int64_t(capacity))
            {
                ring.clear();
                total = 0;
            }
        }
    }

    inline void TimeSeries::Push(uint64_t value) noexcept
    {
        if (ring.size() >= capacity)
        {
            total -= ring.front();
            ring.erase(ring.begin());
        }

        ring.push_back(value);
        total += value;
    }
}



// Implementation of: class MetricSet
namespace CCHI::Prof {

    inline MetricSet::MetricSet() noexcept
        : counters      ()
        , gauges        ()
        , histograms    ()
        , series        ()
    { }

    inline Counter& MetricSet::C(const std::string& name)
    {
        auto& slot = counters[name];
        if (!slot)
            slot = std::make_unique<Counter>();
        return *slot;
    }

    inline Gauge& MetricSet::G(const std::string& name)
    {
        auto& slot = gauges[name];
        if (!slot)
            slot = std::make_unique<Gauge>();
        return *slot;
    }

    inline Histogram& MetricSet::H(const std::string& name)
    {
        auto& slot = histograms[name];
        if (!slot)
            slot = std::make_unique<Histogram>();
        return *slot;
    }

    inline TimeSeries& MetricSet::S(const std::string& name, uint64_t windowTicks, bool gauge)
    {
        auto& slot = series[name];
        if (!slot)
            slot = std::make_unique<TimeSeries>(windowTicks, 4096, gauge);
        return *slot;
    }

    inline bool MetricSet::HasCounter(const std::string& name) const noexcept
    {
        return counters.find(name) != counters.end();
    }

    inline bool MetricSet::HasGauge(const std::string& name) const noexcept
    {
        return gauges.find(name) != gauges.end();
    }

    inline bool MetricSet::HasHistogram(const std::string& name) const noexcept
    {
        return histograms.find(name) != histograms.end();
    }

    inline bool MetricSet::HasSeries(const std::string& name) const noexcept
    {
        return series.find(name) != series.end();
    }

    inline const Counter* MetricSet::GetCounter(const std::string& name) const noexcept
    {
        auto it = counters.find(name);
        return it != counters.end() ? it->second.get() : nullptr;
    }

    inline const Gauge* MetricSet::GetGauge(const std::string& name) const noexcept
    {
        auto it = gauges.find(name);
        return it != gauges.end() ? it->second.get() : nullptr;
    }

    inline const Histogram* MetricSet::GetHistogram(const std::string& name) const noexcept
    {
        auto it = histograms.find(name);
        return it != histograms.end() ? it->second.get() : nullptr;
    }

    inline const TimeSeries* MetricSet::GetSeries(const std::string& name) const noexcept
    {
        auto it = series.find(name);
        return it != series.end() ? it->second.get() : nullptr;
    }

    namespace details {
        template<class T>
        inline std::vector<std::string> SortedKeys(const std::unordered_map<std::string, std::unique_ptr<T>>& map)
        {
            std::vector<std::string> keys;
            keys.reserve(map.size());
            for (const auto& kv : map)
                keys.push_back(kv.first);
            std::sort(keys.begin(), keys.end());
            return keys;
        }
    }

    inline std::vector<std::string> MetricSet::ListCounters() const
    {
        return details::SortedKeys(counters);
    }

    inline std::vector<std::string> MetricSet::ListGauges() const
    {
        return details::SortedKeys(gauges);
    }

    inline std::vector<std::string> MetricSet::ListHistograms() const
    {
        return details::SortedKeys(histograms);
    }

    inline std::vector<std::string> MetricSet::ListSeries() const
    {
        return details::SortedKeys(series);
    }

    inline void MetricSet::MergeFrom(const MetricSet& other, const std::string& prefix)
    {
        for (const auto& kv : other.counters)
            C(prefix + kv.first).Add(kv.second->Get());

        for (const auto& kv : other.gauges)
            G(prefix + kv.first).MergeFrom(*kv.second);

        for (const auto& kv : other.histograms)
            H(prefix + kv.first).MergeFrom(*kv.second);

        // series are intentionally not merged (window alignment not guaranteed)
    }

    inline void MetricSet::Clear() noexcept
    {
        counters.clear();
        gauges.clear();
        histograms.clear();
        series.clear();
    }
}

#endif // __CCHI__CCHI_PROF__METRICS
