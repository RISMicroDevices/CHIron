//#pragma once

//#ifndef __CHI__CHI_UTIL_FLIT_BUILDER_RSP
//#define __CHI__CHI_UTIL_FLIT_BUILDER_RSP

#ifndef CHI_UTIL_FLIT_BUILDER_RSP__STANDALONE
#   define CHI_ISSUE_EB_ENABLE
#   include "chi_util_flit_builder_rsp_header.hpp"                      // IWYU pragma: keep
#endif

#include "chi_util_flit_builder/chi_util_flit_builder_common.hpp"       // IWYU pragma: keep
#include "chi_util_flit_builder/chi_util_flit_builder_rsp_details.hpp"  // IWYU pragma: keep


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_RSP_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_RSP_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_RSP_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_RSP_EB
#endif


/*
namespace CHI {
*/
    namespace Flits {

        // enumeration
        namespace RSPBuildTarget {
            inline constexpr RSPBuildTargetEnumBack RespLCrdReturn   ("RespLCrdReturn"  , Opcodes::RSP::RespLCrdReturn  , &Xact::ResponseFieldMappings::RspLCrdReturn   );
            inline constexpr RSPBuildTargetEnumBack SnpResp          ("SnpResp"         , Opcodes::RSP::SnpResp         , &Xact::ResponseFieldMappings::SnpResp         );
            inline constexpr RSPBuildTargetEnumBack CompAck          ("CompAck"         , Opcodes::RSP::CompAck         , &Xact::ResponseFieldMappings::CompAck         );
            inline constexpr RSPBuildTargetEnumBack RetryAck         ("RetryAck"        , Opcodes::RSP::RetryAck        , &Xact::ResponseFieldMappings::RetryAck        );
            inline constexpr RSPBuildTargetEnumBack Comp             ("Comp"            , Opcodes::RSP::Comp            , &Xact::ResponseFieldMappings::Comp            );
            inline constexpr RSPBuildTargetEnumBack CompDBIDResp     ("CompDBIDResp"    , Opcodes::RSP::CompDBIDResp    , &Xact::ResponseFieldMappings::CompDBIDResp    );
            inline constexpr RSPBuildTargetEnumBack DBIDResp         ("DBIDResp"        , Opcodes::RSP::DBIDResp        , &Xact::ResponseFieldMappings::DBIDResp        );
            inline constexpr RSPBuildTargetEnumBack PCrdGrant        ("PCrdGrant"       , Opcodes::RSP::PCrdGrant       , &Xact::ResponseFieldMappings::PCrdGrant       );
            inline constexpr RSPBuildTargetEnumBack ReadReceipt      ("ReadReceipt"     , Opcodes::RSP::ReadReceipt     , &Xact::ResponseFieldMappings::ReadReceipt     );
            inline constexpr RSPBuildTargetEnumBack SnpRespFwded     ("SnpRespFwded"    , Opcodes::RSP::SnpRespFwded    , &Xact::ResponseFieldMappings::SnpRespFwded    );
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr RSPBuildTargetEnumBack TagMatch         ("TagMatch"        , Opcodes::RSP::TagMatch        , &Xact::ResponseFieldMappings::TagMatch        );
            inline constexpr RSPBuildTargetEnumBack RespSepData      ("RespSepData"     , Opcodes::RSP::RespSepData     , &Xact::ResponseFieldMappings::RespSepData     );
            inline constexpr RSPBuildTargetEnumBack Persist          ("Persist"         , Opcodes::RSP::Persist         , &Xact::ResponseFieldMappings::Persist         );
            inline constexpr RSPBuildTargetEnumBack CompPersist      ("CompPersist"     , Opcodes::RSP::CompPersist     , &Xact::ResponseFieldMappings::CompPersist     );
            inline constexpr RSPBuildTargetEnumBack DBIDRespOrd      ("DBIDRespOrd"     , Opcodes::RSP::DBIDRespOrd     , &Xact::ResponseFieldMappings::DBIDRespOrd     );
            // 0x0F
            inline constexpr RSPBuildTargetEnumBack StashDone        ("StashDone"       , Opcodes::RSP::StashDone       , &Xact::ResponseFieldMappings::StashDone       );
            inline constexpr RSPBuildTargetEnumBack CompStashDone    ("CompStashDone"   , Opcodes::RSP::CompStashDone   , &Xact::ResponseFieldMappings::CompStashDone   );
            // 0x12 - 0x13
            inline constexpr RSPBuildTargetEnumBack CompCMO          ("CompCMO"         , Opcodes::RSP::CompCMO         , &Xact::ResponseFieldMappings::CompCMO         );
            // 0x15 - 0x1F
#endif
        }

        // RSP builders
        template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able = details::MakeRSPFlitBuildability<T, config>()>
        class RSPFlitBuilder
        {
            template<RSPBuildTargetEnum, RSPFlitConfigurationConcept, details::RSPFlitBuildability>
            friend class RSPFlitBuilder;

        protected:
            RSP<config>     flit    = { 0 };

        public:
            constexpr RSPFlitBuilder() noexcept;

            template<details::RSPFlitBuildability nextAble>
            constexpr RSPFlitBuilder(const RSPFlitBuilder<T, config, nextAble>& other) noexcept;

        public:
            constexpr RSP<config>::qos_t                QoS() const noexcept requires is_field_applicable<T->fields->QoS>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::QoS>()>
                                                        QoS(typename RSP<config>::qos_t qos) const noexcept requires is_field_applicable<T->fields->QoS>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::QoS>()>&
                                                        SetQoS(typename RSP<config>::qos_t qos) noexcept requires is_field_applicable<T->fields->QoS>;

            constexpr RSP<config>::tgtid_t              TgtID() const noexcept requires is_field_applicable<T->fields->TgtID>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TgtID>()>
                                                        TgtID(typename RSP<config>::tgtid_t tgtID) const noexcept requires is_field_applicable<T->fields->TgtID>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TgtID>()>&
                                                        SetTgtID(typename RSP<config>::tgtid_t tgtID) noexcept requires is_field_applicable<T->fields->TgtID>;

            constexpr RSP<config>::srcid_t              SrcID() const noexcept requires is_field_applicable<T->fields->SrcID>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::SrcID>()>
                                                        SrcID(typename RSP<config>::srcid_t srcID) const noexcept requires is_field_applicable<T->fields->SrcID>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::SrcID>()>&
                                                        SetSrcID(typename RSP<config>::srcid_t srcID) noexcept requires is_field_applicable<T->fields->SrcID>;

            constexpr RSP<config>::txnid_t              TxnID() const noexcept requires is_field_applicable<T->fields->TxnID>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TxnID>()>
                                                        TxnID(typename RSP<config>::txnid_t txnID) const noexcept requires is_field_applicable<T->fields->TxnID>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TxnID>()>&
                                                        SetTxnID(typename RSP<config>::txnid_t txnID) noexcept requires is_field_applicable<T->fields->TxnID>;
            
            constexpr RSP<config>::opcode_t             Opcode() const noexcept requires is_field_applicable<T->fields->Opcode>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::Opcode>()>
                                                        Opcode(typename RSP<config>::opcode_t opcode) const noexcept requires is_field_applicable<T->fields->Opcode>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::Opcode>()>&
                                                        SetOpcode(typename RSP<config>::opcode_t opcode) noexcept requires is_field_applicable<T->fields->Opcode>;

            constexpr RSP<config>::resperr_t            RespErr() const noexcept requires is_field_applicable<T->fields->RespErr>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::RespErr>()>
                                                        RespErr(typename RSP<config>::resperr_t respErr) const noexcept requires is_field_applicable<T->fields->RespErr>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::RespErr>()>&
                                                        SetRespErr(typename RSP<config>::resperr_t respErr) noexcept requires is_field_applicable<T->fields->RespErr>;

            constexpr RSP<config>::resp_t               Resp() const noexcept requires is_field_applicable<T->fields->Resp>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::Resp>()>
                                                        Resp(typename RSP<config>::resp_t resp) const noexcept requires is_field_applicable<T->fields->Resp>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::Resp>()>&
                                                        SetResp(typename RSP<config>::resp_t resp) noexcept requires is_field_applicable<T->fields->Resp>;
            
            constexpr RSP<config>::fwdstate_t           FwdState() const noexcept requires is_field_applicable<T->fields->FwdState>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::FwdState>()>
                                                        FwdState(typename RSP<config>::fwdstate_t fwdState) const noexcept requires is_field_applicable<T->fields->FwdState>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::FwdState>()>&
                                                        SetFwdState(typename RSP<config>::fwdstate_t fwdState) noexcept requires is_field_applicable<T->fields->FwdState>;

            constexpr RSP<config>::datapull_t           DataPull() const noexcept requires is_field_applicable<T->fields->DataPull>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::DataPull>()>
                                                        DataPull(typename RSP<config>::datapull_t dataPull) const noexcept requires is_field_applicable<T->fields->DataPull>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::DataPull>()>&
                                                        SetDataPull(typename RSP<config>::datapull_t dataPull) noexcept requires is_field_applicable<T->fields->DataPull>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr RSP<config>::cbusy_t              CBusy() const noexcept requires is_field_applicable<T->fields->CBusy>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::CBusy>()>
                                                        CBusy(typename RSP<config>::cbusy_t cBusy) const noexcept requires is_field_applicable<T->fields->CBusy>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::CBusy>()>&
                                                        SetCBusy(typename RSP<config>::cbusy_t cBusy) noexcept requires is_field_applicable<T->fields->CBusy>;
#endif

            constexpr RSP<config>::dbid_t               DBID() const noexcept requires is_field_applicable<T->fields->DBID>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::DBID>()>
                                                        DBID(typename RSP<config>::dbid_t dbid) const noexcept requires is_field_applicable<T->fields->DBID>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::DBID>()>&
                                                        SetDBID(typename RSP<config>::dbid_t dbid) noexcept requires is_field_applicable<T->fields->DBID>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr RSP<config>::pgroupid_t           PGroupID() const noexcept requires is_field_applicable<T->fields->PGroupID>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::PGroupID>()>
                                                        PGroupID(typename RSP<config>::pgroupid_t pGroupID) const noexcept requires is_field_applicable<T->fields->PGroupID>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::PGroupID>()>&
                                                        SetPGroupID(typename RSP<config>::pgroupid_t pGroupID) noexcept requires is_field_applicable<T->fields->PGroupID>;

            constexpr RSP<config>::stashgroupid_t       StashGroupID() const noexcept requires is_field_applicable<T->fields->StashGroupID>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::StashGroupID>()>
                                                        StashGroupID(typename RSP<config>::stashgroupid_t stashGroupID) const noexcept requires is_field_applicable<T->fields->StashGroupID>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::StashGroupID>()>&
                                                        SetStashGroupID(typename RSP<config>::stashgroupid_t stashGroupID) noexcept requires is_field_applicable<T->fields->StashGroupID>;

            constexpr RSP<config>::taggroupid_t         TagGroupID() const noexcept requires is_field_applicable<T->fields->TagGroupID>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TagGroupID>()>
                                                        TagGroupID(typename RSP<config>::taggroupid_t tagGroupID) const noexcept requires is_field_applicable<T->fields->TagGroupID>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TagGroupID>()>&
                                                        SetTagGroupID(typename RSP<config>::taggroupid_t tagGroupID) noexcept requires is_field_applicable<T->fields->TagGroupID>;
#endif

            constexpr RSP<config>::pcrdtype_t           PCrdType() const noexcept requires is_field_applicable<T->fields->PCrdType>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::PCrdType>()>
                                                        PCrdType(typename RSP<config>::pcrdtype_t pCrdType) const noexcept requires is_field_applicable<T->fields->PCrdType>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::PCrdType>()>&
                                                        SetPCrdType(typename RSP<config>::pcrdtype_t pCrdType) noexcept requires is_field_applicable<T->fields->PCrdType>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr RSP<config>::tagop_t              TagOp() const noexcept requires is_field_applicable<T->fields->TagOp>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TagOp>()>
                                                        TagOp(typename RSP<config>::tagop_t tagOp) const noexcept requires is_field_applicable<T->fields->TagOp>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TagOp>()>&
                                                        SetTagOp(typename RSP<config>::tagop_t tagOp) noexcept requires is_field_applicable<T->fields->TagOp>;
#endif

            constexpr RSP<config>::tracetag_t           TraceTag() const noexcept requires is_field_applicable<T->fields->TraceTag>;
            consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TraceTag>()>
                                                        TraceTag(typename RSP<config>::tracetag_t traceTag) const noexcept requires is_field_applicable<T->fields->TraceTag>;
            constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TraceTag>()>&
                                                        SetTraceTag(typename RSP<config>::tracetag_t traceTag) noexcept requires is_field_applicable<T->fields->TraceTag>;

            //
            consteval RSP<config>                       Eval() const noexcept requires details::is_buildable<able>;
            constexpr RSP<config>                       Build() const noexcept requires details::is_buildable<able>;

            consteval RSP<config>                       EvalUnsafe() const noexcept;
            constexpr RSP<config>                       BuildUnsafe() const noexcept;
        };
    }
/*
}
*/


// Implementation of: class RSPFlitBuilder
namespace /*CHI::*/Flits {

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, able>::RSPFlitBuilder() noexcept
    {
        RSP<config> flit = { 0 };

#define _INIT_IF(field) \
        if constexpr (HasDefaultFieldValue<T->fields->field>())
    
#define _FIELD_INIT(field) \
        _INIT_IF(field) flit.field() = GetDefaultFieldValue<T->fields->field>();

#define _FIELD_INIT_PCF(field, dst) \
        _INIT_IF(field) flit.dst() = PCF##dst##From##field <config>(flit.dst(), GetDefaultFieldValue<T->fields->field>());

#define _FIELD_INIT_OPT(field) \
        if constexpr (RSP<config>::has##field) \
            _INIT_IF(field) flit.field() = GetDefaultFieldValue<T->fields->field>();

        flit.Opcode() = T->opcode;

        _FIELD_INIT(QoS             );
        _FIELD_INIT(TgtID           );
        _FIELD_INIT(SrcID           );
        _FIELD_INIT(TxnID           );
        _FIELD_INIT(Opcode          );
        _FIELD_INIT(RespErr         );
        _FIELD_INIT(Resp            );
        _FIELD_INIT(FwdState        );
        _FIELD_INIT(DataPull        );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT(CBusy           );
#endif
        _FIELD_INIT(DBID            );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT_PCF(PGroupID        , DBID);
        _FIELD_INIT_PCF(StashGroupID    , DBID);
        _FIELD_INIT_PCF(TagGroupID      , DBID);
#endif
        _FIELD_INIT(PCrdType        );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT(TagOp           );
#endif
        _FIELD_INIT(TraceTag        );

#undef _INIT_IF
#undef _FIELD_INIT
#undef _FIELD_INIT_PCF
#undef _FIELD_INIT_OPT

        this->flit = flit;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    template<details::RSPFlitBuildability nextAble>
    inline constexpr RSPFlitBuilder<T, config, able>::RSPFlitBuilder(const RSPFlitBuilder<T, config, nextAble>& other) noexcept
    {
        this->flit = other.flit;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::qos_t RSPFlitBuilder<T, config, able>::QoS() const noexcept requires is_field_applicable<T->fields->QoS>
    {
        return flit.QoS();
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::QoS>()>
            RSPFlitBuilder<T, config, able>::QoS(typename RSP<config>::qos_t qos) const noexcept requires is_field_applicable<T->fields->QoS>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::QoS>()>(*this);
        builder.flit.QoS() = qos;
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::QoS>()>&
            RSPFlitBuilder<T, config, able>::SetQoS(typename RSP<config>::qos_t qos) noexcept requires is_field_applicable<T->fields->QoS>
    {
        this->flit.QoS() = qos;
        return *this;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::tgtid_t RSPFlitBuilder<T, config, able>::TgtID() const noexcept requires is_field_applicable<T->fields->TgtID>
    {
        return flit.TgtID();
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TgtID>()>
            RSPFlitBuilder<T, config, able>::TgtID(typename RSP<config>::tgtid_t tgtID) const noexcept requires is_field_applicable<T->fields->TgtID>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TgtID>()>(*this);
        builder.flit.TgtID() = tgtID;
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TgtID>()>&
            RSPFlitBuilder<T, config, able>::SetTgtID(typename RSP<config>::tgtid_t tgtID) noexcept requires is_field_applicable<T->fields->TgtID>
    {
        this->flit.TgtID() = tgtID;
        return *this;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::srcid_t RSPFlitBuilder<T, config, able>::SrcID() const noexcept requires is_field_applicable<T->fields->SrcID>
    {
        return flit.SrcID();
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::SrcID>()>
            RSPFlitBuilder<T, config, able>::SrcID(typename RSP<config>::srcid_t srcID) const noexcept requires is_field_applicable<T->fields->SrcID>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::SrcID>()>(*this);
        builder.flit.SrcID() = srcID;
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::SrcID>()>&
            RSPFlitBuilder<T, config, able>::SetSrcID(typename RSP<config>::srcid_t srcID) noexcept requires is_field_applicable<T->fields->SrcID>
    {
        this->flit.SrcID() = srcID;
        return *this;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::txnid_t RSPFlitBuilder<T, config, able>::TxnID() const noexcept requires is_field_applicable<T->fields->TxnID>
    {
        return flit.TxnID();
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TxnID>()>
            RSPFlitBuilder<T, config, able>::TxnID(typename RSP<config>::txnid_t txnID) const noexcept requires is_field_applicable<T->fields->TxnID>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TxnID>()>(*this);
        builder.flit.TxnID() = txnID;
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TxnID>()>&
            RSPFlitBuilder<T, config, able>::SetTxnID(typename RSP<config>::txnid_t txnID) noexcept requires is_field_applicable<T->fields->TxnID>
    {
        this->flit.TxnID() = txnID;
        return *this;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::opcode_t RSPFlitBuilder<T, config, able>::Opcode() const noexcept requires is_field_applicable<T->fields->Opcode>
    {
        return flit.Opcode();
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::Opcode>()>
            RSPFlitBuilder<T, config, able>::Opcode(typename RSP<config>::opcode_t opcode) const noexcept requires is_field_applicable<T->fields->Opcode>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::Opcode>()>(*this);
        builder.flit.Opcode() = opcode;
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::Opcode>()>&
            RSPFlitBuilder<T, config, able>::SetOpcode(typename RSP<config>::opcode_t opcode) noexcept requires is_field_applicable<T->fields->Opcode>
    {
        this->flit.Opcode() = opcode;
        return *this;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::resperr_t RSPFlitBuilder<T, config, able>::RespErr() const noexcept requires is_field_applicable<T->fields->RespErr>
    {
        return flit.RespErr();
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::RespErr>()>
            RSPFlitBuilder<T, config, able>::RespErr(typename RSP<config>::resperr_t respErr) const noexcept requires is_field_applicable<T->fields->RespErr>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::RespErr>()>(*this);
        builder.flit.RespErr() = respErr;
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::RespErr>()>&
            RSPFlitBuilder<T, config, able>::SetRespErr(typename RSP<config>::resperr_t respErr) noexcept requires is_field_applicable<T->fields->RespErr>
    {
        this->flit.RespErr() = respErr;
        return *this;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::resp_t RSPFlitBuilder<T, config, able>::Resp() const noexcept requires is_field_applicable<T->fields->Resp>
    {
        return flit.Resp();
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::Resp>()>
            RSPFlitBuilder<T, config, able>::Resp(typename RSP<config>::resp_t resp) const noexcept requires is_field_applicable<T->fields->Resp>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::Resp>()>(*this);
        builder.flit.Resp() = resp;
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::Resp>()>&
            RSPFlitBuilder<T, config, able>::SetResp(typename RSP<config>::resp_t resp) noexcept requires is_field_applicable<T->fields->Resp>
    {
        this->flit.Resp() = resp;
        return *this;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::fwdstate_t RSPFlitBuilder<T, config, able>::FwdState() const noexcept requires is_field_applicable<T->fields->FwdState>
    {
        return flit.FwdState();
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::FwdState>()>
            RSPFlitBuilder<T, config, able>::FwdState(typename RSP<config>::fwdstate_t fwdState) const noexcept requires is_field_applicable<T->fields->FwdState>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::FwdState>()>(*this);
        builder.flit.FwdState() = fwdState;
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::FwdState>()>&
            RSPFlitBuilder<T, config, able>::SetFwdState(typename RSP<config>::fwdstate_t fwdState) noexcept requires is_field_applicable<T->fields->FwdState>
    {
        this->flit.FwdState() = fwdState;
        return *this;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::datapull_t RSPFlitBuilder<T, config, able>::DataPull() const noexcept requires is_field_applicable<T->fields->DataPull>
    {
        return PCFFwdStateToDataPull<config>(flit.FwdState());
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::DataPull>()>
            RSPFlitBuilder<T, config, able>::DataPull(typename RSP<config>::datapull_t dataPull) const noexcept requires is_field_applicable<T->fields->DataPull>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::DataPull>()>(*this);
        builder.flit.FwdState() = PCFFwdStateFromDataPull<config>(flit.FwdState(), dataPull);
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::DataPull>()>&
            RSPFlitBuilder<T, config, able>::SetDataPull(typename RSP<config>::datapull_t dataPull) noexcept requires is_field_applicable<T->fields->DataPull>
    {
        this->flit.FwdState() = PCFFwdStateFromDataPull<config>(flit.FwdState(), dataPull);
        return *this;
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::cbusy_t RSPFlitBuilder<T, config, able>::CBusy() const noexcept requires is_field_applicable<T->fields->CBusy>
    {
        return flit.CBusy();
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::CBusy>()>
            RSPFlitBuilder<T, config, able>::CBusy(typename RSP<config>::cbusy_t cBusy) const noexcept requires is_field_applicable<T->fields->CBusy>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::CBusy>()>(*this);
        builder.flit.CBusy() = cBusy;
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::CBusy>()>&
            RSPFlitBuilder<T, config, able>::SetCBusy(typename RSP<config>::cbusy_t cBusy) noexcept requires is_field_applicable<T->fields->CBusy>
    {
        this->flit.CBusy() = cBusy;
        return *this;
    }
#endif

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::dbid_t RSPFlitBuilder<T, config, able>::DBID() const noexcept requires is_field_applicable<T->fields->DBID>
    {
        return flit.DBID();
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::DBID>()>
            RSPFlitBuilder<T, config, able>::DBID(typename RSP<config>::dbid_t dbid) const noexcept requires is_field_applicable<T->fields->DBID>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::DBID>()>(*this);
        builder.flit.DBID() = dbid;
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::DBID>()>&
            RSPFlitBuilder<T, config, able>::SetDBID(typename RSP<config>::dbid_t dbid) noexcept requires is_field_applicable<T->fields->DBID>
    {
        this->flit.DBID() = dbid;
        return *this;
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::pgroupid_t RSPFlitBuilder<T, config, able>::PGroupID() const noexcept requires is_field_applicable<T->fields->PGroupID>
    {
        return PCFDBIDToPGroupID<config>(flit.DBID());
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::PGroupID>()>
            RSPFlitBuilder<T, config, able>::PGroupID(typename RSP<config>::pgroupid_t pGroupID) const noexcept requires is_field_applicable<T->fields->PGroupID>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::PGroupID>()>(*this);
        builder.flit.DBID() = PCFDBIDFromPGroupID<config>(flit.DBID(), pGroupID);
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::PGroupID>()>&
            RSPFlitBuilder<T, config, able>::SetPGroupID(typename RSP<config>::pgroupid_t pGroupID) noexcept requires is_field_applicable<T->fields->PGroupID>
    {
        this->flit.DBID() = PCFDBIDFromPGroupID<config>(flit.DBID(), pGroupID);
        return *this;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::stashgroupid_t RSPFlitBuilder<T, config, able>::StashGroupID() const noexcept requires is_field_applicable<T->fields->StashGroupID>
    {
        return PCFDBIDToStashGroupID<config>(flit.DBID());
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::StashGroupID>()>
            RSPFlitBuilder<T, config, able>::StashGroupID(typename RSP<config>::stashgroupid_t stashGroupID) const noexcept requires is_field_applicable<T->fields->StashGroupID>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::StashGroupID>()>(*this);
        builder.flit.DBID() = PCFDBIDFromStashGroupID<config>(flit.DBID(), stashGroupID);
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::StashGroupID>()>&
            RSPFlitBuilder<T, config, able>::SetStashGroupID(typename RSP<config>::stashgroupid_t stashGroupID) noexcept requires is_field_applicable<T->fields->StashGroupID>
    {
        this->flit.DBID() = PCFDBIDFromStashGroupID<config>(flit.DBID(), stashGroupID);
        return *this;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::taggroupid_t RSPFlitBuilder<T, config, able>::TagGroupID() const noexcept requires is_field_applicable<T->fields->TagGroupID>
    {
        return PCFDBIDToTagGroupID<config>(flit.DBID());
    }
    
    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TagGroupID>()>
            RSPFlitBuilder<T, config, able>::TagGroupID(typename RSP<config>::taggroupid_t tagGroupID) const noexcept requires is_field_applicable<T->fields->TagGroupID>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TagGroupID>()>(*this);
        builder.flit.DBID() = PCFDBIDFromTagGroupID<config>(flit.DBID(), tagGroupID);
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TagGroupID>()>&
            RSPFlitBuilder<T, config, able>::SetTagGroupID(typename RSP<config>::taggroupid_t tagGroupID) noexcept requires is_field_applicable<T->fields->TagGroupID>
    {
        this->flit.DBID() = PCFDBIDFromTagGroupID<config>(flit.DBID(), tagGroupID);
        return *this;
    }
#endif

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::pcrdtype_t RSPFlitBuilder<T, config, able>::PCrdType() const noexcept requires is_field_applicable<T->fields->PCrdType>
    {
        return flit.PCrdType();
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::PCrdType>()>
            RSPFlitBuilder<T, config, able>::PCrdType(typename RSP<config>::pcrdtype_t pCrdType) const noexcept requires is_field_applicable<T->fields->PCrdType>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::PCrdType>()>(*this);
        builder.flit.PCrdType() = pCrdType;
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::PCrdType>()>&
            RSPFlitBuilder<T, config, able>::SetPCrdType(typename RSP<config>::pcrdtype_t pCrdType) noexcept requires is_field_applicable<T->fields->PCrdType>
    {
        this->flit.PCrdType() = pCrdType;
        return *this;
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::tagop_t RSPFlitBuilder<T, config, able>::TagOp() const noexcept requires is_field_applicable<T->fields->TagOp>
    {
        return flit.TagOp();
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TagOp>()>
            RSPFlitBuilder<T, config, able>::TagOp(typename RSP<config>::tagop_t tagOp) const noexcept requires is_field_applicable<T->fields->TagOp>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TagOp>()>(*this);
        builder.flit.TagOp() = tagOp;
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TagOp>()>&
            RSPFlitBuilder<T, config, able>::SetTagOp(typename RSP<config>::tagop_t tagOp) noexcept requires is_field_applicable<T->fields->TagOp>
    {
        this->flit.TagOp() = tagOp;
        return *this;
    }
#endif

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config>::tracetag_t RSPFlitBuilder<T, config, able>::TraceTag() const noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        return flit.TraceTag();
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TraceTag>()>
            RSPFlitBuilder<T, config, able>::TraceTag(typename RSP<config>::tracetag_t traceTag) const noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        auto builder = RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TraceTag>()>(*this);
        builder.flit.TraceTag() = traceTag;
        return builder;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSPFlitBuilder<T, config, details::NextRSPFlitBuildability<T, able, details::RSPFlitField::TraceTag>()>&
            RSPFlitBuilder<T, config, able>::SetTraceTag(typename RSP<config>::tracetag_t traceTag) noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        this->flit.TraceTag() = traceTag;
        return *this;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSP<config> RSPFlitBuilder<T, config, able>::Eval() const noexcept requires details::is_buildable<able>
    {
        return flit;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config> RSPFlitBuilder<T, config, able>::Build() const noexcept requires details::is_buildable<able>
    {
        return flit;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline consteval RSP<config> RSPFlitBuilder<T, config, able>::EvalUnsafe() const noexcept
    {
        return flit;
    }

    template<RSPBuildTargetEnum T, RSPFlitConfigurationConcept config, details::RSPFlitBuildability able>
    inline constexpr RSP<config> RSPFlitBuilder<T, config, able>::BuildUnsafe() const noexcept
    {
        return flit;
    }
}


#endif // __CHI__CHI_UTIL_FLIT_BUILDER_RSP_*

//#endif // __CHI__CHI_UTIL_FLIT_BUILDER_RSP
