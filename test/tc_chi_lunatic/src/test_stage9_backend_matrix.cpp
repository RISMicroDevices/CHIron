#include "axi_model.hpp"
#include "checked_subordinate.hpp"
#include "oracle_driver.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

using namespace CHI::Lunatic;
using namespace LunaticTest;

namespace {

    template<typename config>
    class MatrixAXIEndpoint {
        using Node = HomeNode<config>;
        Node& node;

    public:
        explicit MatrixAXIEndpoint(Node& node) : node(node) { }
        std::optional<AXIAddress> PeekAW() const { return node.PeekTXAW(); }
        std::optional<AXIWriteData> PeekW() const { return node.PeekTXW(); }
        std::optional<AXIAddress> PeekAR() const { return node.PeekTXAR(); }
        std::optional<AXIAddress> PopAW() { return node.PopTXAW(); }
        std::optional<AXIWriteData> PopW() { return node.PopTXW(); }
        std::optional<AXIAddress> PopAR() { return node.PopTXAR(); }
        bool PushB(const AXIWriteResponse& response) { return node.PushRXB(response); }
        bool PushR(const AXIReadData& response) { return node.PushRXR(response); }
    };

    template<typename config>
    class MatrixDriver {
        using Checked = CheckedDriver<config>;

    public:
        Checked                       driver;
        BackendKind                   backend;
        CheckedSubordinate<config>    subordinate;
        AXIModel                      axi;
        MatrixAXIEndpoint<config>     endpoint;

        explicit MatrixDriver(Options options)
            : driver(options), backend(driver.driver.node.GetOptions().backend),
              axi(config::dataWidth / 8), endpoint(driver.driver.node)
        {
            if (backend == BackendKind::CHI)
                subordinate.UseCombinedWriteResponse(driver.driver.node.GetOptions().retryEnabled);
        }

        void Tick()
        {
            if (backend == BackendKind::CHI)
                subordinate.Step(driver.driver);
            else if (backend == BackendKind::AXI)
                axi.Tick(endpoint);
            driver.driver.Tick();
        }

        template<typename Predicate>
        void Until(Predicate predicate)
        {
            for (size_t cycle = 0; cycle < 1000; ++cycle)
            {
                if (predicate())
                    return;
                Tick();
            }
            std::string message = "backend matrix timeout, width=" + std::to_string(config::dataWidth) +
                ", backend=" + std::to_string(static_cast<unsigned>(backend));
            for (const auto& error : driver.driver.node.GetErrors())
                message += "; " + error.reason;
            throw std::runtime_error(message);
        }

        typename Checked::RSP NextRSP()
        {
            Until([&] { return static_cast<bool>(driver.driver.node.PeekTXRSP()); });
            return driver.NextRSP(driver.driver.node.PeekTXRSP()->Opcode());
        }

        typename Checked::DAT NextDAT()
        {
            Until([&] { return static_cast<bool>(driver.driver.node.PeekTXDAT()); });
            return driver.NextDAT(CHI::Eb::Opcodes::DAT::CompData);
        }

        typename Checked::SNP NextSNP(uint16_t opcode)
        {
            Until([&] { return static_cast<bool>(driver.driver.node.PeekTXSNP()); });
            return driver.NextSNP(opcode);
        }

        LineData Read(NodeId source, TransactionId txnId, uint64_t address,
            uint16_t opcode = CHI::Eb::Opcodes::REQ::ReadNoSnp,
            uint8_t expectedResp = CHI::Eb::Resps::CompData_I, bool issueRequest = true)
        {
            if (issueRequest)
                driver.PushRequest(driver.Request(opcode, source, txnId, address, true));
            constexpr size_t bytes = config::dataWidth / 8;
            LineData result{};
            DataBufferId dbid = 0;
            for (size_t beat = 0; beat < LINE_SIZE / bytes; ++beat)
            {
                const auto data = NextDAT();
                LUNATIC_CHECK(data.TgtID() == source && data.TxnID() == txnId);
                LUNATIC_CHECK(data.DataID() == beat * bytes / 16);
                LUNATIC_CHECK(data.Resp() == expectedResp);
                dbid = data.DBID();
                for (size_t index = 0; index < bytes; ++index)
                    result[beat * bytes + index] = uint8_t(data.Data()[index / 8] >> ((index % 8) * 8));
            }
            driver.CompAck(source, dbid);
            return result;
        }

        void Write(NodeId source, TransactionId txnId, uint64_t address, const LineData& data)
        {
            ByteEnables enables{};
            enables.fill(1);
            driver.PushRequest(driver.Request(CHI::Eb::Opcodes::REQ::WriteNoSnpFull,
                source, txnId, address));
            const auto response = NextRSP();
            LUNATIC_CHECK(response.Opcode() == CHI::Eb::Opcodes::RSP::DBIDResp ||
                response.Opcode() == CHI::Eb::Opcodes::RSP::CompDBIDResp);
            driver.WriteData(source, txnId, response.DBID(), data, enables,
                CHI::Eb::Opcodes::DAT::NonCopyBackWrData);
            if (response.Opcode() == CHI::Eb::Opcodes::RSP::DBIDResp)
            {
                const auto completion = NextRSP();
                LUNATIC_CHECK(completion.Opcode() == CHI::Eb::Opcodes::RSP::Comp);
                LUNATIC_CHECK(completion.TxnID() == txnId);
            }
        }

        void Seed(const LineKey& line, const LineData& data)
        {
            if (backend == BackendKind::CHI)
                subordinate.Write(line, data);
            else if (backend == BackendKind::AXI)
                axi.SetLine(line.address, data);
            else
                driver.driver.node.GetMemory().WriteFull(line, data);
        }

        LineData Memory(const LineKey& line) const
        {
            if (backend == BackendKind::CHI)
                return subordinate.Read(line);
            if (backend == BackendKind::AXI)
                return axi.ReadLine(line.address);
            return driver.driver.node.GetMemory().Read(line);
        }

        void Quiesce()
        {
            Until([&] {
                return driver.driver.node.Drain() &&
                    (backend != BackendKind::CHI || !subordinate.HasInflight()) &&
                    (backend != BackendKind::AXI || axi.Drain());
            });
            for (const auto& [requester, oracle] : driver.oracles)
                LUNATIC_CHECK(!oracle->HasInflight());
            LUNATIC_CHECK(driver.driver.node.ValidateInvariants());
            LUNATIC_CHECK(driver.driver.node.GetErrors().empty());
            LUNATIC_CHECK(axi.Errors().empty());
        }
    };

    LineData MatrixPattern(uint8_t seed)
    {
        LineData data{};
        for (size_t index = 0; index < data.size(); ++index)
            data[index] = uint8_t(seed + index * 31 + index / 4);
        return data;
    }

    template<typename config>
    size_t RunBackendMatrix()
    {
        size_t cells = 0;
        for (const PolicyKind policy : { PolicyKind::NINE, PolicyKind::Exclusive, PolicyKind::Inclusive })
            for (const BackendKind backend : { BackendKind::Internal, BackendKind::CHI, BackendKind::AXI })
                for (const bool retry : { false, true })
                {
                    Options options;
                    options.policy = policy;
                    options.backend = backend;
                    options.subordinateId = 6;
                    options.retryEnabled = retry;
                    MatrixDriver<config> driver(options);
                    const LineKey line = LineKey::FromAddress(0xc0000 + cells * LINE_SIZE, false);
                    const LineData initial = MatrixPattern(uint8_t(17 + cells));
                    const LineData update = MatrixPattern(uint8_t(131 + cells));
                    driver.Seed(line, initial);
                    LUNATIC_CHECK(driver.Read(4, 1, line.address) == initial);
                    if (backend == BackendKind::CHI && retry)
                        driver.subordinate.RetryNextWrite(policy == PolicyKind::Inclusive);
                    driver.Write(4, 2, line.address, update);
                    driver.Quiesce();
                    LUNATIC_CHECK(driver.Memory(line) == update);
                    LUNATIC_CHECK(driver.Read(5, 3, line.address) == update);
                    driver.Quiesce();
                    LUNATIC_CHECK(driver.Read(6, 4, line.address, CHI::Eb::Opcodes::REQ::ReadUnique,
                        CHI::Eb::Resps::CompData_UC) == update);
                    driver.Quiesce();
                    LUNATIC_CHECK(driver.driver.driver.node.InspectRequester(line, 6) == CacheState::UC);
                    LUNATIC_CHECK(driver.driver.driver.node.InspectHome(line).valid == (policy != PolicyKind::Exclusive));

                    driver.driver.PushRequest(driver.driver.Request(CHI::Eb::Opcodes::REQ::ReadShared,
                        4, 5, line.address, true));
                    const auto snoop = driver.NextSNP(CHI::Eb::Opcodes::SNP::SnpShared);
                    LUNATIC_CHECK(snoop.target == 6);
                    driver.driver.SnoopData(6, snoop.flit.TxnID(), update, CHI::Eb::Resps::SnpRespData_SC);
                    LUNATIC_CHECK(driver.Read(4, 5, line.address, CHI::Eb::Opcodes::REQ::ReadShared,
                        CHI::Eb::Resps::CompData_SC, false) == update);
                    driver.Quiesce();
                    LUNATIC_CHECK(driver.driver.driver.node.InspectRequester(line, 6) == CacheState::SC);
                    LUNATIC_CHECK(driver.driver.driver.node.InspectRequester(line, 4) == CacheState::SC);
                    LUNATIC_CHECK(driver.driver.driver.node.InspectHome(line).valid == (policy != PolicyKind::Exclusive));
                    if (backend == BackendKind::CHI)
                    {
                        const auto& opcodes = driver.subordinate.Opcodes();
                        LUNATIC_CHECK(std::find(opcodes.begin(), opcodes.end(),
                            CHI::Eb::Opcodes::REQ::ReadNoSnp) != opcodes.end());
                        LUNATIC_CHECK(std::find(opcodes.begin(), opcodes.end(),
                            CHI::Eb::Opcodes::REQ::WriteNoSnpFull) != opcodes.end());
                    }
                    if (backend == BackendKind::AXI)
                    {
                        LUNATIC_CHECK(!driver.axi.CompletedReads().empty());
                        LUNATIC_CHECK(!driver.axi.CompletedWrites().empty());
                    }
                    ++cells;
                }
        return cells;
    }

}

LUNATIC_TEST(Stage9BackendMatrix128)
{
    LUNATIC_CHECK(RunBackendMatrix<Config128>() == 18);
}

LUNATIC_TEST(Stage9BackendMatrix256)
{
    LUNATIC_CHECK(RunBackendMatrix<Config256>() == 18);
}

LUNATIC_TEST(Stage9BackendMatrix512)
{
    LUNATIC_CHECK(RunBackendMatrix<Config512>() == 18);
}

namespace {

    template<typename config>
    void ResetMatrixGeneration(MatrixDriver<config>& driver, const LineKey& line, const LineData& data)
    {
        LUNATIC_CHECK(driver.backend != BackendKind::CHI);
        driver.driver.driver.node.Reset();
        driver.axi.Reset();
        driver.driver.oracles.clear();
        driver.driver.driver.now = 0;
        driver.Seed(line, data);
        LUNATIC_CHECK(driver.driver.driver.node.Drain());
        LUNATIC_CHECK(driver.axi.Drain());
        LUNATIC_CHECK(!driver.driver.driver.node.HasInflight());
        LUNATIC_CHECK(driver.driver.driver.node.GetPerformanceCounters() == PerformanceCounters{});
        LUNATIC_CHECK(driver.driver.driver.node.GetErrors().empty());
    }

}

LUNATIC_TEST(Stage9BackendResetGenerations)
{
    Options options;
    options.policy = PolicyKind::NINE;
    options.backend = BackendKind::AXI;
    options.retryEnabled = true;
    MatrixDriver<Config256> driver(options);
    const LineKey line = LineKey::FromAddress(0xd0000, false);
    const LineData initial = MatrixPattern(37);
    driver.Seed(line, initial);
    driver.driver.PushRequest(driver.driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp,
        4, 7, line.address, true));
    driver.Until([&] {
        return !driver.axi.CompletedReads().empty() && driver.driver.driver.node.PeekTXDAT();
    });
    LUNATIC_CHECK(driver.driver.driver.node.HasInflight());
    ResetMatrixGeneration(driver, line, initial);
    LUNATIC_CHECK(driver.Read(4, 7, line.address) == initial);
    driver.Quiesce();
    LUNATIC_CHECK(driver.axi.CompletedReads().size() == 1);
    LUNATIC_CHECK(driver.driver.driver.node.GetPerformanceCounters().acceptedRXREQ == 1);
    LUNATIC_CHECK(driver.driver.driver.node.GetPerformanceCounters().retiredTransactions == 1);

    const LineKey retryLine = LineKey::FromAddress(0xd1000, false);
    const LineData retryData = MatrixPattern(149);
    driver.Seed(retryLine, retryData);
    bool forceRetry = true;
    size_t retryRequests = 0;
    driver.driver.driver.node.GetEvents()->OnRequestAdmission.Register(
        Gravity::MakeListener<RequestAdmissionEvent>("matrix-reset-retry", 0,
            [&](RequestAdmissionEvent& event) {
                if (forceRetry && event.request.txnId == 8 && event.request.allowRetry)
                {
                    forceRetry = false;
                    ++retryRequests;
                    event.RequestRetry();
                }
            }));
    driver.driver.PushRequest(driver.driver.Request(CHI::Eb::Opcodes::REQ::ReadNoSnp,
        4, 8, retryLine.address, true));
    driver.Until([&] {
        const auto response = driver.driver.driver.node.PeekTXRSP();
        return retryRequests == 1 && response &&
            response->Opcode() == CHI::Eb::Opcodes::RSP::RetryAck;
    });
    LUNATIC_CHECK(driver.driver.driver.node.HasInflight());
    ResetMatrixGeneration(driver, retryLine, retryData);
    LUNATIC_CHECK(driver.Read(4, 8, retryLine.address) == retryData);
    driver.Quiesce();
    LUNATIC_CHECK(driver.driver.driver.node.GetPerformanceCounters().emittedRetryAcks == 0);
    LUNATIC_CHECK(driver.driver.driver.node.GetPerformanceCounters().retiredTransactions == 1);

    options.backend = BackendKind::Internal;
    MatrixDriver<Config256> snoopDriver(options);
    const LineKey snoopLine = LineKey::FromAddress(0xd2000, false);
    const LineData snoopData = MatrixPattern(191);
    snoopDriver.Seed(snoopLine, snoopData);
    LUNATIC_CHECK(snoopDriver.driver.ReadData(4, 9, snoopLine.address,
        CHI::Eb::Opcodes::REQ::ReadUnique) == snoopData);
    snoopDriver.Quiesce();
    snoopDriver.driver.PushRequest(snoopDriver.driver.Request(CHI::Eb::Opcodes::REQ::ReadUnique,
        5, 10, snoopLine.address, true));
    snoopDriver.Until([&] { return static_cast<bool>(snoopDriver.driver.driver.node.PeekTXSNP()); });
    LUNATIC_CHECK(snoopDriver.driver.driver.node.HasInflight());
    ResetMatrixGeneration(snoopDriver, snoopLine, snoopData);
    LUNATIC_CHECK(snoopDriver.Read(5, 10, snoopLine.address) == snoopData);
    snoopDriver.Quiesce();
    LUNATIC_CHECK(!snoopDriver.driver.driver.node.PeekTXSNP());
    LUNATIC_CHECK(snoopDriver.driver.driver.node.GetPerformanceCounters().emittedTXSNP == 0);
}
