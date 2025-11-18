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

        class KeyBack {
        public:
            const char* const   name;
            const char* const   canonicalName;
            
        public:
            inline constexpr KeyBack(const char* name, const char* canonicalName) noexcept
                : name(name), canonicalName(canonicalName) { };

        public:
            inline constexpr operator const KeyBack*() const noexcept
            { return this; }
        };

        using Key = const KeyBack*;

        namespace Keys {

            namespace REQ {
                inline constexpr KeyBack QoS                ("REQ.QoS",             "QoS"               );
                inline constexpr KeyBack TgtID              ("REQ.TgtID",           "TgtID"             );
                inline constexpr KeyBack SrcID              ("REQ.SrcID",           "SrcID"             );
                inline constexpr KeyBack TxnID              ("REQ.TxnID",           "TxnID"             );
                inline constexpr KeyBack ReturnNID          ("REQ.ReturnNID",       "ReturnNID"         );
                inline constexpr KeyBack StashNID           ("REQ.StashNID",        "StashNID"          );
                inline constexpr KeyBack SLCRepHint         ("REQ.SLCRepHint",      "SLCRepHint"        );
                inline constexpr KeyBack StashNIDValid      ("REQ.StashNIDValid",   "StashNIDValid"     );
                inline constexpr KeyBack Endian             ("REQ.Endian",          "Endian"            );
                inline constexpr KeyBack Deep               ("REQ.Deep",            "Deep"              );
                inline constexpr KeyBack ReturnTxnID        ("REQ.ReturnTxnID",     "ReturnTxnID"       );
                inline constexpr KeyBack StashLPIDValid     ("REQ.StashLPIDValid",  "StashLPIDValid"    );
                inline constexpr KeyBack StashLPID          ("REQ.StashLPID",       "StashLPID"         );
                inline constexpr KeyBack Size               ("REQ.Size",            "Size"              );
                inline constexpr KeyBack Addr               ("REQ.Addr",            "Addr"              );
                inline constexpr KeyBack NS                 ("REQ.NS",              "NS"                );
                inline constexpr KeyBack LikelyShared       ("REQ.LikelyShared",    "LikelyShared"      );
                inline constexpr KeyBack AllowRetry         ("REQ.AllowRetry",      "AllowRetry"        );
                inline constexpr KeyBack Order              ("REQ.Order",           "Order"             );
                inline constexpr KeyBack PCrdType           ("REQ.PCrdType",        "PCrdType"          );
                inline constexpr KeyBack MemAttr            ("REQ.MemAttr",         "MemAttr"           );
                inline constexpr KeyBack SnpAttr            ("REQ.SnpAttr",         "SnpAttr"           );
                inline constexpr KeyBack DoDWT              ("REQ.DoDWT",           "DoDWT"             );
                inline constexpr KeyBack LPID               ("REQ.LPID",            "LPID"              );
                inline constexpr KeyBack PGroupID           ("REQ.PGroupID",        "PGroupID"          );
                inline constexpr KeyBack StashGroupID       ("REQ.StashGroupID",    "StashGroupID"      );
                inline constexpr KeyBack TagGroupID         ("REQ.TagGroupID",      "TagGroupID"        );
                inline constexpr KeyBack Excl               ("REQ.Excl",            "Excl"              );
                inline constexpr KeyBack SnoopMe            ("REQ.SnoopMe",         "SnoopMe"           );
                inline constexpr KeyBack ExpCompAck         ("REQ.ExpCompAck",      "ExpCompAck"        );
                inline constexpr KeyBack TagOp              ("REQ.TagOp",           "TagOp"             );
                inline constexpr KeyBack TraceTag           ("REQ.TraceTag",        "TraceTag"          );
                inline constexpr KeyBack MPAM               ("REQ.MPAM",            "MPAM"              );
                inline constexpr KeyBack RSVDC              ("REQ.RSVDC",           "RSVDC"             );
            }

            namespace RSP {
                inline constexpr KeyBack QoS                ("RSP.QoS",             "QoS"               );
                inline constexpr KeyBack TgtID              ("RSP.TgtID",           "TgtId"             );
                inline constexpr KeyBack SrcID              ("RSP.SrcID",           "SrcID"             );
                inline constexpr KeyBack TxnID              ("RSP.TxnID",           "TxnID"             );
                inline constexpr KeyBack RespErr            ("RSP.RespErr",         "RespErr"           );
                inline constexpr KeyBack Resp               ("RSP.Resp",            "Resp"              );
                inline constexpr KeyBack FwdState           ("RSP.FwdState",        "FwdState"          );
                inline constexpr KeyBack DataPull           ("RSP.DataPull",        "DataPull"          );
                inline constexpr KeyBack CBusy              ("RSP.CBusy",           "CBusy"             );
                inline constexpr KeyBack DBID               ("RSP.DBID",            "DBID"              );
                inline constexpr KeyBack PGroupID           ("RSP.PGroupID",        "PGroupID"          );
                inline constexpr KeyBack StashGroupID       ("RSP.StashGroupID",    "StashGroupID"      );
                inline constexpr KeyBack TagGroupID         ("RSP.TagGroupID",      "TagGroupID"        );
                inline constexpr KeyBack PCrdType           ("RSP.PCrdType",        "PCrdType"          );
                inline constexpr KeyBack TagOp              ("RSP.TagOp",           "TagOp"             );
                inline constexpr KeyBack TraceTag           ("RSP.TraceTag",        "TraceTag"          );
            }

            namespace DAT {
                inline constexpr KeyBack QoS                ("DAT.QoS",             "QoS"               );
                inline constexpr KeyBack TgtID              ("DAT.TgtID",           "TgtID"             );
                inline constexpr KeyBack SrcID              ("DAT.SrcID",           "SrcID"             );
                inline constexpr KeyBack TxnID              ("DAT.TxnID",           "TxnID"             );
                inline constexpr KeyBack HomeNID            ("DAT.HomeNID",         "HomeNID"           );
                inline constexpr KeyBack RespErr            ("DAT.RespErr",         "RespErr"           );
                inline constexpr KeyBack Resp               ("DAT.Resp",            "Resp"              );
                inline constexpr KeyBack FwdState           ("DAT.FwdState",        "FwdState"          );
                inline constexpr KeyBack DataPull           ("DAT.DataPull",        "DataPull"          );
                inline constexpr KeyBack DataSource         ("DAT.DataSource",      "DataSource"        );
                inline constexpr KeyBack CBusy              ("DAT.CBusy",           "CBusy"             );
                inline constexpr KeyBack DBID               ("DAT.DBID",            "DBID"              );
                inline constexpr KeyBack CCID               ("DAT.CCID",            "CCID"              );
                inline constexpr KeyBack DataID             ("DAT.DataID",          "DataID"            );
                inline constexpr KeyBack TagOp              ("DAT.TagOp",           "TagOp"             );
                inline constexpr KeyBack Tag                ("DAT.Tag",             "Tag"               );
                inline constexpr KeyBack TU                 ("DAT.TU",              "TU"                );
                inline constexpr KeyBack TraceTag           ("DAT.TraceTag",        "TraceTag"          );
                inline constexpr KeyBack RSVDC              ("DAT.RSVDC",           "RSVDC"             );
                inline constexpr KeyBack BE                 ("DAT.BE",              "BE"                );
                inline constexpr KeyBack Data               ("DAT.Data",            "Data"              );
                inline constexpr KeyBack DataCheck          ("DAT.DataCheck",       "DataCheck"         );
                inline constexpr KeyBack Poison             ("DAT.Poison",          "Poison"            );                
            }

            namespace SNP {
                inline constexpr KeyBack QoS                ("SNP.QoS",             "QoS"               );
                inline constexpr KeyBack SrcID              ("SNP.SrcID",           "SrcID"             );
                inline constexpr KeyBack TxnID              ("SNP.TxnID",           "TxnID"             );
                inline constexpr KeyBack FwdNID             ("SNP.FwdNID",          "FwdNID"            );
                inline constexpr KeyBack FwdTxnID           ("SNP.FwdTxnID",        "FwdTxnID"          );
                inline constexpr KeyBack StashLPIDValid     ("SNP.StashLPIDValid",  "StashLPIDValid"    );
                inline constexpr KeyBack StashLPID          ("SNP.StashLPID",       "StashLPID"         );
                inline constexpr KeyBack VMIDExt            ("SNP.VMIDExt",         "VMIDExt"           );
                inline constexpr KeyBack Addr               ("SNP.Addr",            "Addr"              );
                inline constexpr KeyBack NS                 ("SNP.NS",              "NS"                );
                inline constexpr KeyBack DoNotGoToSD        ("SNP.DoNotGoToSD",     "DoNotGoToSD"       );
                inline constexpr KeyBack DoNotDataPull      ("SNP.DoNotDataPull",   "DoNotDataPull"     );
                inline constexpr KeyBack RetToSrc           ("SNP.RetToSrc",        "RetToSrc"          );
                inline constexpr KeyBack TraceTag           ("SNP.TraceTag",        "TraceTag"          );
                inline constexpr KeyBack MPAM               ("SNP.MPAM",            "MPAM"              );
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
