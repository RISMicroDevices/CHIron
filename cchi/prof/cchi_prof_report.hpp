#pragma once

#ifndef __CCHI__CCHI_PROF__REPORT
#define __CCHI__CCHI_PROF__REPORT

#include <cstdint>
#include <string>
#include <sstream>
#include <iomanip>

#include "cchi_prof_metrics.hpp"


/*
*   CCHI Performance Profiling Infrastructure: report writers.
*
*   Renders a MetricSet as:
*   - ToText():          human-readable aligned tables (stdout report)
*   - ToJSON():          machine-readable JSON document
*   - CountersToCSV():   name,value rows
*   - HistogramsToCSV(): name,count,min,p25,p50,p75,p90,p99,max,mean rows
*   - SeriesToCSV():     long format name,window_index,value rows
*
*   Latency values are reported in simulation ticks; ReportContext records the
*   tick unit (ticksPerCycle: 1 for the earth harness, 2 for the v3 harness
*   which ticks in half-cycles) so numbers stay comparable across harnesses.
*/
namespace CCHI::Prof {

    // Identifies the run and the tick unit.
    struct ReportContext {
        std::string     title           = "CCHI Profiler";
        uint64_t        ticksPerCycle   = 1;
        uint64_t        elapsedTicks    = 0;

        inline ReportContext() noexcept {}
    };


    class Report {
    public:
        inline static std::string     ToText(const MetricSet& metrics, const ReportContext& context = ReportContext());
        inline static std::string     ToJSON(const MetricSet& metrics, const ReportContext& context = ReportContext());

        inline static std::string     CountersToCSV(const MetricSet& metrics);
        inline static std::string     HistogramsToCSV(const MetricSet& metrics);
        inline static std::string     SeriesToCSV(const MetricSet& metrics);

    private:
        inline static std::string     EscapeJSON(const std::string& str);
    };
}



// Implementation of: class Report
namespace CCHI::Prof {

    namespace details {

        inline std::ostringstream& WriteHistRow(std::ostringstream& oss,
                                                const std::string& name,
                                                const Histogram& hist)
        {
            oss << std::left << std::setw(56) << name
                << std::right << std::setw(10) << hist.GetCount()
                << std::setw(10) << hist.GetMin()
                << std::setw(10) << hist.GetPercentile(25.0)
                << std::setw(10) << hist.GetPercentile(50.0)
                << std::setw(10) << hist.GetPercentile(75.0)
                << std::setw(10) << hist.GetPercentile(90.0)
                << std::setw(10) << hist.GetPercentile(99.0)
                << std::setw(10) << hist.GetMax()
                << std::setw(12) << std::fixed << std::setprecision(2) << hist.GetMean()
                << "\n";
            return oss;
        }
    }

    inline std::string Report::ToText(const MetricSet& metrics, const ReportContext& context)
    {
        std::ostringstream oss;

        oss << "==== " << context.title << " ====\n";
        oss << "ticks_per_cycle = " << context.ticksPerCycle
            << ", elapsed_ticks = " << context.elapsedTicks << "\n\n";

        auto counters = metrics.ListCounters();
        if (!counters.empty())
        {
            oss << "-- Counters --\n";
            for (const auto& name : counters)
            {
                const Counter* c = metrics.GetCounter(name);
                oss << std::left << std::setw(56) << name
                    << std::right << std::setw(12) << (c ? c->Get() : 0) << "\n";
            }
            oss << "\n";
        }

        auto gauges = metrics.ListGauges();
        if (!gauges.empty())
        {
            oss << "-- Gauges --\n";
            oss << std::left << std::setw(56) << "name"
                << std::right << std::setw(12) << "current"
                << std::setw(12) << "min"
                << std::setw(12) << "max" << "\n";
            for (const auto& name : gauges)
            {
                const Gauge* g = metrics.GetGauge(name);
                oss << std::left << std::setw(56) << name
                    << std::right << std::setw(12) << (g ? g->Get() : 0)
                    << std::setw(12) << (g ? g->GetMin() : 0)
                    << std::setw(12) << (g ? g->GetMax() : 0) << "\n";
            }
            oss << "\n";
        }

        auto histograms = metrics.ListHistograms();
        if (!histograms.empty())
        {
            oss << "-- Histograms (ticks; percentiles are bucket lower bounds) --\n";
            oss << std::left << std::setw(56) << "name"
                << std::right << std::setw(10) << "count"
                << std::setw(10) << "min"
                << std::setw(10) << "p25"
                << std::setw(10) << "p50"
                << std::setw(10) << "p75"
                << std::setw(10) << "p90"
                << std::setw(10) << "p99"
                << std::setw(10) << "max"
                << std::setw(12) << "mean" << "\n";
            for (const auto& name : histograms)
            {
                const Histogram* h = metrics.GetHistogram(name);
                if (h)
                    details::WriteHistRow(oss, name, *h);
            }
            oss << "\n";
        }

        auto series = metrics.ListSeries();
        if (!series.empty())
        {
            oss << "-- Series --\n";
            oss << std::left << std::setw(56) << "name"
                << std::right << std::setw(10) << "window"
                << std::setw(10) << "windows"
                << std::setw(12) << "total"
                << std::setw(12) << "max_win"
                << std::setw(12) << "mean_win" << "\n";
            for (const auto& name : series)
            {
                const TimeSeries* s = metrics.GetSeries(name);
                if (!s)
                    continue;

                oss << std::left << std::setw(56) << name
                    << std::right << std::setw(10) << s->GetWindowTicks()
                    << std::setw(10) << s->GetWindowCount()
                    << std::setw(12) << (s->IsGauge() ? 0 : s->GetTotal())
                    << std::setw(12) << s->GetMaxWindow()
                    << std::setw(12) << std::fixed << std::setprecision(2) << s->GetMeanWindow()
                    << (s->IsGauge() ? " (gauge)" : "") << "\n";
            }
            oss << "\n";
        }

        return oss.str();
    }

    inline std::string Report::ToJSON(const MetricSet& metrics, const ReportContext& context)
    {
        std::ostringstream oss;

        oss << "{\n";
        oss << "  \"title\": \"" << EscapeJSON(context.title) << "\",\n";
        oss << "  \"ticks_per_cycle\": " << context.ticksPerCycle << ",\n";
        oss << "  \"elapsed_ticks\": " << context.elapsedTicks << ",\n";

        oss << "  \"counters\": {";
        {
            bool first = true;
            for (const auto& name : metrics.ListCounters())
            {
                const Counter* c = metrics.GetCounter(name);
                if (!c)
                    continue;
                oss << (first ? "\n" : ",\n");
                oss << "    \"" << EscapeJSON(name) << "\": " << c->Get();
                first = false;
            }
            oss << (first ? "" : "\n  ") << "},\n";
        }

        oss << "  \"gauges\": {";
        {
            bool first = true;
            for (const auto& name : metrics.ListGauges())
            {
                const Gauge* g = metrics.GetGauge(name);
                if (!g)
                    continue;
                oss << (first ? "\n" : ",\n");
                oss << "    \"" << EscapeJSON(name) << "\": {"
                    << "\"value\": " << g->Get()
                    << ", \"min\": " << g->GetMin()
                    << ", \"max\": " << g->GetMax() << "}";
                first = false;
            }
            oss << (first ? "" : "\n  ") << "},\n";
        }

        oss << "  \"histograms\": {";
        {
            bool first = true;
            for (const auto& name : metrics.ListHistograms())
            {
                const Histogram* h = metrics.GetHistogram(name);
                if (!h)
                    continue;
                oss << (first ? "\n" : ",\n");
                oss << "    \"" << EscapeJSON(name) << "\": {"
                    << "\"count\": " << h->GetCount()
                    << ", \"min\": " << h->GetMin()
                    << ", \"p25\": " << h->GetPercentile(25.0)
                    << ", \"p50\": " << h->GetPercentile(50.0)
                    << ", \"p75\": " << h->GetPercentile(75.0)
                    << ", \"p90\": " << h->GetPercentile(90.0)
                    << ", \"p99\": " << h->GetPercentile(99.0)
                    << ", \"max\": " << h->GetMax()
                    << ", \"mean\": " << std::fixed << std::setprecision(4) << h->GetMean()
                    << "}";
                first = false;
            }
            oss << (first ? "" : "\n  ") << "},\n";
        }

        oss << "  \"series\": {";
        {
            bool first = true;
            for (const auto& name : metrics.ListSeries())
            {
                const TimeSeries* s = metrics.GetSeries(name);
                if (!s)
                    continue;
                oss << (first ? "\n" : ",\n");
                oss << "    \"" << EscapeJSON(name) << "\": {"
                    << "\"window_ticks\": " << s->GetWindowTicks()
                    << ", \"gauge\": " << (s->IsGauge() ? "true" : "false")
                    << ", \"windows\": [";
                for (size_t i = 0; i < s->GetWindowCount(); i++)
                    oss << (i ? "," : "") << s->GetWindow(i);
                oss << "]}";
                first = false;
            }
            oss << (first ? "" : "\n  ") << "}\n";
        }

        oss << "}\n";
        return oss.str();
    }

    inline std::string Report::CountersToCSV(const MetricSet& metrics)
    {
        std::ostringstream oss;

        oss << "name,value\n";
        for (const auto& name : metrics.ListCounters())
        {
            const Counter* c = metrics.GetCounter(name);
            oss << name << "," << (c ? c->Get() : 0) << "\n";
        }

        return oss.str();
    }

    inline std::string Report::HistogramsToCSV(const MetricSet& metrics)
    {
        std::ostringstream oss;

        oss << "name,count,min,p25,p50,p75,p90,p99,max,mean\n";
        for (const auto& name : metrics.ListHistograms())
        {
            const Histogram* h = metrics.GetHistogram(name);
            if (!h)
                continue;
            oss << name << ","
                << h->GetCount() << ","
                << h->GetMin() << ","
                << h->GetPercentile(25.0) << ","
                << h->GetPercentile(50.0) << ","
                << h->GetPercentile(75.0) << ","
                << h->GetPercentile(90.0) << ","
                << h->GetPercentile(99.0) << ","
                << h->GetMax() << ","
                << std::fixed << std::setprecision(4) << h->GetMean() << "\n";
        }

        return oss.str();
    }

    inline std::string Report::SeriesToCSV(const MetricSet& metrics)
    {
        std::ostringstream oss;

        oss << "name,window_index,value\n";
        for (const auto& name : metrics.ListSeries())
        {
            const TimeSeries* s = metrics.GetSeries(name);
            if (!s)
                continue;
            for (size_t i = 0; i < s->GetWindowCount(); i++)
                oss << name << "," << i << "," << s->GetWindow(i) << "\n";
        }

        return oss.str();
    }

    inline std::string Report::EscapeJSON(const std::string& str)
    {
        std::string out;
        out.reserve(str.size());

        for (char ch : str)
        {
            switch (ch)
            {
                case '\"':  out += "\\\"";  break;
                case '\\':  out += "\\\\";  break;
                case '\n':  out += "\\n";   break;
                case '\r':  out += "\\r";   break;
                case '\t':  out += "\\t";   break;
                default:    out += ch;      break;
            }
        }

        return out;
    }
}

#endif // __CCHI__CCHI_PROF__REPORT
