//#pragma once

//#ifndef __CHI__CHI_XACT_CHECKERS_FIELD
//#define __CHI__CHI_XACT_CHECKERS_FIELD

#ifndef CHI_XACT_CHECKERS_FIELD__STANDALONE
#   include "chi_xact_checkers_field_header.hpp"              // IWYU pragma: keep

#   include "../spec/chi_protocol_encoding_header.hpp"  // IWYU pragma: keep
#   include "../spec/chi_protocol_encoding.hpp"         // IWYU pragma: keep

#   include "chi_xact_base_header.hpp"                  // IWYU pragma: keep
#   include "chi_xact_base.hpp"                         // IWYU pragma: keep

#   include "chi_xact_field_header.hpp"                 // IWYU pragma: keep
#   ifdef CHI_ISSUE_B_ENABLE
#       include "chi_xact_field_b.hpp"                  // IWYU pragma: keep
#   endif
#   ifdef CHI_ISSUE_EB_ENABLE
#       include "chi_xact_field_eb.hpp"                 // IWYU pragma: keep
#   endif
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_XACT_CHECKERS_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_CHECKERS_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_XACT_CHECKERS_FIELD_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_CHECKERS_FIELD_EB
#endif


/*
namespace CHI {
*/
    namespace Xact {
        
        /*
        * Basic Field Checkers based on Field Mapping Table
        * without transaction specific checks.
        */
        template<REQFlitConfigurationConcept    config,
                 CHI::IOLevelConnectionConcept  conn        = CHI::Connection<>>
        class RequestFieldMappingChecker {
        public:
            RequestFieldMappingTable    table;

        public:
            XactDenialEnum      Check(const Flits::REQ<config, conn>&) noexcept;
        };

        template<RSPFlitConfigurationConcept    config,
                 CHI::IOLevelConnectionConcept  conn        = CHI::Connection<>>
        class ResponseFieldMappingChecker {
        public:
            ResponseFieldMappingTable   table;

        public:
            XactDenialEnum      Check(const Flits::RSP<config, conn>&) noexcept;
        };

        template<DATFlitConfigurationConcept    config,
                 CHI::IOLevelConnectionConcept  conn        = CHI::Connection<>>
        class DataFieldMappingChecker {
        public:
            DataFieldMappingTable       table;

        public:
            XactDenialEnum      Check(const Flits::DAT<config, conn>&) noexcept;
        };

        template<SNPFlitConfigurationConcept    config,
                 CHI::IOLevelConnectionConcept  conn        = CHI::Connection<>>
        class SnoopFieldMappingChecker {
        public:
            SnoopFieldMappingTable      table;

        public:
            XactDenialEnum      Check(const Flits::SNP<config, conn>&) noexcept;
        };

        template<FlitConfigurationConcept       config,
                 CHI::IOLevelConnectionConcept  conn        = CHI::Connection<>>
        class FieldMappingChecker {
            // TODO
        };
        /**/


        namespace details {

            template<class Tv>
            inline bool Check(FieldConvention conv, Tv val)
            {
                switch (conv)
                {
                    case FieldConvention::A0:
                        if (val != 0)
                            return false;
                        break;
                    
                    case FieldConvention::A1:
                        if (val != 1)
                            return false;
                        break;

                    case FieldConvention::I0:
                        if (val != 0)
                            return false;
                        break;

                    case FieldConvention::X:
                        break;

                    case FieldConvention::Y:
                        // should be checked by Fine-Grained Field Checkers, not here
                        break;

                    case FieldConvention::B8:
                        if (val != Sizes::B8)
                            return false;
                        break;
                    
                    case FieldConvention::B64:
                        if (val != Sizes::B64)
                            return false;
                        break;
                    
                    case FieldConvention::S:
                        break;

                    case FieldConvention::D:
                        // TODO: pass MPAM default values to here maybe
                        break;

                    default:
                        return false;
                }

                return true;
            }

            template<class Tv>
            inline bool CheckDataArray(FieldConvention conv, Tv val)
            {
                switch (conv)
                {
                    case FieldConvention::X:
                        return true;

                    case FieldConvention::Y:
                        return true;

                    default:
                        return false;
                }

                return true;
            }
        }
    }
/*
}
*/


// Implementation of: class RequestFieldMappingChecker
namespace /*CHI::*/Xact {
    /*
    RequestFieldMappingTable    table;
    */

    template<REQFlitConfigurationConcept    config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum RequestFieldMappingChecker<config, conn>::Check(const Flits::REQ<config, conn>& reqFlit) noexcept
    {
        #define CHECK_REQ(field, denial) \
            if (!details::Check(mapping->field, reqFlit.field())) \
                return XactDenial::DENIED_REQ_FIELD_##denial;

        #define CHECK_REQ_IF(field, denial) \
            if constexpr (Flits::REQ<config, conn>::has##field) \
                if (!details::Check(mapping->field, reqFlit.field())) \
                    return XactDenial::DENIED_REQ_FIELD_##denial;
        
        #define CHECK_REQ_EX(field, req_field, denial) \
            if (!details::Check(mapping->field, req_field)) \
                return XactDenial::DENIED_REQ_FIELD_##denial;

        RequestFieldMapping mapping = table.Get(reqFlit.Opcode());

        if (!mapping)
            return XactDenial::DENIED_OPCODE;

        CHECK_REQ   (QoS            , QOS           );
        CHECK_REQ   (TgtID          , TGTID         );
        CHECK_REQ   (SrcID          , SRCID         );
        CHECK_REQ   (TxnID          , TXNID         );
        CHECK_REQ   (Opcode         , OPCODE        );
        CHECK_REQ   (AllowRetry     , ALLOWRETRY    );
        CHECK_REQ   (PCrdType       , PCRDTYPE      );
        CHECK_REQ_IF(RSVDC          , RSVDC         );
#ifdef CHI_ISSUE_EB_ENABLE
        CHECK_REQ   (TagOp          , TAGOP         );
#endif
        CHECK_REQ   (TraceTag       , TRACETAG      );
#ifdef CHI_ISSUE_EB_ENABLE
        CHECK_REQ_IF(MPAM           , MPAM          );
#endif
        CHECK_REQ   (Addr           , ADDR          );
        CHECK_REQ   (NS             , NS            );
        CHECK_REQ   (Size           , SIZE          );
        CHECK_REQ_EX(Allocate       , MemAttrs::ExtractAllocate(reqFlit.MemAttr()) != 0  , MEMATTR_ALLOCATE  );
        CHECK_REQ_EX(Cacheable      , MemAttrs::ExtractCacheable(reqFlit.MemAttr()) != 0 , MEMATTR_CACHEABLE );
        CHECK_REQ_EX(Device         , MemAttrs::ExtractDevice(reqFlit.MemAttr()) != 0    , MEMATTR_DEVICE    );
        CHECK_REQ_EX(EWA            , MemAttrs::ExtractEWA(reqFlit.MemAttr()) != 0       , MEMATTR_EWA       );
        CHECK_REQ   (SnpAttr        , SNPATTR       );
#ifdef CHI_ISSUE_EB_ENABLE
        CHECK_REQ   (DoDWT          , DODWT         );
#endif
        CHECK_REQ   (Order          , ORDER         );
        CHECK_REQ   (LikelyShared   , LIKELYSHARED  );
        CHECK_REQ   (Excl           , EXCL          );
        CHECK_REQ   (SnoopMe        , SNOOPME       );
        CHECK_REQ   (ExpCompAck     , EXPCOMPACK    );
        CHECK_REQ   (LPID           , LPID          );
#ifdef CHI_ISSUE_EB_ENABLE
        CHECK_REQ   (TagGroupID     , TAGGROUPID    );
        CHECK_REQ   (StashGroupID   , STASHGROUPID  );
        CHECK_REQ   (PGroupID       , PGROUPID      );
#endif
        CHECK_REQ   (ReturnNID      , RETURNNID     );
        CHECK_REQ   (StashNID       , STASHNID      );
#ifdef CHI_ISSUE_EB_ENABLE
        CHECK_REQ   (SLCRepHint     , SLCREPHINT    );
#endif
        CHECK_REQ   (StashNIDValid  , STASHNIDVALID );
        CHECK_REQ   (Endian         , ENDIAN        );
#ifdef CHI_ISSUE_EB_ENABLE
        CHECK_REQ   (Deep           , DEEP          );
#endif
        CHECK_REQ   (ReturnTxnID    , RETURNTXNID   );
        CHECK_REQ   (StashLPIDValid , STASHLPIDVALID);
        CHECK_REQ   (StashLPID      , STASHLPID     );

        #undef CHECK_REQ
        #undef CHECK_REQ_IF
        #undef CHECK_REQ_EX

        return XactDenial::ACCEPTED;
    }
}


// Implementation of: class ResponseFieldMappingChecker
namespace /*CHI::*/Xact {
    /*
    ResponseFieldMappingTable   table;
    */

    template<RSPFlitConfigurationConcept    config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum ResponseFieldMappingChecker<config, conn>::Check(const Flits::RSP<config, conn>& rspFlit) noexcept
    {
        #define CHECK_RSP(field, denial) \
            if (!details::Check(mapping->field, rspFlit.field())) \
                return XactDenial::DENIED_RSP_FIELD_##denial;

        #define CHECK_RSP_IF(field, denial) \
            if constexpr (Flits::RSP<config, conn>::has##field) \
                if (!details::Check(mapping->field, rspFlit.field())) \
                    return XactDenial::DENIED_RSP_FIELD_##denial;
        
        #define CHECK_RSP_EX(field, rsp_field, denial) \
            if (!details::Check(mapping->field, rsp_field)) \
                return XactDenial::DENIED_RSP_FIELD_##denial;

        ResponseFieldMapping mapping = table.Get(rspFlit.Opcode());

        if (!mapping)
            return XactDenial::DENIED_OPCODE;

        CHECK_RSP   (QoS            , QOS           );
        CHECK_RSP   (TgtID          , TGTID         );
        CHECK_RSP   (SrcID          , SRCID         );
        CHECK_RSP   (TxnID          , TXNID         );
        CHECK_RSP   (Opcode         , OPCODE        );
        CHECK_RSP   (RespErr        , RESPERR       );
        CHECK_RSP   (Resp           , RESP          );
#ifdef CHI_ISSUE_EB_ENABLE
        CHECK_RSP   (CBusy          , CBUSY         );
#endif
        CHECK_RSP   (DBID           , DBID          );
#ifdef CHI_ISSUE_EB_ENABLE
        CHECK_RSP   (TagGroupID     , TAGGROUPID    );
        CHECK_RSP   (StashGroupID   , STASHGROUPID  );
        CHECK_RSP   (PGroupID       , PGROUPID      );
#endif
        CHECK_RSP   (PCrdType       , PCRDTYPE      );
        CHECK_RSP   (FwdState       , FWDSTATE      );
        CHECK_RSP   (DataPull       , DATAPULL      );
#ifdef CHI_ISSUE_EB_ENABLE
        CHECK_RSP   (TagOp          , TAGOP         );
#endif
        CHECK_RSP   (TraceTag       , TRACETAG      );

        #undef CHECK_RSP
        #undef CHECK_RSP_IF
        #undef CHECK_RSP_EX

        return XactDenial::ACCEPTED;
    }
}


// Implementation of: class DataFieldMappingChecker
namespace /*CHI::*/Xact {
    /*
    DataFieldMappingTable       table;
    */

    template<DATFlitConfigurationConcept    config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum DataFieldMappingChecker<config, conn>::Check(const Flits::DAT<config, conn>& datFlit) noexcept
    {
        #define CHECK_DAT(field, denial) \
            if (!details::Check(mapping->field, datFlit.field())) \
                return XactDenial::DENIED_DAT_FIELD_##denial;
        
        #define CHECK_DAT_DS(field, denial) \
            if (!details::CheckDataArray(mapping->field, datFlit.field())) \
                return XactDenial::DENIED_DAT_FIELD_##denial;

        #define CHECK_DAT_IF(field, denial) \
            if constexpr (Flits::DAT<config, conn>::has##field) \
                if (!details::Check(mapping->field, datFlit.field())) \
                    return XactDenial::DENIED_DAT_FIELD_##denial;
        
        #define CHECK_DAT_EX(field, dat_field, denial) \
            if (!details::Check(mapping->field, dat_field)) \
                return XactDenial::DENIED_DAT_FIELD_##denial;

        DataFieldMapping mapping = table.Get(datFlit.Opcode());

        if (!mapping)
            return XactDenial::DENIED_OPCODE;

        CHECK_DAT   (QoS            , QOS           );
        CHECK_DAT   (TgtID          , TGTID         );
        CHECK_DAT   (SrcID          , SRCID         );
        CHECK_DAT   (TxnID          , TXNID         );
        CHECK_DAT   (Opcode         , OPCODE        );
        CHECK_DAT   (RespErr        , RESPERR       );
        CHECK_DAT   (Resp           , RESP          );
#ifdef CHI_ISSUE_EB_ENABLE
        CHECK_DAT   (CBusy          , CBUSY         );
#endif
        CHECK_DAT   (DBID           , DBID          );
        CHECK_DAT   (CCID           , CCID          );
        CHECK_DAT   (DataID         , DATAID        );
        CHECK_DAT_IF(RSVDC          , RSVDC         );
        CHECK_DAT   (BE             , BE            );
        CHECK_DAT_DS(Data           , DATA          );
        CHECK_DAT   (HomeNID        , HOMENID       );
        CHECK_DAT   (FwdState       , FWDSTATE      );
        CHECK_DAT   (DataPull       , DATAPULL      );
        CHECK_DAT   (DataSource     , DATASOURCE    );
        CHECK_DAT   (TraceTag       , TRACETAG      );
        CHECK_DAT_IF(DataCheck      , DATACHECK     );
        CHECK_DAT_IF(Poison         , POISON        );
        CHECK_DAT   (TagOp          , TAGOP         );
        CHECK_DAT   (Tag            , TAG           );
        CHECK_DAT   (TU             , TU            );

        #undef CHECK_DAT
        #undef CHECK_DAT_DS
        #undef CHECK_DAT_IF
        #undef CHECK_DAT_EX

        return XactDenial::ACCEPTED;
    }
}


// Implementation of: class SnoopFieldMappingChecker
namespace /*CHI::*/Xact {
    /*
    SnoopFieldMappingTable      table;
    */

    template<SNPFlitConfigurationConcept    config,
             CHI::IOLevelConnectionConcept  conn>
    inline XactDenialEnum SnoopFieldMappingChecker<config, conn>::Check(const Flits::SNP<config, conn>& snpFlit) noexcept
    {
        #define CHECK_SNP(field, denial) \
            if (!details::Check(mapping->field, snpFlit.field())) \
                return XactDenial::DENIED_SNP_FIELD_##denial;
        
        #define CHECK_SNP_IF(field, denial) \
            if constexpr (Flits::SNP<config, conn>::has##field) \
                if (!details::Check(mapping->field, snpFlit.field())) \
                    return XactDenial::DENIED_SNP_FIELD_##denial;
        
        #define CHECK_SNP_EX(field, snp_field, denial) \
            if (!details::Check(mapping->field, snp_field)) \
                return XactDenial::DENIED_SNP_FIELD_##denial;

        SnoopFieldMapping mapping = table.Get(snpFlit.Opcode());

        if (!mapping)
            return XactDenial::DENIED_OPCODE;

        CHECK_SNP   (QoS            , QOS           );
        CHECK_SNP   (SrcID          , SRCID         );
        CHECK_SNP   (TxnID          , TXNID         );
        CHECK_SNP   (Opcode         , OPCODE        );
        CHECK_SNP   (Addr           , ADDR          );
        CHECK_SNP   (NS             , NS            );
        CHECK_SNP   (FwdNID         , FWDNID        );
        CHECK_SNP   (FwdTxnID       , FWDTXNID      );
        CHECK_SNP   (StashLPIDValid , STASHLPIDVALID);
        CHECK_SNP   (StashLPID      , STASHLPID     );
        CHECK_SNP   (VMIDExt        , VMIDEXT       );
        CHECK_SNP   (DoNotGoToSD    , DONOTGOTOSD   );
#ifdef CHI_ISSUE_B_ENABLE
        CHECK_SNP   (DoNotDataPull  , DONOTDATAPULL );
#endif
        CHECK_SNP   (RetToSrc       , RETTOSRC      );
        CHECK_SNP   (TraceTag       , TRACETAG      );
#ifdef CHI_ISSUE_EB_ENABLE
        CHECK_SNP_IF(MPAM           , MPAM          );
#endif

        #undef CHECK_SNP
        #undef CHECK_SNP_IF
        #undef CHECK_SNP_EX

        return XactDenial::ACCEPTED;
    }
}


#endif // __CHI__CHI_XACT_CHECKERS_*

//#endif // __CHI__CHI_XACT_CHECKERS
