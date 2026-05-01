//#pragma once

//#ifndef __CHI__CHI_EXPRESSO_FLIT
//#define __CHI__CHI_EXPRESSO_FLIT

#ifndef CHI_EXPRESSO_FLIT__STANDALONE
#   define CHI_ISSUE_EB_ENABLE
#   include "chi_expresso_flit_header.hpp"      // IWYU pragma: keep
#   include "../spec/chi_protocol_flits.hpp"    // IWYU pragma: keep
#   include "../util/chi_util_decoding.hpp"     // IWYU pragma: keep
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

        class KeyIteration {
        protected:
            KeyIterator iterBegin;
            KeyIterator iterEnd;

        public:
            inline constexpr KeyIteration(KeyIterator iterBegin, KeyIterator iterEnd) noexcept
            : iterBegin(iterBegin), iterEnd(iterEnd) {}

        public:
            inline constexpr KeyIterator begin() const noexcept { return iterBegin; };
            inline constexpr KeyIterator end() const noexcept { return iterEnd; };
        };

        enum class ValueType {
            Empty,
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
            inline Value() noexcept;
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
            inline ValueVector(size_t size, const uint64_t* array) noexcept;
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


        template<FlitConfigurationConcept config>
        class Mapper {
        protected:
            KeyValueMap map;

        public:
            inline Mapper&  Map(const Flits::REQ<config>& reqFlit) noexcept;
            inline Mapper&  Map(const Flits::SNP<config>& snpFlit) noexcept;
            inline Mapper&  Map(const Flits::RSP<config>& rspFlit) noexcept;
            inline Mapper&  Map(const Flits::DAT<config>& datFlit) noexcept;

        public:
            inline KeyValueMap&         Get() noexcept;
            inline const KeyValueMap&   Get() const noexcept;

        protected:
            inline void     MapIntegral(Key key, uint64_t value) noexcept;
            inline void     MapVector(Key key, size_t size, const uint64_t* vec) noexcept;
        };

        template<FlitConfigurationConcept config>
        inline KeyValueMap Map(const Flits::REQ<config>& reqFlit) noexcept
        { return std::move(Mapper<config>().Map(reqFlit).Get()); }

        template<FlitConfigurationConcept config>
        inline KeyValueMap Map(const Flits::SNP<config>& snpFlit) noexcept
        { return std::move(Mapper<config>().Map(snpFlit).Get()); }

        template<FlitConfigurationConcept config>
        inline KeyValueMap Map(const Flits::RSP<config>& rspFlit) noexcept
        { return std::move(Mapper<config>().Map(rspFlit).Get()); }

        template<FlitConfigurationConcept config>
        inline KeyValueMap Map(const Flits::DAT<config>& datFlit) noexcept
        { return std::move(Mapper<config>().Map(datFlit).Get()); }


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

        using PlainFormatter = DefaultFormatter;


        template<FlitConfigurationConcept config = FlitConfiguration<>>
        class DecodingFormatter : public virtual PlainFormatter {
            /*
            *NOTICE: The Default Plain Formatter provides three parameters,
                     exceptions might be thrown with formatting string with more than three parameters.

            *NOTICE: MTE (TagMatch, TagOp...) not supported.
                
            Example:
                Formatting string: "{} = {} ({})"
                Output with specific field key-value:
                    - REQ::QoS      = 0x01 -> "QoS = 1 (1)"
                    - REQ::Opcode   = 0x06 -> "Opcode = <unknown> (6)"
                    - REQ::Opcode   = 0x07 -> "Opcode = ReadUnique (7)"
                    - REQ::MemAttr  = 0x03 -> "MemAttr = EWA, Device (3)"
                    - REQ::RespErr  = 0x00 -> "RespErr = OK (0)"
                    ...
                    - DAT::Data     = 0xFFFFFFFF_CCCC... -> "0xFFFFFFFFCCCC... (256)"
                    ...
            */
        protected:
            REQOpcodeDecoder<Flits::REQ<config>> reqDecoder;
            SNPOpcodeDecoder<Flits::SNP<config>> snpDecoder;
            RSPOpcodeDecoder<Flits::RSP<config>> rspDecoder;
            DATOpcodeDecoder<Flits::DAT<config>> datDecoder;

        protected:
            template<Key key, typename Tflit>
            inline std::string _FormatDecodingOpcode(const Opcodes::DecoderBase<Tflit>* decoder, const KeyValueMap&, const format_func& func) const;

            template<Key key>
            inline std::string _FormatDecodingNS(const KeyValueMap&, const format_func&) const;

            template<Key key>
            inline std::string _FormatDecodingSize(const KeyValueMap&, const format_func&) const;

            template<Key key>
            inline std::string _FormatDecodingMemAttr(const KeyValueMap&, const format_func&) const;

            template<Key key>
            inline std::string _FormatDecodingSnpAttr(const KeyValueMap&, const format_func&) const;

            template<Key key>
            inline std::string _FormatDecodingOrder(const KeyValueMap&, const format_func&) const;

            template<Key key>
            inline std::string _FormatDecodingEndian(const KeyValueMap&, const format_func&) const;

            template<Key key>
            inline std::string _FormatDecodingResp(const KeyValueMap&, const format_func&) const;

            template<Key key>
            inline std::string _FormatDecodingFwdState(const KeyValueMap&, const format_func&) const;

            template<Key key>
            inline std::string _FormatDecodingRespErr(const KeyValueMap&, const format_func&) const;

        public:
            inline virtual std::string FormatREQOpcode(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatRSPOpcode(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATOpcode(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPOpcode(const KeyValueMap&, const format_func&) const override;

            inline virtual std::string FormatSNPAddr(const KeyValueMap&, const format_func&) const override;

            inline virtual std::string FormatREQNS(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatSNPNS(const KeyValueMap&, const format_func&) const override;

            inline virtual std::string FormatREQSize(const KeyValueMap&, const format_func&) const override;

            inline virtual std::string FormatREQMemAttr(const KeyValueMap&, const format_func&) const override;

            inline virtual std::string FormatREQSnpAttr(const KeyValueMap&, const format_func&) const override;

            inline virtual std::string FormatREQOrder(const KeyValueMap&, const format_func&) const override;
            
            inline virtual std::string FormatREQEndian(const KeyValueMap&, const format_func&) const override;

            inline virtual std::string FormatRSPResp(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATResp(const KeyValueMap&, const format_func&) const override;

            inline virtual std::string FormatRSPFwdState(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATFwdState(const KeyValueMap&, const format_func&) const override;

            inline virtual std::string FormatRSPRespErr(const KeyValueMap&, const format_func&) const override;
            inline virtual std::string FormatDATRespErr(const KeyValueMap&, const format_func&) const override;
        };


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

                inline constexpr KeyIteration iteration() { return KeyIteration(begin(), end()); };
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

                inline constexpr KeyIteration iteration() { return KeyIteration(begin(), end()); };
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

                inline constexpr KeyIteration iteration() { return KeyIteration(begin(), end()); };
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

                inline constexpr KeyIteration iteration() { return KeyIteration(begin(), end()); };
            }

            #undef _FMTCALL
        }


        class Filter {
        public:
            inline virtual bool IsAccepted(Key key, const KeyValueMap& map) const noexcept = 0;
        };

        class NoFilter : public Filter {
        public:
            inline virtual constexpr bool IsAccepted(Key, const KeyValueMap&) const noexcept override { return true; }
        };

        class ListFilter : public Filter {
        protected:
            std::unordered_set<Key>                     filter          = { };
            bool                                        filterBlacklist = false;

        public:
            inline void SetFilterAsBlacklist() noexcept;
            inline void SetFilterAsWhitelist() noexcept;
            inline bool IsFilterBlacklist() const noexcept;
            inline bool IsFilterWhitelist() const noexcept;

            inline void ClearFilter() noexcept;
            inline void ClearFilter(Key key) noexcept;
            inline void ClearFilter(std::initializer_list<Key> keys) noexcept;
            inline void ClearFilter(KeyCategory category) noexcept;

            inline void SetFilter(Key key) noexcept;
            inline void SetFilter(std::initializer_list<Key> keys) noexcept;

            inline bool IsAccepted(Key key, const KeyValueMap& map) const noexcept override;
        };

        class FormatterProvider {
        public:
            inline virtual std::shared_ptr<Formatter> Get(Key key, const KeyValueMap& map) const noexcept = 0;
        };

        class NoFormatterProvider : public FormatterProvider {
        public:
            inline virtual std::shared_ptr<Formatter> Get(Key key, const KeyValueMap& map) const noexcept override { return {}; };
        };

        class ReferenceFormatterProvider : public FormatterProvider {
        public:
            std::shared_ptr<Formatter>      formatter;

        public:
            inline ReferenceFormatterProvider() noexcept;
            inline ReferenceFormatterProvider(std::shared_ptr<Formatter>) noexcept;

        public:
            inline virtual std::shared_ptr<Formatter> Get(Key key, const KeyValueMap& map) const noexcept override;
        };

        class DefaultFormatterProvider : public ReferenceFormatterProvider {
        public:
            inline DefaultFormatterProvider() noexcept : ReferenceFormatterProvider(std::make_shared<DefaultFormatter>()) {};
        };

        class OverridableFormatterProvider : public FormatterProvider {
        protected:
            class FormatterOverride {
            public:
                std::shared_ptr<Formatter>                      formatter;
                std::function<bool(const KeyValueMap&, Key)>    predicate;
            };

        protected:
            std::shared_ptr<Formatter>                  formatter;
            std::unordered_map<Key, FormatterOverride>  formatterOverrides;

        public:
            inline OverridableFormatterProvider() noexcept;
            inline OverridableFormatterProvider(std::shared_ptr<Formatter> defaultFormatter) noexcept;

        public:
            inline void SetDefault(std::shared_ptr<Formatter> formatter) noexcept;
            inline std::shared_ptr<Formatter> GetDefault() const noexcept;

            inline void SetOverride(Key key, std::shared_ptr<Formatter> formatter) noexcept;
            inline void SetOverride(Key key, std::shared_ptr<Formatter> formatter, std::function<bool(const KeyValueMap&, Key)> predicate) noexcept;
            inline void SetOverride(std::initializer_list<Key> keys, std::shared_ptr<Formatter> formatter) noexcept;
            inline void SetOverride(std::initializer_list<Key> keys, std::shared_ptr<Formatter> formatter, std::function<bool(const KeyValueMap&, Key)> predicate) noexcept;

            inline void ClearOverride(Key key) noexcept;
            inline void ClearOverride(std::initializer_list<Key> keys) noexcept;
            inline void ClearOverride() noexcept;

        public:
            inline virtual std::shared_ptr<Formatter> Get(Key key, const KeyValueMap& map) const noexcept override;
        };

        class FormatModifier {
        public:
            inline virtual std::string Modify(Key key, const KeyValueMap& map, const std::string& formatted) const noexcept = 0;
        };

        class NoFormatModifier : public FormatModifier {
        public:
            inline virtual constexpr std::string Modify(Key, const KeyValueMap&, const std::string& formatted) const noexcept override { return formatted; }
        };

        class FunctionalFormatModifier : public FormatModifier {
        public:
            using ModifierFunction = std::function<std::string(Key key, const KeyValueMap&, const std::string&)>;

            inline static std::string _Default(Key, const KeyValueMap&, const std::string&) noexcept;

        public:
            ModifierFunction    modifier;

        public:
            inline FunctionalFormatModifier() noexcept;
            inline FunctionalFormatModifier(ModifierFunction modifier) noexcept;

        public:
            inline virtual std::string Modify(Key key, const KeyValueMap& map, const std::string& formatted) const noexcept override;
        };

        class Printer {
        protected:
            std::shared_ptr<FormatterProvider>          formatter;
            std::shared_ptr<FormatModifier>             modifier;
            std::shared_ptr<Filter>                     filter;

        public:
            inline Printer() noexcept;
            inline Printer(std::shared_ptr<FormatterProvider> formatter,
                           std::shared_ptr<FormatModifier>    modifier = std::make_shared<NoFormatModifier>(),
                           std::shared_ptr<Filter>            filter   = std::make_shared<NoFilter>()) noexcept;

        public:
            inline std::shared_ptr<FormatterProvider> GetFormatter() const noexcept;
            inline void SetFormatter(std::shared_ptr<FormatterProvider> formatter) noexcept;

            inline std::shared_ptr<FormatModifier> GetModifier() const noexcept;
            inline void SetModifier(std::shared_ptr<FormatModifier> modifier) noexcept;

            inline std::shared_ptr<Filter> GetFilter() const noexcept;
            inline void SetFilter(std::shared_ptr<Filter> filter) noexcept;

        public:
            template<FlitConfigurationConcept config>
            inline bool PrintFlit(std::ostream&, const Flits::REQ<config>&) const;

            template<FlitConfigurationConcept config>
            inline bool PrintFlit(std::ostream&, const Flits::SNP<config>&) const;

            template<FlitConfigurationConcept config>
            inline bool PrintFlit(std::ostream&, const Flits::RSP<config>&) const;
            
            template<FlitConfigurationConcept config>
            inline bool PrintFlit(std::ostream&, const Flits::DAT<config>&) const;

        public:
            inline virtual bool PrintREQ(std::ostream&, const KeyValueMap&) const;
            inline virtual bool PrintSNP(std::ostream&, const KeyValueMap&) const;
            inline virtual bool PrintRSP(std::ostream&, const KeyValueMap&) const;
            inline virtual bool PrintDAT(std::ostream&, const KeyValueMap&) const;

        protected:
            inline virtual bool Print(const KeyIteration&, std::ostream&, const KeyValueMap&) const = 0;
        };

        class DefaultPrinter : public Printer {
        public:
            public:
            static constexpr const char* DEFAULT_FORMAT     = "{} = {} ({:#x})";
            static constexpr const char* DEFAULT_SEPERATOR  = ", ";

            format_func         format      = [](auto) { return DEFAULT_FORMAT; };
            std::string         seperator   = DEFAULT_SEPERATOR; 

        public:
            inline DefaultPrinter() noexcept;
            inline DefaultPrinter(std::shared_ptr<FormatterProvider> formatter,
                                  std::shared_ptr<FormatModifier>    modifier = std::make_shared<NoFormatModifier>(),
                                  std::shared_ptr<Filter>            filter   = std::make_shared<NoFilter>()) noexcept;

        protected:
            inline virtual bool Print(const KeyIteration&, std::ostream&, const KeyValueMap&) const override;
        };

        using PlainPrinter = DefaultPrinter;
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

    inline Value::Value() noexcept
        : type(ValueType::Empty)
    { }

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

    inline ValueVector::ValueVector(size_t size, const uint64_t* array) noexcept
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

    template<FlitConfigurationConcept config>
    inline Mapper<config>& Mapper<config>::Map(const Flits::REQ<config>& reqFlit) noexcept
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
        MapIntegral(Keys::REQ::Opcode           , reqFlit.Opcode());
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
        if constexpr (Flits::REQ<config>::hasMPAM)
            MapIntegral(Keys::REQ::MPAM             , reqFlit.MPAM());
#endif
        if constexpr (Flits::REQ<config>::hasRSVDC)
            MapIntegral(Keys::REQ::RSVDC            , reqFlit.RSVDC());

        return *this;
    }

    template<FlitConfigurationConcept config>
    inline Mapper<config>& Mapper<config>::Map(const Flits::SNP<config>& snpFlit) noexcept
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
        if constexpr (Flits::SNP<config>::hasMPAM)
            MapIntegral(Keys::SNP::MPAM,            snpFlit.MPAM());
#endif
        return *this;
    }

    template<FlitConfigurationConcept config>
    inline Mapper<config>& Mapper<config>::Map(const Flits::RSP<config>& rspFlit) noexcept
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

        return this;
    }

    template<FlitConfigurationConcept config>
    inline Mapper<config>& Mapper<config>::Map(const Flits::DAT<config>& datFlit) noexcept
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
        if constexpr (Flits::DAT<config>::hasRSVDC)
            MapIntegral(Keys::DAT::RSVDC            , datFlit.RSVDC());
        MapIntegral(Keys::DAT::BE               , datFlit.BE());
        MapVector(Keys::DAT::Data, Flits::DAT<config>::DATA_WIDTH / 64, datFlit.Data());
        if constexpr (Flits::DAT<config>::hasDataCheck)
            MapIntegral(Keys::DAT::DataCheck        , datFlit.DataCheck());
        if constexpr (Flits::DAT<config>::hasPoison)
            MapIntegral(Keys::DAT::Poison           , datFlit.Poison());

        return *this;
    }

    template<FlitConfigurationConcept config>
    inline KeyValueMap& Mapper<config>::Get() noexcept
    {
        return map;
    }

    template<FlitConfigurationConcept config>
    inline const KeyValueMap& Mapper<config>::Get() const noexcept
    {
        return map;
    }

    template<FlitConfigurationConcept config>
    inline void Mapper<config>::MapIntegral(Key key, uint64_t value) noexcept
    {
        map[key] = ValueIntegral(value);
    }

    template<FlitConfigurationConcept config>
    inline void Mapper<config>::MapVector(Key key, size_t size, const uint64_t* vec) noexcept
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

    inline bool DefaultFormatter::ExtractIntegral(const KeyValueMap& kv, Key key, uint64_t* dst) const
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

    inline bool DefaultFormatter::ExtractVector(const KeyValueMap& kv, Key key, const std::vector<uint64_t>** dst) const
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

    inline std::string DefaultFormatter::_FormatNonDecodingIntegral(const KeyValueMap& kv, Key key, const format_func& fmt) const
    {
        uint64_t integral;

        if (!ExtractIntegral(kv, key, &integral))
            return std::string();

        return std::vformat(fmt(key), std::make_format_args(key->canonicalName, integral, integral));
    }

    inline std::string DefaultFormatter::_FormatNonDecodingVector(const KeyValueMap& kv, Key key, const format_func& fmt) const
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

    inline std::string DefaultFormatter::FormatREQQoS(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::QoS, fmt);
    }

    inline std::string DefaultFormatter::FormatREQTgtID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::TgtID, fmt);
    }

    inline std::string DefaultFormatter::FormatREQSrcID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::SrcID, fmt);
    }

    inline std::string DefaultFormatter::FormatREQTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::TxnID, fmt);
    }

    inline std::string DefaultFormatter::FormatREQReturnNID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::ReturnNID, fmt);
    }

    inline std::string DefaultFormatter::FormatREQStashNID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::StashNID, fmt);
    }

    inline std::string DefaultFormatter::FormatREQSLCRepHint(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::SLCRepHint, fmt);
    }

    inline std::string DefaultFormatter::FormatREQStashNIDValid(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::StashNIDValid, fmt);
    }

    inline std::string DefaultFormatter::FormatREQEndian(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::Endian, fmt);
    }

    inline std::string DefaultFormatter::FormatREQDeep(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::Deep, fmt);
    }

    inline std::string DefaultFormatter::FormatREQReturnTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::ReturnTxnID, fmt);
    }

    inline std::string DefaultFormatter::FormatREQStashLPIDValid(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::StashLPIDValid, fmt);
    }

    inline std::string DefaultFormatter::FormatREQStashLPID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::StashLPID, fmt);
    }

    inline std::string DefaultFormatter::FormatREQOpcode(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::Opcode, fmt);
    }

    inline std::string DefaultFormatter::FormatREQSize(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::Size, fmt);
    }

    inline std::string DefaultFormatter::FormatREQAddr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::Addr, fmt);
    }

    inline std::string DefaultFormatter::FormatREQNS(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::NS, fmt);
    }

    inline std::string DefaultFormatter::FormatREQLikelyShared(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::LikelyShared, fmt);
    }

    inline std::string DefaultFormatter::FormatREQAllowRetry(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::AllowRetry, fmt);
    }

    inline std::string DefaultFormatter::FormatREQOrder(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::Order, fmt);
    }

    inline std::string DefaultFormatter::FormatREQPCrdType(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::PCrdType, fmt);
    }

    inline std::string DefaultFormatter::FormatREQMemAttr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::MemAttr, fmt);
    }

    inline std::string DefaultFormatter::FormatREQSnpAttr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::SnpAttr, fmt);
    }

    inline std::string DefaultFormatter::FormatREQDoDWT(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::DoDWT, fmt);
    }

    inline std::string DefaultFormatter::FormatREQLPID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::LPID, fmt);
    }

    inline std::string DefaultFormatter::FormatREQPGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::PGroupID, fmt);
    }

    inline std::string DefaultFormatter::FormatREQStashGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::StashGroupID, fmt);
    }

    inline std::string DefaultFormatter::FormatREQTagGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::TagGroupID, fmt);
    }

    inline std::string DefaultFormatter::FormatREQExcl(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::Excl, fmt);
    }

    inline std::string DefaultFormatter::FormatREQSnoopMe(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::SnoopMe, fmt);
    }

    inline std::string DefaultFormatter::FormatREQExpCompAck(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::ExpCompAck, fmt);
    }

    inline std::string DefaultFormatter::FormatREQTagOp(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::TagOp, fmt);
    }

    inline std::string DefaultFormatter::FormatREQTraceTag(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::TraceTag, fmt);
    }

    inline std::string DefaultFormatter::FormatREQMPAM(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::MPAM, fmt);
    }

    inline std::string DefaultFormatter::FormatREQRSVDC(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::REQ::RSVDC, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPQoS(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::QoS, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPTgtID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::TgtID, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPSrcID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::SrcID, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::TxnID, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPOpcode(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::Opcode, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPRespErr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::RespErr, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPResp(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::Resp, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPFwdState(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::FwdState, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPDataPull(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::DataPull, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPCBusy(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::CBusy, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPDBID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::DBID, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPPGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::PGroupID, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPStashGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::StashGroupID, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPTagGroupID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::TagGroupID, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPPCrdType(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::PCrdType, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPTagOp(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::TagOp, fmt);
    }

    inline std::string DefaultFormatter::FormatRSPTraceTag(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::RSP::TraceTag, fmt);
    }

    inline std::string DefaultFormatter::FormatDATQoS(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::QoS, fmt);
    }

    inline std::string DefaultFormatter::FormatDATTgtID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::TgtID, fmt);
    }

    inline std::string DefaultFormatter::FormatDATSrcID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::SrcID, fmt);
    }

    inline std::string DefaultFormatter::FormatDATTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::TxnID, fmt);
    }

    inline std::string DefaultFormatter::FormatDATHomeNID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::HomeNID, fmt);
    }

    inline std::string DefaultFormatter::FormatDATOpcode(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::Opcode, fmt);
    }

    inline std::string DefaultFormatter::FormatDATRespErr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::RespErr, fmt);
    }

    inline std::string DefaultFormatter::FormatDATResp(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::Resp, fmt);
    }

    inline std::string DefaultFormatter::FormatDATFwdState(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::FwdState, fmt);
    }

    inline std::string DefaultFormatter::FormatDATDataPull(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::DataPull, fmt);
    }

    inline std::string DefaultFormatter::FormatDATDataSource(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::DataSource, fmt);
    }

    inline std::string DefaultFormatter::FormatDATCBusy(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::CBusy, fmt);
    }

    inline std::string DefaultFormatter::FormatDATDBID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::DBID, fmt);
    }

    inline std::string DefaultFormatter::FormatDATCCID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::CCID, fmt);
    }

    inline std::string DefaultFormatter::FormatDATDataID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::DataID, fmt);
    }

    inline std::string DefaultFormatter::FormatDATTagOp(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::TagOp, fmt);
    }

    inline std::string DefaultFormatter::FormatDATTag(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::Tag, fmt);
    }

    inline std::string DefaultFormatter::FormatDATTU(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::TU, fmt);
    }

    inline std::string DefaultFormatter::FormatDATTraceTag(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::TraceTag, fmt);
    }

    inline std::string DefaultFormatter::FormatDATRSVDC(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::RSVDC, fmt);
    }

    inline std::string DefaultFormatter::FormatDATBE(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::BE, fmt);
    }

    inline std::string DefaultFormatter::FormatDATData(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingVector(kv, Keys::DAT::Data, fmt);
    }

    inline std::string DefaultFormatter::FormatDATDataCheck(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::DataCheck, fmt);
    }

    inline std::string DefaultFormatter::FormatDATPoison(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::DAT::Poison, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPQoS(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::QoS, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPSrcID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::SrcID, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::TxnID, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPFwdTxnID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::FwdTxnID, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPFwdNID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::FwdNID, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPStashLPIDValid(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::StashLPIDValid, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPStashLPID(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::StashLPID, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPVMIDExt(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::VMIDExt, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPOpcode(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::Opcode, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPAddr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::Addr, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPNS(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::NS, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPDoNotGoToSD(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::DoNotGoToSD, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPDoNotDataPull(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::DoNotDataPull, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPRetToSrc(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::RetToSrc, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPTraceTag(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::TraceTag, fmt);
    }

    inline std::string DefaultFormatter::FormatSNPMPAM(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatNonDecodingIntegral(kv, Keys::SNP::MPAM, fmt);
    }
}


// Implementation of: class DecodingFormatter
namespace /*CHI::*/Expresso::Flit {

    template<FlitConfigurationConcept config>
    template<Key key, typename Tflit>
    inline std::string DecodingFormatter<config>::_FormatDecodingOpcode(const Opcodes::DecoderBase<Tflit>* decoder, const KeyValueMap& kv, const format_func& fmt) const
    {
        uint64_t intOpcode;
        if (!this->ExtractIntegral(kv, key, &intOpcode))
            return std::string();

        const auto& opcodeInfo = decoder->Decode(intOpcode);

        if (!opcodeInfo.IsValid())
            return std::vformat(fmt(key), std::make_format_args(key->canonicalName, "<Unknown>", intOpcode));

        std::string opcodeName = opcodeInfo.GetName();

        return std::vformat(fmt(key), std::make_format_args(key->canonicalName, opcodeName, intOpcode));
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatREQOpcode(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingOpcode<Keys::REQ::Opcode>(&reqDecoder, kv, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatRSPOpcode(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingOpcode<Keys::RSP::Opcode>(&rspDecoder, kv, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatDATOpcode(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingOpcode<Keys::DAT::Opcode>(&datDecoder, kv, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatSNPOpcode(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingOpcode<Keys::SNP::Opcode>(&snpDecoder, kv, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatSNPAddr(const KeyValueMap& kv, const format_func& fmt) const
    {
        constexpr Key key = Keys::SNP::Addr;

        uint64_t snpAddr;
        if (!this->ExtractIntegral(kv, key, &snpAddr))
            return std::string();

        uint64_t snpPAddr = snpAddr << 3;

        return std::vformat(fmt(key), std::make_format_args(key->canonicalName, snpPAddr, snpAddr));
    }

    template<FlitConfigurationConcept config>
    template<Key key>
    inline std::string DecodingFormatter<config>::_FormatDecodingNS(const KeyValueMap& kv, const format_func& fmt) const
    {
        uint64_t intNS;
        if (!this->ExtractIntegral(kv, key, &intNS))
            return std::string();

        return std::vformat(fmt(key), std::make_format_args(key->canonicalName, NSs::ToEnum(intNS)->name, intNS));
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatREQNS(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingNS<Keys::REQ::NS>(kv, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatSNPNS(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingNS<Keys::SNP::NS>(kv, fmt);
    }

    template<FlitConfigurationConcept config>
    template<Key key>
    inline std::string DecodingFormatter<config>::_FormatDecodingSize(const KeyValueMap& kv, const format_func& fmt) const
    {
        uint64_t intSize;
        if (!this->ExtractIntegral(kv, key, &intSize))
            return std::string();

        return std::vformat(fmt(key), std::make_format_args(key->canonicalName, Sizes::ToEnum(intSize)->name, intSize));
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatREQSize(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingSize<Keys::REQ::Size>(kv, fmt);
    }

    template<FlitConfigurationConcept config>
    template<Key key>
    inline std::string DecodingFormatter<config>::_FormatDecodingMemAttr(const KeyValueMap& kv, const format_func& fmt) const
    {
        uint64_t intMemAttr;
        if (!this->ExtractIntegral(kv, key, &intMemAttr))
            return std::string();

        return std::vformat(fmt(key), std::make_format_args(key->canonicalName, MemAttrs::ToEnum(intMemAttr)->name, intMemAttr));
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatREQMemAttr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingMemAttr<Keys::REQ::MemAttr>(kv, fmt);
    }

    template<FlitConfigurationConcept config>
    template<Key key>
    inline std::string DecodingFormatter<config>::_FormatDecodingSnpAttr(const KeyValueMap& kv, const format_func& fmt) const
    {
        uint64_t intSnpAttr;
        if (!this->ExtractIntegral(kv, key, &intSnpAttr))
            return std::string();

        return std::vformat(fmt(key), std::make_format_args(key->canonicalName, SnpAttrs::ToEnum(intSnpAttr)->name, intSnpAttr));
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatREQSnpAttr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingSnpAttr<Keys::REQ::SnpAttr>(kv, fmt);
    }

    template<FlitConfigurationConcept config>
    template<Key key>
    inline std::string DecodingFormatter<config>::_FormatDecodingOrder(const KeyValueMap& kv, const format_func& fmt) const
    {
        uint64_t intOrder;
        if (!this->ExtractIntegral(kv, key, &intOrder))
            return std::string();

        return std::vformat(fmt(key), std::make_format_args(key->canonicalName, Orders::ToEnum(intOrder)->name, intOrder));
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatREQOrder(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingOrder<Keys::REQ::Order>(kv, fmt);
    }

    template<FlitConfigurationConcept config>
    template<Key key>
    inline std::string DecodingFormatter<config>::_FormatDecodingEndian(const KeyValueMap& kv, const format_func& fmt) const
    {
        uint64_t intEndian;
        if (!this->ExtractIntegral(kv, key, &intEndian))
            return std::string();

        return std::vformat(fmt(key), std::make_format_args(key->canonicalName, Endians::ToEnum(intEndian)->name, intEndian));
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatREQEndian(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingEndian<Keys::REQ::Endian>(kv, fmt);
    }

    template<FlitConfigurationConcept config>
    template<Key key>
    inline std::string DecodingFormatter<config>::_FormatDecodingResp(const KeyValueMap& kv, const format_func& fmt) const
    {
        uint64_t intResp;
        if (!this->ExtractIntegral(kv, key, &intResp))
            return std::string();

        if constexpr (key == Keys::RSP::Resp)
        {
            uint64_t intOpcode;
            if (this->ExtractIntegral(kv, Keys::RSP::Opcode, &intOpcode))
            {
                RespEnum resp;
                switch (intOpcode)
                {
                    case Opcodes::RSP::SnpResp:
                    case Opcodes::RSP::SnpRespFwded:
                        resp = Resps::Snoop::ToEnum(intResp);
                        break;

                    case Opcodes::RSP::Comp:
                    case Opcodes::RSP::RespSepData:
                        resp = Resps::Comp::ToEnum(intResp);
                        break;

                    default:
                        resp = Resps::ToEnum(intResp);
                        break;
                }

                return std::vformat(fmt(key), std::make_format_args(key->canonicalName, resp->name, intResp));
            }
        }

        if constexpr (key == Keys::DAT::Resp)
        {
            uint64_t intOpcode;
            if (this->ExtractIntegral(kv, Keys::DAT::Opcode, &intOpcode))
            {
                RespEnum resp;
                switch (intOpcode)
                {
                    case Opcodes::DAT::SnpRespData:
                    case Opcodes::DAT::SnpRespDataPtl:
                    case Opcodes::DAT::SnpRespDataFwded:
                        resp = Resps::Snoop::ToEnum(intResp);
                        break;

                    case Opcodes::DAT::CompData:
                    case Opcodes::DAT::DataSepResp:
                        resp = Resps::Comp::ToEnum(intResp);
                        break;

                    case Opcodes::DAT::CopyBackWrData:
                        resp = Resps::WriteData::ToEnum(intResp);
                        break;

                    default:
                        resp = Resps::ToEnum(intResp);
                        break;
                }

                return std::vformat(fmt(key), std::make_format_args(key->canonicalName, resp->name, intResp));
            }
        }

        return std::vformat(fmt(key), std::make_format_args(key->canonicalName, Resps::ToEnum(intResp)->name, intResp));
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatRSPResp(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingResp<Keys::RSP::Resp>(kv, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatDATResp(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingResp<Keys::DAT::Resp>(kv, fmt);
    }

    template<FlitConfigurationConcept config>
    template<Key key>
    inline std::string DecodingFormatter<config>::_FormatDecodingFwdState(const KeyValueMap& kv, const format_func& fmt) const
    {
        uint64_t intFwdState;
        if (!this->ExtractIntegral(kv, key, &intFwdState))
            return std::string();

        return std::vformat(fmt(key), std::make_format_args(key->canonicalName, FwdStates::ToEnum(intFwdState)->name, intFwdState));
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatRSPFwdState(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingFwdState<Keys::RSP::FwdState>(kv, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatDATFwdState(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingFwdState<Keys::DAT::FwdState>(kv, fmt);
    }

    template<FlitConfigurationConcept config>
    template<Key key>
    inline std::string DecodingFormatter<config>::_FormatDecodingRespErr(const KeyValueMap& kv, const format_func& fmt) const
    {
        uint64_t intRespErr;
        if (!this->ExtractIntegral(kv, key, &intRespErr))
            return std::string();

        return std::vformat(fmt(key), std::make_format_args(key->canonicalName, RespErrs::ToEnum(intRespErr)->name, intRespErr));
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatRSPRespErr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingRespErr<Keys::RSP::RespErr>(kv, fmt);
    }

    template<FlitConfigurationConcept config>
    inline std::string DecodingFormatter<config>::FormatDATRespErr(const KeyValueMap& kv, const format_func& fmt) const
    {
        return _FormatDecodingRespErr<Keys::DAT::RespErr>(kv, fmt);
    }
}


// Implementation of: class ListFilter
namespace /*CHI::*/Expresso::Flit {

    inline void ListFilter::SetFilterAsBlacklist() noexcept
    {
        filterBlacklist = true;
    }

    inline void ListFilter::SetFilterAsWhitelist() noexcept
    {
        filterBlacklist = false;
    }

    inline bool ListFilter::IsFilterBlacklist() const noexcept
    {
        return filterBlacklist;
    }

    inline bool ListFilter::IsFilterWhitelist() const noexcept
    {
        return !filterBlacklist;
    }

    inline void ListFilter::ClearFilter() noexcept
    {
        filter.clear();
    }

    inline void ListFilter::ClearFilter(Key key) noexcept
    {
        filter.erase(key);
    }

    inline void ListFilter::ClearFilter(std::initializer_list<Key> keys) noexcept
    {
        for (Key key : keys)
            ClearFilter(key);
    }

    inline void ListFilter::ClearFilter(KeyCategory category) noexcept
    {
        auto iter = filter.begin();
        while (iter != filter.end())
        {
            if ((*iter)->category == category)
                iter = filter.erase(iter);
            else
                iter++;
        }
    }

    inline void ListFilter::SetFilter(Key key) noexcept
    {
        filter.insert(key);
    }

    inline void ListFilter::SetFilter(std::initializer_list<Key> keys) noexcept
    {
        for (Key key : keys)
            SetFilter(key);
    }

    inline bool ListFilter::IsAccepted(Key key, const KeyValueMap& map) const noexcept
    {
        return filter.contains(key) ^ filterBlacklist;
    }
}

// Implementation of: class ReferenceFormatterProvider
namespace /*CHI::*/Expresso::Flit {

    inline ReferenceFormatterProvider::ReferenceFormatterProvider() noexcept
        : formatter ()
    { }

    inline ReferenceFormatterProvider::ReferenceFormatterProvider(std::shared_ptr<Formatter> formatter) noexcept
        : formatter (formatter)
    { }

    inline std::shared_ptr<Formatter> ReferenceFormatterProvider::Get(Key key, const KeyValueMap& kv) const noexcept
    {
        return formatter;
    }
}

// Implementation of: class OverridableFormatterProvider
namespace /*CHI::*/Expresso::Flit {

    inline OverridableFormatterProvider::OverridableFormatterProvider() noexcept
        : formatter             ()
        , formatterOverrides    ()
    { }

    inline OverridableFormatterProvider::OverridableFormatterProvider(std::shared_ptr<Formatter> defaultFormatter) noexcept
        : formatter             (defaultFormatter)
        , formatterOverrides    ()
    { }

    inline void OverridableFormatterProvider::SetDefault(std::shared_ptr<Formatter> formatter) noexcept
    {
        this->formatter = formatter;
    }

    inline std::shared_ptr<Formatter> OverridableFormatterProvider::GetDefault() const noexcept
    {
        return formatter;
    }

    inline void OverridableFormatterProvider::SetOverride(Key key, std::shared_ptr<Formatter> formatter) noexcept
    {
        SetOverride(key, formatter, [](auto, auto) { return true; });
    }

    inline void OverridableFormatterProvider::SetOverride(Key key, std::shared_ptr<Formatter> formatter, std::function<bool(const KeyValueMap&, Key)> predicate) noexcept
    {
        formatterOverrides[key] = { .formatter = formatter, .predicate = predicate };
    }

    inline void OverridableFormatterProvider::SetOverride(std::initializer_list<Key> keys, std::shared_ptr<Formatter> formatter) noexcept
    {
        for (Key key : keys)
            SetOverride(key, formatter);
    }

    inline void OverridableFormatterProvider::SetOverride(std::initializer_list<Key> keys, std::shared_ptr<Formatter> formatter, std::function<bool(const KeyValueMap&, Key)> predicate) noexcept
    {
        for (Key key : keys)
            SetOverride(key, formatter, predicate);
    }

    inline void OverridableFormatterProvider::ClearOverride(Key key) noexcept
    {
        formatterOverrides.erase(key);
    }

    inline void OverridableFormatterProvider::ClearOverride(std::initializer_list<Key> keys) noexcept
    {
        for (Key key : keys)
            ClearOverride(key);
    }

    inline void OverridableFormatterProvider::ClearOverride() noexcept
    {
        formatterOverrides.clear();
    }

    inline std::shared_ptr<Formatter> OverridableFormatterProvider::Get(Key key, const KeyValueMap& kv) const noexcept
    {
        auto iter = formatterOverrides.find(key);

        if (iter != formatterOverrides.end() && iter->second.predicate(kv, key))
            return iter->second.formatter;

        return formatter;
    }
}

// Implementation of: class FunctionalFormatModifier
namespace /*CHI::*/Expresso::Flit {

    inline std::string FunctionalFormatModifier::_Default(Key, const KeyValueMap&, const std::string& str) noexcept
    {
        return str;
    }

    inline FunctionalFormatModifier::FunctionalFormatModifier() noexcept
        : modifier  (_Default)
    { }

    inline FunctionalFormatModifier::FunctionalFormatModifier(ModifierFunction modifier) noexcept
        : modifier  (modifier)
    { }

    inline std::string FunctionalFormatModifier::Modify(Key key, const KeyValueMap& kv, const std::string& formatted) const noexcept
    {
        return modifier(key, kv, formatted);
    }
}

// Implementation of: class Printer
namespace /*CHI::*/Expresso::Flit {

    inline Printer::Printer() noexcept
        : Printer(std::shared_ptr<FormatterProvider>())
    { }

    inline Printer::Printer(std::shared_ptr<FormatterProvider> formatter,
                            std::shared_ptr<FormatModifier>    modifier,
                            std::shared_ptr<Filter>            filter) noexcept
        : formatter (formatter)
        , modifier  (modifier)
        , filter    (filter)
    { }

    inline std::shared_ptr<FormatterProvider> Printer::GetFormatter() const noexcept
    {
        return formatter;
    }

    inline void Printer::SetFormatter(std::shared_ptr<FormatterProvider> formatter) noexcept
    {
        this->formatter = formatter;
    }

    inline std::shared_ptr<FormatModifier> Printer::GetModifier() const noexcept
    {
        return modifier;
    }

    inline void Printer::SetModifier(std::shared_ptr<FormatModifier> modifier) noexcept
    {
        this->modifier = modifier;
    }

    inline std::shared_ptr<Filter> Printer::GetFilter() const noexcept
    {
        return filter;
    }

    inline void Printer::SetFilter(std::shared_ptr<Filter> filter) noexcept
    {
        this->filter = filter;
    }

    template<FlitConfigurationConcept config>
    inline bool Printer::PrintFlit(std::ostream& os, const Flits::REQ<config>& reqFlit) const
    {
        return PrintREQ(os, Map(reqFlit));
    }

    template<FlitConfigurationConcept config>
    inline bool Printer::PrintFlit(std::ostream& os, const Flits::SNP<config>& snpFlit) const
    {
        return PrintSNP(os, Map(snpFlit));
    }

    template<FlitConfigurationConcept config>
    inline bool Printer::PrintFlit(std::ostream& os, const Flits::RSP<config>& rspFlit) const
    {
        return PrintRSP(os, Map(rspFlit));
    }

    template<FlitConfigurationConcept config>
    inline bool Printer::PrintFlit(std::ostream& os, const Flits::DAT<config>& datFlit) const
    {
        return PrintDAT(os, Map(datFlit));
    }

    inline bool Printer::PrintREQ(std::ostream& os, const KeyValueMap& kv) const
    {
        return Print(Keys::REQ::iteration(), os, kv);
    }

    inline bool Printer::PrintSNP(std::ostream& os, const KeyValueMap& kv) const
    {
        return Print(Keys::SNP::iteration(), os, kv);
    }

    inline bool Printer::PrintRSP(std::ostream& os, const KeyValueMap& kv) const
    {
        return Print(Keys::RSP::iteration(), os, kv);
    }

    inline bool Printer::PrintDAT(std::ostream& os, const KeyValueMap& kv) const
    {
        return Print(Keys::DAT::iteration(), os, kv);
    }
}

// Implementation of: class DefaultPrinter
namespace /*CHI::*/Expresso::Flit {

    inline DefaultPrinter::DefaultPrinter() noexcept
        : Printer   (std::make_shared<DefaultFormatterProvider>())
    { }

    inline DefaultPrinter::DefaultPrinter(std::shared_ptr<FormatterProvider> formatter,
                                          std::shared_ptr<FormatModifier>    modifier,
                                          std::shared_ptr<Filter>            filter) noexcept
        : Printer   (formatter, modifier, filter)
    { }

    inline bool DefaultPrinter::Print(const KeyIteration& iters, std::ostream& os, const KeyValueMap& kv) const
    {
        if (!this->formatter)
            return false;

        for (auto iter = iters.begin(); iter != iters.end(); iter++)
        {
            if (this->filter && !this->filter->IsAccepted(*iter, kv))
                continue;

            const auto& formatter = this->formatter->Get(*iter, kv);

            if (!formatter)
                return false;

            std::string formatted = formatter->Format(kv, *iter, format(*iter));

            if (formatted.empty())
                continue;

            if (iter != iters.begin())
                os << seperator;

            if (this->modifier)
                formatted = this->modifier->Modify(*iter, kv, formatted);

            os << formatted;
        }

        return true;
    }
}


#endif // __CHI__CHI_EXPRESSO_FLIT_*

//#endif
