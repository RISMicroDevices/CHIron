#pragma once

#include "driver.hpp"

#include <sstream>

namespace LunaticTest::Timing {

    struct Row {
        CHI::Lunatic::ProfileKind profile;
        const char*               id;
    };

    inline const char* ProfileName(CHI::Lunatic::ProfileKind profile)
    {
        return profile == CHI::Lunatic::ProfileKind::Type3 ? "Type3" : "Type7";
    }

    inline void Check(const Row& row, bool condition, const char* expression, const char* file, int line)
    {
        if (condition)
            return;
        std::ostringstream message;
        message << file << ':' << line << " [" << ProfileName(row.profile) << ' ' << row.id << "] " << expression;
        throw std::runtime_error(message.str());
    }

    inline void ExpectTime(const Row& row, CHI::Lunatic::Time actual, CHI::Lunatic::Time expected,
                           const char* label)
    {
        if (actual == expected)
            return;
        std::ostringstream message;
        message << '[' << ProfileName(row.profile) << ' ' << row.id << "] " << label
                << ": expected " << expected << ", got " << actual;
        throw std::runtime_error(message.str());
    }

    inline CHI::Lunatic::Options ProfileOptions(CHI::Lunatic::ProfileKind profile)
    {
        CHI::Lunatic::Options options = profile == CHI::Lunatic::ProfileKind::Type3
            ? CHI::Lunatic::MakeType3Options() : CHI::Lunatic::MakeType7Options();
        options.backend = CHI::Lunatic::BackendKind::Internal;
        options.poisonEnabled = false;
        options.dataCheckEnabled = false;
        options.dmtEnabled = false;
        options.dctEnabled = false;
        options.dwtEnabled = false;
        options.directPersistEnabled = false;
        options.inputQueueLimit = 64;
        options.outputQueueLimit = 64;
        return options;
    }

    inline CHI::Lunatic::Request MakeRequest(uint16_t opcode, CHI::Lunatic::NodeId source = 4,
        CHI::Lunatic::TransactionId txnId = 1, uint64_t address = 0x100000)
    {
        CHI::Lunatic::Request request;
        request.source = source;
        request.target = 2;
        request.txnId = txnId;
        request.opcode = opcode;
        request.address = address;
        request.size = CHI::Lunatic::IsAtomicRequest(opcode) ? 2 : 6;
        request.allowRetry = false;
        request.snpAttr = CHI::Lunatic::IsCoherentRequest(opcode) ||
            opcode == CHI::Eb::Opcodes::REQ::ReadOnce;
        request.memAttr = CHI::Lunatic::IsCoherentRequest(opcode) ||
            opcode == CHI::Eb::Opcodes::REQ::ReadOnce
            ? CHI::Eb::MemAttrs::WriteBack_Allocate : CHI::Eb::MemAttrs::NonCacheable_Bufferable;
        return request;
    }

    struct CoreDriver {
        CHI::Lunatic::Core::Impl impl;

        explicit CoreDriver(CHI::Lunatic::Options options)
            : impl(2, options, std::make_shared<CHI::Lunatic::EventHub>())
        {
        }

        CHI::Lunatic::Transaction& Add(const CHI::Lunatic::Request& request)
        {
            impl.AdmitRequest(request);
            CHI::Lunatic::Transaction* transaction = impl.tracker.FindRequest(request.source, request.txnId);
            if (!transaction)
                throw std::runtime_error("timing test request was not admitted");
            transaction->dbid = transaction->handle;
            return *transaction;
        }

        void TickTo(CHI::Lunatic::Time target)
        {
            while (impl.time < target)
            {
                ++impl.time;
                impl.Tick();
            }
        }
    };

}

#define TIMING_CHECK(row, expression) \
    ::LunaticTest::Timing::Check(row, static_cast<bool>(expression), #expression, __FILE__, __LINE__)
