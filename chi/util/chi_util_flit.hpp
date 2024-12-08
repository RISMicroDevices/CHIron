//#pragma once

//#ifndef __CHI__CHI_UTIL_FLIT
//#define __CHI__CHI_UTIL_FLIT

#ifndef CHI_UTIL_FLIT__STANDALONE
#   include "chi_util_flit_header.hpp"
#   include "../spec/chi_protocol_flits.hpp"
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_UTIL_FLIT_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_UTIL_FLIT_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_UTIL_FLIT_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_UTIL_FLIT_EB
#endif


/*
namespace CHI {
*/
    namespace Flits {

        //
        /*
        Use Flit De-Serializers to decode the flits from FLIT bundles to the C++ Flit objects.
        */
        template<REQFlitConfigurationConcept        config, 
                 CHI::IOLevelConnectionConcept      conn>
        inline bool DeserializeREQ(REQ<config, conn>& flit, uint32_t* flitBits, size_t bitLength) noexcept;

        template<RSPFlitConfigurationConcept        config, 
                 CHI::IOLevelConnectionConcept      conn>
        inline bool DeserializeRSP(RSP<config, conn>& flit, uint32_t* flitBits, size_t bitLength) noexcept;

        template<RSPFlitConfigurationConcept        config, 
                 CHI::IOLevelConnectionConcept      conn>
        inline bool DeserializeSNP(SNP<config, conn>& flit, uint32_t* flitBits, size_t bitLength) noexcept;

        template<RSPFlitConfigurationConcept        config, 
                 CHI::IOLevelConnectionConcept      conn>
        inline bool DeserializeDAT(DAT<config, conn>& flit, uint32_t* flitBits, size_t bitLength) noexcept;
        //


        //
        /*
        Use Flit Format Evaluators to evaluate flit format at run-time.
        *NOTICE: This utility only provides the computed format information of CHI Flit.
                 The memory structure IS NOT and WILL NOT BE provided in near future,
                 since this was a niche demand.
        */

        // REQ Flit measure
        class REQMeasure {
        public:
            /*
            Parameters for CHI REQ Flit
            */
            class Parameters {
            public:
                size_t  nodeIdWidth;
                size_t  reqAddrWidth;
                size_t  reqRsvdcWidth;
#ifdef CHI_ISSUE_EB_ENABLE
                bool    mpamPresent;
#endif

            public:
                bool    Check() const noexcept;
            };

        public:
            struct ListOfField {
                size_t  QoS;
                size_t  TgtID;
                size_t  SrcID;
                size_t  TxnID;
                size_t  ReturnNID;
                size_t  StashNID;
#ifdef CHI_ISSUE_EB_ENABLE
                size_t  SLCRepHint;
#endif
                size_t  StashNIDValid;
                size_t  Endian;
#ifdef CHI_ISSUE_EB_ENABLE
                size_t  Deep;
#endif
                size_t  ReturnTxnID;
                size_t  StashLPID;
                size_t  StashLPIDValid;
                size_t  Opcode;
                size_t  Size;
                size_t  Addr;
                size_t  NS;
                size_t  LikelyShared;
                size_t  AllowRetry;
                size_t  Order;
                size_t  PCrdType;
                size_t  MemAttr;
                size_t  SnpAttr;
#ifdef CHI_ISSUE_EB_ENABLE
                size_t  DoDWT;
#endif
                size_t  LPID;
#ifdef CHI_ISSUE_EB_ENABLE
                size_t  PGroupID;
                size_t  StashGroupID;
                size_t  TagGroupID;
#endif
                size_t  Excl;
                size_t  SnoopMe;
                size_t  ExpCompAck;
#ifdef CHI_ISSUE_EB_ENABLE
                size_t  TagOp;
#endif
                size_t  TraceTag;
#ifdef CHI_ISSUE_EB_ENABLE
                size_t  MPAM;
#endif
                size_t  RSVDC;
            };

        public:
            /*
            Measurements of CHI REQ Flit
            */

            struct : public ListOfField {
                size_t  _;
            } width;

            struct : public ListOfField {
                size_t  _;
            } lsb;

            struct : public ListOfField {
                size_t  _;
            } msb;

        public:
            bool    Eval(const Parameters& params) noexcept;
        };

        // DAT Flit measure
        class DATMeasure {
        public:
            /*
            Parameters for CHI DAT Flit
            */
            class Parameters {
            public:
                size_t  nodeIdWidth;
                size_t  dataWidth;
                size_t  datRsvdcWidth;
                bool    dataCheckPresent;
                bool    poisonPresent;

            public:
                bool    Check() const noexcept;
            };

        public:
            struct ListOfField {
                size_t  QoS;
                size_t  TgtID;
                size_t  SrcID;
                size_t  TxnID;
                size_t  HomeNID;
                size_t  Opcode;
                size_t  RespErr;
                size_t  Resp;
                size_t  FwdState;
                size_t  DataPull;
                size_t  DataSource;
#ifdef CHI_ISSUE_EB_ENABLE
                size_t  CBusy;
#endif
                size_t  DBID;
                size_t  CCID;
                size_t  DataID;
#ifdef CHI_ISSUE_EB_ENABLE
                size_t  TagOp;
                size_t  Tag;
                size_t  TU;
#endif
                size_t  TraceTag;
                size_t  RSVDC;
                size_t  BE;
                size_t  Data;
                size_t  DataCheck;
                size_t  Poison;
            };

        public:
            /*
            Measurements of CHI DAT Flit
            */

            struct : public ListOfField {
                size_t  _;
            } width;

            struct : public ListOfField {
                size_t  _;
            } lsb;

            struct : public ListOfField {
                size_t  _;
            } msb;

        public:
            bool    Eval(const Parameters& params) noexcept;
        };

        // RSP Flit measure
        class RSPMeasure {
        public:
            /*
            Parameters for CHI DAT Flit
            */
            class Parameters {
            public:
                size_t  nodeIdWidth;

            public:
                bool    Check() const noexcept;
            };

        public:
            struct ListOfField {
                size_t  QoS;
                size_t  TgtID;
                size_t  SrcID;
                size_t  TxnID;
                size_t  Opcode;
                size_t  RespErr;
                size_t  Resp;
                size_t  FwdState;
                size_t  DataPull;
#ifdef CHI_ISSUE_EB_ENABLE
                size_t  CBusy;
#endif
                size_t  DBID;
#ifdef CHI_ISSUE_EB_ENABLE
                size_t  PGroupID;
                size_t  StashGroupID;
                size_t  TagGroupID;
#endif
                size_t  PCrdType;
#ifdef CHI_ISSUE_EB_ENABLE
                size_t  TagOp;
#endif
                size_t  TraceTag;
            };

        public:
            /*
            Measurements of CHI RSP Flit
            */

            struct : public ListOfField {
                size_t  _;
            } width;

            struct : public ListOfField {
                size_t  _;
            } lsb;

            struct : public ListOfField {
                size_t  _;
            } msb;

        public:
            bool    Eval(const Parameters& params) noexcept;
        };

        // SNP Flit measure
        class SNPMeasure {
        public:
            /*
            Parameters for CHI SNP Flit
            */
            class Parameters {
            public:
                size_t  nodeIdWidth;
                size_t  reqAddrWidth;
#ifdef CHI_ISSUE_EB_ENABLE
                bool    mpamPresent;
#endif
            public:
                bool    Check() const noexcept;
            };

        public:
            struct ListOfField {
                size_t  QoS;
                size_t  SrcID;
                size_t  TxnID;
                size_t  FwdNID;
                size_t  FwdTxnID;
                size_t  StashLPID;
                size_t  StashLPIDValid;
                size_t  VMIDExt;
                size_t  Opcode;
                size_t  Addr;
                size_t  NS;
                size_t  DoNotGoToSD;
#ifdef CHI_ISSUE_B_ENABLE
                size_t  DoNotDataPull;
#endif
                size_t  RetToSrc;
                size_t  TraceTag;
#ifdef CHI_ISSUE_EB_ENABLE
                size_t  MPAM;
#endif
            };

        public:
            /*
            Measurements of CHI SNP Flit
            */

            struct : public ListOfField {
                size_t  _;
            } width;

            struct : public ListOfField {
                size_t  _;
            } lsb;

            struct : public ListOfField {
                size_t  _;
            } msb;

        public:
            bool    Eval(const Parameters& params) noexcept;
        };
    }
/*
}
*/


/*
namespace CHI {
*/
    namespace Flits {

        namespace details {

            /*
            Flit walker.
            */
            class FlitWalker {
            public:
                size_t index;
                size_t offset;

            public:
                uint32_t*   flitBits;

            public:
                FlitWalker(uint32_t* flitBits)
                    : index     (0)
                    , offset    (0)
                    , flitBits  (flitBits)
                { }

                inline uint32_t Walk32(size_t width = 32) noexcept
                {
                    uint32_t field
                        = (flitBits[index] >> offset) & (0xFFFFFFFF >> (32 - width));

                    int32_t next_width = offset + width - 32;
                    if (next_width > 0)
                    {
                        field |= (flitBits[++index] & (0xFFFFFFFF >> (32 - next_width))) << (width - next_width);
                        offset = next_width;
                    }
                    else if ((offset += width) == 32)
                    {
                        offset = 0;
                        index++;
                    }

                    return field;
                }

                inline void Finish(size_t width)
                {
                    assert(index * 32 + offset == width);
                }
            };
        }

        // Flit de-serializer implementations.
        /*
        REQ flit de-serializer.
        */
        template<REQFlitConfigurationConcept    config, 
                 CHI::IOLevelConnectionConcept  conn>
        inline bool DeserializeREQ(REQ<config, conn>& flit, uint32_t* flitBits, size_t bitLength) noexcept
        {
            using REQ_t = REQ<config, conn>;

            if (bitLength != REQ_t::WIDTH)
                return false;

            details::FlitWalker walker(flitBits);

            flit.QoS()              = walker.Walk32(REQ_t::QOS_WIDTH);
            flit.TgtID()            = walker.Walk32(REQ_t::TGTID_WIDTH);
            flit.SrcID()            = walker.Walk32(REQ_t::SRCID_WIDTH);
            flit.TxnID()            = walker.Walk32(REQ_t::TXNID_WIDTH);
            flit.ReturnNID()        = walker.Walk32(REQ_t::RETURNNID_WIDTH);
        //  flit.StashNID()
#ifdef CHI_ISSUE_EB_ENABLE
        //  flit.SLCRepHint()
#endif
            flit.StashNIDValid()    = walker.Walk32(REQ_t::STASHNIDVALID_WIDTH);
        //  flit.Endian()
#ifdef CHI_ISSUE_EB_ENABLE
        //  flit.Deep()
#endif
            flit.ReturnTxnID()      = walker.Walk32(REQ_t::RETURNTXNID_WIDTH);
        //  flit.StashLPID()
        //  flit.StashLPIDValid()
            flit.Opcode()           = walker.Walk32(REQ_t::OPCODE_WIDTH);
            flit.Size()             = walker.Walk32(REQ_t::SSIZE_WIDTH);
            flit.Addr()             = uint64_t(walker.Walk32())
                                    | uint64_t(walker.Walk32(REQ_t::ADDR_WIDTH - 32));
            flit.NS()               = walker.Walk32(REQ_t::NS_WIDTH);
            flit.LikelyShared()     = walker.Walk32(REQ_t::LIKELYSHARED_WIDTH);
            flit.AllowRetry()       = walker.Walk32(REQ_t::ALLOWRETRY_WIDTH);
            flit.Order()            = walker.Walk32(REQ_t::ORDER_WIDTH);
            flit.PCrdType()         = walker.Walk32(REQ_t::PCRDTYPE_WIDTH);
            flit.MemAttr()          = walker.Walk32(REQ_t::MEMATTR_WIDTH);
            flit.SnpAttr()          = walker.Walk32(REQ_t::SNPATTR_WIDTH);
#ifdef CHI_ISSUE_EB_ENABLE
        //  flit.DoDWT()
#endif
#ifdef CHI_ISSUE_B_ENABLE
            flit.LPID()             = walker.Walk(REQ_t::LPID_WIDTH);
#endif
        //  flit.PGroupID()
        //  flit.StashGroupID()
#ifdef CHI_ISSUE_EB_ENABLE
            flit.TagGroupID()       = walker.Walk32(REQ_t::TAGGROUPID_WIDTH);
#endif
            flit.Excl()             = walker.Walk32(REQ_t::EXCL_WIDTH);
        //  flit.SnoopMe()
            flit.ExpCompAck()       = walker.Walk32(REQ_t::EXPCOMPACK_WIDTH);
#ifdef CHI_ISSUE_EB_ENABLE
            flit.TagOp()            = walker.Walk32(REQ_t::TAGOP_WIDTH);
#endif
            flit.TraceTag()         = walker.Walk32(REQ_t::TRACETAG_WIDTH);
#ifdef CHI_ISSUE_EB_ENABLE
            if constexpr (REQ_t::hasMPAM)
            {
                flit.MPAM()         = walker.Walk32(REQ_t::MPAM_WIDTH);
            }
#endif

            if constexpr (REQ_t::hasRSVDC)
            {
                flit.RSVDC()        = walker.Walk32(REQ_t::RSVDC_WIDTH);
            }

            //
            walker.Finish(REQ_t::WIDTH);

            return true;
        }

        /*
        RSP flit de-serializer.
        */
        template<RSPFlitConfigurationConcept    config, 
                 CHI::IOLevelConnectionConcept  conn>
        inline bool DeserializeRSP(RSP<config, conn>& flit, uint32_t* flitBits, size_t bitLength) noexcept
        {
            using RSP_t = RSP<config, conn>;

            if (bitLength != RSP_t::WIDTH)
                return false;
            
            details::FlitWalker walker(flitBits);

            flit.QoS()              = walker.Walk32(RSP_t::QOS_WIDTH);
            flit.TgtID()            = walker.Walk32(RSP_t::TGTID_WIDTH);
            flit.SrcID()            = walker.Walk32(RSP_t::SRCID_WIDTH);
            flit.TxnID()            = walker.Walk32(RSP_t::TXNID_WIDTH);
            flit.Opcode()           = walker.Walk32(RSP_t::OPCODE_WIDTH);
            flit.RespErr()          = walker.Walk32(RSP_t::RESPERR_WIDTH);
            flit.Resp()             = walker.Walk32(RSP_t::RESP_WIDTH);
            flit.FwdState()         = walker.Walk32(RSP_t::FWDSTATE_WIDTH);
        //  flit.DataPull()
#ifdef CHI_ISSUE_EB_ENABLE
            flit.CBusy()            = walker.Walk32(RSP_t::CBUSY_WIDTH);
#endif
            flit.DBID()             = walker.Walk32(RSP_t::DBID_WIDTH);
#ifdef CHI_ISSUE_EB_ENABLE
        //  flit.PGroupID()
        //  flit.StashGroupID()
        //  flit.TagGroupID()
#endif
            flit.PCrdType()         = walker.Walk32(RSP_t::PCRDTYPE_WIDTH);
#ifdef CHI_ISSUE_EB_ENABLE
            flit.TagOp()            = walker.Walk32(RSP_t::TAGOP_WIDTH);
#endif
            flit.TraceTag()         = walker.Walk32(RSP_t::TRACETAG_WIDTH);

            //
            walker.Finish(RSP_t::WIDTH);

            return true;
        }

        /*
        SNP flit de-serializer.
        */
        template<SNPFlitConfigurationConcept    config, 
                 CHI::IOLevelConnectionConcept  conn>
        inline bool DeserializeSNP(SNP<config, conn>& flit, uint32_t* flitBits, size_t bitLength) noexcept
        {
            using SNP_t = SNP<config, conn>;

            if (bitLength != SNP_t::WIDTH)
                return false;

            details::FlitWalker walker(flitBits);

            flit.QoS()              = walker.Walk32(SNP_t::QOS_WIDTH);
            flit.SrcID()            = walker.Walk32(SNP_t::SRCID_WIDTH);
            flit.TxnID()            = walker.Walk32(SNP_t::TXNID_WIDTH);
            flit.FwdNID()           = walker.Walk32(SNP_t::FWDNID_WIDTH);
            flit.FwdTxnID()         = walker.Walk32(SNP_t::FWDTXNID_WIDTH);
        //  flit.StashLPID()
        //  flit.StashLPIDValid()
        //  flit.VMIDExt()
            flit.Opcode()           = walker.Walk32(SNP_t::OPCODE_WIDTH);
            flit.Addr()             = uint64_t(walker.Walk32())
                                    | uint64_t(walker.Walk32(SNP_t::ADDR_WIDTH - 32));
            flit.NS()               = walker.Walk32(SNP_t::NS_WIDTH);
            flit.DoNotGoToSD()      = walker.Walk32(SNP_t::DONOTGOTOSD_WIDTH);
#ifdef CHI_ISSUE_B_ENABLE
        //  flit.DoNotDataPull()
#endif
            flit.RetToSrc()         = walker.Walk32(SNP_t::RETTOSRC_WIDTH);
            flit.TraceTag()         = walker.Walk32(SNP_t::TRACETAG_WIDTH);

#ifdef CHI_ISSUE_EB_ENABLE
            if constexpr (SNP_t::hasMPAM)
            {
                flit.MPAM()         = walker.Walk32(SNP_t::MPAM_WIDTH);
            }
#endif

            //
            walker.Finish(SNP_t::WIDTH);

            return true;
        }

        /*
        DAT flit de-serialzer.
        */
        template<DATFlitConfigurationConcept    config, 
                 CHI::IOLevelConnectionConcept  conn>
        inline bool DeserializeDAT(DAT<config, conn>& flit, uint32_t* flitBits, size_t bitLength) noexcept
        {
            using DAT_t = DAT<config, conn>;

            if (bitLength != DAT_t::WIDTH)
                return false;

            details::FlitWalker walker(flitBits);

            flit.QoS()              = walker.Walk32(DAT_t::QOS_WIDTH);
            flit.TgtID()            = walker.Walk32(DAT_t::TGTID_WIDTH);
            flit.SrcID()            = walker.Walk32(DAT_t::SRCID_WIDTH);
            flit.TxnID()            = walker.Walk32(DAT_t::TXNID_WIDTH);
            flit.HomeNID()          = walker.Walk32(DAT_t::HOMENID_WIDTH);
            flit.Opcode()           = walker.Walk32(DAT_t::OPCODE_WIDTH);
            flit.RespErr()          = walker.Walk32(DAT_t::RESPERR_WIDTH);
            flit.Resp()             = walker.Walk32(DAT_t::RESP_WIDTH);
        //  flit.FwdState()
        //  flit.DataPull()
            flit.DataSource()       = walker.Walk32(DAT_t::DATASOURCE_WIDTH);
#ifdef CHI_ISSUE_EB_ENABLE
            flit.CBusy()            = walker.Walk32(DAT_t::CBUSY_WIDTH);
#endif
            flit.DBID()             = walker.Walk32(DAT_t::DBID_WIDTH);
            flit.CCID()             = walker.Walk32(DAT_t::CCID_WIDTH);
            flit.DataID()           = walker.Walk32(DAT_t::DATAID_WIDTH);
#ifdef CHI_ISSUE_EB_ENABLE
            flit.TagOp()            = walker.Walk32(DAT_t::TAGOP_WIDTH);
            flit.Tag()              = walker.Walk32(DAT_t::TAG_WIDTH);
            flit.TU()               = walker.Walk32(DAT_t::TU_WIDTH);
#endif
            flit.TraceTag()         = walker.Walk32(DAT_t::TRACETAG_WIDTH);

            if constexpr (DAT_t::hasRSVDC)
            {
                flit.RSVDC()        = walker.Walk32(DAT_t::RSVDC_WIDTH);
            }

            if constexpr (DAT_t::BE_WIDTH == 64)
            {
                flit.BE()           =  uint64_t(walker.Walk32())
                                    | (uint64_t(walker.Walk32()) << 32);
            }
            else
            {
                flit.BE()           = walker.Walk32(DAT_t::BE_WIDTH);
            }

            if constexpr (DAT_t::DATA_WIDTH >= 128)
            {
                flit.Data()[0]      =  uint64_t(walker.Walk32())
                                    | (uint64_t(walker.Walk32()) << 32);
                
                flit.Data()[1]      =  uint64_t(walker.Walk32())
                                    | (uint64_t(walker.Walk32()) << 32);
            }

            if constexpr (DAT_t::DATA_WIDTH >= 256)
            {
                flit.Data()[2]      =  uint64_t(walker.Walk32())
                                    | (uint64_t(walker.Walk32()) << 32);
                
                flit.Data()[3]      =  uint64_t(walker.Walk32())
                                    | (uint64_t(walker.Walk32()) << 32);
            }

            if constexpr (DAT_t::DATA_WIDTH >= 512)
            {
                flit.Data()[4]      =  uint64_t(walker.Walk32())
                                    | (uint64_t(walker.Walk32()) << 32);
                
                flit.Data()[5]      =  uint64_t(walker.Walk32())
                                    | (uint64_t(walker.Walk32()) << 32);

                flit.Data()[6]      =  uint64_t(walker.Walk32())
                                    | (uint64_t(walker.Walk32()) << 32);
                
                flit.Data()[7]      =  uint64_t(walker.Walk32())
                                    | (uint64_t(walker.Walk32()) << 32);
            }

            if constexpr (DAT_t::hasDataCheck)
            {
                if constexpr (DAT_t::DATACHECK_WIDTH == 64)
                {
                    flit.DataCheck()    =  uint64_t(walker.Walk32())
                                        | (uint64_t(walker.Walk32()) << 32);
                }
                else
                {
                    flit.DataCheck()    = walker.Walk32(DAT_t::DATACHECK_WIDTH);
                }
            }

            if constexpr (DAT_t::hasPoison)
            {
                flit.Poison() = walker.Walk32(DAT_t::POISON_WIDTH);
            }

            //
            walker.Finish(DAT_t::WIDTH);

            return true;
        }

        //
    }
/*
}
*/


// Implementation of: class REQMeasure::Parameters
/*
namespace CHI {
*/
    namespace Flits {

        inline bool REQMeasure::Parameters::Check() const noexcept
        {
            if (!CHI::CheckNodeIdWidth(nodeIdWidth))
                return false;

            if (!CHI::CheckReqAddrWidth(reqAddrWidth))
                return false;

            if (!CHI::CheckRSVDCWidth(reqRsvdcWidth))
                return false;

            return true;
        }
    }
/*
}
*/

// Implementation of: class REQMeasure
/*
namespace CHI {
*/
    namespace Flits {

        inline bool REQMeasure::Eval(const Parameters& params) noexcept
        {
            if (!params.Check())
                return false;

            // QoS
            width   .QoS = 4;
            lsb     .QoS = 0;
            msb     .QoS = 3;

            // TgtID
            width   .TgtID = params.nodeIdWidth;
            lsb     .TgtID = msb.QoS + 1;
            msb     .TgtID = msb.QoS + width.TgtID;

            // SrcID
            width   .SrcID = params.nodeIdWidth;
            lsb     .SrcID = msb.TgtID + 1;
            msb     .SrcID = msb.TgtID + width.SrcID;

            // TxnID
#ifdef CHI_ISSUE_B_ENABLE
            width   .TxnID = 8;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width   .TxnID = 12;
#endif
            lsb     .TxnID = msb.SrcID + 1;
            msb     .TxnID = msb.SrcID + width.TxnID;

            // ReturnNID
            width   .ReturnNID = params.nodeIdWidth;
            lsb     .ReturnNID = msb.TxnID + 1;
            msb     .ReturnNID = msb.TxnID + width.ReturnNID;

            // StashNID
            width   .StashNID = params.nodeIdWidth;
            lsb     .StashNID = msb.TxnID + 1;
            msb     .StashNID = msb.TxnID + width.StashNID;

#ifdef CHI_ISSUE_EB_ENABLE
            // SLCRepHint
            width   .SLCRepHint = 7;
            lsb     .SLCRepHint = msb.TxnID + 1;
            msb     .SLCRepHint = msb.TxnID + width.SLCRepHint;
#endif

            // StashNIDValid
            width   .StashNIDValid = 1;
            lsb     .StashNIDValid = msb.ReturnNID + 1;
            msb     .StashNIDValid = msb.ReturnNID + width.StashNIDValid;

            // Endian
            width   .Endian = 1;
            lsb     .Endian = msb.ReturnNID + 1;
            msb     .Endian = msb.ReturnNID + width.Endian;

#ifdef CHI_ISSUE_EB_ENABLE
            // Deep
            width   .Deep = 1;
            lsb     .Deep = msb.ReturnNID + 1;
            msb     .Deep = msb.ReturnNID + width.Deep;
#endif

            // ReturnTxnID
#ifdef CHI_ISSUE_B_ENABLE
            width   .ReturnTxnID = 8;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width   .ReturnTxnID = 12;
#endif
            lsb     .ReturnTxnID = msb.StashNIDValid + 1;
            msb     .ReturnTxnID = msb.StashNIDValid + width.ReturnTxnID;

            // StashLPID
            width   .StashLPID = 5;
            lsb     .StashLPID = msb.StashNIDValid + 1;
            msb     .StashLPID = msb.StashNIDValid + width.StashLPID;

            // StashLPIDValid
            width   .StashLPIDValid = 1;
            lsb     .StashLPIDValid = msb.StashLPID + 1;
            msb     .StashLPIDValid = msb.StashLPID + width.StashLPIDValid;

            // Opcode
#ifdef CHI_ISSUE_B_ENABLE
            width   .Opcode = 6;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width   .Opcode = 7;
#endif
            lsb     .Opcode = msb.ReturnTxnID + 1;
            msb     .Opcode = msb.ReturnTxnID + width.Opcode;

            // Size
            width   .Size = 3;
            lsb     .Size = msb.Opcode + 1;
            msb     .Size = msb.Opcode + width.Size;

            // Addr
            width   .Addr = params.reqRsvdcWidth;
            lsb     .Addr = msb.Size + 1;
            msb     .Addr = msb.Size + width.Addr;

            // NS
            width   .NS = 1;
            lsb     .NS = msb.Addr + 1;
            msb     .NS = msb.Addr + width.NS;

            // LikelyShared
            width   .LikelyShared = 1;
            lsb     .LikelyShared = msb.NS + 1;
            msb     .LikelyShared = msb.NS + width.LikelyShared;

            // AllowRetry
            width   .AllowRetry = 1;
            lsb     .AllowRetry = msb.LikelyShared + 1;
            msb     .AllowRetry = msb.LikelyShared + width.AllowRetry;

            // Order
            width   .Order = 2;
            lsb     .Order = msb.AllowRetry + 1;
            msb     .Order = msb.AllowRetry + width.Order;

            // PCrdType
            width   .PCrdType = 4;
            lsb     .PCrdType = msb.Order + 1;
            msb     .PCrdType = msb.Order + width.PCrdType;

            // MemAttr
            width   .MemAttr = 4;
            lsb     .MemAttr = msb.PCrdType + 1;
            msb     .MemAttr = msb.PCrdType + width.MemAttr;

            // SnpAttr
            width   .SnpAttr = 1;
            lsb     .SnpAttr = msb.MemAttr + 1;
            msb     .SnpAttr = msb.MemAttr + width.SnpAttr;

#ifdef CHI_ISSUE_EB_ENABLE
            // DoDWT
            width   .DoDWT = 1;
            lsb     .DoDWT = msb.MemAttr + 1;
            msb     .DoDWT = msb.MemAttr + width.DoDWT;
#endif

            // LPID
            width   .LPID = 5;
            lsb     .LPID = msb.SnpAttr + 1;
            msb     .LPID = msb.SnpAttr + width.LPID;

#ifdef CHI_ISSUE_EB_ENABLE
            // PGroupID
            width   .PGroupID = 8;
            lsb     .PGroupID = msb.SnpAttr + 1;
            msb     .PGroupID = msb.SnpAttr + width.PGroupID;

            // StashGroupID
            width   .StashGroupID = 8;
            lsb     .StashGroupID = msb.SnpAttr + 1;
            msb     .StashGroupID = msb.SnpAttr + width.StashGroupID;

            // TagGroupID
            width   .TagGroupID = 8;
            lsb     .TagGroupID = msb.SnpAttr + 1;
            msb     .TagGroupID = msb.SnpAttr + width.TagGroupID;
#endif

            // Excl
            width   .Excl = 1;
#ifdef CHI_ISSUE_B_ENABLE
            lsb     .Excl = msb.LPID + 1;
            msb     .Excl = msb.LPID + width.Excl;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            lsb     .Excl = msb.TagGroupID + 1;
            msb     .Excl = msb.TagGroupID + width.Excl;
#endif

            // SnoopMe
            width   .SnoopMe = 1;
#ifdef CHI_ISSUE_B_ENABLE
            lsb     .SnoopMe = msb.LPID + 1;
            msb     .SnoopMe = msb.LPID + width.SnoopMe;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            lsb     .SnoopMe = msb.TagGroupID + 1;
            msb     .SnoopMe = msb.TagGroupID + width.SnoopMe;
#endif

            // ExpCompAck
            width   .ExpCompAck = 1;
            lsb     .ExpCompAck = msb.Excl + 1;
            msb     .ExpCompAck = msb.Excl + width.ExpCompAck;

#ifdef CHI_ISSUE_EB_ENABLE
            // TagOp
            width   .TagOp = 2;
            lsb     .TagOp = msb.ExpCompAck + 1;
            msb     .TagOp = msb.ExpCompAck + width.TagOp;
#endif

            // TraceTag
            width   .TraceTag = 1;
#ifdef CHI_ISSUE_B_ENABLE
            lsb     .TraceTag = msb.ExpCompAck + 1;
            msb     .TraceTag = msb.ExpCompAck + width.TraceTag;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            lsb     .TraceTag = msb.TagOp + 1;
            msb     .TraceTag = msb.TagOp + width.TraceTag;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
            // MPAM
            width   .MPAM = params.mpamPresent ? 11 : 0;
            lsb     .MPAM = msb.TraceTag + 1;
            msb     .MPAM = msb.TraceTag + width.MPAM;
#endif

            // RSVDC
            width   .RSVDC = params.reqRsvdcWidth;
#ifdef CHI_ISSUE_B_ENABLE
            lsb     .RSVDC = msb.TraceTag + 1;
            msb     .RSVDC = msb.TraceTag + width.RSVDC;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            lsb     .RSVDC = msb.MPAM + 1;
            msb     .RSVDC = msb.MPAM + width.RSVDC;
#endif

            //
#ifdef CHI_ISSUE_B_ENABLE
            width._ = width.QoS         + width.TgtID       + width.SrcID       + width.TxnID
                    + width.ReturnNID   + width.StashNIDValid                   + width.ReturnTxnID
                    + width.Opcode      + width.Size        + width.Addr        + width.NS
                    + width.LikelyShared                    + width.AllowRetry  + width.Order
                    + width.PCrdType    + width.MemAttr     + width.SnpAttr     + width.LPID
                    + width.Excl        + width.ExpCompAck  + width.TraceTag    + width.RSVDC;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width._ = width.QoS         + width.TgtID       + width.SrcID       + width.TxnID
                    + width.ReturnNID   + width.StashNIDValid                   + width.ReturnTxnID
                    + width.Opcode      + width.Size        + width.Addr        + width.NS
                    + width.LikelyShared                    + width.AllowRetry  + width.Order
                    + width.PCrdType    + width.MemAttr     + width.SnpAttr     + width.TagGroupID
                    + width.Excl        + width.ExpCompAck  + width.TagOp       + width.TraceTag
                    + width.MPAM        + width.RSVDC;
#endif
            lsb._   = 0;
            msb._   = width._ - 1;

            //
            return true;
        }
    }
/*
}
*/


// Implementation of: class DATMeasure::Parameters
/*
namespace CHI {
*/
    namespace Flits {

        inline bool DATMeasure::Parameters::Check() const noexcept
        {
            if (!CHI::CheckNodeIdWidth(nodeIdWidth))
                return false;

            if (!CHI::CheckDataWidth(dataWidth))
                return false;

            if (!CHI::CheckRSVDCWidth(datRsvdcWidth))
                return false;

            return true;
        }
    }
/*
}
*/

// Implementation of: class DATMeasure
/*
namespace CHI {
*/
    namespace Flits {

        inline bool DATMeasure::Eval(const Parameters& params) noexcept
        {
            if (!params.Check())
                return false;

            // QoS
            width   .QoS = 4;
            lsb     .QoS = 0;
            msb     .QoS = 3;

            // TgtID
            width   .TgtID = params.nodeIdWidth;
            lsb     .TgtID = msb.QoS + 1;
            msb     .TgtID = msb.QoS + width.TgtID;

            // SrcID
            width   .SrcID = params.nodeIdWidth;
            lsb     .SrcID = msb.TgtID + 1;
            msb     .SrcID = msb.TgtID + width.SrcID;

            // TxnID
#ifdef CHI_ISSUE_B_ENABLE
            width   .TxnID = 8;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width   .TxnID = 12;
#endif
            lsb     .TxnID = msb.SrcID + 1;
            msb     .TxnID = msb.SrcID + width.TxnID;

            // HomeNID
            width   .HomeNID = params.nodeIdWidth;
            lsb     .HomeNID = msb.TxnID + 1;
            msb     .HomeNID = msb.TxnID + width.HomeNID;

            // Opcode
#ifdef CHI_ISSUE_B_ENABLE
            width   .Opcode = 3;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width   .Opcode = 4;
#endif
            lsb     .Opcode = msb.HomeNID + 1;
            msb     .Opcode = msb.HomeNID + width.Opcode;

            // RespErr
            width   .RespErr = 2;
            lsb     .RespErr = msb.Opcode + 1;
            msb     .RespErr = msb.Opcode + width.RespErr;

            // Resp
            width   .Resp = 3;
            lsb     .Resp = msb.RespErr + 1;
            msb     .Resp = msb.RespErr + width.Resp;

            // FwdState
            width   .FwdState = 3;
            lsb     .FwdState = msb.Resp + 1;
            msb     .FwdState = msb.Resp + width.FwdState;

            // DataPull
            width   .DataPull = 3;
            lsb     .DataPull = msb.Resp + 1;
            msb     .DataPull = msb.Resp + width.DataPull;

            // DataSource
#ifdef CHI_ISSUE_B_ENABLE
            width   .DataSource = 3;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width   .DataSource = 4;
#endif
            lsb     .DataSource = msb.Resp + 1;
            msb     .DataSource = msb.Resp + width.DataSource;

#ifdef CHI_ISSUE_EB_ENABLE
            // CBusy
            width   .CBusy = 3;
            lsb     .CBusy = msb.DataSource + 1;
            msb     .CBusy = msb.DataSource + width.CBusy;
#endif

            // DBID
#ifdef CHI_ISSUE_B_ENABLE
            width   .DBID = 8;
            lsb     .DBID = msb.DataSource + 1;
            msb     .DBID = msb.DataSource + width.DBID;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width   .DBID = 12;
            lsb     .DBID = msb.CBusy + 1;
            msb     .DBID = msb.CBusy + width.DBID;
#endif

            // CCID
            width   .CCID = 2;
            lsb     .CCID = msb.DBID + 1;
            msb     .CCID = msb.DBID + width.CCID;

            // DataID
            width   .DataID = 2;
            lsb     .DataID = msb.CCID + 1;
            msb     .DataID = msb.CCID + width.DataID;

#ifdef CHI_ISSUE_EB_ENABLE
            // TagOp
            width   .TagOp = 2;
            lsb     .TagOp = msb.DataID + 1;
            msb     .TagOp = msb.DataID + width.TagOp;

            // Tag
            width   .Tag = params.dataWidth / 32;
            lsb     .Tag = msb.TagOp + 1;
            msb     .Tag = msb.TagOp + width.Tag;

            // TU
            width   .TU = params.dataWidth / 128;
            lsb     .TU = msb.Tag + 1;
            msb     .TU = msb.Tag + width.TU;
#endif

            // TraceTag
            width   .TraceTag = 1;
#ifdef CHI_ISSUE_B_ENABLE
            lsb     .TraceTag = msb.DataID + 1;
            msb     .TraceTag = msb.DataID + width.TraceTag;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            lsb     .TraceTag = msb.TU + 1;
            msb     .TraceTag = msb.TU + width.TraceTag;
#endif

            // RSVDC
            width   .RSVDC = params.datRsvdcWidth;
            lsb     .RSVDC = msb.TraceTag + 1;
            msb     .RSVDC = msb.TraceTag + width.RSVDC;

            // BE
            width   .BE = params.dataWidth / 8;
            lsb     .BE = msb.RSVDC + 1;
            msb     .BE = msb.RSVDC + width.BE;

            // Data
            width   .Data = params.dataWidth;
            lsb     .Data = msb.BE + 1;
            msb     .Data = msb.BE + width.Data;

            // DataCheck
            width   .DataCheck = params.dataCheckPresent ? 1 : 0;
            lsb     .DataCheck = msb.Data + 1;
            msb     .DataCheck = msb.Data + width.DataCheck;

            // Poison
            width   .Poison = params.poisonPresent ? 1 : 0;
            lsb     .Poison = msb.DataCheck + 1;
            msb     .Poison = msb.DataCheck + width.Poison;

            //
#ifdef CHI_ISSUE_B_ENABLE
            width._ = width.QoS         + width.TgtID           + width.SrcID           + width.TxnID
                    + width.HomeNID     + width.Opcode          + width.RespErr         + width.Resp
                    + width.DataSource                          + width.DBID            + width.CCID
                    + width.DataID      + width.TraceTag        + width.RSVDC           + width.BE
                    + width.Data        + width.DataCheck       + width.Poison;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width._ = width.QoS         + width.TgtID           + width.SrcID           + width.TxnID
                    + width.HomeNID     + width.Opcode          + width.RespErr         + width.Resp
                    + width.DataSource                          + width.CBusy           + width.DBID
                    + width.CCID        + width.DataID          + width.TagOp           + width.Tag
                    + width.TU          + width.TraceTag        + width.RSVDC           + width.BE
                    + width.Data        + width.DataCheck       + width.Poison;
#endif
            lsb._   = 0;
            msb._   = width._ - 1;

            //
            return true;
        }
    }
/*
}
*/


// Implementation of: class RSPMeasure::Parameters
/*
namespace CHI {
*/
    namespace Flits {

        inline bool RSPMeasure::Parameters::Check() const noexcept
        {
            if (!CHI::CheckNodeIdWidth(nodeIdWidth))
                return false;

            return true;
        }
    }
/*
}
*/

// Implementation of: class RSPMeasure
/*
namespace CHI {
*/
    namespace Flits {

        inline bool RSPMeasure::Eval(const Parameters& params) noexcept
        {
            if (!params.Check())
                return false;

            // QoS
            width   .QoS = 4;
            lsb     .QoS = 0;
            msb     .QoS = 3;

            // TgtID
            width   .TgtID = params.nodeIdWidth;
            lsb     .TgtID = msb.QoS + 1;
            msb     .TgtID = msb.QoS + width.TgtID;

            // SrcID
            width   .SrcID = params.nodeIdWidth;
            lsb     .SrcID = msb.TgtID + 1;
            msb     .SrcID = msb.TgtID + width.SrcID;

            // TxnID
#ifdef CHI_ISSUE_B_ENABLE
            width   .TxnID = 8;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width   .TxnID = 12;
#endif
            lsb     .TxnID = msb.SrcID + 1;
            msb     .TxnID = msb.SrcID + width.TxnID;

            // Opcode
#ifdef CHI_ISSUE_B_ENABLE
            width   .Opcode = 4;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width   .Opcode = 5;
#endif
            lsb     .Opcode = msb.TxnID + 1;
            msb     .Opcode = msb.TxnID + width.Opcode;

            // RespErr
            width   .RespErr = 2;
            lsb     .RespErr = msb.Opcode + 1;
            msb     .RespErr = msb.Opcode + width.RespErr;

            // Resp
            width   .Resp = 3;
            lsb     .Resp = msb.RespErr + 1;
            msb     .Resp = msb.RespErr + width.Resp;

            // FwdState
            width   .FwdState = 3;
            lsb     .FwdState = msb.Resp + 1;
            msb     .FwdState = msb.Resp + width.FwdState;

            // DataPull
            width   .DataPull = 3;
            lsb     .DataPull = msb.Resp + 1;
            lsb     .DataPull = msb.Resp + width.DataPull;

#ifdef CHI_ISSUE_EB_ENABLE
            // CBusy
            width   .CBusy = 3;
            lsb     .CBusy = msb.FwdState + 1;
            msb     .CBusy = msb.FwdState + width.CBusy;
#endif

            // DBID
#ifdef CHI_ISSUE_B_ENABLE
            width   .DBID = 8;
            lsb     .DBID = msb.FwdState + 1;
            msb     .DBID = msb.FwdState + width.DBID;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width   .DBID = 12;
            lsb     .DBID = msb.CBusy + 1;
            msb     .DBID = msb.CBusy + width.DBID;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
            // PGroupID
            width   .PGroupID = 8;
            lsb     .PGroupID = msb.CBusy + 1;
            msb     .PGroupID = msb.CBusy + width.PGroupID;

            // StashGroupID
            width   .StashGroupID = 8;
            lsb     .StashGroupID = msb.CBusy + 1;
            msb     .StashGroupID = msb.CBusy + width.StashGroupID;

            // TagGroupID
            width   .TagGroupID = 8;
            lsb     .TagGroupID = msb.CBusy + 1;
            msb     .TagGroupID = msb.CBusy + width.TagGroupID;
#endif

            // PCrdType
            width   .PCrdType = 4;
            lsb     .PCrdType = msb.DBID + 1;
            msb     .PCrdType = msb.DBID + width.PCrdType;

#ifdef CHI_ISSUE_EB_ENABLE
            // TagOp
            width   .TagOp = 2;
            lsb     .TagOp = msb.PCrdType + 1;
            msb     .TagOp = msb.PCrdType + width.TagOp;
#endif

            // TraceTag
            width   .TraceTag = 1;
#ifdef CHI_ISSUE_B_ENABLE
            lsb     .TraceTag = msb.PCrdType + 1;
            msb     .TraceTag = msb.PCrdType + width.TraceTag;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            lsb     .TraceTag = msb.TagOp + 1;
            msb     .TraceTag = msb.TagOp + width.TraceTag;
#endif

            //
#ifdef CHI_ISSUE_B_ENABLE
            width._ = width.QoS         + width.TgtID           + width.SrcID           + width.TxnID
                    + width.Opcode      + width.RespErr         + width.Resp            + width.FwdState
                    + width.DBID        + width.PCrdType        + width.TraceTag;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width._ = width.QoS         + width.TgtID           + width.SrcID           + width.TxnID
                    + width.Opcode      + width.RespErr         + width.Resp            + width.FwdState
                    + width.CBusy       + width.DBID            + width.PCrdType        + width.TagOp
                    + width.TraceTag;
#endif
            lsb  ._ = 0;
            msb  ._ = width._ -1;

            //
            return true;
        }
    }
/*
}
*/



// Implementation of: class SNPMeasure::Parameters
/*
namespace CHI {
*/
    namespace Flits {

        inline bool SNPMeasure::Parameters::Check() const noexcept
        {
            if (!CHI::CheckNodeIdWidth(nodeIdWidth))
                return false;

            if (!CHI::CheckReqAddrWidth(reqAddrWidth))
                return false;

            return true; 
        }
    }
/*
}
*/

// Implementation of: class SNPMeasure
/*
namespace CHI {
*/
    namespace Flits {

        inline bool SNPMeasure::Eval(const Parameters& params) noexcept
        {
            if (!params.Check())
                return false;

            // QoS
            width   .QoS = 4;
            lsb     .QoS = 0;
            msb     .QoS = 3;

            // SrcID
            width   .SrcID = params.nodeIdWidth;
            lsb     .SrcID = msb.QoS + 1;
            msb     .SrcID = msb.QoS + width.SrcID;

            // TxnID
#ifdef CHI_ISSUE_B_ENABLE
            width   .TxnID = 8;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width   .TxnID = 12;
#endif
            lsb     .TxnID = msb.SrcID + 1;
            msb     .TxnID = msb.SrcID + width.TxnID;

            // FwdNID
            width   .FwdNID = params.nodeIdWidth;
            lsb     .FwdNID = msb.TxnID + 1;
            msb     .FwdNID = msb.TxnID + width.FwdNID;

            // FwdTxnID
#ifdef CHI_ISSUE_B_ENABLE
            width   .FwdTxnID = 8;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width   .FwdTxnID = 12;
#endif
            lsb     .FwdTxnID = msb.FwdNID + 1;
            msb     .FwdTxnID = msb.FwdNID + width.FwdTxnID;

            // StashLPID
            width   .StashLPID = 5;
            lsb     .StashLPID = msb.FwdNID + 1;
            msb     .StashLPID = msb.FwdNID + width.StashLPID;

            // StashLPIDValid
            width   .StashLPIDValid = 1;
            lsb     .StashLPIDValid = msb.StashLPID + 1;
            msb     .StashLPIDValid = msb.StashLPID + width.StashLPIDValid;

            // VMIDExt
            width   .VMIDExt = 8;
            lsb     .VMIDExt = msb.FwdNID + 1;
            msb     .VMIDExt = msb.FwdNID + width.VMIDExt;

            // Opcode
            width   .Opcode = 5;
            lsb     .Opcode = msb.FwdTxnID + 1;
            msb     .Opcode = msb.FwdTxnID + width.Opcode;

            // Addr
            width   .Addr = params.reqAddrWidth - 3;
            lsb     .Addr = msb.Opcode + 1;
            msb     .Addr = msb.Opcode + width.Addr;

            // NS
            width   .NS = 1;
            lsb     .NS = msb.Addr + 1;
            msb     .NS = msb.Addr + width.NS;

            // DoNotGoToSD
            width   .DoNotGoToSD = 1;
            lsb     .DoNotGoToSD = msb.NS + 1;
            msb     .DoNotGoToSD = msb.NS + width.DoNotGoToSD;

#ifdef CHI_ISSUE_B_ENABLE
            // DoNotDataPull
            width   .DoNotDataPull = 1;
            lsb     .DoNotDataPull = msb.NS + 1;
            msb     .DoNotDataPull = msb.NS + width.DoNotDataPull;
#endif

            // RetToSrc
            width   .RetToSrc = 1;
            lsb     .RetToSrc = msb.DoNotGoToSD + 1;
            msb     .RetToSrc = msb.DoNotGoToSD + width.RetToSrc;

            // TraceTag
            width   .TraceTag = 1;
            lsb     .TraceTag = msb.RetToSrc + 1;
            msb     .TraceTag = msb.RetToSrc + width.TraceTag;

#ifdef CHI_ISSUE_EB_ENABLE
            // MPAM
            width   .MPAM = params.mpamPresent ? 11 : 0;
            lsb     .MPAM = msb.TraceTag + 1;
            msb     .MPAM = msb.TraceTag + width.MPAM;
#endif

            //
#ifdef CHI_ISSUE_B_ENABLE
            width._ = width.QoS         + width.SrcID           + width.TxnID           + width.FwdNID
                    + width.FwdTxnID    + width.Opcode          + width.Addr            + width.NS
                    + width.DoNotGoToSD                         + width.RetToSrc        + width.TraceTag;
#endif
#ifdef CHI_ISSUE_EB_ENABLE
            width._ = width.QoS         + width.SrcID           + width.TxnID           + width.FwdNID
                    + width.FwdTxnID    + width.Opcode          + width.Addr            + width.NS
                    + width.DoNotGoToSD                         + width.RetToSrc        + width.TraceTag
                    + width.MPAM;
#endif
            lsb  ._ = 0;
            msb  ._ = width._ - 1;

            //
            return true;
        }
    }
/*
}
*/


#endif // __CHI__CHI_UTIL_FLIT_*

//#endif // __CHI__CHI_UTIL_FLIT
