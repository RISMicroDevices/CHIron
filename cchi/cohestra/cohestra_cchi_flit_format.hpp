#pragma once

#include "../spec/cchi_protocol_flits.hpp"
#include "../spec/cchi_protocol_encoding.hpp"
#include "../util/cchi_util_decoding.hpp"

#include "../../common/utility.hpp"

#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>


namespace Cohestra {

    /*
    *   Shared, dependency-free decoded flit formatting helpers.
    *
    *   Used by CCHIFlitLogger and TransactionWatchdog so both components
    *   emit the same decoded-field / raw-value style.
    */
    namespace CCHIFlitFormat {

        // Transaction hash-tag token.  Stored on Xaction::companion, it lets
        // the logger and the watchdog refer to the same transaction ID without
        // an external map.
        inline constexpr CompanionTypeBack<uint64_t>    XactHashTagBack{};
        inline constexpr CompanionType<uint64_t>        XactHashTag = &XactHashTagBack;

        template<typename T>
        inline void AppendRaw(std::ostringstream& oss, const char* name, const T& value, size_t width) noexcept;

        inline void AppendRawValue(std::ostringstream& oss, const char* name, uint64_t value, size_t width) noexcept;

        inline void AppendDecodedField(std::ostringstream& oss, const char* decoded, const char* name, uint64_t value, size_t width) noexcept;

        template<typename T>
        inline void AppendData(std::ostringstream& oss, const T* data, size_t wordCount) noexcept;

        template<typename Cfg>
        inline std::string FormatEVT(const CCHI::Flits::EVT<Cfg>& flit) noexcept;

        template<typename Cfg>
        inline std::string FormatREQ(const CCHI::Flits::REQ<Cfg>& flit) noexcept;

        template<typename Cfg>
        inline std::string FormatSNP(const CCHI::Flits::SNP<Cfg>& flit) noexcept;

        template<typename Cfg>
        inline std::string FormatDnRSP(const CCHI::Flits::DnRSP<Cfg>& flit) noexcept;

        template<typename Cfg>
        inline std::string FormatUpRSP(const CCHI::Flits::UpRSP<Cfg>& flit) noexcept;

        template<typename Cfg>
        inline std::string FormatDnDAT(const CCHI::Flits::DnDAT<Cfg>& flit) noexcept;

        template<typename Cfg>
        inline std::string FormatUpDAT(const CCHI::Flits::UpDAT<Cfg>& flit) noexcept;
    }
}


// Implementation of: namespace CCHIFlitFormat
namespace Cohestra {

    namespace CCHIFlitFormat {

        inline void AppendRawValue(std::ostringstream& oss, const char* name, uint64_t value, size_t width) noexcept
        {
            size_t digits = width ? (width + 3) / 4 : 1;

            oss << name << "=0x";

            char oldFill = oss.fill('0');
            oss << std::hex << std::setw(static_cast<int>(digits)) << value << std::dec;
            oss.fill(oldFill);
        }

        template<typename T>
        inline void AppendRaw(std::ostringstream& oss, const char* name, const T& value, size_t width) noexcept
        {
            if constexpr (std::is_same_v<std::decay_t<T>, std::monostate>)
            {
                (void)oss;
                (void)name;
                (void)width;
                return;
            }
            else
            {
                oss << ' ';
                AppendRawValue(oss, name, static_cast<uint64_t>(value), width);
            }
        }

        inline void AppendDecodedField(std::ostringstream& oss, const char* decoded, const char* name, uint64_t value, size_t width) noexcept
        {
            if (decoded && decoded[0])
            {
                oss << ' ' << decoded << " (";
                AppendRawValue(oss, name, value, width);
                oss << ')';
            }
            else
            {
                oss << ' ';
                AppendRawValue(oss, name, value, width);
            }
        }

        template<typename T>
        inline void AppendData(std::ostringstream& oss, const T* data, size_t wordCount) noexcept
        {
            oss << " Data=[";

            for (size_t i = 0; i < wordCount; ++i)
            {
                if (i)
                    oss << ",";

                char oldFill = oss.fill('0');
                oss << std::hex << std::setw(16) << static_cast<uint64_t>(data[i]) << std::dec;
                oss.fill(oldFill);
            }

            oss << "]";
        }


        template<typename Cfg>
        inline std::string FormatEVT(const CCHI::Flits::EVT<Cfg>& flit) noexcept
        {
            std::ostringstream oss;

            if constexpr (CCHI::Flits::EVT<Cfg>::hasOpcode)
            {
                const auto& info = CCHI::EVTOpcodeDecoder<CCHI::Flits::EVT<Cfg>>::INSTANCE.Decode(flit.Opcode);
                const char* decoded = info.IsValid() ? info.GetName() : nullptr;
                AppendDecodedField(oss, decoded, "Opcode", static_cast<uint64_t>(flit.Opcode), CCHI::Flits::EVT<Cfg>::OPCODE_WIDTH);
            }

            AppendRaw(oss, "TxnID",    flit.TxnID,    CCHI::Flits::EVT<Cfg>::TXNID_WIDTH);
            AppendRaw(oss, "SrcID",    flit.SrcID,    CCHI::Flits::EVT<Cfg>::SRCID_WIDTH);
            AppendRaw(oss, "TgtID",    flit.TgtID,    CCHI::Flits::EVT<Cfg>::TGTID_WIDTH);
            AppendRaw(oss, "Addr",     flit.Addr,     CCHI::Flits::EVT<Cfg>::ADDR_WIDTH);
            AppendRaw(oss, "NS",       flit.NS,       CCHI::Flits::EVT<Cfg>::NS_WIDTH);
            AppendRaw(oss, "MemAttr",  flit.MemAttr,  CCHI::Flits::EVT<Cfg>::MEMATTR_WIDTH);

            if constexpr (CCHI::Flits::EVT<Cfg>::hasWayValid)
                AppendRaw(oss, "WayValid", flit.WayValid, CCHI::Flits::EVT<Cfg>::WAYVALID_WIDTH);

            if constexpr (CCHI::Flits::EVT<Cfg>::hasWay)
                AppendRaw(oss, "Way",      flit.Way,      CCHI::Flits::EVT<Cfg>::WAY_WIDTH);

            AppendRaw(oss, "TraceTag", flit.TraceTag, CCHI::Flits::EVT<Cfg>::TRACETAG_WIDTH);

            return oss.str();
        }

        template<typename Cfg>
        inline std::string FormatREQ(const CCHI::Flits::REQ<Cfg>& flit) noexcept
        {
            std::ostringstream oss;

            if constexpr (CCHI::Flits::REQ<Cfg>::hasOpcode)
            {
                const auto& info = CCHI::REQOpcodeDecoder<CCHI::Flits::REQ<Cfg>>::INSTANCE.Decode(flit.Opcode);
                const char* decoded = info.IsValid() ? info.GetName() : nullptr;
                AppendDecodedField(oss, decoded, "Opcode", static_cast<uint64_t>(flit.Opcode), CCHI::Flits::REQ<Cfg>::OPCODE_WIDTH);
            }

            AppendRaw(oss, "TxnID",      flit.TxnID,      CCHI::Flits::REQ<Cfg>::TXNID_WIDTH);
            AppendRaw(oss, "SrcID",      flit.SrcID,      CCHI::Flits::REQ<Cfg>::SRCID_WIDTH);
            AppendRaw(oss, "TgtID",      flit.TgtID,      CCHI::Flits::REQ<Cfg>::TGTID_WIDTH);

            {
                auto sizeEnum = CCHI::Sizes::ToEnum(flit.Size);
                const char* decoded = (sizeEnum && sizeEnum->IsValid()) ? sizeEnum->name : nullptr;
                AppendDecodedField(oss, decoded, "Size", static_cast<uint64_t>(flit.Size), CCHI::Flits::REQ<Cfg>::SSIZE_WIDTH);
            }

            AppendRaw(oss, "Addr",       flit.Addr,       CCHI::Flits::REQ<Cfg>::ADDR_WIDTH);

            if constexpr (CCHI::Flits::REQ<Cfg>::hasTagAlias)
                AppendRaw(oss, "TagAlias",   flit.TagAlias,   CCHI::Flits::REQ<Cfg>::TAGALIAS_WIDTH);

            AppendRaw(oss, "NS",         flit.NS,         CCHI::Flits::REQ<Cfg>::NS_WIDTH);
            AppendRaw(oss, "Order",      flit.Order,      CCHI::Flits::REQ<Cfg>::ORDER_WIDTH);
            AppendRaw(oss, "MemAttr",    flit.MemAttr,    CCHI::Flits::REQ<Cfg>::MEMATTR_WIDTH);
            AppendRaw(oss, "Excl",       flit.Excl,       CCHI::Flits::REQ<Cfg>::EXCL_WIDTH);
            AppendRaw(oss, "ExpCompData",flit.ExpCompData,CCHI::Flits::REQ<Cfg>::EXPCOMPDATA_WIDTH);

            if constexpr (CCHI::Flits::REQ<Cfg>::WAYVALID_WIDTH > 0)
                AppendRaw(oss, "WayValid",   flit.WayValid,   CCHI::Flits::REQ<Cfg>::WAYVALID_WIDTH);

            if constexpr (CCHI::Flits::REQ<Cfg>::WAY_WIDTH > 0)
                AppendRaw(oss, "Way",        flit.Way,        CCHI::Flits::REQ<Cfg>::WAY_WIDTH);

            AppendRaw(oss, "TraceTag",   flit.TraceTag,   CCHI::Flits::REQ<Cfg>::TRACETAG_WIDTH);

            return oss.str();
        }

        template<typename Cfg>
        inline std::string FormatSNP(const CCHI::Flits::SNP<Cfg>& flit) noexcept
        {
            std::ostringstream oss;

            if constexpr (CCHI::Flits::SNP<Cfg>::hasOpcode)
            {
                const auto& info = CCHI::SNPOpcodeDecoder<CCHI::Flits::SNP<Cfg>>::INSTANCE.Decode(flit.Opcode);
                const char* decoded = info.IsValid() ? info.GetName() : nullptr;
                AppendDecodedField(oss, decoded, "Opcode", static_cast<uint64_t>(flit.Opcode), CCHI::Flits::SNP<Cfg>::OPCODE_WIDTH);
            }

            AppendRaw(oss, "TxnID",    flit.TxnID,    CCHI::Flits::SNP<Cfg>::TXNID_WIDTH);
            AppendRaw(oss, "SrcID",    flit.SrcID,    CCHI::Flits::SNP<Cfg>::SRCID_WIDTH);
            AppendRaw(oss, "TgtID",    flit.TgtID,    CCHI::Flits::SNP<Cfg>::TGTID_WIDTH);

            {
                // SNP::Addr carries PA >> 3; restore the full PA.
                uint64_t lineIndex = static_cast<uint64_t>(flit.Addr);
                uint64_t pa        = lineIndex << 3;

                oss << ' ';
                AppendRawValue(oss, "Addr", pa, CCHI::Flits::SNP<Cfg>::ADDR_WIDTH + 3);
                oss << ' ';
                AppendRawValue(oss, "AddrLine", lineIndex, CCHI::Flits::SNP<Cfg>::ADDR_WIDTH);
            }

            AppendRaw(oss, "NS",       flit.NS,       CCHI::Flits::SNP<Cfg>::NS_WIDTH);
            AppendRaw(oss, "TraceTag", flit.TraceTag, CCHI::Flits::SNP<Cfg>::TRACETAG_WIDTH);

            return oss.str();
        }

        template<typename Cfg>
        inline std::string FormatDnRSP(const CCHI::Flits::DnRSP<Cfg>& flit) noexcept
        {
            std::ostringstream oss;

            if constexpr (CCHI::Flits::DnRSP<Cfg>::hasOpcode)
            {
                const auto& info = CCHI::DnRSPOpcodeDecoder<CCHI::Flits::DnRSP<Cfg>>::INSTANCE.Decode(flit.Opcode);
                const char* decoded = info.IsValid() ? info.GetName() : nullptr;
                AppendDecodedField(oss, decoded, "Opcode", static_cast<uint64_t>(flit.Opcode), CCHI::Flits::DnRSP<Cfg>::OPCODE_WIDTH);
            }

            AppendRaw(oss, "TxnID",    flit.TxnID,    CCHI::Flits::DnRSP<Cfg>::TXNID_WIDTH);
            AppendRaw(oss, "SrcID",    flit.SrcID,    CCHI::Flits::DnRSP<Cfg>::SRCID_WIDTH);
            AppendRaw(oss, "TgtID",    flit.TgtID,    CCHI::Flits::DnRSP<Cfg>::TGTID_WIDTH);
            AppendRaw(oss, "DBID",     flit.DBID,     CCHI::Flits::DnRSP<Cfg>::DBID_WIDTH);
            AppendRaw(oss, "RespErr",  flit.RespErr,  CCHI::Flits::DnRSP<Cfg>::RESPERR_WIDTH);

            {
                auto respEnum = CCHI::Resps::ToEnum(flit.Resp);
                const char* decoded = (respEnum && respEnum->IsValid()) ? respEnum->name : nullptr;
                AppendDecodedField(oss, decoded, "Resp", static_cast<uint64_t>(flit.Resp), CCHI::Flits::DnRSP<Cfg>::RESP_WIDTH);
            }

            AppendRaw(oss, "CBusy",    flit.CBusy,    CCHI::Flits::DnRSP<Cfg>::CBUSY_WIDTH);

            if constexpr (CCHI::Flits::DnRSP<Cfg>::hasWayValid)
                AppendRaw(oss, "WayValid", flit.WayValid, CCHI::Flits::DnRSP<Cfg>::WAYVALID_WIDTH);

            if constexpr (CCHI::Flits::DnRSP<Cfg>::hasWay)
                AppendRaw(oss, "Way",      flit.Way,      CCHI::Flits::DnRSP<Cfg>::WAY_WIDTH);

            AppendRaw(oss, "TraceTag", flit.TraceTag, CCHI::Flits::DnRSP<Cfg>::TRACETAG_WIDTH);

            return oss.str();
        }

        template<typename Cfg>
        inline std::string FormatUpRSP(const CCHI::Flits::UpRSP<Cfg>& flit) noexcept
        {
            std::ostringstream oss;

            if constexpr (CCHI::Flits::UpRSP<Cfg>::hasOpcode)
            {
                const auto& info = CCHI::UpRSPOpcodeDecoder<CCHI::Flits::UpRSP<Cfg>>::INSTANCE.Decode(flit.Opcode);
                const char* decoded = info.IsValid() ? info.GetName() : nullptr;
                AppendDecodedField(oss, decoded, "Opcode", static_cast<uint64_t>(flit.Opcode), CCHI::Flits::UpRSP<Cfg>::OPCODE_WIDTH);
            }

            AppendRaw(oss, "TxnID",    flit.TxnID,    CCHI::Flits::UpRSP<Cfg>::TXNID_WIDTH);
            AppendRaw(oss, "SrcID",    flit.SrcID,    CCHI::Flits::UpRSP<Cfg>::SRCID_WIDTH);
            AppendRaw(oss, "TgtID",    flit.TgtID,    CCHI::Flits::UpRSP<Cfg>::TGTID_WIDTH);
            AppendRaw(oss, "RespErr",  flit.RespErr,  CCHI::Flits::UpRSP<Cfg>::RESPERR_WIDTH);

            {
                auto respEnum = CCHI::Resps::ToEnum(flit.Resp);
                const char* decoded = (respEnum && respEnum->IsValid()) ? respEnum->name : nullptr;
                AppendDecodedField(oss, decoded, "Resp", static_cast<uint64_t>(flit.Resp), CCHI::Flits::UpRSP<Cfg>::RESP_WIDTH);
            }

            AppendRaw(oss, "TraceTag", flit.TraceTag, CCHI::Flits::UpRSP<Cfg>::TRACETAG_WIDTH);

            return oss.str();
        }

        template<typename Cfg>
        inline std::string FormatDnDAT(const CCHI::Flits::DnDAT<Cfg>& flit) noexcept
        {
            std::ostringstream oss;

            if constexpr (CCHI::Flits::DnDAT<Cfg>::hasOpcode)
            {
                const auto& info = CCHI::DnDATOpcodeDecoder<CCHI::Flits::DnDAT<Cfg>>::INSTANCE.Decode(flit.Opcode);
                const char* decoded = info.IsValid() ? info.GetName() : nullptr;
                AppendDecodedField(oss, decoded, "Opcode", static_cast<uint64_t>(flit.Opcode), CCHI::Flits::DnDAT<Cfg>::OPCODE_WIDTH);
            }

            AppendRaw(oss, "TxnID",      flit.TxnID,      CCHI::Flits::DnDAT<Cfg>::TXNID_WIDTH);
            AppendRaw(oss, "SrcID",      flit.SrcID,      CCHI::Flits::DnDAT<Cfg>::SRCID_WIDTH);
            AppendRaw(oss, "TgtID",      flit.TgtID,      CCHI::Flits::DnDAT<Cfg>::TGTID_WIDTH);
            AppendRaw(oss, "DBID",       flit.DBID,       CCHI::Flits::DnDAT<Cfg>::DBID_WIDTH);
            AppendRaw(oss, "RespErr",    flit.RespErr,    CCHI::Flits::DnDAT<Cfg>::RESPERR_WIDTH);

            {
                auto respEnum = CCHI::Resps::ToEnum(flit.Resp);
                const char* decoded = (respEnum && respEnum->IsValid()) ? respEnum->name : nullptr;
                AppendDecodedField(oss, decoded, "Resp", static_cast<uint64_t>(flit.Resp), CCHI::Flits::DnDAT<Cfg>::RESP_WIDTH);
            }

            AppendRaw(oss, "DataSource", flit.DataSource, CCHI::Flits::DnDAT<Cfg>::DATASOURCE_WIDTH);
            AppendRaw(oss, "CBusy",      flit.CBusy,      CCHI::Flits::DnDAT<Cfg>::CBUSY_WIDTH);

            if constexpr (CCHI::Flits::DnDAT<Cfg>::hasWayValid)
                AppendRaw(oss, "WayValid",   flit.WayValid,   CCHI::Flits::DnDAT<Cfg>::WAYVALID_WIDTH);

            if constexpr (CCHI::Flits::DnDAT<Cfg>::hasWay)
                AppendRaw(oss, "Way",        flit.Way,        CCHI::Flits::DnDAT<Cfg>::WAY_WIDTH);

            if constexpr (CCHI::Flits::DnDAT<Cfg>::hasDataID)
                AppendRaw(oss, "DataID",     flit.DataID,     CCHI::Flits::DnDAT<Cfg>::DATAID_WIDTH);

            AppendData(oss, flit.Data, CCHI::Flits::DnDAT<Cfg>::DATA_WIDTH / 64);
            AppendRaw(oss, "TraceTag",   flit.TraceTag,   CCHI::Flits::DnDAT<Cfg>::TRACETAG_WIDTH);

            return oss.str();
        }

        template<typename Cfg>
        inline std::string FormatUpDAT(const CCHI::Flits::UpDAT<Cfg>& flit) noexcept
        {
            std::ostringstream oss;

            if constexpr (CCHI::Flits::UpDAT<Cfg>::hasOpcode)
            {
                const auto& info = CCHI::UpDATOpcodeDecoder<CCHI::Flits::UpDAT<Cfg>>::INSTANCE.Decode(flit.Opcode);
                const char* decoded = info.IsValid() ? info.GetName() : nullptr;
                AppendDecodedField(oss, decoded, "Opcode", static_cast<uint64_t>(flit.Opcode), CCHI::Flits::UpDAT<Cfg>::OPCODE_WIDTH);
            }

            AppendRaw(oss, "TxnID",    flit.TxnID,    CCHI::Flits::UpDAT<Cfg>::TXNID_WIDTH);
            AppendRaw(oss, "SrcID",    flit.SrcID,    CCHI::Flits::UpDAT<Cfg>::SRCID_WIDTH);
            AppendRaw(oss, "TgtID",    flit.TgtID,    CCHI::Flits::UpDAT<Cfg>::TGTID_WIDTH);
            AppendRaw(oss, "RespErr",  flit.RespErr,  CCHI::Flits::UpDAT<Cfg>::RESPERR_WIDTH);

            {
                auto respEnum = CCHI::Resps::ToEnum(flit.Resp);
                const char* decoded = (respEnum && respEnum->IsValid()) ? respEnum->name : nullptr;
                AppendDecodedField(oss, decoded, "Resp", static_cast<uint64_t>(flit.Resp), CCHI::Flits::UpDAT<Cfg>::RESP_WIDTH);
            }

            if constexpr (CCHI::Flits::UpDAT<Cfg>::hasDataID)
                AppendRaw(oss, "DataID",   flit.DataID,   CCHI::Flits::UpDAT<Cfg>::DATAID_WIDTH);

            AppendData(oss, flit.Data, CCHI::Flits::UpDAT<Cfg>::DATA_WIDTH / 64);
            AppendRaw(oss, "BE",       flit.BE,       CCHI::Flits::UpDAT<Cfg>::BE_WIDTH);
            AppendRaw(oss, "TraceTag", flit.TraceTag, CCHI::Flits::UpDAT<Cfg>::TRACETAG_WIDTH);

            return oss.str();
        }
    }
}
