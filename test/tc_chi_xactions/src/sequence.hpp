#pragma once

#include "tc_common.hpp"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace XactTest {

    namespace XactDenial = Xact::XactDenial;

    struct Case {
        std::string           name;
        std::function<void()> run;
    };

    inline std::vector<Case>& Registry()
    {
        static std::vector<Case> cases;
        return cases;
    }

    struct Register {
        Register(std::string name, std::function<void()> run)
        {
            Registry().push_back({ std::move(name), std::move(run) });
        }
    };

    inline void Check(bool condition, const char* expression, const char* file, int line)
    {
        if (!condition)
            throw std::runtime_error(std::string(file) + ":" + std::to_string(line) + ": " + expression);
    }

    inline void Accept(Xact::XactDenialEnum denial, const char* expression, const char* file, int line)
    {
        if (denial != Xact::XactDenial::ACCEPTED)
            throw std::runtime_error(std::string(file) + ":" + std::to_string(line) + ": " + expression +
                ": " + denial->name);
    }

    inline void Run(size_t& totalCount, size_t& errCountFail, std::vector<std::string>& errList,
                    const char* filter = nullptr)
    {
        for (const Case& test : Registry())
        {
            if (filter && *filter && test.name.find(filter) == std::string::npos)
                continue;
            ++totalCount;
            try
            {
                test.run();
                std::cout << "[PASS] " << test.name << std::endl;
            }
            catch (const std::exception& error)
            {
                ++errCountFail;
                errList.push_back(test.name + ": " + error.what());
                std::cout << "[FAIL] " << test.name << std::endl;
            }
        }
    }

    inline Flits::REQ<config> Request(uint16_t opcode, uint16_t txnId = 1)
    {
        Flits::REQ<config> request{};
        request.SrcID() = 4;
        request.TgtID() = 2;
        request.TxnID() = txnId;
        request.Opcode() = opcode;
        request.Addr() = 0x10000;
        request.Size() = 6;
        request.AllowRetry() = 1;
        request.ExpCompAck() = opcode == Opcodes::REQ::ReadShared || opcode == Opcodes::REQ::ReadUnique ||
            opcode == Opcodes::REQ::ReadClean || opcode == Opcodes::REQ::ReadNotSharedDirty ||
            opcode == Opcodes::REQ::ReadPreferUnique || opcode == Opcodes::REQ::MakeUnique ||
            opcode == Opcodes::REQ::MakeReadUnique;
        request.SnpAttr() = 1;
        request.MemAttr() = MemAttrs::WriteBack_Allocate;
        return request;
    }

    inline Flits::RSP<config> Response(uint16_t source, uint16_t target, uint16_t opcode,
                                       uint16_t txnId, uint16_t dbid = 0)
    {
        Flits::RSP<config> response{};
        response.SrcID() = source;
        response.TgtID() = target;
        response.TxnID() = txnId;
        response.Opcode() = opcode;
        response.DBID() = dbid;
        return response;
    }

    inline Flits::DAT<config> Data(uint16_t source, uint16_t target, uint16_t opcode,
                                   uint16_t txnId, uint8_t dataId, uint16_t dbid = 0)
    {
        Flits::DAT<config> data{};
        data.SrcID() = source;
        data.TgtID() = target;
        data.TxnID() = txnId;
        data.Opcode() = opcode;
        data.DataID() = dataId;
        data.DBID() = dbid;
        data.BE() = static_cast<typename Flits::DAT<config>::be_t>(~uint64_t(0));
        return data;
    }

    class RNFixture {
    public:
        Xact::Global<config>   global;
        Xact::RNFJoint<config> joint;

        RNFixture()
        {
            global.TOPOLOGY.SetNode(2, Xact::NodeType::HN_F);
            global.TOPOLOGY.SetNode(4, Xact::NodeType::RN_F);
        }
    };

    class SNFixture {
    public:
        Xact::Global<config>   global;
        Xact::SNFJoint<config> joint;

        SNFixture()
        {
            global.TOPOLOGY.SetNode(2, Xact::NodeType::HN_F);
            global.TOPOLOGY.SetNode(6, Xact::NodeType::SN_F);
        }
    };

}

#define XACT_TEST(name) \
    static void name(); \
    static ::XactTest::Register register_##name(#name, name); \
    static void name()

#define XACT_CHECK(expression) \
    ::XactTest::Check(static_cast<bool>(expression), #expression, __FILE__, __LINE__)

#define XACT_ACCEPT(expression) \
    ::XactTest::Accept((expression), #expression, __FILE__, __LINE__)
