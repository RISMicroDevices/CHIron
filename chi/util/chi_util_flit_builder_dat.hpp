//#pragma once

//#ifndef __CHI__CHI_UTIL_FLIT_BUILDER_DAT
//#define __CHI__CHI_UTIL_FLIT_BUILDER_DAT

#ifndef CHI_UTIL_FLIT_BUILDER_DAT__STANDALONE
#   define CHI_ISSUE_EB_ENABLE
#   include "chi_util_flit_builder_dat_header.hpp"                      // IWYU pragma: keep
#endif

#include "chi_util_flit_builder/chi_util_flit_builder_common.hpp"       // IWYU pragma: keep
#include "chi_util_flit_builder/chi_util_flit_builder_dat_details.hpp"  // IWYU pragma: keep


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_DAT_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_DAT_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_DAT_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_DAT_EB
#endif


/*
namespace CHI {
*/
    namespace Flits {

        // enumerations
        namespace DATBuildTarget {
            inline constexpr DATBuildTargetEnumBack DataLCrdReturn      ("DataLCrdReturn"   , Opcodes::DAT::DataLCrdReturn      , &Xact::DataFieldMappings::DatLCrdReturn       );
            inline constexpr DATBuildTargetEnumBack SnpRespData         ("SnpRespData"      , Opcodes::DAT::SnpRespData         , &Xact::DataFieldMappings::SnpRespData         );
            inline constexpr DATBuildTargetEnumBack CopyBackWrData      ("CopyBackWrData"   , Opcodes::DAT::CopyBackWrData      , &Xact::DataFieldMappings::CopyBackWrData      );
            inline constexpr DATBuildTargetEnumBack NonCopyBackWrData   ("NonCopyBackWrData", Opcodes::DAT::NonCopyBackWrData   , &Xact::DataFieldMappings::NonCopyBackWrData   );
            inline constexpr DATBuildTargetEnumBack CompData            ("CompData"         , Opcodes::DAT::CompData            , &Xact::DataFieldMappings::CompData            );
            inline constexpr DATBuildTargetEnumBack SnpRespDataPtl      ("SnpRespDataPtl"   , Opcodes::DAT::SnpRespDataPtl      , &Xact::DataFieldMappings::SnpRespDataPtl      );
            inline constexpr DATBuildTargetEnumBack SnpRespDataFwded    ("SnpRespDataFwded" , Opcodes::DAT::SnpRespDataFwded    , &Xact::DataFieldMappings::SnpRespDataFwded    );
            inline constexpr DATBuildTargetEnumBack WriteDataCancel     ("WriteDataCancel"  , Opcodes::DAT::WriteDataCancel     , &Xact::DataFieldMappings::WriteDataCancel     );
#ifdef CHI_ISSUE_EB_ENABLE
            // 0x08 - 0x0A
            inline constexpr DATBuildTargetEnumBack DataSepResp         ("DataSepResp"      , Opcodes::DAT::DataSepResp         , &Xact::DataFieldMappings::DataSepResp         );
            inline constexpr DATBuildTargetEnumBack NCBWrDataCompAck    ("NCBWrDataCompAck" , Opcodes::DAT::NCBWrDataCompAck    , &Xact::DataFieldMappings::NCBWrDataCompAck    );
            // 0x0D - 0x0F
#endif
        }

        // DAT builders
        template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able = details::MakeDATFlitBuildability<T, config>()>
        class DATFlitBuilder
        {
            template<DATBuildTargetEnum, REQFlitConfigurationConcept, details::DATFlitBuildability>
            friend class DATFlitBuilder;

        protected:
            DAT<config>     flit    = { 0 };

        public:
            constexpr DATFlitBuilder() noexcept;

            template<details::DATFlitBuildability nextAble>
            constexpr DATFlitBuilder(const DATFlitBuilder<T, config, nextAble>&) noexcept;

        public:
            constexpr DAT<config>::qos_t                QoS() noexcept requires is_field_applicable<T->fields->QoS>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::QoS>()>
                                                        QoS(typename DAT<config>::qos_t qos) const noexcept requires is_field_applicable<T->fields->QoS>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::QoS>()>&
                                                        SetQoS(typename DAT<config>::qos_t qos) noexcept requires is_field_applicable<T->fields->QoS>;

            constexpr DAT<config>::tgtid_t              TgtID() noexcept requires is_field_applicable<T->fields->TgtID>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TgtID>()>
                                                        TgtID(typename DAT<config>::tgtid_t tgtID) const noexcept requires is_field_applicable<T->fields->TgtID>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TgtID>()>&
                                                        SetTgtID(typename DAT<config>::tgtid_t tgtID) noexcept requires is_field_applicable<T->fields->TgtID>;

            constexpr DAT<config>::srcid_t              SrcID() noexcept requires is_field_applicable<T->fields->SrcID>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::SrcID>()>
                                                        SrcID(typename DAT<config>::srcid_t srcID) const noexcept requires is_field_applicable<T->fields->SrcID>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::SrcID>()>&
                                                        SetSrcID(typename DAT<config>::srcid_t srcID) noexcept requires is_field_applicable<T->fields->SrcID>;

            constexpr DAT<config>::txnID_t              TxnID() noexcept requires is_field_applicable<T->fields->TxnID>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TxnID>()>
                                                        TxnID(typename DAT<config>::txnID_t txnID) const noexcept requires is_field_applicable<T->fields->TxnID>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TxnID>()>&
                                                        SetTxnID(typename DAT<config>::txnID_t txnID) noexcept requires is_field_applicable<T->fields->TxnID>;

            constexpr DAT<config>::homenid_t            HomeNID() noexcept requires is_field_applicable<T->fields->HomeNID>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::HomeNID>()>
                                                        HomeNID(typename DAT<config>::homenid_t homeNID) const noexcept requires is_field_applicable<T->fields->HomeNID>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::HomeNID>()>&
                                                        SetHomeNID(typename DAT<config>::homenid_t homeNID) noexcept requires is_field_applicable<T->fields->HomeNID>;

            constexpr DAT<config>::opcode_t             Opcode() noexcept requires is_field_applicable<T->fields->Opcode>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Opcode>()>
                                                        Opcode(typename DAT<config>::opcode_t opcode) const noexcept requires is_field_applicable<T->fields->Opcode>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Opcode>()>&
                                                        SetOpcode(typename DAT<config>::opcode_t opcode) noexcept requires is_field_applicable<T->fields->Opcode>;

            constexpr DAT<config>::resperr_t            RespErr() noexcept requires is_field_applicable<T->fields->RespErr>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::RespErr>()>
                                                        RespErr(typename DAT<config>::resperr_t respErr) const noexcept requires is_field_applicable<T->fields->RespErr>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::RespErr>()>&
                                                        SetRespErr(typename DAT<config>::resperr_t respErr) noexcept requires is_field_applicable<T->fields->RespErr>;

            constexpr DAT<config>::resp_t               Resp() noexcept requires is_field_applicable<T->fields->Resp>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Resp>()>
                                                        Resp(typename DAT<config>::resp_t resp) const noexcept requires is_field_applicable<T->fields->Resp>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Resp>()>&
                                                        SetResp(typename DAT<config>::resp_t resp) noexcept requires is_field_applicable<T->fields->Resp>;

            constexpr DAT<config>::fwdstate_t           FwdState() noexcept requires is_field_applicable<T->fields->FwdState>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::FwdState>()>
                                                        FwdState(typename DAT<config>::fwdstate_t fwdState) const noexcept requires is_field_applicable<T->fields->FwdState>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::FwdState>()>&
                                                        SetFwdState(typename DAT<config>::fwdstate_t fwdState) noexcept requires is_field_applicable<T->fields->FwdState>;

            constexpr DAT<config>::datapull_t           DataPull() noexcept requires is_field_applicable<T->fields->DataPull>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataPull>()>
                                                        DataPull(typename DAT<config>::datapull_t dataPull) const noexcept requires is_field_applicable<T->fields->DataPull>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataPull>()>&
                                                        SetDataPull(typename DAT<config>::datapull_t dataPull) noexcept requires is_field_applicable<T->fields->DataPull>;

            constexpr DAT<config>::datasource_t         DataSource() noexcept requires is_field_applicable<T->fields->DataSource>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataSource>()>
                                                        DataSource(typename DAT<config>::datasource_t dataSource) const noexcept requires is_field_applicable<T->fields->DataSource>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataSource>()>&
                                                        SetDataSource(typename DAT<config>::datasource_t dataSource) noexcept requires is_field_applicable<T->fields->DataSource>;
        
#ifdef CHI_ISSUE_EB_ENABLE
            constexpr DAT<config>::cbusy_t              CBusy() noexcept requires is_field_applicable<T->fields->CBusy>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::CBusy>()>
                                                        CBusy(typename DAT<config>::cbusy_t cbusy) const noexcept requires is_field_applicable<T->fields->CBusy>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::CBusy>()>&
                                                        SetCBusy(typename DAT<config>::cbusy_t cbusy) noexcept requires is_field_applicable<T->fields->CBusy>;
#endif

            constexpr DAT<config>::dbid_t               DBID() noexcept requires is_field_applicable<T->fields->DBID>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DBID>()>
                                                        DBID(typename DAT<config>::dbid_t dbid) const noexcept requires is_field_applicable<T->fields->DBID>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DBID>()>&
                                                        SetDBID(typename DAT<config>::dbid_t dbid) noexcept requires is_field_applicable<T->fields->DBID>;

            constexpr DAT<config>::ccid_t               CCID() noexcept requires is_field_applicable<T->fields->CCID>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::CCID>()>
                                                        CCID(typename DAT<config>::ccid_t ccid) const noexcept requires is_field_applicable<T->fields->CCID>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::CCID>()>&
                                                        SetCCID(typename DAT<config>::ccid_t ccid) noexcept requires is_field_applicable<T->fields->CCID>;

            constexpr DAT<config>::dataid_t             DataID() noexcept requires is_field_applicable<T->fields->DataID>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataID>()>
                                                        DataID(typename DAT<config>::dataid_t dataID) const noexcept requires is_field_applicable<T->fields->DataID>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataID>()>&
                                                        SetDataID(typename DAT<config>::dataid_t dataID) noexcept requires is_field_applicable<T->fields->DataID>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr DAT<config>::tagop_t              TagOp() noexcept requires is_field_applicable<T->fields->TagOp>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TagOp>()>
                                                        TagOp(typename DAT<config>::tagop_t tagOp) const noexcept requires is_field_applicable<T->fields->TagOp>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TagOp>()>&
                                                        SetTagOp(typename DAT<config>::tagop_t tagOp) noexcept requires is_field_applicable<T->fields->TagOp>;

            constexpr DAT<config>::tag_t                Tag() noexcept requires is_field_applicable<T->fields->Tag>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Tag>()>
                                                        Tag(typename DAT<config>::tag_t tag) const noexcept requires is_field_applicable<T->fields->Tag>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Tag>()>&
                                                        SetTag(typename DAT<config>::tag_t tag) noexcept requires is_field_applicable<T->fields->Tag>;

            constexpr DAT<config>::tu_t                 TU() noexcept requires is_field_applicable<T->fields->TU>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TU>()>
                                                        TU(typename DAT<config>::tu_t tu) const noexcept requires is_field_applicable<T->fields->TU>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TU>()>&
                                                        SetTU(typename DAT<config>::tu_t tu) noexcept requires is_field_applicable<T->fields->TU>;
#endif

            constexpr DAT<config>::tracetag_t           TraceTag() noexcept requires is_field_applicable<T->fields->TraceTag>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TraceTag>()>
                                                        TraceTag(typename DAT<config>::tracetag_t traceTag) const noexcept requires is_field_applicable<T->fields->TraceTag>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TraceTag>()>&
                                                        SetTraceTag(typename DAT<config>::tracetag_t traceTag) noexcept requires is_field_applicable<T->fields->TraceTag>;

            constexpr DAT<config>::rsvdc_t              RSVDC() noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<DAT<config>>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::RSVDC>()>
                                                        RSVDC(typename DAT<config>::rsvdc_t rsvdc) const noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<DAT<config>>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::RSVDC>()>&
                                                        SetRSVDC(typename DAT<config>::rsvdc_t rsvdc) noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<DAT<config>>;

            constexpr DAT<config>::be_t                 BE() noexcept requires is_field_applicable<T->fields->BE>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::BE>()>
                                                        BE(typename DAT<config>::be_t be) const noexcept requires is_field_applicable<T->fields->BE>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::BE>()>&
                                                        SetBE(typename DAT<config>::be_t be) noexcept requires is_field_applicable<T->fields->BE>;

            constexpr DAT<config>::data_t               Data() noexcept requires is_field_applicable<T->fields->Data>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Data>()>
                                                        Data(typename DAT<config>::data_t data) const noexcept requires is_field_applicable<T->fields->Data>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Data>()>&
                                                        SetData(typename DAT<config>::data_t data) noexcept requires is_field_applicable<T->fields->Data>;

            constexpr DAT<config>::datacheck_t          DataCheck() noexcept requires is_field_applicable<T->fields->DataCheck> && has_datacheck<DAT<config>>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataCheck>()>
                                                        DataCheck(typename DAT<config>::datacheck_t dataCheck) const noexcept requires is_field_applicable<T->fields->DataCheck> && has_datacheck<DAT<config>>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataCheck>()>&
                                                        SetDataCheck(typename DAT<config>::datacheck_t dataCheck) noexcept requires is_field_applicable<T->fields->DataCheck> && has_datacheck<DAT<config>>;

            constexpr DAT<config>::poison_t             Poison() noexcept requires is_field_applicable<T->fields->Poison> && has_poison<DAT<config>>;
            consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Poison>()>
                                                        Poison(typename DAT<config>::poison_t poison) const noexcept requires is_field_applicable<T->fields->Poison> && has_poison<DAT<config>>;
            constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Poison>()>&
                                                        SetPoison(typename DAT<config>::poison_t poison) noexcept requires is_field_applicable<T->fields->Poison> && has_poison<DAT<config>>;

            //
            consteval DAT<config>                       Eval() const noexcept requires details::is_buildable<able>;
            constexpr DAT<config>                       Build() const noexcept requires details::is_buildable<able>;

            consteval DAT<config>                       EvalUnsafe() const noexcept;
            constexpr DAT<config>                       BuildUnsafe() const noexcept;

            constexpr DATFlitBuilder<T, config, details::always_buildable_dat>
                                                        Unsafe() const noexcept;
        };

        template<DATBuildTargetEnum T, DATFlitConfigurationConcept config>
        consteval DATFlitBuilder<T, config, details::MakeDATFlitBuildability<T, config>()> EvalDATFlitBuilder() noexcept;

        template<DATBuildTargetEnum T, DATFlitConfigurationConcept config>
        constexpr DATFlitBuilder<T, config, details::MakeDATFlitBuildability<T, config>()> BuildDATFlitBuilder() noexcept;
    }
/*
}
*/


// Implementation of: class DATFlitBuilder
namespace /*CHI::*/Flits {

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, able>::DATFlitBuilder() noexcept
    {
        DAT<config> flit = { 0 };

#define _INIT_IF(field) \
        if constexpr (HasDefaultFieldValue<T->fields->field>())

#define _FIELD_INIT(field) \
        _INIT_IF(field) flit.field() = GetDefaultFieldValue<T->fields->field>();

#define _FIELD_INIT_PCF(field, dst) \
        _INIT_IF(field) flit.dst() = PCF##dst##From##field <config>(flit.dst(), GetDefaultFieldValue<T->fields->field>());

#define _FIELD_INIT_OPT(field) \
        if constexpr (REQ<config>::has##field) \
            _INIT_IF(field) flit.field() = GetDefaultFieldValue<T->fields->field>();

        flit.Opcode() = T->opcode;

        _FIELD_INIT(QoS             );
        _FIELD_INIT(TgtID           );
        _FIELD_INIT(SrcID           );
        _FIELD_INIT(TxnID           );
        _FIELD_INIT(HomeNID         );
        _FIELD_INIT(Opcode          );
        _FIELD_INIT(RespErr         );
        _FIELD_INIT(Resp            );
        _FIELD_INIT(DataSource      );
        _FIELD_INIT_PCF(FwdState        , DataSource);
        _FIELD_INIT_PCF(DataPull        , DataSource);
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT(CBusy           );
#endif
        _FIELD_INIT(DBID            );
        _FIELD_INIT(CCID            );
        _FIELD_INIT(DataID          );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT(TagOp           );
        _FIELD_INIT(Tag             );
        _FIELD_INIT(TU              );
#endif
        _FIELD_INIT(TraceTag        );
        _FIELD_INIT_OPT(RSVDC           );
        _FIELD_INIT(BE              );
        _FIELD_INIT(Data            );
        _FIELD_INIT_OPT(DataCheck       );
        _FIELD_INIT_OPT(Poison           );

#undef _INIT_IF
#undef _FIELD_INIT
#undef _FIELD_INIT_PCF
#undef _FIELD_INIT_OPT

        this->flit = flit;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    template<details::DATFlitBuildability nextAble>
    inline constexpr DATFlitBuilder<T, config, able>::DATFlitBuilder(const DATFlitBuilder<T, config, nextAble>& other) noexcept
    {
        this->flit = other.flit;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::qos_t DATFlitBuilder<T, config, able>::QoS() noexcept requires is_field_applicable<T->fields->QoS>
    {
        return this->flit.QoS();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::QoS>()>
            DATFlitBuilder<T, config, able>::QoS(typename DAT<config>::qos_t qos) const noexcept requires is_field_applicable<T->fields->QoS>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::QoS>()> nextBuilder(*this);
        nextBuilder.flit.QoS() = qos;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::QoS>()>&
            DATFlitBuilder<T, config, able>::SetQoS(typename DAT<config>::qos_t qos) noexcept requires is_field_applicable<T->fields->QoS>
    {
        this->flit.QoS() = qos;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::tgtid_t DATFlitBuilder<T, config, able>::TgtID() noexcept requires is_field_applicable<T->fields->TgtID>
    {
        return this->flit.TgtID();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TgtID>()>
            DATFlitBuilder<T, config, able>::TgtID(typename DAT<config>::tgtid_t tgtID) const noexcept requires is_field_applicable<T->fields->TgtID>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TgtID>()> nextBuilder(*this);
        nextBuilder.flit.TgtID() = tgtID;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TgtID>()>&
            DATFlitBuilder<T, config, able>::SetTgtID(typename DAT<config>::tgtid_t tgtID) noexcept requires is_field_applicable<T->fields->TgtID>
    {
        this->flit.TgtID() = tgtID;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::srcid_t DATFlitBuilder<T, config, able>::SrcID() noexcept requires is_field_applicable<T->fields->SrcID>
    {
        return this->flit.SrcID();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::SrcID>()>
            DATFlitBuilder<T, config, able>::SrcID(typename DAT<config>::srcid_t srcID) const noexcept requires is_field_applicable<T->fields->SrcID>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::SrcID>()> nextBuilder(*this);
        nextBuilder.flit.SrcID() = srcID;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::SrcID>()>&
            DATFlitBuilder<T, config, able>::SetSrcID(typename DAT<config>::srcid_t srcID) noexcept requires is_field_applicable<T->fields->SrcID>
    {
        this->flit.SrcID() = srcID;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::txnID_t DATFlitBuilder<T, config, able>::TxnID() noexcept requires is_field_applicable<T->fields->TxnID>
    {
        return this->flit.TxnID();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TxnID>()>
            DATFlitBuilder<T, config, able>::TxnID(typename DAT<config>::txnID_t txnID) const noexcept requires is_field_applicable<T->fields->TxnID>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TxnID>()> nextBuilder(*this);
        nextBuilder.flit.TxnID() = txnID;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TxnID>()>&
            DATFlitBuilder<T, config, able>::SetTxnID(typename DAT<config>::txnID_t txnID) noexcept requires is_field_applicable<T->fields->TxnID>
    {
        this->flit.TxnID() = txnID;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::homenid_t DATFlitBuilder<T, config, able>::HomeNID() noexcept requires is_field_applicable<T->fields->HomeNID>
    {
        return this->flit.HomeNID();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::HomeNID>()>
            DATFlitBuilder<T, config, able>::HomeNID(typename DAT<config>::homenid_t homeNID) const noexcept requires is_field_applicable<T->fields->HomeNID>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::HomeNID>()> nextBuilder(*this);
        nextBuilder.flit.HomeNID() = homeNID;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::HomeNID>()>&
            DATFlitBuilder<T, config, able>::SetHomeNID(typename DAT<config>::homenid_t homeNID) noexcept requires is_field_applicable<T->fields->HomeNID>
    {
        this->flit.HomeNID() = homeNID;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::opcode_t DATFlitBuilder<T, config, able>::Opcode() noexcept requires is_field_applicable<T->fields->Opcode>
    {
        return this->flit.Opcode();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Opcode>()>
            DATFlitBuilder<T, config, able>::Opcode(typename DAT<config>::opcode_t opcode) const noexcept requires is_field_applicable<T->fields->Opcode>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Opcode>()> nextBuilder(*this);
        nextBuilder.flit.Opcode() = opcode;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Opcode>()>&
            DATFlitBuilder<T, config, able>::SetOpcode(typename DAT<config>::opcode_t opcode) noexcept requires is_field_applicable<T->fields->Opcode>
    {
        this->flit.Opcode() = opcode;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::resperr_t DATFlitBuilder<T, config, able>::RespErr() noexcept requires is_field_applicable<T->fields->RespErr>
    {
        return this->flit.RespErr();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::RespErr>()>
            DATFlitBuilder<T, config, able>::RespErr(typename DAT<config>::resperr_t respErr) const noexcept requires is_field_applicable<T->fields->RespErr>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::RespErr>()> nextBuilder(*this);
        nextBuilder.flit.RespErr() = respErr;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::RespErr>()>&
            DATFlitBuilder<T, config, able>::SetRespErr(typename DAT<config>::resperr_t respErr) noexcept requires is_field_applicable<T->fields->RespErr>
    {
        this->flit.RespErr() = respErr;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::resp_t DATFlitBuilder<T, config, able>::Resp() noexcept requires is_field_applicable<T->fields->Resp>
    {
        return this->flit.Resp();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Resp>()>
            DATFlitBuilder<T, config, able>::Resp(typename DAT<config>::resp_t resp) const noexcept requires is_field_applicable<T->fields->Resp>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Resp>()> nextBuilder(*this);
        nextBuilder.flit.Resp() = resp;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Resp>()>&
            DATFlitBuilder<T, config, able>::SetResp(typename DAT<config>::resp_t resp) noexcept requires is_field_applicable<T->fields->Resp>
    {
        this->flit.Resp() = resp;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::datasource_t DATFlitBuilder<T, config, able>::DataSource() noexcept requires is_field_applicable<T->fields->DataSource>
    {
        return this->flit.DataSource();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataSource>()>
            DATFlitBuilder<T, config, able>::DataSource(typename DAT<config>::datasource_t dataSource) const noexcept requires is_field_applicable<T->fields->DataSource>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataSource>()> nextBuilder(*this);
        nextBuilder.flit.DataSource() = dataSource;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataSource>()>&
            DATFlitBuilder<T, config, able>::SetDataSource(typename DAT<config>::datasource_t dataSource) noexcept requires is_field_applicable<T->fields->DataSource>
    {
        this->flit.DataSource() = dataSource;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::fwdstate_t DATFlitBuilder<T, config, able>::FwdState() noexcept requires is_field_applicable<T->fields->FwdState>
    {
        return details::PCFDataSourceToFwdState<config>(this->flit.DataSource());
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::FwdState>()>
            DATFlitBuilder<T, config, able>::FwdState(typename DAT<config>::fwdstate_t fwdState) const noexcept requires is_field_applicable<T->fields->FwdState>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::FwdState>()> nextBuilder(*this);
        nextBuilder.flit.DataSource() = details::PCFDataSourceFromFwdState<config>(fwdState);
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::FwdState>()>&
            DATFlitBuilder<T, config, able>::SetFwdState(typename DAT<config>::fwdstate_t fwdState) noexcept requires is_field_applicable<T->fields->FwdState>
    {
        this->flit.DataSource() = details::PCFDataSourceFromFwdState<config>(fwdState);
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::datapull_t DATFlitBuilder<T, config, able>::DataPull() noexcept requires is_field_applicable<T->fields->DataPull>
    {
        return details::PCFDataSourceToDataPull<config>(this->flit.DataSource());
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataPull>()>
            DATFlitBuilder<T, config, able>::DataPull(typename DAT<config>::datapull_t dataPull) const noexcept requires is_field_applicable<T->fields->DataPull>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataPull>()> nextBuilder(*this);
        nextBuilder.flit.DataSource() = details::PCFDataSourceFromDataPull<config>(dataPull);
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataPull>()>&
            DATFlitBuilder<T, config, able>::SetDataPull(typename DAT<config>::datapull_t dataPull) noexcept requires is_field_applicable<T->fields->DataPull>
    {
        this->flit.DataSource() = details::PCFDataSourceFromDataPull<config>(dataPull);
        return *this;
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::cbusy_t DATFlitBuilder<T, config, able>::CBusy() noexcept requires is_field_applicable<T->fields->CBusy>
    {
        return this->flit.CBusy();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::CBusy>()>
            DATFlitBuilder<T, config, able>::CBusy(typename DAT<config>::cbusy_t cbusy) const noexcept requires is_field_applicable<T->fields->CBusy>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::CBusy>()> nextBuilder(*this);
        nextBuilder.flit.CBusy() = cbusy;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::CBusy>()>&
            DATFlitBuilder<T, config, able>::SetCBusy(typename DAT<config>::cbusy_t cbusy) noexcept requires is_field_applicable<T->fields->CBusy>
    {
        this->flit.CBusy() = cbusy;
        return *this;
    }
#endif

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::dbid_t DATFlitBuilder<T, config, able>::DBID() noexcept requires is_field_applicable<T->fields->DBID>
    {
        return this->flit.DBID();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DBID>()>
            DATFlitBuilder<T, config, able>::DBID(typename DAT<config>::dbid_t dbid) const noexcept requires is_field_applicable<T->fields->DBID>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DBID>()> nextBuilder(*this);
        nextBuilder.flit.DBID() = dbid;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DBID>()>&
            DATFlitBuilder<T, config, able>::SetDBID(typename DAT<config>::dbid_t dbid) noexcept requires is_field_applicable<T->fields->DBID>
    {
        this->flit.DBID() = dbid;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::ccid_t DATFlitBuilder<T, config, able>::CCID() noexcept requires is_field_applicable<T->fields->CCID>
    {
        return this->flit.CCID();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::CCID>()>
            DATFlitBuilder<T, config, able>::CCID(typename DAT<config>::ccid_t ccid) const noexcept requires is_field_applicable<T->fields->CCID>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::CCID>()> nextBuilder(*this);
        nextBuilder.flit.CCID() = ccid;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::CCID>()>&
            DATFlitBuilder<T, config, able>::SetCCID(typename DAT<config>::ccid_t ccid) noexcept requires is_field_applicable<T->fields->CCID>
    {
        this->flit.CCID() = ccid;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::dataid_t DATFlitBuilder<T, config, able>::DataID() noexcept requires is_field_applicable<T->fields->DataID>
    {
        return this->flit.DataID();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataID>()>
            DATFlitBuilder<T, config, able>::DataID(typename DAT<config>::dataid_t dataID) const noexcept requires is_field_applicable<T->fields->DataID>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataID>()> nextBuilder(*this);
        nextBuilder.flit.DataID() = dataID;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataID>()>&
            DATFlitBuilder<T, config, able>::SetDataID(typename DAT<config>::dataid_t dataID) noexcept requires is_field_applicable<T->fields->DataID>
    {
        this->flit.DataID() = dataID;
        return *this;
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::tagop_t DATFlitBuilder<T, config, able>::TagOp() noexcept requires is_field_applicable<T->fields->TagOp>
    {
        return this->flit.TagOp();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TagOp>()>
            DATFlitBuilder<T, config, able>::TagOp(typename DAT<config>::tagop_t tagOp) const noexcept requires is_field_applicable<T->fields->TagOp>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TagOp>()> nextBuilder(*this);
        nextBuilder.flit.TagOp() = tagOp;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TagOp>()>&
            DATFlitBuilder<T, config, able>::SetTagOp(typename DAT<config>::tagop_t tagOp) noexcept requires is_field_applicable<T->fields->TagOp>
    {
        this->flit.TagOp() = tagOp;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::tag_t DATFlitBuilder<T, config, able>::Tag() noexcept requires is_field_applicable<T->fields->Tag>
    {
        return this->flit.Tag();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Tag>()>
            DATFlitBuilder<T, config, able>::Tag(typename DAT<config>::tag_t tag) const noexcept requires is_field_applicable<T->fields->Tag>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Tag>()> nextBuilder(*this);
        nextBuilder.flit.Tag() = tag;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Tag>()>&
            DATFlitBuilder<T, config, able>::SetTag(typename DAT<config>::tag_t tag) noexcept requires is_field_applicable<T->fields->Tag>
    {
        this->flit.Tag() = tag;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::tu_t DATFlitBuilder<T, config, able>::TU() noexcept requires is_field_applicable<T->fields->TU>
    {
        return this->flit.TU();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TU>()>
            DATFlitBuilder<T, config, able>::TU(typename DAT<config>::tu_t tu) const noexcept requires is_field_applicable<T->fields->TU>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TU>()> nextBuilder(*this);
        nextBuilder.flit.TU() = tu;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TU>()>&
            DATFlitBuilder<T, config, able>::SetTU(typename DAT<config>::tu_t tu) noexcept requires is_field_applicable<T->fields->TU>
    {
        this->flit.TU() = tu;
        return *this;
    }
#endif

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config>::tracetag_t DATFlitBuilder<T, config, able>::TraceTag() noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        return this->flit.TraceTag();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TraceTag>()>
            DATFlitBuilder<T, config, able>::TraceTag(typename DAT<config>::tracetag_t traceTag) const noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TraceTag>()> nextBuilder(*this);
        nextBuilder.flit.TraceTag() = traceTag;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::TraceTag>()>&
            DATFlitBuilder<T, config, able>::SetTraceTag(typename DAT<config>::tracetag_t traceTag) noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        this->flit.TraceTag() = traceTag;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr typename DAT<config>::rsvdc_t DATFlitBuilder<T, config, able>::RSVDC() noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<DAT<config>>
    {
        return this->flit.RSVDC();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::RSVDC>()>
            DATFlitBuilder<T, config, able>::RSVDC(typename DAT<config>::rsvdc_t rsvdc) const noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<DAT<config>>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::RSVDC>()> nextBuilder(*this);
        nextBuilder.flit.RSVDC() = rsvdc;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::RSVDC>()>&
            DATFlitBuilder<T, config, able>::SetRSVDC(typename DAT<config>::rsvdc_t rsvdc) noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<DAT<config>>
    {
        this->flit.RSVDC() = rsvdc;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr typename DAT<config>::be_t DATFlitBuilder<T, config, able>::BE() noexcept requires is_field_applicable<T->fields->BE>
    {
        return this->flit.BE();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::BE>()>
            DATFlitBuilder<T, config, able>::BE(typename DAT<config>::be_t be) const noexcept requires is_field_applicable<T->fields->BE>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::BE>()> nextBuilder(*this);
        nextBuilder.flit.BE() = be;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::BE>()>&
            DATFlitBuilder<T, config, able>::SetBE(typename DAT<config>::be_t be) noexcept requires is_field_applicable<T->fields->BE>
    {
        this->flit.BE() = be;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr typename DAT<config>::data_t DATFlitBuilder<T, config, able>::Data() noexcept requires is_field_applicable<T->fields->Data>
    {
        return this->flit.Data();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Data>()>
            DATFlitBuilder<T, config, able>::Data(const typename DAT<config>::data_t data) const noexcept requires is_field_applicable<T->fields->Data>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Data>()> nextBuilder(*this);
        nextBuilder.flit.Data() = data;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Data>()>&
            DATFlitBuilder<T, config, able>::SetData(const typename DAT<config>::data_t data) noexcept requires is_field_applicable<T->fields->Data>
    {
        this->flit.Data() = data;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr typename DAT<config>::datacheck_t DATFlitBuilder<T, config, able>::DataCheck() noexcept requires is_field_applicable<T->fields->DataCheck> && has_datacheck<DAT<config>>
    {
        return this->flit.DataCheck();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataCheck>()>
            DATFlitBuilder<T, config, able>::DataCheck(typename DAT<config>::datacheck_t dataCheck) const noexcept requires is_field_applicable<T->fields->DataCheck> && has_datacheck<DAT<config>>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataCheck>()> nextBuilder(*this);
        nextBuilder.flit.DataCheck() = dataCheck;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::DataCheck>()>&
            DATFlitBuilder<T, config, able>::SetDataCheck(typename DAT<config>::datacheck_t dataCheck) noexcept requires is_field_applicable<T->fields->DataCheck> && has_datacheck<DAT<config>>
    {
        this->flit.DataCheck() = dataCheck;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr typename DAT<config>::poison_t DATFlitBuilder<T, config, able>::Poison() noexcept requires is_field_applicable<T->fields->Poison> && has_poison<DAT<config>>
    {
        return this->flit.Poison();
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Poison>()>
            DATFlitBuilder<T, config, able>::Poison(typename DAT<config>::poison_t poison) const noexcept requires is_field_applicable<T->fields->Poison> && has_poison<DAT<config>>
    {
        DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Poison>()> nextBuilder(*this);
        nextBuilder.flit.Poison() = poison;
        return nextBuilder;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::NextDATFlitBuildability<T, able, details::DATFlitField::Poison>()>&
            DATFlitBuilder<T, config, able>::SetPoison(typename DAT<config>::poison_t poison) noexcept requires is_field_applicable<T->fields->Poison> && has_poison<DAT<config>>
    {
        this->flit.Poison() = poison;
        return *this;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DAT<config> DATFlitBuilder<T, config, able>::Eval() const noexcept requires details::is_buildable<able>
    {
        return this->flit;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config> DATFlitBuilder<T, config, able>::Build() const noexcept requires details::is_buildable<able>
    {
        return this->flit;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline consteval DAT<config> DATFlitBuilder<T, config, able>::EvalUnsafe() const noexcept
    {
        return this->flit;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DAT<config> DATFlitBuilder<T, config, able>::BuildUnsafe() const noexcept
    {
        return this->flit;
    }

    template<DATBuildTargetEnum T, DATFlitConfigurationConcept config, details::DATFlitBuildability able>
    inline constexpr DATFlitBuilder<T, config, details::always_buildable_dat> DATFlitBuilder<T, config, able>::Unsafe() const noexcept
    {
        return DATFlitBuilder<T, config, details::always_buildable_dat>(*this);
    }
}


#endif // __CHI__CHI_UTIL_FLIT_BUILDER_DAT

//#endif // __CHI__CHI_UTIL_FLIT_BUILDER_DAT
