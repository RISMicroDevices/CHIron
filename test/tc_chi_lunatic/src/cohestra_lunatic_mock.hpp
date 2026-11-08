#pragma once

#include "cchi/cohestra/cohestra_lunatic.hpp"

#include <deque>
#include <map>
#include <optional>
#include <set>

namespace LunaticTest {

    class FakeCHIToRNInterface : public Cohestra::CHI::CHIToRNInterface,
                                 public Cohestra::CHI::CHITransportStatusInterface,
                                 public Cohestra::CHI::CHITransportResetInterface {
    public:
        struct Port {
            std::deque<Cohestra::CHI::RSPFlit>  rxrsp;
            std::deque<Cohestra::CHI::DATFlit>  rxdat;
            std::deque<Cohestra::CHI::SNPFlit>  rxsnp;
            std::deque<Cohestra::CHI::REQFlit>  txreq;
            std::deque<Cohestra::CHI::RSPFlit>  txrsp;
            std::deque<Cohestra::CHI::DATFlit>  txdat;
        };

        std::map<size_t, Port>  ports;
        size_t                  outputLimit = 4;
        bool                    creditValid = true;

        FakeCHIToRNInterface(std::initializer_list<size_t> indices = { 0, 1 })
        {
            for (size_t index : indices)
                ports[index];
        }

        void TickPreHandshake(uint64_t) noexcept override { }
        void TickPostHandshake(uint64_t) noexcept override { }
        void Reset() noexcept override
        {
            for (auto& [index, port] : ports)
                port = Port{};
            creditValid = true;
        }

        size_t GetPortCount() const noexcept override { return ports.size(); }
        std::set<size_t> GetPortIndices() const noexcept override
        {
            std::set<size_t> result;
            for (const auto& [index, port] : ports)
                result.insert(index);
            return result;
        }
        size_t GetPortMaxIndex() const noexcept override { return ports.empty() ? 0 : ports.rbegin()->first; }

        bool PushRXRSP(size_t index, const Cohestra::CHI::RSPFlit& flit) noexcept override
        {
            return Push(ports.at(index).rxrsp, flit);
        }
        bool PushRXDAT(size_t index, const Cohestra::CHI::DATFlit& flit) noexcept override
        {
            return Push(ports.at(index).rxdat, flit);
        }
        bool PushRXSNP(size_t index, const Cohestra::CHI::SNPFlit& flit) noexcept override
        {
            return Push(ports.at(index).rxsnp, flit);
        }

        bool HasTXREQ(size_t index) const noexcept override { return !ports.at(index).txreq.empty(); }
        bool HasTXRSP(size_t index) const noexcept override { return !ports.at(index).txrsp.empty(); }
        bool HasTXDAT(size_t index) const noexcept override { return !ports.at(index).txdat.empty(); }

        std::optional<Cohestra::CHI::REQFlit> PeekTXREQ(size_t index) const noexcept override
        {
            return Peek(ports.at(index).txreq);
        }
        std::optional<Cohestra::CHI::REQFlit> PopTXREQ(size_t index) noexcept override
        {
            return Pop(ports.at(index).txreq);
        }
        std::optional<Cohestra::CHI::RSPFlit> PeekTXRSP(size_t index) const noexcept override
        {
            return Peek(ports.at(index).txrsp);
        }
        std::optional<Cohestra::CHI::RSPFlit> PopTXRSP(size_t index) noexcept override
        {
            return Pop(ports.at(index).txrsp);
        }
        std::optional<Cohestra::CHI::DATFlit> PeekTXDAT(size_t index) const noexcept override
        {
            return Peek(ports.at(index).txdat);
        }
        std::optional<Cohestra::CHI::DATFlit> PopTXDAT(size_t index) noexcept override
        {
            return Pop(ports.at(index).txdat);
        }

        bool SetAllRXRSPBufferSize(size_t size) noexcept override { outputLimit = size; return true; }
        bool SetAllRXDATBufferSize(size_t size) noexcept override { outputLimit = size; return true; }
        bool SetAllRXSNPBufferSize(size_t size) noexcept override { outputLimit = size; return true; }

        bool IsDrained() const noexcept override
        {
            for (const auto& [index, port] : ports)
                if (!port.rxrsp.empty() || !port.rxdat.empty() || !port.rxsnp.empty() ||
                    !port.txreq.empty() || !port.txrsp.empty() || !port.txdat.empty())
                    return false;
            return true;
        }
        bool ValidateCreditAccounting() const noexcept override { return creditValid; }

        void InjectREQ(size_t index, const Cohestra::CHI::REQFlit& flit) { ports.at(index).txreq.push_back(flit); }
        void InjectRSP(size_t index, const Cohestra::CHI::RSPFlit& flit) { ports.at(index).txrsp.push_back(flit); }
        void InjectDAT(size_t index, const Cohestra::CHI::DATFlit& flit) { ports.at(index).txdat.push_back(flit); }

        std::optional<Cohestra::CHI::RSPFlit> TakeRSP(size_t index) { return Pop(ports.at(index).rxrsp); }
        std::optional<Cohestra::CHI::DATFlit> TakeDAT(size_t index) { return Pop(ports.at(index).rxdat); }
        std::optional<Cohestra::CHI::SNPFlit> TakeSNP(size_t index) { return Pop(ports.at(index).rxsnp); }

    private:
        template<typename T>
        bool Push(std::deque<T>& queue, const T& flit)
        {
            if (queue.size() >= outputLimit)
                return false;
            queue.push_back(flit);
            return true;
        }

        template<typename T>
        static std::optional<T> Peek(const std::deque<T>& queue)
        {
            if (queue.empty())
                return std::nullopt;
            return queue.front();
        }

        template<typename T>
        static std::optional<T> Pop(std::deque<T>& queue)
        {
            if (queue.empty())
                return std::nullopt;
            T flit = queue.front();
            queue.pop_front();
            return flit;
        }
    };

}
