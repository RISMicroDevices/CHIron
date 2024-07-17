//#pragma once

//#ifndef __CHI__CHI_UTIL_FLIT
//#define __CHI__CHI_UTIL_FLIT

#ifndef CHI_UTIL_FLIT__STANDALONE
#   include "chi_util_flit_header.hpp"
#   include "../spec/chi_protocol_flits.hpp"
#endif


/*
namespace CHI {
*/
    namespace Flits {

        namespace details {

            /*
            Flit walker.
            */
            class FlitWalker {
            private:
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

                inline uint32_t Walk(size_t width) noexcept
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

            flit.QoS()              = walker.Walk(REQ_t::QOS_WIDTH);
            flit.TgtID()            = walker.Walk(REQ_t::TGTID_WIDTH);
            flit.SrcID()            = walker.Walk(REQ_t::SRCID_WIDTH);
            flit.TxnID()            = walker.Walk(REQ_t::TXNID_WIDTH);
            flit.ReturnNID()        = walker.Walk(REQ_t::RETURNTXNID_WIDTH);
        //  flit.StashNID()
#ifdef CHI_ISSUE_EB_ENABLE
        //  flit.SLCRepHint()
#endif
            flit.StashNIDValid()    = walker.Walk(REQ_t::STASHNIDVALID_WIDTH);
        //  flit.Endian()
#ifdef CHI_ISSUE_EB_ENABLE
        //  flit.Deep()
#endif
            flit.ReturnTxnID()      = walker.Walk(REQ_t::RETURNTXNID_WIDTH);
        //  flit.StashLPID()
        //  flit.StashLPIDValid()
            flit.Opcode()           = walker.Walk(REQ_t::OPCODE_WIDTH);
            flit.Size()             = walker.Walk(REQ_t::SSIZE_WIDTH);
            flit.Addr()             = walker.Walk(REQ_t::ADDR_WIDTH);
            flit.NS()               = walker.Walk(REQ_t::NS_WIDTH);
            flit.LikelyShared()     = walker.Walk(REQ_t::LIKELYSHARED_WIDTH);
            flit.AllowRetry()       = walker.Walk(REQ_t::ALLOWRETRY_WIDTH);
            flit.Order()            = walker.Walk(REQ_t::ORDER_WIDTH);
            flit.PCrdType()         = walker.Walk(REQ_t::PCRDTYPE_WIDTH);
            flit.MemAttr()          = walker.Walk(REQ_t::MEMATTR_WIDTH);
            flit.SnpAttr()          = walker.Walk(REQ_t::SNPATTR_WIDTH);
#ifdef CHI_ISSUE_EB_ENABLE
        //  flit.DoDWT()
#endif
#ifdef CHI_ISSUE_B_ENABLE
            flit.LPID()             = walker.Walk(REQ_t::LPID_WIDTH);
#endif
        //  flit.PGroupID()
        //  flit.StashGroupID()
#ifdef CHI_ISSUE_EB_ENABLE
            flit.TagGroupID()       = walker.Walk(REQ_t::TAGGROUPID_WIDTH);
#endif
            flit.Excl()             = walker.Walk(REQ_t::EXCL_WIDTH);
        //  flit.SnoopMe()
            flit.ExpCompAck()       = walker.Walk(REQ_t::EXPCOMPACK_WIDTH);
#ifdef CHI_ISSUE_EB_ENABLE
            flit.TagOp()            = walker.Walk(REQ_t::TAGOP_WIDTH);
#endif
            flit.TraceTag()         = walker.Walk(REQ_t::TRACETAG_WIDTH);
#ifdef CHI_ISSUE_EB_ENABLE
            if constexpr (REQ_t::hasMPAM)
            {
                flit.MPAM()         = walker.Walk(REQ_t::MPAM_WIDTH);
            }
#endif

            if constexpr (REQ_t::hasRSVDC)
            {
                flit.RSVDC()        = walker.Walk(REQ_t::RSVDC_WIDTH);
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

            flit.QoS()              = walker.Walk(RSP_t::QOS_WIDTH);
            flit.TgtID()            = walker.Walk(RSP_t::TGTID_WIDTH);
            flit.SrcID()            = walker.Walk(RSP_t::SRCID_WIDTH);
            flit.TxnID()            = walker.Walk(RSP_t::TXNID_WIDTH);
            flit.Opcode()           = walker.Walk(RSP_t::OPCODE_WIDTH);
            flit.RespErr()          = walker.Walk(RSP_t::RESPERR_WIDTH);
            flit.Resp()             = walker.Walk(RSP_t::RESP_WIDTH);
            flit.FwdState()         = walker.Walk(RSP_t::FWDSTATE_WIDTH);
        //  flit.DataPull()
#ifdef CHI_ISSUE_EB_ENABLE
            flit.CBusy()            = walker.Walk(RSP_t::CBUSY_WIDTH);
#endif
            flit.DBID()             = walker.Walk(RSP_t::DBID_WIDTH);
#ifdef CHI_ISSUE_EB_ENABLE
        //  flit.PGroupID()
        //  flit.StashGroupID()
        //  flit.TagGroupID()
#endif
            flit.PCrdType()         = walker.Walk(RSP_t::PCRDTYPE_WIDTH);
#ifdef CHI_ISSUE_EB_ENABLE
            flit.TagOp()            = walker.Walk(RSP_t::TAGOP_WIDTH);
#endif
            flit.TraceTag()         = walker.Walk(RSP_t::TRACETAG_WIDTH);

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

            flit.QoS()              = walker.Walk(SNP_t::QOS_WIDTH);
            flit.SrcID()            = walker.Walk(SNP_t::SRCID_WIDTH);
            flit.TxnID()            = walker.Walk(SNP_t::TXNID_WIDTH);
            flit.FwdNID()           = walker.Walk(SNP_t::FWDNID_WIDTH);
            flit.FwdTxnID()         = walker.Walk(SNP_t::FWDTXNID_WIDTH);
        //  flit.StashLPID()
        //  flit.StashLPIDValid()
        //  flit.VMIDExt()
            flit.Opcode()           = walker.Walk(SNP_t::OPCODE_WIDTH);
            flit.Addr()             = walker.Walk(SNP_t::ADDR_WIDTH);
            flit.NS()               = walker.Walk(SNP_t::NS_WIDTH);
            flit.DoNotGoToSD()      = walker.Walk(SNP_t::DONOTGOTOSD_WIDTH);
#ifdef CHI_ISSUE_B_ENABLE
        //  flit.DoNotDataPull()
#endif
            flit.RetToSrc()         = walker.Walk(SNP_t::RETTOSRC_WIDTH);
            flit.TraceTag()         = walker.Walk(SNP_t::TRACETAG_WIDTH);

#ifdef CHI_ISSUE_EB_ENABLE
            if constexpr (SNP_t::hasMPAM)
            {
                flit.MPAM()         = walker.Walk(SNP_t::MPAM_WIDTH);
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

            flit.QoS()              = walker.Walk(DAT_t::QOS_WIDTH);
            flit.TgtID()            = walker.Walk(DAT_t::TGTID_WIDTH);
            flit.SrcID()            = walker.Walk(DAT_t::SRCID_WIDTH);
            flit.TxnID()            = walker.Walk(DAT_t::TXNID_WIDTH);
            flit.HomeNID()          = walker.Walk(DAT_t::HOMENID_WIDTH);
            flit.Opcode()           = walker.Walk(DAT_t::OPCODE_WIDTH);
            flit.RespErr()          = walker.Walk(DAT_t::RESPERR_WIDTH);
            flit.Resp()             = walker.Walk(DAT_t::RESP_WIDTH);
        //  flit.FwdState()
        //  flit.DataPull()
            flit.DataSource()       = walker.Walk(DAT_t::DATASOURCE_WIDTH);
#ifdef CHI_ISSUE_EB_ENABLE
            flit.CBusy()            = walker.Walk(DAT_t::CBUSY_WIDTH);
#endif
            flit.DBID()             = walker.Walk(DAT_t::DBID_WIDTH);
            flit.CCID()             = walker.Walk(DAT_t::CCID_WIDTH);
            flit.DataID()           = walker.Walk(DAT_t::DATAID_WIDTH);
#ifdef CHI_ISSUE_EB_ENABLE
            flit.TagOp()            = walker.Walk(DAT_t::TAGOP_WIDTH);
            flit.Tag()              = walker.Walk(DAT_t::TAG_WIDTH);
            flit.TU()               = walker.Walk(DAT_t::TU_WIDTH);
#endif
            flit.TraceTag()         = walker.Walk(DAT_t::TRACETAG_WIDTH);

            if constexpr (DAT_t::hasRSVDC)
            {
                flit.RSVDC()        = walker.Walk(DAT_t::RSVDC_WIDTH);
            }

            if constexpr (DAT_t::BE_WIDTH == 64)
            {
                flit.BE()           =  uint64_t(walker.Walk(32))
                                    | (uint64_t(walker.Walk(32)) << 32);
            }
            else
            {
                flit.BE()           = walker.Walk(DAT_t::BE_WIDTH);
            }

            if constexpr (DAT_t::DATA_WIDTH >= 128)
            {
                flit.Data()[0]      =  uint64_t(walker.Walk(32))
                                    | (uint64_t(walker.Walk(32)) << 32);
                
                flit.Data()[1]      =  uint64_t(walker.Walk(32))
                                    | (uint64_t(walker.Walk(32)) << 32);
            }

            if constexpr (DAT_t::DATA_WIDTH >= 256)
            {
                flit.Data()[2]      =  uint64_t(walker.Walk(32))
                                    | (uint64_t(walker.Walk(32)) << 32);
                
                flit.Data()[3]      =  uint64_t(walker.Walk(32))
                                    | (uint64_t(walker.Walk(32)) << 32);
            }

            if constexpr (DAT_t::DATA_WIDTH >= 512)
            {
                flit.Data()[4]      =  uint64_t(walker.Walk(32))
                                    | (uint64_t(walker.Walk(32)) << 32);
                
                flit.Data()[5]      =  uint64_t(walker.Walk(32))
                                    | (uint64_t(walker.Walk(32)) << 32);

                flit.Data()[6]      =  uint64_t(walker.Walk(32))
                                    | (uint64_t(walker.Walk(32)) << 32);
                
                flit.Data()[7]      =  uint64_t(walker.Walk(32))
                                    | (uint64_t(walker.Walk(32)) << 32);
            }

            if constexpr (DAT_t::hasDataCheck)
            {
                if constexpr (DAT_t::DATACHECK_WIDTH == 64)
                {
                    flit.DataCheck()    =  uint64_t(walker.Walk(32))
                                        | (uint64_t(walker.Walk(32)) << 32);
                }
                else
                {
                    flit.DataCheck()    = walker.Walk(DAT_t::DATACHECK_WIDTH);
                }
            }

            if constexpr (DAT_t::hasPoison)
            {
                flit.Poison() = walker.Walk(DAT_t::POISON_WIDTH);
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

//#endif // __CHI__CHI_UTIL_FLIT
