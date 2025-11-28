//#pragma once

//#ifndef __CHI__CHI_EXPRESSO_FLIT
//#define __CHI__CHI_EXPRESSO_FLIT

#ifndef CHI_EXPRESSO_FLIT__STANDALONE
#   define CHI_ISSUE_EB_ENABLE
#   include "chi_expresso_flit_header.hpp"      // IWYU pragma: keep
#   include "../spec/chi_protocol_flits.hpp"    // IWYU pragma: keep
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_EXPRESSO_FLIT_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_EXPRESSO_FLIT_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_EXPRESSO_FLIT_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_EXPRESSO_FLIT_EB
#endif


/*
namespace CHI {
*/
    namespace Expresso::Flit {

        class KeyBack;

        using Key = const KeyBack*;
        class Value;
        using KeyValueMap = std::unordered_map<Key, Value>;

        class Formatter;

        using format_func = std::function<std::string(Key)>;

        using formatter_func = std::string(const Formatter*, const KeyValueMap&, const format_func&);
        using formatter_handle = std::function<formatter_func>;


        enum class KeyCategory {
            REQ = 0,
            RSP,
            DAT,
            SNP
        };

        class KeyBack {
        public:
            const KeyBack* const    prev;
            const KeyCategory       category;
            const unsigned int      ordinal;
            const char* const       name;
            const char* const       canonicalName;
            formatter_func* const   formatterCaller;

        public:
            inline constexpr KeyBack(const KeyCategory          category,
                                     const char*                name,
                                     const char*                canonicalName,
                                     formatter_func*            formatterCaller) noexcept
                : prev              (nullptr)
                , category          (category)
                , ordinal           (0)
                , name              (name)
                , canonicalName     (canonicalName)
                , formatterCaller   (formatterCaller)
            { }

            inline constexpr KeyBack(const KeyCategory          category,
                                     const char*                name,
                                     const char*                canonicalName,
                                     const KeyBack* const       prev,
                                     formatter_func*            formatterCaller) noexcept
                : prev              (prev)
                , category          (category)
                , ordinal           (prev->ordinal + 1)
                , name              (name)
                , canonicalName     (canonicalName)
                , formatterCaller   (formatterCaller)
            { }

        public:
            inline constexpr operator const KeyBack*() const noexcept
            { return this; }

        public:
            inline std::string Format(const Formatter& f, const KeyValueMap& kv, const format_func& fmt) const;
            inline std::string Format(const Formatter& f, const KeyValueMap& kv, const std::string& fmt) const;
        };

        using Key = const KeyBack*;

        class KeyIterator {
        protected:
            Key current;

        public:
            inline constexpr KeyIterator(Key current) noexcept : current(current) {}

        public: // iterator implementation
            using iterator_category = std::forward_iterator_tag;
            using value_type = const KeyBack*;
            using difference_type = std::monostate;

            inline value_type operator*() const noexcept { return current; }
            inline value_type operator->() const noexcept { return current; }
            inline KeyIterator& operator++() noexcept { if (current) current = current->prev; return *this; }
            inline KeyIterator operator++(int) noexcept { auto tmp = *this; ++*this; return tmp; }

            inline constexpr bool operator==(const KeyIterator& obj) const noexcept { return current == obj.current; }
            inline constexpr bool operator!=(const KeyIterator& obj) const noexcept { return current != obj.current; }
        };

        enum class ValueType {
            Integral,
            Vector
        };

        class Value {
        protected:
            ValueType                   type;
            union       {
                uint64_t                value;
                std::vector<uint64_t>*  vector;
            };

        public:
            inline Value(ValueType type) noexcept;
            inline Value(const Value& obj) noexcept;
            inline Value(Value&& obj) noexcept;
            inline ~Value() noexcept;

        protected:
            inline Value(const std::vector<uint64_t>& vec) noexcept;
            inline Value(std::vector<uint64_t>&& vec) noexcept;

        public:
            inline Value& operator=(const Value&) noexcept;
            inline Value& operator=(Value&&) noexcept;

        public:
            inline ValueType                    GetType() const noexcept;
            inline bool                         IsIntegral() const noexcept;
            inline bool                         IsVector() const noexcept;

            inline uint64_t                     GetValue() const noexcept;
            inline std::vector<uint64_t>*       GetVector() const noexcept;

            inline uint64_t&                    AsIntegral() noexcept;
            inline uint64_t                     AsIntegral() const noexcept;

            inline std::vector<uint64_t>&       AsVector() noexcept;
            inline const std::vector<uint64_t>& AsVector() const noexcept;
        };

        class ValueIntegral : public Value {
        public:
            inline ValueIntegral(uint64_t value = 0) noexcept;

        public:
            inline void             SetValue(uint64_t value) noexcept;
        };

        static_assert(sizeof(ValueIntegral) == sizeof(Value));

        class ValueVector : public Value {
        public:
            inline ValueVector(size_t size) noexcept;
            inline ValueVector(size_t size, uint64_t* array) noexcept;
            inline ValueVector(const std::vector<uint64_t>& vec) noexcept;
            inline ValueVector(std::vector<uint64_t>&& vec) noexcept;
            inline ValueVector(const ValueVector& obj) noexcept;
            inline ValueVector(ValueVector&& obj) noexcept;
            inline ~ValueVector() noexcept;

        public:
            inline size_t           GetSize() const noexcept;

            inline uint64_t         GetValue(size_t index) const noexcept;
            inline void             SetValue(size_t index, uint64_t value) noexcept;

            inline uint64_t&        operator[](size_t index) noexcept;
            inline uint64_t         operator[](size_t index) const noexcept;

            inline std::random_access_iterator auto begin() noexcept;
            inline std::random_access_iterator auto end() noexcept;

            inline std::random_access_iterator auto begin() const noexcept;
            inline std::random_access_iterator auto end() const noexcept;

            inline std::random_access_iterator auto rbegin() noexcept;
            inline std::random_access_iterator auto rend() noexcept;

            inline std::random_access_iterator auto rbegin() const noexcept;
            inline std::random_access_iterator auto rend() const noexcept;
        };

        static_assert(sizeof(ValueVector) == sizeof(Value));


        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn>
        class Mapper {
        protected:
            KeyValueMap map;

        public:
            inline void     Map(const Flits::REQ<config, conn>& reqFlit) noexcept;
            inline void     Map(const Flits::SNP<config, conn>& snpFlit) noexcept;
            inline void     Map(const Flits::RSP<config, conn>& rspFlit) noexcept;
            inline void     Map(const Flits::DAT<config, conn>& datFlit) noexcept;

        public:
            inline KeyValueMap&         Get() noexcept;
            inline const KeyValueMap&   Get() const noexcept;

        protected:
            inline void     MapIntegral(Key key, uint64_t value) noexcept;
            inline void     MapVector(Key key, size_t size, uint64_t* vec) noexcept;
        };


        class Formatter {
        public:
            inline std::string Format(const KeyValueMap&, Key, const format_func& fmt) const;
            inline std::string Format(const KeyValueMap&, Key, const std::string& fmt) const;

        public:
            inline std::string FormatQoS(const KeyValueMap&, const format_func&) const; // REQ, RSP, DAT, SNP
            inline std::string FormatTgtID(const KeyValueMap&, const format_func&) const; // REQ, RSP, DAT
            inline std::string FormatSrcID(const KeyValueMap&, const format_func&) const; // REQ, RSP, DAT, SNP
            inline std::string FormatTxnID(const KeyValueMap&, const format_func&) const; // REQ, RSP, DAT, SNP
            inline std::string FormatReturnNID(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatStashNID(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatSLCRepHint(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatStashNIDValid(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatEndian(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatDeep(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatReturnTxnID(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatStashLPIDValid(const KeyValueMap&, const format_func&) const; // REQ, SNP
            inline std::string FormatStashLPID(const KeyValueMap&, const format_func&) const; // REQ, SNP
            inline std::string FormatOpcode(const KeyValueMap&, const format_func&) const; // REQ, RSP, DAT, SNP
            inline std::string FormatSize(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatAddr(const KeyValueMap&, const format_func&) const; // REQ, SNP
            inline std::string FormatNS(const KeyValueMap&, const format_func&) const; // REQ, SNP
            inline std::string FormatLikelyShared(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatAllowRetry(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatOrder(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatPCrdType(const KeyValueMap&, const format_func&) const; // REQ, RSP
            inline std::string FormatMemAttr(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatSnpAttr(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatDoDWT(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatLPID(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatPGroupID(const KeyValueMap&, const format_func&) const; // REQ, RSP
            inline std::string FormatStashGroupID(const KeyValueMap&, const format_func&) const; // REQ, RSP
            inline std::string FormatTagGroupID(const KeyValueMap&, const format_func&) const; // REQ, RSP
            inline std::string FormatExcl(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatSnoopMe(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatExpCompAck(const KeyValueMap&, const format_func&) const; // REQ
            inline std::string FormatTagOp(const KeyValueMap&, const format_func&) const; // REQ, RSP, DAT
            inline std::string FormatTraceTag(const KeyValueMap&, const format_func&) const; // REQ, RSP, DAT, SNP
            inline std::string FormatMPAM(const KeyValueMap&, const format_func&) const; // REQ, SNP
            inline std::string FormatRSVDC(const KeyValueMap&, const format_func&) const; // REQ, DAT
            inline std::string FormatRespErr(const KeyValueMap&, const format_func&) const; // RSP, DAT
            inline std::string FormatResp(const KeyValueMap&, const format_func&) const; // RSP, DAT
            inline std::string FormatFwdState(const KeyValueMap&, const format_func&) const; // RSP, DAT
            inline std::string FormatDataPull(const KeyValueMap&, const format_func&) const; // RSP, DAT
            inline std::string FormatCBusy(const KeyValueMap&, const format_func&) const; // RSP, DAT
            inline std::string FormatDBID(const KeyValueMap&, const format_func&) const; // RSP, DAT
            inline std::string FormatHomeNID(const KeyValueMap&, const format_func&) const; // DAT
            inline std::string FormatDataSource(const KeyValueMap&, const format_func&) const; // DAT
            inline std::string FormatCCID(const KeyValueMap&, const format_func&) const; // DAT
            inline std::string FormatDataID(const KeyValueMap&, const format_func&) const; // DAT
            inline std::string FormatTag(const KeyValueMap&, const format_func&) const; // DAT
            inline std::string FormatTU(const KeyValueMap&, const format_func&) const; // DAT
            inline std::string FormatBE(const KeyValueMap&, const format_func&) const; // DAT
            inline std::string FormatData(const KeyValueMap&, const format_func&) const; // DAT
            inline std::string FormatDataCheck(const KeyValueMap&, const format_func&) const; // DAT
            inline std::string FormatPoison(const KeyValueMap&, const format_func&) const; // DAT
            inline std::string FormatFwdTxnID(const KeyValueMap&, const format_func&) const; // SNP
            inline std::string FormatFwdNID(const KeyValueMap&, const format_func&) const; // SNP
            inline std::string FormatVMIDExt(const KeyValueMap&, const format_func&) const; // SNP
            inline std::string FormatDoNotGoToSD(const KeyValueMap&, const format_func&) const; // SNP
            inline std::string FormatDoNotDataPull(const KeyValueMap&, const format_func&) const; // SNP
            inline std::string FormatRetToSrc(const KeyValueMap&, const format_func&) const; // SNP

        public:
            virtual std::string FormatREQQoS(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQTgtID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQSrcID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQTxnID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQReturnNID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQStashNID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQSLCRepHint(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQStashNIDValid(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQEndian(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQDeep(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQReturnTxnID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQStashLPIDValid(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQStashLPID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQOpcode(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQSize(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQAddr(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQNS(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQLikelyShared(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQAllowRetry(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQOrder(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQPCrdType(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQMemAttr(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQSnpAttr(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQDoDWT(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQLPID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQPGroupID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQStashGroupID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQTagGroupID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQExcl(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQSnoopMe(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQExpCompAck(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQTagOp(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQTraceTag(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQMPAM(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatREQRSVDC(const KeyValueMap&, const format_func&) const = 0;

        public:
            virtual std::string FormatRSPQoS(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPTgtID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPSrcID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPTxnID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPOpcode(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPRespErr(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPResp(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPFwdState(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPDataPull(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPCBusy(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPDBID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPPGroupID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPStashGroupID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPTagGroupID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPPCrdType(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPTagOp(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatRSPTraceTag(const KeyValueMap&, const format_func&) const = 0;

        public:
            virtual std::string FormatDATQoS(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATTgtID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATSrcID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATTxnID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATHomeNID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATOpcode(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATRespErr(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATResp(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATFwdState(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATDataPull(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATDataSource(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATCBusy(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATDBID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATCCID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATDataID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATTagOp(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATTag(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATTU(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATTraceTag(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATRSVDC(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATBE(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATData(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATDataCheck(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatDATPoison(const KeyValueMap&, const format_func&) const = 0;

        public:
            virtual std::string FormatSNPQoS(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPSrcID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPTxnID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPFwdTxnID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPFwdNID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPStashLPIDValid(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPStashLPID(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPVMIDExt(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPOpcode(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPAddr(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPNS(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPDoNotGoToSD(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPDoNotDataPull(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPRetToSrc(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPTraceTag(const KeyValueMap&, const format_func&) const = 0;
            virtual std::string FormatSNPMPAM(const KeyValueMap&, const format_func&) const = 0;
        };

        
        // *NOTICE: Formatter is not responsible for any value checking,
        //          which should be done before being added into KeyValueMap.
        template<FlitConfigurationConcept       config>
        class DefaultFormatter : public virtual Formatter {
            /*
            *NOTICE: The Default Plain Formatter provides three parameters,
                     exceptions might be thrown with formatting string with more than three parameters.
                
            Example:
                Formatting string: "{} = {} ({})"
                Output with specific field key-value:
                    - REQ::QoS      = 0x01 -> "QoS = 1 (1)"
                    - REQ::Opcode   = 0x06 -> "Opcode = 6 (6)"
                    - REQ::Opcode   = 0x07 -> "Opcode = 7 (7)"
                    - REQ::MemAttr  = 0x03 -> "MemAttr = 3 (3)"
                    - REQ::RespErr  = 0x00 -> "RespErr = 0 (0)"
                    ...
                    - DAT::Data     = 0xFFFFFFFF_CCCC... -> "0xFFFFFFFFCCCC... (256)"
                    ...
            */
        protected:
            inline bool ExtractIntegral(const KeyValueMap&, Key, uint64_t* dst = nullptr) const;
            inline bool ExtractVector(const KeyValueMap&, Key, const std::vector<uint64_t>** dst = nullptr) const;

        protected:
            inline std::string  _FormatNonDecodingIntegral(const KeyValueMap&, Key, const format_func&) const;
            inline std::string  _FormatNonDecodingVector(const KeyValueMap&, Key, const format_func&) const;

        public:
            inline virtual std::string  FormatREQQoS(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQTgtID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQSrcID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQTxnID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQReturnNID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQStashNID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQSLCRepHint(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQStashNIDValid(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQEndian(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQDeep(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQReturnTxnID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQStashLPIDValid(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQStashLPID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQOpcode(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQSize(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQAddr(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQNS(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQLikelyShared(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQAllowRetry(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQOrder(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQPCrdType(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQMemAttr(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQSnpAttr(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQDoDWT(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQLPID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQPGroupID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQStashGroupID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQTagGroupID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQExcl(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQSnoopMe(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQExpCompAck(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQTagOp(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQTraceTag(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQMPAM(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string  FormatREQRSVDC(const KeyValueMap&, const format_func&) const override;

        public:
            inline virtual std::string FormatRSPQoS(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPTgtID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPSrcID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPTxnID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPOpcode(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPRespErr(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPResp(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPFwdState(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPDataPull(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPCBusy(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPDBID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPPGroupID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPStashGroupID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPTagGroupID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPPCrdType(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPTagOp(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPTraceTag(const KeyValueMap&, const format_func&) const override;
          
        public:
            inline virtual std::string FormatDATQoS(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATTgtID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATSrcID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATTxnID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATHomeNID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATOpcode(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATRespErr(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATResp(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATFwdState(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATDataPull(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATDataSource(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATCBusy(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATDBID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATCCID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATDataID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATTagOp(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATTag(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATTU(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATTraceTag(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATRSVDC(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATBE(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATData(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATDataCheck(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATPoison(const KeyValueMap&, const format_func&) const override;

        public:
            inline virtual std::string FormatSNPQoS(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPSrcID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPTxnID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPFwdTxnID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPFwdNID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPStashLPIDValid(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPStashLPID(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPVMIDExt(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPOpcode(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPAddr(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPNS(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPDoNotGoToSD(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPDoNotDataPull(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPRetToSrc(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPTraceTag(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPMPAM(const KeyValueMap&, const format_func&) const override;
        };

        template<FlitConfigurationConcept       config>
        using PlainFormatter = DefaultFormatter<config>;


        namespace Keys {

            #define _FMTCALL(func) [](const Formatter* f, const KeyValueMap& kv, const format_func& fmt) { return f->func(kv, fmt); }

            namespace REQ {
                inline constexpr KeyBack RSVDC              (KeyCategory::REQ,  "REQ.RSVDC",           "RSVDC"                              , _FMTCALL(FormatREQRSVDC));
                inline constexpr KeyBack MPAM               (KeyCategory::REQ,  "REQ.MPAM",            "MPAM"              , RSVDC          , _FMTCALL(FormatREQMPAM));
                inline constexpr KeyBack TraceTag           (KeyCategory::REQ,  "REQ.TraceTag",        "TraceTag"          , MPAM           , _FMTCALL(FormatREQTraceTag));
                inline constexpr KeyBack TagOp              (KeyCategory::REQ,  "REQ.TagOp",           "TagOp"             , TraceTag       , _FMTCALL(FormatREQTagOp));
                inline constexpr KeyBack ExpCompAck         (KeyCategory::REQ,  "REQ.ExpCompAck",      "ExpCompAck"        , TagOp          , _FMTCALL(FormatREQExpCompAck));
                inline constexpr KeyBack SnoopMe            (KeyCategory::REQ,  "REQ.SnoopMe",         "SnoopMe"           , ExpCompAck     , _FMTCALL(FormatREQSnoopMe));
                inline constexpr KeyBack Excl               (KeyCategory::REQ,  "REQ.Excl",            "Excl"              , SnoopMe        , _FMTCALL(FormatREQExcl));
                inline constexpr KeyBack TagGroupID         (KeyCategory::REQ,  "REQ.TagGroupID",      "TagGroupID"        , Excl           , _FMTCALL(FormatREQTagGroupID));
                inline constexpr KeyBack StashGroupID       (KeyCategory::REQ,  "REQ.StashGroupID",    "StashGroupID"      , TagGroupID     , _FMTCALL(FormatREQStashGroupID));
                inline constexpr KeyBack PGroupID           (KeyCategory::REQ,  "REQ.PGroupID",        "PGroupID"          , StashGroupID   , _FMTCALL(FormatREQPGroupID));
                inline constexpr KeyBack LPID               (KeyCategory::REQ,  "REQ.LPID",            "LPID"              , PGroupID       , _FMTCALL(FormatREQLPID));
                inline constexpr KeyBack DoDWT              (KeyCategory::REQ,  "REQ.DoDWT",           "DoDWT"             , LPID           , _FMTCALL(FormatREQDoDWT));
                inline constexpr KeyBack SnpAttr            (KeyCategory::REQ,  "REQ.SnpAttr",         "SnpAttr"           , DoDWT          , _FMTCALL(FormatREQSnpAttr));
                inline constexpr KeyBack MemAttr            (KeyCategory::REQ,  "REQ.MemAttr",         "MemAttr"           , SnpAttr        , _FMTCALL(FormatREQMemAttr));
                inline constexpr KeyBack PCrdType           (KeyCategory::REQ,  "REQ.PCrdType",        "PCrdType"          , MemAttr        , _FMTCALL(FormatREQPCrdType));
                inline constexpr KeyBack Order              (KeyCategory::REQ,  "REQ.Order",           "Order"             , PCrdType       , _FMTCALL(FormatREQOrder));
                inline constexpr KeyBack AllowRetry         (KeyCategory::REQ,  "REQ.AllowRetry",      "AllowRetry"        , Order          , _FMTCALL(FormatREQAllowRetry));
                inline constexpr KeyBack LikelyShared       (KeyCategory::REQ,  "REQ.LikelyShared",    "LikelyShared"      , AllowRetry     , _FMTCALL(FormatREQLikelyShared));
                inline constexpr KeyBack NS                 (KeyCategory::REQ,  "REQ.NS",              "NS"                , LikelyShared   , _FMTCALL(FormatREQNS));
                inline constexpr KeyBack Addr               (KeyCategory::REQ,  "REQ.Addr",            "Addr"              , NS             , _FMTCALL(FormatREQAddr));
                inline constexpr KeyBack Size               (KeyCategory::REQ,  "REQ.Size",            "Size"              , Addr           , _FMTCALL(FormatREQSize));
                inline constexpr KeyBack Opcode             (KeyCategory::REQ,  "REQ.Opcode",          "Opcode"            , Size           , _FMTCALL(FormatREQOpcode));
                inline constexpr KeyBack StashLPID          (KeyCategory::REQ,  "REQ.StashLPID",       "StashLPID"         , Opcode         , _FMTCALL(FormatREQStashLPID));
                inline constexpr KeyBack StashLPIDValid     (KeyCategory::REQ,  "REQ.StashLPIDValid",  "StashLPIDValid"    , StashLPID      , _FMTCALL(FormatREQStashLPIDValid));
                inline constexpr KeyBack ReturnTxnID        (KeyCategory::REQ,  "REQ.ReturnTxnID",     "ReturnTxnID"       , StashLPIDValid , _FMTCALL(FormatREQReturnTxnID));
                inline constexpr KeyBack Deep               (KeyCategory::REQ,  "REQ.Deep",            "Deep"              , ReturnTxnID    , _FMTCALL(FormatREQDeep));
                inline constexpr KeyBack Endian             (KeyCategory::REQ,  "REQ.Endian",          "Endian"            , Deep           , _FMTCALL(FormatREQEndian));
                inline constexpr KeyBack StashNIDValid      (KeyCategory::REQ,  "REQ.StashNIDValid",   "StashNIDValid"     , Endian         , _FMTCALL(FormatREQStashNIDValid));
                inline constexpr KeyBack SLCRepHint         (KeyCategory::REQ,  "REQ.SLCRepHint",      "SLCRepHint"        , StashNIDValid  , _FMTCALL(FormatREQSLCRepHint));
                inline constexpr KeyBack StashNID           (KeyCategory::REQ,  "REQ.StashNID",        "StashNID"          , SLCRepHint     , _FMTCALL(FormatREQStashNID));
                inline constexpr KeyBack ReturnNID          (KeyCategory::REQ,  "REQ.ReturnNID",       "ReturnNID"         , StashNID       , _FMTCALL(FormatREQReturnNID));
                inline constexpr KeyBack TxnID              (KeyCategory::REQ,  "REQ.TxnID",           "TxnID"             , ReturnNID      , _FMTCALL(FormatREQTxnID));
                inline constexpr KeyBack SrcID              (KeyCategory::REQ,  "REQ.SrcID",           "SrcID"             , TxnID          , _FMTCALL(FormatREQSrcID));
                inline constexpr KeyBack TgtID              (KeyCategory::REQ,  "REQ.TgtID",           "TgtID"             , SrcID          , _FMTCALL(FormatREQTgtID));
                inline constexpr KeyBack QoS                (KeyCategory::REQ,  "REQ.QoS",             "QoS"               , TgtID          , _FMTCALL(FormatREQQoS));

                inline constexpr KeyIterator begin() { return KeyIterator(QoS); }
                inline constexpr KeyIterator end() { return KeyIterator(nullptr); }
            }

            namespace RSP {
                inline constexpr KeyBack TraceTag           (KeyCategory::RSP,  "RSP.TraceTag",        "TraceTag"                           , _FMTCALL(FormatRSPTraceTag));
                inline constexpr KeyBack TagOp              (KeyCategory::RSP,  "RSP.TagOp",           "TagOp"             , TraceTag       , _FMTCALL(FormatRSPTagOp));
                inline constexpr KeyBack PCrdType           (KeyCategory::RSP,  "RSP.PCrdType",        "PCrdType"          , TagOp          , _FMTCALL(FormatRSPPCrdType));
                inline constexpr KeyBack TagGroupID         (KeyCategory::RSP,  "RSP.TagGroupID",      "TagGroupID"        , PCrdType       , _FMTCALL(FormatRSPTagGroupID));
                inline constexpr KeyBack StashGroupID       (KeyCategory::RSP,  "RSP.StashGroupID",    "StashGroupID"      , TagGroupID     , _FMTCALL(FormatRSPStashGroupID));
                inline constexpr KeyBack PGroupID           (KeyCategory::RSP,  "RSP.PGroupID",        "PGroupID"          , StashGroupID   , _FMTCALL(FormatRSPPGroupID));
                inline constexpr KeyBack DBID               (KeyCategory::RSP,  "RSP.DBID",            "DBID"              , PGroupID       , _FMTCALL(FormatRSPDBID));
                inline constexpr KeyBack CBusy              (KeyCategory::RSP,  "RSP.CBusy",           "CBusy"             , DBID           , _FMTCALL(FormatRSPCBusy));
                inline constexpr KeyBack DataPull           (KeyCategory::RSP,  "RSP.DataPull",        "DataPull"          , CBusy          , _FMTCALL(FormatRSPDataPull));
                inline constexpr KeyBack FwdState           (KeyCategory::RSP,  "RSP.FwdState",        "FwdState"          , DataPull       , _FMTCALL(FormatRSPFwdState));
                inline constexpr KeyBack Resp               (KeyCategory::RSP,  "RSP.Resp",            "Resp"              , FwdState       , _FMTCALL(FormatRSPResp));
                inline constexpr KeyBack RespErr            (KeyCategory::RSP,  "RSP.RespErr",         "RespErr"           , Resp           , _FMTCALL(FormatRSPRespErr));
                inline constexpr KeyBack Opcode             (KeyCategory::RSP,  "RSP.Opcode",          "Opcode"            , RespErr        , _FMTCALL(FormatRSPOpcode));
                inline constexpr KeyBack TxnID              (KeyCategory::RSP,  "RSP.TxnID",           "TxnID"             , Opcode         , _FMTCALL(FormatRSPTxnID));
                inline constexpr KeyBack SrcID              (KeyCategory::RSP,  "RSP.SrcID",           "SrcID"             , TxnID          , _FMTCALL(FormatRSPSrcID));
                inline constexpr KeyBack TgtID              (KeyCategory::RSP,  "RSP.TgtID",           "TgtID"             , SrcID          , _FMTCALL(FormatRSPTgtID));
                inline constexpr KeyBack QoS                (KeyCategory::RSP,  "RSP.QoS",             "QoS"               , TgtID          , _FMTCALL(FormatRSPQoS));

                inline constexpr KeyIterator begin() { return KeyIterator(QoS); }
                inline constexpr KeyIterator end() { return KeyIterator(nullptr); }
            }

            namespace DAT {
                inline constexpr KeyBack Poison             (KeyCategory::DAT,  "DAT.Poison",          "Poison"                             , _FMTCALL(FormatDATPoison));
                inline constexpr KeyBack DataCheck          (KeyCategory::DAT,  "DAT.DataCheck",       "DataCheck"         , Poison         , _FMTCALL(FormatDATDataCheck));
                inline constexpr KeyBack Data               (KeyCategory::DAT,  "DAT.Data",            "Data"              , DataCheck      , _FMTCALL(FormatDATData));
                inline constexpr KeyBack BE                 (KeyCategory::DAT,  "DAT.BE",              "BE"                , Data           , _FMTCALL(FormatDATBE));
                inline constexpr KeyBack RSVDC              (KeyCategory::DAT,  "DAT.RSVDC",           "RSVDC"             , BE             , _FMTCALL(FormatDATRSVDC));
                inline constexpr KeyBack TraceTag           (KeyCategory::DAT,  "DAT.TraceTag",        "TraceTag"          , RSVDC          , _FMTCALL(FormatDATTraceTag));
                inline constexpr KeyBack TU                 (KeyCategory::DAT,  "DAT.TU",              "TU"                , TraceTag       , _FMTCALL(FormatDATTU));
                inline constexpr KeyBack Tag                (KeyCategory::DAT,  "DAT.Tag",             "Tag"               , TU             , _FMTCALL(FormatDATTag));
                inline constexpr KeyBack TagOp              (KeyCategory::DAT,  "DAT.TagOp",           "TagOp"             , Tag            , _FMTCALL(FormatDATTagOp));
                inline constexpr KeyBack DataID             (KeyCategory::DAT,  "DAT.DataID",          "DataID"            , TagOp          , _FMTCALL(FormatDATDataID));
                inline constexpr KeyBack CCID               (KeyCategory::DAT,  "DAT.CCID",            "CCID"              , DataID         , _FMTCALL(FormatDATCCID));
                inline constexpr KeyBack DBID               (KeyCategory::DAT,  "DAT.DBID",            "DBID"              , CCID           , _FMTCALL(FormatDATDBID));
                inline constexpr KeyBack CBusy              (KeyCategory::DAT,  "DAT.CBusy",           "CBusy"             , DBID           , _FMTCALL(FormatDATCBusy));
                inline constexpr KeyBack DataSource         (KeyCategory::DAT,  "DAT.DataSource",      "DataSource"        , CBusy          , _FMTCALL(FormatDATDataSource));
                inline constexpr KeyBack DataPull           (KeyCategory::DAT,  "DAT.DataPull",        "DataPull"          , DataSource     , _FMTCALL(FormatDATDataPull));
                inline constexpr KeyBack FwdState           (KeyCategory::DAT,  "DAT.FwdState",        "FwdState"          , DataPull       , _FMTCALL(FormatDATFwdState));
                inline constexpr KeyBack Resp               (KeyCategory::DAT,  "DAT.Resp",            "Resp"              , FwdState       , _FMTCALL(FormatDATResp));
                inline constexpr KeyBack RespErr            (KeyCategory::DAT,  "DAT.RespErr",         "RespErr"           , Resp           , _FMTCALL(FormatDATRespErr));
                inline constexpr KeyBack Opcode             (KeyCategory::DAT,  "DAT.Opcode",          "Opcode"            , RespErr        , _FMTCALL(FormatDATOpcode));
                inline constexpr KeyBack HomeNID            (KeyCategory::DAT,  "DAT.HomeNID",         "HomeNID"           , Opcode         , _FMTCALL(FormatDATHomeNID));
                inline constexpr KeyBack TxnID              (KeyCategory::DAT,  "DAT.TxnID",           "TxnID"             , HomeNID        , _FMTCALL(FormatDATTxnID));
                inline constexpr KeyBack SrcID              (KeyCategory::DAT,  "DAT.SrcID",           "SrcID"             , TxnID          , _FMTCALL(FormatDATSrcID));
                inline constexpr KeyBack TgtID              (KeyCategory::DAT,  "DAT.TgtID",           "TgtID"             , SrcID          , _FMTCALL(FormatDATTgtID));
                inline constexpr KeyBack QoS                (KeyCategory::DAT,  "DAT.QoS",             "QoS"               , TgtID          , _FMTCALL(FormatDATQoS));

                inline constexpr KeyIterator begin() { return KeyIterator(QoS); }
                inline constexpr KeyIterator end() { return KeyIterator(nullptr); }
            }

            namespace SNP {
                inline constexpr KeyBack MPAM               (KeyCategory::SNP,  "SNP.MPAM",            "MPAM"                               , _FMTCALL(FormatSNPMPAM));
                inline constexpr KeyBack TraceTag           (KeyCategory::SNP,  "SNP.TraceTag",        "TraceTag"          , MPAM           , _FMTCALL(FormatSNPTraceTag));
                inline constexpr KeyBack RetToSrc           (KeyCategory::SNP,  "SNP.RetToSrc",        "RetToSrc"          , TraceTag       , _FMTCALL(FormatSNPRetToSrc));
                inline constexpr KeyBack DoNotDataPull      (KeyCategory::SNP,  "SNP.DoNotDataPull",   "DoNotDataPull"     , RetToSrc       , _FMTCALL(FormatSNPDoNotDataPull));
                inline constexpr KeyBack DoNotGoToSD        (KeyCategory::SNP,  "SNP.DoNotGoToSD",     "DoNotGoToSD"       , DoNotDataPull  , _FMTCALL(FormatSNPDoNotGoToSD));
                inline constexpr KeyBack NS                 (KeyCategory::SNP,  "SNP.NS",              "NS"                , DoNotGoToSD    , _FMTCALL(FormatSNPNS));
                inline constexpr KeyBack Addr               (KeyCategory::SNP,  "SNP.Addr",            "Addr"              , NS             , _FMTCALL(FormatSNPAddr));
                inline constexpr KeyBack Opcode             (KeyCategory::SNP,  "SNP.Opcode",          "Opcode"            , Addr           , _FMTCALL(FormatSNPOpcode));
                inline constexpr KeyBack VMIDExt            (KeyCategory::SNP,  "SNP.VMIDExt",         "VMIDExt"           , Opcode         , _FMTCALL(FormatSNPVMIDExt));
                inline constexpr KeyBack StashLPID          (KeyCategory::SNP,  "SNP.StashLPID",       "StashLPID"         , VMIDExt        , _FMTCALL(FormatSNPStashLPID));
                inline constexpr KeyBack StashLPIDValid     (KeyCategory::SNP,  "SNP.StashLPIDValid",  "StashLPIDValid"    , StashLPID      , _FMTCALL(FormatSNPStashLPIDValid));
                inline constexpr KeyBack FwdTxnID           (KeyCategory::SNP,  "SNP.FwdTxnID",        "FwdTxnID"          , StashLPIDValid , _FMTCALL(FormatSNPFwdTxnID));
                inline constexpr KeyBack FwdNID             (KeyCategory::SNP,  "SNP.FwdNID",          "FwdNID"            , FwdTxnID       , _FMTCALL(FormatSNPFwdNID));
                inline constexpr KeyBack TxnID              (KeyCategory::SNP,  "SNP.TxnID",           "TxnID"             , FwdNID         , _FMTCALL(FormatSNPTxnID));
                inline constexpr KeyBack SrcID              (KeyCategory::SNP,  "SNP.SrcID",           "SrcID"             , TxnID          , _FMTCALL(FormatSNPSrcID));
                inline constexpr KeyBack QoS                (KeyCategory::SNP,  "SNP.QoS",             "QoS"               , SrcID          , _FMTCALL(FormatSNPQoS));

                inline constexpr KeyIterator begin() { return KeyIterator(QoS); }
                inline constexpr KeyIterator end() { return KeyIterator(nullptr); }
            }

            #undef _FMTCALL
        }
    }
/*
}
*/


// Implementation of: class KeyBack
namespace /*CHI::*/Expresso::Flit {

    inline std::string KeyBack::Format(const Formatter& f, const KeyValueMap& kv, const format_func& fmt) const
    {
        return formatterCaller(&f, kv, fmt);
    }

    inline std::string KeyBack::Format(const Formatter& f, const KeyValueMap& kv, const std::string& fmt) const
    {
        return formatterCaller(&f, kv, [fmt](auto) { return fmt; });
    }
}


// Implementation of: class Value
namespace /*CHI::*/Expresso::Flit {

    inline Value::Value(ValueType type) noexcept
        : type(type)
    {
        if (type == ValueType::Vector)
            this->vector = new std::vector<uint64_t>;
    }

    inline Value::Value(const Value& obj) noexcept
        : type(obj.type)
    {
        if (type == ValueType::Integral)
            this->value = obj.value;
        else if (type == ValueType::Vector)
            this->vector = new std::vector<uint64_t>(*obj.vector);
    }

    inline Value::Value(Value&& obj) noexcept
        : type(obj.type)
    {
        if (type == ValueType::Integral)
            this->value = obj.value;
        else if (type == ValueType::Vector)
            this->vector = new std::vector<uint64_t>(std::move(*obj.vector));
    }

    inline Value::~Value() noexcept
    {
        if (type == ValueType::Vector)
        {
            if (this->vector)
                delete this->vector;

            this->vector = nullptr;
        }
    }

    inline Value::Value(const std::vector<uint64_t>& vec) noexcept
        : type(ValueType::Vector)
    {
        this->vector = new std::vector<uint64_t>(vec);
    }

    inline Value::Value(std::vector<uint64_t>&& vec) noexcept
        : type(ValueType::Vector)
    {
        this->vector = new std::vector<uint64_t>(vec);
    }

    inline Value& Value::operator=(const Value& obj) noexcept
    {
        if (this->type == ValueType::Vector)
        {
            if (this->vector)
                delete this->vector;
        }

        this->type = obj.type;

        if (this->type == ValueType::Integral)
            this->value = obj.value;
        else if (this->type == ValueType::Vector)
            this->vector = new std::vector<uint64_t>(*obj.vector);

        return *this;
    }

    inline Value& Value::operator=(Value&& obj) noexcept
    {
        if (this->type == ValueType::Vector)
        {
            if (this->vector)
                delete this->vector;
        }

        this->type = obj.type;

        if (this->type == ValueType::Integral)
            this->value = obj.value;
        else if (this->type == ValueType::Vector)
            this->vector = new std::vector<uint64_t>(std::move(*obj.vector));

        return *this;
    }

    inline ValueType Value::GetType() const noexcept
    {
        return type;
    }

    inline bool Value::IsIntegral() const noexcept
    {
        return GetType() == ValueType::Integral;
    }
    
    inline bool Value::IsVector() const noexcept
    {
        return GetType() == ValueType::Vector;
    }

    inline uint64_t Value::GetValue() const noexcept
    {
        return value;
    }

    inline std::vector<uint64_t>* Value::GetVector() const noexcept
    {
        return vector;
    }

    inline uint64_t& Value::AsIntegral() noexcept
    {
        return value;
    }

    inline uint64_t Value::AsIntegral() const noexcept
    {
        return value;
    }

    inline std::vector<uint64_t>& Value::AsVector() noexcept
    {
        return *vector;
    }

    inline const std::vector<uint64_t>& Value::AsVector() const noexcept
    {
        return *vector;
    }
}


// Implementation of: class ValueIntegral
namespace /*CHI::*/Expresso::Flit {

    inline ValueIntegral::ValueIntegral(uint64_t value) noexcept
        : Value(ValueType::Integral)
    {
        this->value = value;
    }

    inline void ValueIntegral::SetValue(uint64_t value) noexcept
    {
        this->value = value;
    }
}


// Implementation of: class ValueVector
namespace /*CHI::*/Expresso::Flit {

    inline ValueVector::ValueVector(size_t size) noexcept
        : Value(ValueType::Vector)
    {
        this->vector->reserve(size);

        for (size_t i = 0; i < size; i++)
            this->vector->push_back(0);
    }

    inline ValueVector::ValueVector(size_t size, uint64_t* array) noexcept
        : Value(ValueType::Vector)
    {
        this->vector->reserve(size);

        for (size_t i = 0; i < size; i++)
            this->vector->push_back(array[i]);
    }

    inline ValueVector::ValueVector(const std::vector<uint64_t>& vec) noexcept
        : Value(vec)
    { }

    inline ValueVector::ValueVector(std::vector<uint64_t>&& vec) noexcept
        : Value(vec)
    { }

    inline ValueVector::ValueVector(const ValueVector& obj) noexcept
        : Value(ValueType::Vector)
    {
        this->vector = new std::vector<uint64_t>(*obj.vector);
    }

    inline ValueVector::ValueVector(ValueVector&& obj) noexcept
        : Value(ValueType::Vector)
    {
        this->vector = new std::vector<uint64_t>(std::move(*obj.vector));
    }

    inline ValueVector::~ValueVector() noexcept
    {
        if (this->vector)
            delete vector;
        
        this->vector = nullptr;
    }

    inline size_t ValueVector::GetSize() const noexcept
    {
        return vector->size();
    }

    inline uint64_t ValueVector::GetValue(size_t index) const noexcept
    {
        return (*vector)[index];
    }

    inline void ValueVector::SetValue(size_t index, uint64_t value) noexcept
    {
        (*vector)[index] = value;
    }

    inline uint64_t& ValueVector::operator[](size_t index) noexcept
    {
        return (*vector)[index];
    }

    inline uint64_t ValueVector::operator[](size_t index) const noexcept
    {
        return (*vector)[index];
    }

    inline std::random_access_iterator auto ValueVector::begin() noexcept
    {
        return vector->begin();
    }

    inline std::random_access_iterator auto ValueVector::end() noexcept
    {
        return vector->end();
    }

    inline std::random_access_iterator auto ValueVector::begin() const noexcept
    {
        return vector->begin();
    }

    inline std::random_access_iterator auto ValueVector::end() const noexcept
    {
        return vector->end();
    }

    inline std::random_access_iterator auto ValueVector::rbegin() noexcept
    {
        return vector->rbegin();
    }

    inline std::random_access_iterator auto ValueVector::rend() noexcept
    {
        return vector->rend();
    }

    inline std::random_access_iterator auto ValueVector::rbegin() const noexcept
    {
        return vector->rbegin();
    }

    inline std::random_access_iterator auto ValueVector::rend() const noexcept
    {
        return vector->rend();
    }
}


// Implementation of: class Mapper
namespace /*CHI::*/Expresso::Flit {

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void Mapper<config, conn>::Map(const Flits::REQ<config, conn>& reqFlit) noexcept
    {
        MapIntegral(Keys::REQ::QoS              , reqFlit.QoS());
        MapIntegral(Keys::REQ::TgtID            , reqFlit.TgtID());
        MapIntegral(Keys::REQ::SrcID            , reqFlit.SrcID());
        MapIntegral(Keys::REQ::TxnID            , reqFlit.TxnID());
        MapIntegral(Keys::REQ::ReturnNID        , reqFlit.ReturnNID());
        MapIntegral(Keys::REQ::StashNID         , reqFlit.StashNID());
#ifdef CHI_ISSUE_EB_ENABLE
        MapIntegral(Keys::REQ::SLCRepHint       , reqFlit.SLCRepHint());
#endif
        MapIntegral(Keys::REQ::StashNIDValid    , reqFlit.StashNIDValid());
        MapIntegral(Keys::REQ::Endian           , reqFlit.Endian());
#ifdef CHI_ISSUE_EB_ENABLE
        MapIntegral(Keys::REQ::Deep             , reqFlit.Deep());
#endif
        MapIntegral(Keys::REQ::ReturnTxnID      , reqFlit.ReturnTxnID());
        MapIntegral(Keys::REQ::StashLPIDValid   , reqFlit.StashLPIDValid());
        MapIntegral(Keys::REQ::StashLPID        , reqFlit.StashLPID());
        MapIntegral(Keys::REQ::Size             , reqFlit.Size());
        MapIntegral(Keys::REQ::Addr             , reqFlit.Addr());
        MapIntegral(Keys::REQ::NS               , reqFlit.NS());
        MapIntegral(Keys::REQ::LikelyShared     , reqFlit.LikelyShared());
        MapIntegral(Keys::REQ::AllowRetry       , reqFlit.AllowRetry());
        MapIntegral(Keys::REQ::Order            , reqFlit.Order());
        MapIntegral(Keys::REQ::PCrdType         , reqFlit.PCrdType());
        MapIntegral(Keys::REQ::MemAttr          , reqFlit.MemAttr());
        MapIntegral(Keys::REQ::SnpAttr          , reqFlit.SnpAttr());
#ifdef CHI_ISSUE_EB_ENABLE
        MapIntegral(Keys::REQ::DoDWT            , reqFlit.DoDWT());
#endif
        MapIntegral(Keys::REQ::LPID             , reqFlit.LPID());
#ifdef CHI_ISSUE_EB_ENABLE
        MapIntegral(Keys::REQ::PGroupID         , reqFlit.PGroupID());
        MapIntegral(Keys::REQ::StashGroupID     , reqFlit.StashGroupID());
        MapIntegral(Keys::REQ::TagGroupID       , reqFlit.TagGroupID());
#endif
        MapIntegral(Keys::REQ::Excl             , reqFlit.Excl());
        MapIntegral(Keys::REQ::SnoopMe          , reqFlit.SnoopMe());
        MapIntegral(Keys::REQ::ExpCompAck       , reqFlit.ExpCompAck());
#ifdef CHI_ISSUE_EB_ENABLE
        MapIntegral(Keys::REQ::TagOp            , reqFlit.TagOp());
#endif
        MapIntegral(Keys::REQ::TraceTag         , reqFlit.TraceTag());
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (Flits::REQ<config, conn>::hasMPAM)
            MapIntegral(Keys::REQ::MPAM             , reqFlit.MPAM());
#endif
        if constexpr (Flits::REQ<config, conn>::hasRSVDC)
            MapIntegral(Keys::REQ::RSVDC            , reqFlit.RSVDC());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void Mapper<config, conn>::Map(const Flits::SNP<config, conn>& snpFlit) noexcept
    {
        MapIntegral(Keys::SNP::QoS              , snpFlit.QoS());
        MapIntegral(Keys::SNP::SrcID            , snpFlit.SrcID());
        MapIntegral(Keys::SNP::TxnID            , snpFlit.TxnID());
        MapIntegral(Keys::SNP::FwdNID           , snpFlit.FwdNID());
        MapIntegral(Keys::SNP::FwdTxnID         , snpFlit.FwdTxnID());
        MapIntegral(Keys::SNP::StashLPIDValid   , snpFlit.StashLPIDValid());
        MapIntegral(Keys::SNP::StashLPID        , snpFlit.StashLPID());
        MapIntegral(Keys::SNP::VMIDExt          , snpFlit.VMIDExt());
        MapIntegral(Keys::SNP::Addr             , snpFlit.Addr());
        MapIntegral(Keys::SNP::NS               , snpFlit.NS());
        MapIntegral(Keys::SNP::DoNotGoToSD      , snpFlit.DoNotGoToSD());
#ifdef CHI_ISSUE_B_ENABLE
        MapIntegral(Keys::SNP::DoNotDataPull    , snpFlit.DoNotDataPull());
#endif
        MapIntegral(Keys::SNP::RetToSrc         , snpFlit.RetToSrc());
        MapIntegral(Keys::SNP::TraceTag         , snpFlit.TraceTag());
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (Flits::SNP<config, conn>::hasMPAM)
            MapIntegral(Keys::SNP::MPAM,            snpFlit.MPAM());
#endif
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void Mapper<config, conn>::Map(const Flits::RSP<config, conn>& rspFlit) noexcept
    {
        MapIntegral(Keys::RSP::QoS              , rspFlit.QoS());
        MapIntegral(Keys::RSP::TgtID            , rspFlit.TgtID());
        MapIntegral(Keys::RSP::SrcID            , rspFlit.SrcID());
        MapIntegral(Keys::RSP::TxnID            , rspFlit.TxnID());
        MapIntegral(Keys::RSP::RespErr          , rspFlit.RespErr());
        MapIntegral(Keys::RSP::Resp             , rspFlit.Resp());
        MapIntegral(Keys::RSP::FwdState         , rspFlit.FwdState());
        MapIntegral(Keys::RSP::DataPull         , rspFlit.DataPull());
#ifdef CHI_ISSUE_EB_ENABLE
        MapIntegral(Keys::RSP::CBusy            , rspFlit.CBusy());
#endif
        MapIntegral(Keys::RSP::DBID             , rspFlit.DBID());
#ifdef CHI_ISSUE_EB_ENABLE
        MapIntegral(Keys::RSP::PGroupID         , rspFlit.PGroupID());
        MapIntegral(Keys::RSP::StashGroupID     , rspFlit.StashGroupID());
        MapIntegral(Keys::RSP::TagGroupID       , rspFlit.TagGroupID());
#endif
        MapIntegral(Keys::RSP::PCrdType         , rspFlit.PCrdType());
#ifdef CHI_ISSUE_EB_ENABLE
        MapIntegral(Keys::RSP::TagOp            , rspFlit.TagOp());
#endif
        MapIntegral(Keys::RSP::TraceTag         , rspFlit.TraceTag());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void Mapper<config, conn>::Map(const Flits::DAT<config, conn>& datFlit) noexcept
    {
        MapIntegral(Keys::DAT::QoS              , datFlit.QoS());
        MapIntegral(Keys::DAT::TgtID            , datFlit.TgtID());
        MapIntegral(Keys::DAT::SrcID            , datFlit.SrcID());
        MapIntegral(Keys::DAT::TxnID            , datFlit.TxnID());
        MapIntegral(Keys::DAT::HomeNID          , datFlit.HomeNID());
        MapIntegral(Keys::DAT::RespErr          , datFlit.RespErr());
        MapIntegral(Keys::DAT::Resp             , datFlit.Resp());
        MapIntegral(Keys::DAT::FwdState         , datFlit.FwdState());
        MapIntegral(Keys::DAT::DataPull         , datFlit.DataPull());
        MapIntegral(Keys::DAT::DataSource       , datFlit.DataSource());
#ifdef CHI_ISSUE_EB_ENABLE
        MapIntegral(Keys::DAT::CBusy            , datFlit.CBusy());
#endif
        MapIntegral(Keys::DAT::DBID             , datFlit.DBID());
        MapIntegral(Keys::DAT::CCID             , datFlit.CCID());
        MapIntegral(Keys::DAT::DataID           , datFlit.DataID());
#ifdef CHI_ISSUE_EB_ENABLE
        MapIntegral(Keys::DAT::TagOp            , datFlit.TagOp());
        MapIntegral(Keys::DAT::Tag              , datFlit.Tag());
        MapIntegral(Keys::DAT::TU               , datFlit.TU());
#endif
        MapIntegral(Keys::DAT::TraceTag         , datFlit.TraceTag());
        if constexpr (Flits::DAT<config, conn>::hasRSVDC)
            MapIntegral(Keys::DAT::RSVDC            , datFlit.RSVDC());
        MapIntegral(Keys::DAT::BE               , datFlit.BE());
        MapVector(Keys::DAT::Data, Flits::DAT<config, conn>::DATA_WIDTH / 64, datFlit.Data());
        if constexpr (Flits::DAT<config, conn>::hasDataCheck)
            MapIntegral(Keys::DAT::DataCheck        , datFlit.DataCheck());
        if constexpr (Flits::DAT<config, conn>::hasPoison)
            MapIntegral(Keys::DAT::Poison           , datFlit.Poison());
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void Mapper<config, conn>::MapIntegral(Key key, uint64_t value) noexcept
    {
        map[key] = ValueIntegral(value);
    }

    template<FlitConfigurationConcept       config,
             CHI::IOLevelConnectionConcept  conn>
    inline void Mapper<config, conn>::MapVector(Key key, size_t size, uint64_t* vec) noexcept
    {
        map[key] = ValueVector(size, vec);
    }
}


// Implementation of: class Formatter
namespace /*CHI::*/Expresso::Flit {

    inline std::string Formatter::Format(const KeyValueMap& kv, Key key, const format_func& fmt) const
    {
        return key->formatterCaller(this, kv, fmt);
    }

    inline std::string Formatter::Format(const KeyValueMap& kv, Key key, const std::string& fmt) const
    {
        return key->formatterCaller(this, kv, [fmt](auto) { return fmt; });
    }

    inline std::string Formatter::FormatQoS(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQQoS(kv, fmt);
        if (result.empty()) result = FormatRSPQoS(kv, fmt);
        if (result.empty()) result = FormatDATQoS(kv, fmt);
        if (result.empty()) result = FormatSNPQoS(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatTgtID(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQTgtID(kv, fmt);
        if (result.empty()) result = FormatRSPTgtID(kv, fmt);
        if (result.empty()) result = FormatDATTgtID(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatSrcID(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQSrcID(kv, fmt);
        if (result.empty()) result = FormatRSPSrcID(kv, fmt);
        if (result.empty()) result = FormatDATSrcID(kv, fmt);
        if (result.empty()) result = FormatSNPSrcID(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQTxnID(kv, fmt);
        if (result.empty()) result = FormatRSPTxnID(kv, fmt);
        if (result.empty()) result = FormatDATTxnID(kv, fmt);
        if (result.empty()) result = FormatSNPTxnID(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatReturnNID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQReturnNID(kv, fmt);
    }

    inline std::string Formatter::FormatStashNID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQStashNID(kv, fmt);
    }

    inline std::string Formatter::FormatSLCRepHint(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQSLCRepHint(kv, fmt);
    }

    inline std::string Formatter::FormatStashNIDValid(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQStashNIDValid(kv, fmt);
    }

    inline std::string Formatter::FormatEndian(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQEndian(kv, fmt);
    }

    inline std::string Formatter::FormatDeep(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQDeep(kv, fmt);
    }

    inline std::string Formatter::FormatReturnTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQReturnTxnID(kv, fmt);
    }

    inline std::string Formatter::FormatStashLPIDValid(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQStashLPIDValid(kv, fmt);
        if (result.empty()) result = FormatSNPStashLPIDValid(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatStashLPID(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQStashLPID(kv, fmt);
        if (result.empty()) result = FormatSNPStashLPID(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatOpcode(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQOpcode(kv, fmt);
        if (result.empty()) result = FormatRSPOpcode(kv, fmt);
        if (result.empty()) result = FormatDATOpcode(kv, fmt);
        if (result.empty()) result = FormatSNPOpcode(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatSize(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQSize(kv, fmt);
    }

    inline std::string Formatter::FormatAddr(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQAddr(kv, fmt);
        if (result.empty()) result = FormatSNPAddr(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatNS(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQNS(kv, fmt);
        if (result.empty()) result = FormatSNPNS(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatLikelyShared(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQLikelyShared(kv, fmt);
    }

    inline std::string Formatter::FormatAllowRetry(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQAllowRetry(kv, fmt);
    }

    inline std::string Formatter::FormatOrder(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQOrder(kv, fmt);
    }

    inline std::string Formatter::FormatPCrdType(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQPCrdType(kv, fmt);
        if (result.empty()) result = FormatRSPPCrdType(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatMemAttr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQMemAttr(kv, fmt);
    }

    inline std::string Formatter::FormatSnpAttr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQSnpAttr(kv, fmt);
    }

    inline std::string Formatter::FormatDoDWT(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQDoDWT(kv, fmt);
    }

    inline std::string Formatter::FormatLPID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQLPID(kv, fmt);
    }

    inline std::string Formatter::FormatPGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQPGroupID(kv, fmt);
        if (result.empty()) result = FormatRSPPGroupID(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatStashGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQStashGroupID(kv, fmt);
        if (result.empty()) result = FormatRSPStashGroupID(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatTagGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQTagGroupID(kv, fmt);
        if (result.empty()) result = FormatRSPTagGroupID(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatExcl(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQExcl(kv, fmt);
    }

    inline std::string Formatter::FormatSnoopMe(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQSnoopMe(kv, fmt);
    }

    inline std::string Formatter::FormatExpCompAck(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatREQExpCompAck(kv, fmt);
    }

    inline std::string Formatter::FormatTagOp(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQTagOp(kv, fmt);
        if (result.empty()) result = FormatRSPTagOp(kv, fmt);
        if (result.empty()) result = FormatDATTagOp(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatTraceTag(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQTraceTag(kv, fmt);
        if (result.empty()) result = FormatRSPTraceTag(kv, fmt);
        if (result.empty()) result = FormatDATTraceTag(kv, fmt);
        if (result.empty()) result = FormatSNPTraceTag(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatMPAM(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQMPAM(kv, fmt);
        if (result.empty()) result = FormatSNPMPAM(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatRSVDC(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatREQRSVDC(kv, fmt);
        if (result.empty()) result = FormatDATRSVDC(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatRespErr(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatRSPRespErr(kv, fmt);
        if (result.empty()) result = FormatDATRespErr(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatResp(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatRSPResp(kv, fmt);
        if (result.empty()) result = FormatDATResp(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatFwdState(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatRSPFwdState(kv, fmt);
        if (result.empty()) result = FormatDATFwdState(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatDataPull(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatRSPDataPull(kv, fmt);
        if (result.empty()) result = FormatDATDataPull(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatCBusy(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatRSPCBusy(kv, fmt);
        if (result.empty()) result = FormatDATCBusy(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatDBID(const KeyValueMap& kv, const format_func& fmt) const
    {
        std::string result = FormatRSPDBID(kv, fmt);
        if (result.empty()) result = FormatDATDBID(kv, fmt);
        return result;
    }

    inline std::string Formatter::FormatHomeNID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatDATHomeNID(kv, fmt);
    }

    inline std::string Formatter::FormatDataSource(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatDATDataSource(kv, fmt);
    }

    inline std::string Formatter::FormatCCID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatDATCCID(kv, fmt);
    }

    inline std::string Formatter::FormatDataID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatDATDataID(kv, fmt);
    }

    inline std::string Formatter::FormatTag(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatDATTag(kv, fmt);
    }

    inline std::string Formatter::FormatTU(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatDATTU(kv, fmt);
    }

    inline std::string Formatter::FormatBE(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatDATBE(kv, fmt);
    }

    inline std::string Formatter::FormatData(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatDATData(kv, fmt);
    }

    inline std::string Formatter::FormatDataCheck(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatDATDataCheck(kv, fmt);
    }

    inline std::string Formatter::FormatPoison(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatDATPoison(kv, fmt);
    }

    inline std::string Formatter::FormatFwdTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatSNPFwdTxnID(kv, fmt);
    }

    inline std::string Formatter::FormatFwdNID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatSNPFwdNID(kv, fmt);
    }

    inline std::string Formatter::FormatVMIDExt(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatSNPVMIDExt(kv, fmt);
    }

    inline std::string Formatter::FormatDoNotGoToSD(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatSNPDoNotGoToSD(kv, fmt);
    }

    inline std::string Formatter::FormatDoNotDataPull(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatSNPDoNotDataPull(kv, fmt);
    }

    inline std::string Formatter::FormatRetToSrc(const KeyValueMap& kv, const format_func& fmt) const
    {
        return FormatSNPRetToSrc(kv, fmt);
    }
}


// Implementation of: class DefaultFormatter
namespace /*CHI::*/Expresso::Flit {

    template<FlitConfigurationConcept config>
    inline bool DefaultFormatter<config>::ExtractIntegral(const KeyValueMap& kv, Key key, uint64_t* dst) const
    {
        auto iter = kv.find(key);

        if (iter == kv.end())
            return false;

        if (!iter->second.IsIntegral())
            return false;
        
        if (dst)
            *dst = iter->second.AsIntegral();

        return true;
    }

    template<FlitConfigurationConcept config>
    inline bool DefaultFormatter<config>::ExtractVector(const KeyValueMap& kv, Key key, const std::vector<uint64_t>** dst) const
    {
        auto iter = kv.find(key);

        if (iter == kv.end())
            return false;

        if (!iter->second.IsVector())
            return false;

        if (dst)
            *dst = &iter->second.AsVector();

        return true;
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::_FormatNonDecodingIntegral(const KeyValueMap& kv, Key key, const format_func& fmt) const
    {
        uint64_t integral;

        if (!ExtractIntegral(kv, key, &integral))
            return std::string();

        return std::vformat(fmt(key), std::make_format_args(key->canonicalName, integral, integral));
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::_FormatNonDecodingVector(const KeyValueMap& kv, Key key, const format_func& fmt) const
    {
        const std::vector<uint64_t>* vector;

        if (!ExtractVector(kv, key, &vector))
            return std::string();

        std::ostringstream dataStrOss;
        for (auto dataIter = vector->rbegin(); dataIter != vector->rend(); dataIter++)
            dataStrOss << std::hex << std::setfill('0') << std::setw(16) << *dataIter;
        std::string dataStr = dataStrOss.str();

        size_t bitCount = vector->size() * 64;

        return std::vformat(fmt(key), std::make_format_args(key->canonicalName, dataStr, bitCount));
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQQoS(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::QoS, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQTgtID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::TgtID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQSrcID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::SrcID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::TxnID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQReturnNID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::ReturnNID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQStashNID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::StashNID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQSLCRepHint(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::SLCRepHint, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQStashNIDValid(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::StashNIDValid, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQEndian(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::Endian, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQDeep(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::Deep, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQReturnTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::ReturnTxnID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQStashLPIDValid(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::StashLPIDValid, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQStashLPID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::StashLPID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQOpcode(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::Opcode, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQSize(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::Size, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQAddr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::Addr, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQNS(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::NS, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQLikelyShared(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::LikelyShared, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQAllowRetry(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::AllowRetry, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQOrder(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::Order, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQPCrdType(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::PCrdType, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQMemAttr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::MemAttr, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQSnpAttr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::SnpAttr, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQDoDWT(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::DoDWT, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQLPID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::LPID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQPGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::PGroupID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQStashGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::StashGroupID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQTagGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::TagGroupID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQExcl(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::Excl, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQSnoopMe(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::SnoopMe, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQExpCompAck(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::ExpCompAck, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQTagOp(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::TagOp, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQTraceTag(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::TraceTag, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQMPAM(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::MPAM, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatREQRSVDC(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::RSVDC, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPQoS(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::QoS, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPTgtID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::TgtID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPSrcID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::SrcID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::TxnID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPOpcode(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::Opcode, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPRespErr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::RespErr, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPResp(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::Resp, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPFwdState(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::FwdState, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPDataPull(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::DataPull, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPCBusy(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::CBusy, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPDBID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::DBID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPPGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::PGroupID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPStashGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::StashGroupID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPTagGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::TagGroupID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPPCrdType(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::PCrdType, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPTagOp(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::TagOp, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatRSPTraceTag(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::TraceTag, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATQoS(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::QoS, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATTgtID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::TgtID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATSrcID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::SrcID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::TxnID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATHomeNID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::HomeNID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATOpcode(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::Opcode, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATRespErr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::RespErr, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATResp(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::Resp, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATFwdState(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::FwdState, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATDataPull(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::DataPull, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATDataSource(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::DataSource, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATCBusy(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::CBusy, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATDBID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::DBID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATCCID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::CCID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATDataID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::DataID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATTagOp(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::TagOp, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATTag(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::Tag, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATTU(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::TU, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATTraceTag(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::TraceTag, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATRSVDC(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::RSVDC, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATBE(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::BE, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATData(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingVector(kv, Keys::DAT::Data, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATDataCheck(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::DataCheck, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatDATPoison(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::Poison, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPQoS(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::QoS, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPSrcID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::SrcID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::TxnID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPFwdTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::FwdTxnID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPFwdNID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::FwdNID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPStashLPIDValid(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::StashLPIDValid, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPStashLPID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::StashLPID, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPVMIDExt(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::VMIDExt, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPOpcode(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::Opcode, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPAddr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::Addr, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPNS(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::NS, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPDoNotGoToSD(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::DoNotGoToSD, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPDoNotDataPull(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::DoNotDataPull, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPRetToSrc(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::RetToSrc, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPTraceTag(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::TraceTag, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DefaultFormatter<config>::FormatSNPMPAM(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::MPAM, fmt);
    }
}


#endif // __CHI__CHI_EXPRESSO_FLIT_*

//#endif
