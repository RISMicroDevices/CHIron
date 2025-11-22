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
            
        public:
            inline constexpr KeyBack(const KeyCategory  category,
                                     const char*        name,
                                     const char*        canonicalName) noexcept
                : prev          (nullptr)
                , category      (category)
                , ordinal       (0)
                , name          (name)
                , canonicalName (canonicalName) 
            { }

            inline constexpr KeyBack(const KeyCategory      category,
                                     const char*            name,
                                     const char*            canonicalName,
                                     const KeyBack* const   prev) noexcept
                : prev          (prev)
                , category      (category)
                , ordinal       (prev->ordinal + 1)
                , name          (name)
                , canonicalName (canonicalName) 
            { }

        public:
            inline constexpr operator const KeyBack*() const noexcept
            { return this; }
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


        namespace Keys {

            namespace REQ {
                inline constexpr KeyBack RSVDC              (KeyCategory::REQ,  "REQ.RSVDC",           "RSVDC"             );
                inline constexpr KeyBack MPAM               (KeyCategory::REQ,  "REQ.MPAM",            "MPAM"              , RSVDC          );
                inline constexpr KeyBack TraceTag           (KeyCategory::REQ,  "REQ.TraceTag",        "TraceTag"          , MPAM           );
                inline constexpr KeyBack TagOp              (KeyCategory::REQ,  "REQ.TagOp",           "TagOp"             , TraceTag       );
                inline constexpr KeyBack ExpCompAck         (KeyCategory::REQ,  "REQ.ExpCompAck",      "ExpCompAck"        , TagOp          );
                inline constexpr KeyBack SnoopMe            (KeyCategory::REQ,  "REQ.SnoopMe",         "SnoopMe"           , ExpCompAck     );
                inline constexpr KeyBack Excl               (KeyCategory::REQ,  "REQ.Excl",            "Excl"              , SnoopMe        );
                inline constexpr KeyBack TagGroupID         (KeyCategory::REQ,  "REQ.TagGroupID",      "TagGroupID"        , Excl           );
                inline constexpr KeyBack StashGroupID       (KeyCategory::REQ,  "REQ.StashGroupID",    "StashGroupID"      , TagGroupID     );
                inline constexpr KeyBack PGroupID           (KeyCategory::REQ,  "REQ.PGroupID",        "PGroupID"          , StashGroupID   );
                inline constexpr KeyBack LPID               (KeyCategory::REQ,  "REQ.LPID",            "LPID"              , PGroupID       );
                inline constexpr KeyBack DoDWT              (KeyCategory::REQ,  "REQ.DoDWT",           "DoDWT"             , LPID           );
                inline constexpr KeyBack SnpAttr            (KeyCategory::REQ,  "REQ.SnpAttr",         "SnpAttr"           , DoDWT          );
                inline constexpr KeyBack MemAttr            (KeyCategory::REQ,  "REQ.MemAttr",         "MemAttr"           , SnpAttr        );
                inline constexpr KeyBack PCrdType           (KeyCategory::REQ,  "REQ.PCrdType",        "PCrdType"          , MemAttr        );
                inline constexpr KeyBack Order              (KeyCategory::REQ,  "REQ.Order",           "Order"             , PCrdType       );
                inline constexpr KeyBack AllowRetry         (KeyCategory::REQ,  "REQ.AllowRetry",      "AllowRetry"        , Order          );
                inline constexpr KeyBack LikelyShared       (KeyCategory::REQ,  "REQ.LikelyShared",    "LikelyShared"      , AllowRetry     );
                inline constexpr KeyBack NS                 (KeyCategory::REQ,  "REQ.NS",              "NS"                , LikelyShared   );
                inline constexpr KeyBack Addr               (KeyCategory::REQ,  "REQ.Addr",            "Addr"              , NS             );
                inline constexpr KeyBack Size               (KeyCategory::REQ,  "REQ.Size",            "Size"              , Addr           );
                inline constexpr KeyBack StashLPID          (KeyCategory::REQ,  "REQ.StashLPID",       "StashLPID"         , Size           );
                inline constexpr KeyBack StashLPIDValid     (KeyCategory::REQ,  "REQ.StashLPIDValid",  "StashLPIDValid"    , StashLPID      );
                inline constexpr KeyBack ReturnTxnID        (KeyCategory::REQ,  "REQ.ReturnTxnID",     "ReturnTxnID"       , StashLPIDValid );
                inline constexpr KeyBack Deep               (KeyCategory::REQ,  "REQ.Deep",            "Deep"              , ReturnTxnID    );
                inline constexpr KeyBack Endian             (KeyCategory::REQ,  "REQ.Endian",          "Endian"            , Deep           );
                inline constexpr KeyBack StashNIDValid      (KeyCategory::REQ,  "REQ.StashNIDValid",   "StashNIDValid"     , Endian         );
                inline constexpr KeyBack SLCRepHint         (KeyCategory::REQ,  "REQ.SLCRepHint",      "SLCRepHint"        , StashNIDValid  );
                inline constexpr KeyBack StashNID           (KeyCategory::REQ,  "REQ.StashNID",        "StashNID"          , SLCRepHint     );
                inline constexpr KeyBack ReturnNID          (KeyCategory::REQ,  "REQ.ReturnNID",       "ReturnNID"         , StashNID       );
                inline constexpr KeyBack TxnID              (KeyCategory::REQ,  "REQ.TxnID",           "TxnID"             , ReturnNID      );
                inline constexpr KeyBack SrcID              (KeyCategory::REQ,  "REQ.SrcID",           "SrcID"             , TxnID          );
                inline constexpr KeyBack TgtID              (KeyCategory::REQ,  "REQ.TgtID",           "TgtID"             , SrcID          );
                inline constexpr KeyBack QoS                (KeyCategory::REQ,  "REQ.QoS",             "QoS"               , TgtID          );

                inline constexpr KeyIterator begin() { return KeyIterator(QoS); }
                inline constexpr KeyIterator end() { return KeyIterator(nullptr); }
            }

            namespace RSP {
                inline constexpr KeyBack TraceTag           (KeyCategory::RSP,  "RSP.TraceTag",        "TraceTag"          );
                inline constexpr KeyBack TagOp              (KeyCategory::RSP,  "RSP.TagOp",           "TagOp"             , TraceTag       );
                inline constexpr KeyBack PCrdType           (KeyCategory::RSP,  "RSP.PCrdType",        "PCrdType"          , TagOp          );
                inline constexpr KeyBack TagGroupID         (KeyCategory::RSP,  "RSP.TagGroupID",      "TagGroupID"        , PCrdType       );
                inline constexpr KeyBack StashGroupID       (KeyCategory::RSP,  "RSP.StashGroupID",    "StashGroupID"      , TagGroupID     );
                inline constexpr KeyBack PGroupID           (KeyCategory::RSP,  "RSP.PGroupID",        "PGroupID"          , StashGroupID   );
                inline constexpr KeyBack DBID               (KeyCategory::RSP,  "RSP.DBID",            "DBID"              , PGroupID       );
                inline constexpr KeyBack CBusy              (KeyCategory::RSP,  "RSP.CBusy",           "CBusy"             , DBID           );
                inline constexpr KeyBack DataPull           (KeyCategory::RSP,  "RSP.DataPull",        "DataPull"          , CBusy          );
                inline constexpr KeyBack FwdState           (KeyCategory::RSP,  "RSP.FwdState",        "FwdState"          , DataPull       );
                inline constexpr KeyBack Resp               (KeyCategory::RSP,  "RSP.Resp",            "Resp"              , FwdState       );
                inline constexpr KeyBack RespErr            (KeyCategory::RSP,  "RSP.RespErr",         "RespErr"           , Resp           );
                inline constexpr KeyBack TxnID              (KeyCategory::RSP,  "RSP.TxnID",           "TxnID"             , RespErr        );
                inline constexpr KeyBack SrcID              (KeyCategory::RSP,  "RSP.SrcID",           "SrcID"             , TxnID          );
                inline constexpr KeyBack TgtID              (KeyCategory::RSP,  "RSP.TgtID",           "TgtId"             , SrcID          );
                inline constexpr KeyBack QoS                (KeyCategory::RSP,  "RSP.QoS",             "QoS"               , TgtID          );

                inline constexpr KeyIterator begin() { return KeyIterator(QoS); }
                inline constexpr KeyIterator end() { return KeyIterator(nullptr); }
            }

            namespace DAT {
                inline constexpr KeyBack Poison             (KeyCategory::DAT,  "DAT.Poison",          "Poison"            );
                inline constexpr KeyBack DataCheck          (KeyCategory::DAT,  "DAT.DataCheck",       "DataCheck"         , Poison         );
                inline constexpr KeyBack Data               (KeyCategory::DAT,  "DAT.Data",            "Data"              , DataCheck      );
                inline constexpr KeyBack BE                 (KeyCategory::DAT,  "DAT.BE",              "BE"                , Data           );
                inline constexpr KeyBack RSVDC              (KeyCategory::DAT,  "DAT.RSVDC",           "RSVDC"             , BE             );
                inline constexpr KeyBack TraceTag           (KeyCategory::DAT,  "DAT.TraceTag",        "TraceTag"          , RSVDC          );
                inline constexpr KeyBack TU                 (KeyCategory::DAT,  "DAT.TU",              "TU"                , TraceTag       );
                inline constexpr KeyBack Tag                (KeyCategory::DAT,  "DAT.Tag",             "Tag"               , TU             );
                inline constexpr KeyBack TagOp              (KeyCategory::DAT,  "DAT.TagOp",           "TagOp"             , Tag            );
                inline constexpr KeyBack DataID             (KeyCategory::DAT,  "DAT.DataID",          "DataID"            , TagOp          );
                inline constexpr KeyBack CCID               (KeyCategory::DAT,  "DAT.CCID",            "CCID"              , DataID         );
                inline constexpr KeyBack DBID               (KeyCategory::DAT,  "DAT.DBID",            "DBID"              , CCID           );
                inline constexpr KeyBack CBusy              (KeyCategory::DAT,  "DAT.CBusy",           "CBusy"             , DBID           );
                inline constexpr KeyBack DataSource         (KeyCategory::DAT,  "DAT.DataSource",      "DataSource"        , CBusy          );
                inline constexpr KeyBack DataPull           (KeyCategory::DAT,  "DAT.DataPull",        "DataPull"          , DataSource     );
                inline constexpr KeyBack FwdState           (KeyCategory::DAT,  "DAT.FwdState",        "FwdState"          , DataPull       );
                inline constexpr KeyBack Resp               (KeyCategory::DAT,  "DAT.Resp",            "Resp"              , FwdState       );
                inline constexpr KeyBack RespErr            (KeyCategory::DAT,  "DAT.RespErr",         "RespErr"           , Resp           );
                inline constexpr KeyBack HomeNID            (KeyCategory::DAT,  "DAT.HomeNID",         "HomeNID"           , RespErr        );
                inline constexpr KeyBack TxnID              (KeyCategory::DAT,  "DAT.TxnID",           "TxnID"             , HomeNID        );
                inline constexpr KeyBack SrcID              (KeyCategory::DAT,  "DAT.SrcID",           "SrcID"             , TxnID          );
                inline constexpr KeyBack TgtID              (KeyCategory::DAT,  "DAT.TgtID",           "TgtID"             , SrcID          );
                inline constexpr KeyBack QoS                (KeyCategory::DAT,  "DAT.QoS",             "QoS"               , TgtID          );

                inline constexpr KeyIterator begin() { return KeyIterator(QoS); }
                inline constexpr KeyIterator end() { return KeyIterator(nullptr); }
            }

            namespace SNP {
                inline constexpr KeyBack MPAM               (KeyCategory::SNP,  "SNP.MPAM",            "MPAM"              );
                inline constexpr KeyBack TraceTag           (KeyCategory::SNP,  "SNP.TraceTag",        "TraceTag"          , MPAM           );
                inline constexpr KeyBack RetToSrc           (KeyCategory::SNP,  "SNP.RetToSrc",        "RetToSrc"          , TraceTag       );
                inline constexpr KeyBack DoNotDataPull      (KeyCategory::SNP,  "SNP.DoNotDataPull",   "DoNotDataPull"     , RetToSrc       );
                inline constexpr KeyBack DoNotGoToSD        (KeyCategory::SNP,  "SNP.DoNotGoToSD",     "DoNotGoToSD"       , DoNotDataPull  );
                inline constexpr KeyBack NS                 (KeyCategory::SNP,  "SNP.NS",              "NS"                , DoNotGoToSD    );
                inline constexpr KeyBack Addr               (KeyCategory::SNP,  "SNP.Addr",            "Addr"              , NS             );
                inline constexpr KeyBack VMIDExt            (KeyCategory::SNP,  "SNP.VMIDExt",         "VMIDExt"           , Addr           );
                inline constexpr KeyBack StashLPID          (KeyCategory::SNP,  "SNP.StashLPID",       "StashLPID"         , VMIDExt        );
                inline constexpr KeyBack StashLPIDValid     (KeyCategory::SNP,  "SNP.StashLPIDValid",  "StashLPIDValid"    , StashLPID      );
                inline constexpr KeyBack FwdTxnID           (KeyCategory::SNP,  "SNP.FwdTxnID",        "FwdTxnID"          , StashLPIDValid );
                inline constexpr KeyBack FwdNID             (KeyCategory::SNP,  "SNP.FwdNID",          "FwdNID"            , FwdTxnID       );
                inline constexpr KeyBack TxnID              (KeyCategory::SNP,  "SNP.TxnID",           "TxnID"             , FwdNID         );
                inline constexpr KeyBack SrcID              (KeyCategory::SNP,  "SNP.SrcID",           "SrcID"             , TxnID          );
                inline constexpr KeyBack QoS                (KeyCategory::SNP,  "SNP.QoS",             "QoS"               , SrcID          );

                inline constexpr KeyIterator begin() { return KeyIterator(QoS); }
                inline constexpr KeyIterator end() { return KeyIterator(nullptr); }
            }
        }

        class Value {
        protected:
            union       {
                uint64_t    value;
                uintptr_t   valuep;     // we are not using std::any here, because it throws
            };

        public:
            inline Value() noexcept;
            inline virtual ~Value() noexcept = default;

        public:
            inline uint64_t         GetValue() const noexcept;
            inline uintptr_t        GetValueRef() const noexcept;

            template<class T>
            inline T                GetValue() const noexcept;

            template<class T>
            inline T*               GetValueRef() const noexcept;
        };

        class ValueIntegral : public Value {
        public:
            inline ValueIntegral(uint64_t value = 0) noexcept;
            inline virtual ~ValueIntegral() noexcept = default;

        public:
            inline void             SetValue(uint64_t value) noexcept;
        };

        class ValueVector : public Value {
        public:
            inline ValueVector(size_t size) noexcept;
            inline ValueVector(size_t size, uint64_t* array) noexcept;
            inline ValueVector(const ValueVector& obj) noexcept;
            inline ValueVector(ValueVector&& obj) noexcept;
            inline virtual ~ValueVector() noexcept;

        protected:
            inline std::vector<uint64_t>*       Get() noexcept;
            inline const std::vector<uint64_t>* Get() const noexcept;
            
        public:
            inline size_t           GetSize() const noexcept;

            inline uint64_t         GetValue(size_t index) const noexcept;
            inline void             SetValue(size_t index, uint64_t value) noexcept;

            inline uint64_t&        operator[](size_t index) noexcept;
            inline uint64_t         operator[](size_t index) const noexcept;

            inline std::contiguous_iterator auto begin() noexcept;
            inline std::contiguous_iterator auto end() noexcept;

            inline std::contiguous_iterator auto begin() const noexcept;
            inline std::contiguous_iterator auto end() const noexcept;
        };

        using KeyValueMap = std::unordered_map<Key, Value>;

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

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn>
        class Extracter {

        };

        // TODO
    }
/*
}
*/


// Implementation of: class Value
namespace /*CHI::*/Expresso::Flit {

    inline Value::Value() noexcept
    { }

    inline uint64_t Value::GetValue() const noexcept
    {
        return value;
    }

    inline uintptr_t Value::GetValueRef() const noexcept
    {
        return valuep;
    }

    template<class T>
    inline T Value::GetValue() const noexcept
    {
        return T(value);
    }

    template<class T>
    inline T* Value::GetValueRef() const noexcept
    {
        return (T*)(valuep);
    }
}


// Implementation of: class ValueIntegral
namespace /*CHI::*/Expresso::Flit {

    inline ValueIntegral::ValueIntegral(uint64_t value) noexcept
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
    {
        auto vec = new std::vector<uint64_t>(size);
        this->valuep = reinterpret_cast<uintptr_t>(vec);

        for (size_t i = 0; i < size; i++)
            vec->push_back(0);
    }

    inline ValueVector::ValueVector(size_t size, uint64_t* array) noexcept
    {
        auto vec = new std::vector<uint64_t>(size);
        this->valuep = reinterpret_cast<uintptr_t>(vec);

        for (size_t i = 0; i < size; i++)
            vec->push_back(array[i]);
    }

    inline ValueVector::ValueVector(const ValueVector& obj) noexcept
    {
        this->valuep = reinterpret_cast<uintptr_t>(new std::vector<uint64_t>(*obj.Get()));
    }

    inline ValueVector::ValueVector(ValueVector&& obj) noexcept
    {
        this->valuep = obj.valuep;
        obj.valuep = 0;
    }

    inline ValueVector::~ValueVector() noexcept
    {
        if (this->valuep)
            delete Get();
        
        this->valuep = 0;
    }

    inline std::vector<uint64_t>* ValueVector::Get() noexcept
    {
        return reinterpret_cast<std::vector<uint64_t>*>(valuep);
    }

    inline const std::vector<uint64_t>* ValueVector::Get() const noexcept
    {
        return reinterpret_cast<const std::vector<uint64_t>*>(valuep);
    }

    inline size_t ValueVector::GetSize() const noexcept
    {
        return Get()->size();
    }

    inline uint64_t ValueVector::GetValue(size_t index) const noexcept
    {
        return (*Get())[index];
    }

    inline void ValueVector::SetValue(size_t index, uint64_t value) noexcept
    {
        (*Get())[index] = value;
    }

    inline uint64_t& ValueVector::operator[](size_t index) noexcept
    {
        return (*Get())[index];
    }

    inline uint64_t ValueVector::operator[](size_t index) const noexcept
    {
        return (*Get())[index];
    }

    inline std::contiguous_iterator auto ValueVector::begin() noexcept
    {
        return Get()->begin();
    }

    inline std::contiguous_iterator auto ValueVector::end() noexcept
    {
        return Get()->end();
    }

    inline std::contiguous_iterator auto ValueVector::begin() const noexcept
    {
        return Get()->begin();
    }

    inline std::contiguous_iterator auto ValueVector::end() const noexcept
    {
        return Get()->end();
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


#endif // __CHI__CHI_EXPRESSO_FLIT_*

//#endif
