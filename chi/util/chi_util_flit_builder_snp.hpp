//#pragma once

//#ifndef __CHI__CHI_UTIL_FLIT_BUILDER_SNP
//#define __CHI__CHI_UTIL_FLIT_BUILDER_SNP

#ifndef CHI_UTIL_FLIT_BUILDER_SNP__STANDALONE
#   define CHI_ISSUE_EB_ENABLE
#   include "chi_util_flit_builder_snp_header.hpp"                      // IWYU pragma: keep
#endif

#include "chi_util_flit_builder/chi_util_flit_builder_common.hpp"       // IWYU pragma: keep
#include "chi_util_flit_builder/chi_util_flit_builder_snp_details.hpp"  // IWYU pragma: keep


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_SNP_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_SNP_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_SNP_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_SNP_EB
#endif


/*
namespace CHI {
*/
    namespace Flits {

        // enumerations
        namespace SNPBuildTarget {
            inline constexpr SNPBuildTargetEnumBack SnpLCrdReturn       ("SnpLCrdReturn"        , Opcodes::SNP::SnpLCrdReturn       , &Xact::SnoopFieldMappings::SnpLCrdReturn          );
            inline constexpr SNPBuildTargetEnumBack SnpShared           ("SnpShared"            , Opcodes::SNP::SnpShared           , &Xact::SnoopFieldMappings::SnpShared              );
            inline constexpr SNPBuildTargetEnumBack SnpClean            ("SnpClean"             , Opcodes::SNP::SnpClean            , &Xact::SnoopFieldMappings::SnpClean               );
            inline constexpr SNPBuildTargetEnumBack SnpOnce             ("SnpOnce"              , Opcodes::SNP::SnpOnce             , &Xact::SnoopFieldMappings::SnpOnce                );
            inline constexpr SNPBuildTargetEnumBack SnpNotSharedDirty   ("SnpNotSharedDirty"    , Opcodes::SNP::SnpNotSharedDirty   , &Xact::SnoopFieldMappings::SnpNotSharedDirty      );
            inline constexpr SNPBuildTargetEnumBack SnpUniqueStash      ("SnpUniqueStash"       , Opcodes::SNP::SnpUniqueStash      , &Xact::SnoopFieldMappings::SnpUniqueStash         );
            inline constexpr SNPBuildTargetEnumBack SnpMakeInvalidStash ("SnpMakeInvalidStash"  , Opcodes::SNP::SnpMakeInvalidStash , &Xact::SnoopFieldMappings::SnpMakeInvalidStash    );
            inline constexpr SNPBuildTargetEnumBack SnpUnique           ("SnpUnique"            , Opcodes::SNP::SnpUnique           , &Xact::SnoopFieldMappings::SnpUnique              );
            inline constexpr SNPBuildTargetEnumBack SnpCleanShared      ("SnpCleanShared"       , Opcodes::SNP::SnpCleanShared      , &Xact::SnoopFieldMappings::SnpCleanShared         );
            inline constexpr SNPBuildTargetEnumBack SnpCleanInvalid     ("SnpCleanInvalid"      , Opcodes::SNP::SnpCleanInvalid     , &Xact::SnoopFieldMappings::SnpCleanInvalid        );
            inline constexpr SNPBuildTargetEnumBack SnpMakeInvalid      ("SnpMakeInvalid"       , Opcodes::SNP::SnpMakeInvalid      , &Xact::SnoopFieldMappings::SnpMakeInvalid         );
            inline constexpr SNPBuildTargetEnumBack SnpStashUnique      ("SnpStashUnique"       , Opcodes::SNP::SnpStashUnique      , &Xact::SnoopFieldMappings::SnpStashUnique         );
            inline constexpr SNPBuildTargetEnumBack SnpStashShared      ("SnpStashShared"       , Opcodes::SNP::SnpStashShared      , &Xact::SnoopFieldMappings::SnpStashShared         );
            inline constexpr SNPBuildTargetEnumBack SnpDVMOp            ("SnpDVMOp"             , Opcodes::SNP::SnpDVMOp            , &Xact::SnoopFieldMappings::SnpDVMOp               );
            // 0x0E
            // 0x0F
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr SNPBuildTargetEnumBack SnpQuery            ("SnpQuery"             , Opcodes::SNP::SnpQuery            , &Xact::SnoopFieldMappings::SnpQuery               );
#endif
            inline constexpr SNPBuildTargetEnumBack SnpSharedFwd        ("SnpSharedFwd"         , Opcodes::SNP::SnpSharedFwd        , &Xact::SnoopFieldMappings::SnpSharedFwd           );
            inline constexpr SNPBuildTargetEnumBack SnpCleanFwd         ("SnpCleanFwd"          , Opcodes::SNP::SnpCleanFwd         , &Xact::SnoopFieldMappings::SnpCleanFwd            );
            inline constexpr SNPBuildTargetEnumBack SnpOnceFwd          ("SnpOnceFwd"           , Opcodes::SNP::SnpOnceFwd          , &Xact::SnoopFieldMappings::SnpOnceFwd             );
            inline constexpr SNPBuildTargetEnumBack SnpNotSharedDirtyFwd("SnpNotSharedDirtyFwd" , Opcodes::SNP::SnpNotSharedDirtyFwd, &Xact::SnoopFieldMappings::SnpNotSharedDirtyFwd   );
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr SNPBuildTargetEnumBack SnpPreferUnique     ("SnpPreferUnique"      , Opcodes::SNP::SnpPreferUnique     , &Xact::SnoopFieldMappings::SnpPreferUnique        );
            inline constexpr SNPBuildTargetEnumBack SnpPreferUniqueFwd  ("SnpPreferUniqueFwd"   , Opcodes::SNP::SnpPreferUniqueFwd  , &Xact::SnoopFieldMappings::SnpPreferUniqueFwd     );
#endif
            inline constexpr SNPBuildTargetEnumBack SnpUniqueFwd        ("SnpUniqueFwd"         , Opcodes::SNP::SnpUniqueFwd        , &Xact::SnoopFieldMappings::SnpUniqueFwd           );
            // 0x18 - 0x1F
        }

        // SNP builders
        template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able = MakeSNPFlitBuildability<T, config>()>
        class SNPFlitBuilder
        {
            template<SNPBuildTargetEnum, SNPFlitConfigurationConcept, details::SNPFlitBuildability>
            friend class SNPFlitBuilder;

        protected:
            SNP<config>     flit    = { 0 };

        public:
            constexpr SNPFlitBuilder() noexcept;

            template<details::SNPFlitBuildability nextAble>
            constexpr SNPFlitBuilder(const SNPFlitBuilder<T, config, nextAble>&) noexcept;

        public:
            constexpr SNP<config>::qos_t                QoS() const noexcept requires is_field_applicable<T->fields->QoS>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::QoS>()>
                                                        QoS(typename SNP<config>::qos_t value) const noexcept requires is_field_applicable<T->fields->QoS>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::QoS>()>&
                                                        SetQoS(typename SNP<config>::qos_t value) noexcept requires is_field_applicable<T->fields->QoS>;
        
            constexpr SNP<config>::srcid_t              SrcID() const noexcept requires is_field_applicable<T->fields->SrcID>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::SrcID>()>
                                                        SrcID(typename SNP<config>::srcid_t value) const noexcept requires is_field_applicable<T->fields->SrcID>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::SrcID>()>&
                                                        SetSrcID(typename SNP<config>::srcid_t value) noexcept requires is_field_applicable<T->fields->SrcID>;

            constexpr SNP<config>::txnID_t              TxnID() const noexcept requires is_field_applicable<T->fields->TxnID>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::TxnID>()>
                                                        TxnID(typename SNP<config>::txnID_t value) const noexcept requires is_field_applicable<T->fields->TxnID>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::TxnID>()>&
                                                        SetTxnID(typename SNP<config>::txnID_t value) noexcept requires is_field_applicable<T->fields->TxnID>;

            constexpr SNP<config>::fwdnid_t             FwdNID() const noexcept requires is_field_applicable<T->fields->FwdNID>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::FwdNID>()>
                                                        FwdNID(typename SNP<config>::fwdnid_t value) const noexcept requires is_field_applicable<T->fields->FwdNID>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::FwdNID>()>&
                                                        SetFwdNID(typename SNP<config>::fwdnid_t value) noexcept requires is_field_applicable<T->fields->FwdNID>;

            constexpr SNP<config>::fwdtxnid_t           FwdTxnID() const noexcept requires is_field_applicable<T->fields->FwdTxnID>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::FwdTxnID>()>
                                                        FwdTxnID(typename SNP<config>::fwdtxnid_t value) const noexcept requires is_field_applicable<T->fields->FwdTxnID>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::FwdTxnID>()>&
                                                        SetFwdTxnID(typename SNP<config>::fwdtxnid_t value) noexcept requires is_field_applicable<T->fields->FwdTxnID>;

            constexpr SNP<config>::stashlpidvalid_t     StashLPIDValid() const noexcept requires is_field_applicable<T->fields->StashLPIDValid>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::StashLPIDValid>()>
                                                        StashLPIDValid(typename SNP<config>::stashlpidvalid_t value) const noexcept requires is_field_applicable<T->fields->StashLPIDValid>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::StashLPIDValid>()>&
                                                        SetStashLPIDValid(typename SNP<config>::stashlpidvalid_t value) noexcept requires is_field_applicable<T->fields->StashLPIDValid>;

            constexpr SNP<config>::stashlpid_t          StashLPID() const noexcept requires is_field_applicable<T->fields->StashLPID>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::StashLPID>()>
                                                        StashLPID(typename SNP<config>::stashlpid_t value) const noexcept requires is_field_applicable<T->fields->StashLPID>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::StashLPID>()>&
                                                        SetStashLPID(typename SNP<config>::stashlpid_t value) noexcept requires is_field_applicable<T->fields->StashLPID>;

            constexpr SNP<config>::vmidext_t            VMIDExt() const noexcept requires is_field_applicable<T->fields->VMIDExt>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::VMIDExt>()>
                                                        VMIDExt(typename SNP<config>::vmidext_t value) const noexcept requires is_field_applicable<T->fields->VMIDExt>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::VMIDExt>()>&
                                                        SetVMIDExt(typename SNP<config>::vmidext_t value) noexcept requires is_field_applicable<T->fields->VMIDExt>;

            constexpr SNP<config>::opcode_t             Opcode() const noexcept requires is_field_applicable<T->fields->Opcode>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::Opcode>()>
                                                        Opcode(typename SNP<config>::opcode_t value) const noexcept requires is_field_applicable<T->fields->Opcode>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::Opcode>()>&
                                                        SetOpcode(typename SNP<config>::opcode_t value) noexcept requires is_field_applicable<T->fields->Opcode>;

            constexpr SNP<config>::addr_t               Addr() const noexcept requires is_field_applicable<T->fields->Addr>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::Addr>()>
                                                        Addr(typename SNP<config>::addr_t value) const noexcept requires is_field_applicable<T->fields->Addr>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::Addr>()>&
                                                        SetAddr(typename SNP<config>::addr_t value) noexcept requires is_field_applicable<T->fields->Addr>;

            constexpr SNP<config>::ns_t                 NS() const noexcept requires is_field_applicable<T->fields->NS>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::NS>()>
                                                        NS(typename SNP<config>::ns_t value) const noexcept requires is_field_applicable<T->fields->NS>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::NS>()>&
                                                        SetNS(typename SNP<config>::ns_t value) noexcept requires is_field_applicable<T->fields->NS>;

            constexpr SNP<config>::donotgotosd_t        DoNotGoToSD() const noexcept requires is_field_applicable<T->fields->DoNotGoToSD>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::DoNotGoToSD>()>
                                                        DoNotGoToSD(typename SNP<config>::donotgotosd_t value) const noexcept requires is_field_applicable<T->fields->DoNotGoToSD>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::DoNotGoToSD>()>&
                                                        SetDoNotGoToSD(typename SNP<config>::donotgotosd_t value) noexcept requires is_field_applicable<T->fields->DoNotGoToSD>;

#if defined(CHI_ISSUE_B_ENABLE) || defined(CHI_ISSUE_C_ENABLE)
            constexpr SNP<config>::donotdatapull_t      DoNotDataPull() const noexcept requires is_field_applicable<T->fields->DoNotDataPull>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::DoNotDataPull>()>
                                                        DoNotDataPull(typename SNP<config>::donotdatapull_t value) const noexcept requires is_field_applicable<T->fields->DoNotDataPull>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::DoNotDataPull>()>&
                                                        SetDoNotDataPull(typename SNP<config>::donotdatapull_t value) noexcept requires is_field_applicable<T->fields->DoNotDataPull>;
#endif

            constexpr SNP<config>::rettosrc_t           RetToSrc() const noexcept requires is_field_applicable<T->fields->RetToSrc>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::RetToSrc>()>
                                                        RetToSrc(typename SNP<config>::rettosrc_t value) const noexcept requires is_field_applicable<T->fields->RetToSrc>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::RetToSrc>()>&
                                                        SetRetToSrc(typename SNP<config>::rettosrc_t value) noexcept requires is_field_applicable<T->fields->RetToSrc>;

            constexpr SNP<config>::tracetag_t           TraceTag() const noexcept requires is_field_applicable<T->fields->TraceTag>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::TraceTag>()>
                                                        TraceTag(typename SNP<config>::tracetag_t value) const noexcept requires is_field_applicable<T->fields->TraceTag>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::TraceTag>()>&
                                                        SetTraceTag(typename SNP<config>::tracetag_t value) noexcept requires is_field_applicable<T->fields->TraceTag>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr SNP<config>::mpam_t               MPAM() const noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<SNP<config>>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::MPAM>()>
                                                        MPAM(typename SNP<config>::mpam_t value) const noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<SNP<config>>;
            constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::MPAM>()>&
                                                        SetMPAM(typename SNP<config>::mpam_t value) noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<SNP<config>>;
#endif

            //
            consteval SNP<config>                       Eval() const noexcept requires details::is_buildable<able>;
            constexpr SNP<config>                       Build() const noexcept requires details::is_buildable<able>;

            consteval SNP<config>                       EvalUnsafe() const noexcept;
            constexpr SNP<config>                       BuildUnsafe() const noexcept;

            constexpr SNPFlitBuilder<T, config, details::always_buildable_snp> 
                                                        Unsafe() const noexcept;
        };
    
        template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config>
        consteval SNPFlitBuilder<T, config, details::MakeSNPFlitBuildability<T, config>()> EvalSNPFlitBuilder() noexcept;

        template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config>
        constexpr SNPFlitBuilder<T, config, details::MakeSNPFlitBuildability<T, config>()> MakeSNPFlitBuilder() noexcept;
    }
    
/*
}
*/


// Implementation of: class SNPFlitBuilder
namespace /*CHI:*/Flits {
    
    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    constexpr SNPFlitBuilder<T, config, able>::SNPFlitBuilder() noexcept
    {
        SNP<config> flit = { 0 };

#define _INIT_IF(field) \
        if constexpr (is_field_applicable<T->fields->field>)

#define _FIELD_INIT(field) \
        _INIT_IF(field) flit.field() = GetDefaultFieldValue<T->fields->field>();

#define _FIELD_INIT_PCF(field, dst) \
        _INIT_IF(field) flit.dst() = PCF##dst##From##field <config>(flit.dst(), GetDefaultFieldValue<T->fields->field>());

#define _FIELD_INIT_OPT(field) \
        if constexpr (SNP<config>::has##field) \
            _INIT_IF(field) flit.field() = GetDefaultFieldValue<T->fields->field>();

        flit.Opcode() = T->opcode;

        _FIELD_INIT(QoS             );
        _FIELD_INIT(SrcID           );
        _FIELD_INIT(TxnID           );
        _FIELD_INIT(FwdNID          );
        _FIELD_INIT(FwdTxnID        );
        _FIELD_INIT_PCF(StashLPID       , FwdTxnID);
        _FIELD_INIT_PCF(StashLPIDValid  , FwdTxnID);
        _FIELD_INIT_PCF(VMIDExt         , FwdTxnID);
        _FIELD_INIT(Opcode          );
        _FIELD_INIT(Addr            );
        _FIELD_INIT(NS              );
        _FIELD_INIT(DoNotGoToSD     );
#if defined(CHI_ISSUE_B_ENABLE) || defined(CHI_ISSUE_C_ENABLE)
        _FIELD_INIT_PCF(DoNotDataPull   , DoNotGoToSD);
#endif
        _FIELD_INIT(RetToSrc        );
        _FIELD_INIT(TraceTag        );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT_OPT(MPAM            );
#endif

#undef _INIT_IF
#undef _FIELD_INIT
#undef _FIELD_INIT_PCF
#undef _FIELD_INIT_OPT

        this->flit = flit;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    template<details::SNPFlitBuildability nextAble>
    constexpr SNPFlitBuilder<T, config, able>::SNPFlitBuilder(const SNPFlitBuilder<T, config, nextAble>& other) noexcept
    {
        this->flit = other.flit;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::qos_t SNPFlitBuilder<T, config, able>::QoS() const noexcept requires is_field_applicable<T->fields->QoS>
    {
        return this->flit.QoS();
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::QoS>()> SNPFlitBuilder<T, config, able>::QoS(typename SNP<config>::qos_t value) const noexcept requires is_field_applicable<T->fields->QoS>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::QoS>()> nextBuilder(*this);
        nextBuilder.flit.QoS() = value;
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::QoS>()>& SNPFlitBuilder<T, config, able>::SetQoS(typename SNP<config>::qos_t value) noexcept requires is_field_applicable<T->fields->QoS>
    {
        this->flit.QoS() = value;
        return *this;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::srcid_t SNPFlitBuilder<T, config, able>::SrcID() const noexcept requires is_field_applicable<T->fields->SrcID>
    {
        return this->flit.SrcID();
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::SrcID>()> SNPFlitBuilder<T, config, able>::SrcID(typename SNP<config>::srcid_t value) const noexcept requires is_field_applicable<T->fields->SrcID>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::SrcID>()> nextBuilder(*this);
        nextBuilder.flit.SrcID() = value;
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::SrcID>()>& SNPFlitBuilder<T, config, able>::SetSrcID(typename SNP<config>::srcid_t value) noexcept requires is_field_applicable<T->fields->SrcID>
    {
        this->flit.SrcID() = value;
        return *this;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::txnID_t SNPFlitBuilder<T, config, able>::TxnID() const noexcept requires is_field_applicable<T->fields->TxnID>
    {
        return this->flit.TxnID();
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::TxnID>()> SNPFlitBuilder<T, config, able>::TxnID(typename SNP<config>::txnID_t value) const noexcept requires is_field_applicable<T->fields->TxnID>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::TxnID>()> nextBuilder(*this);
        nextBuilder.flit.TxnID() = value;
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::TxnID>()>& SNPFlitBuilder<T, config, able>::SetTxnID(typename SNP<config>::txnID_t value) noexcept requires is_field_applicable<T->fields->TxnID>
    {
        this->flit.TxnID() = value;
        return *this;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::fwdnid_t SNPFlitBuilder<T, config, able>::FwdNID() const noexcept requires is_field_applicable<T->fields->FwdNID>
    {
        return this->flit.FwdNID();
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::FwdNID>()> SNPFlitBuilder<T, config, able>::FwdNID(typename SNP<config>::fwdnid_t value) const noexcept requires is_field_applicable<T->fields->FwdNID>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::FwdNID>()> nextBuilder(*this);
        nextBuilder.flit.FwdNID() = value;
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::FwdNID>()>& SNPFlitBuilder<T, config, able>::SetFwdNID(typename SNP<config>::fwdnid_t value) noexcept requires is_field_applicable<T->fields->FwdNID>
    {
        this->flit.FwdNID() = value;
        return *this;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::fwdtxnid_t SNPFlitBuilder<T, config, able>::FwdTxnID() const noexcept requires is_field_applicable<T->fields->FwdTxnID>
    {
        return this->flit.FwdTxnID();
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::FwdTxnID>()> SNPFlitBuilder<T, config, able>::FwdTxnID(typename SNP<config>::fwdtxnid_t value) const noexcept requires is_field_applicable<T->fields->FwdTxnID>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::FwdTxnID>()> nextBuilder(*this);
        nextBuilder.flit.FwdTxnID() = value;
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::FwdTxnID>()>& SNPFlitBuilder<T, config, able>::SetFwdTxnID(typename SNP<config>::fwdtxnid_t value) noexcept requires is_field_applicable<T->fields->FwdTxnID>
    {
        this->flit.FwdTxnID() = value;
        return *this;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::stashlpid_t SNPFlitBuilder<T, config, able>::StashLPID() const noexcept requires is_field_applicable<T->fields->StashLPID>
    {
        return details::PCFFwdTxnIDToStashLPID<config>(this->flit.FwdTxnID());
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::StashLPID>()> SNPFlitBuilder<T, config, able>::StashLPID(typename SNP<config>::stashlpid_t value) const noexcept requires is_field_applicable<T->fields->StashLPID>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::StashLPID>()> nextBuilder(*this);
        nextBuilder.flit.FwdTxnID() = details::PCFFwdTxnIDFromStashLPID<config>(this->flit.FwdTxnID(), value);
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::StashLPID>()>& SNPFlitBuilder<T, config, able>::SetStashLPID(typename SNP<config>::stashlpid_t value) noexcept requires is_field_applicable<T->fields->StashLPID>
    {
        this->flit.FwdTxnID() = details::PCFFwdTxnIDFromStashLPID<config>(this->flit.FwdTxnID(), value);
        return *this;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::stashlpidvalid_t SNPFlitBuilder<T, config, able>::StashLPIDValid() const noexcept requires is_field_applicable<T->fields->StashLPIDValid>
    {
        return details::PCFFwdTxnIDToStashLPIDValid<config>(this->flit.FwdTxnID());
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::StashLPIDValid>()> SNPFlitBuilder<T, config, able>::StashLPIDValid(typename SNP<config>::stashlpidvalid_t value) const noexcept requires is_field_applicable<T->fields->StashLPIDValid>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::StashLPIDValid>()> nextBuilder(*this);
        nextBuilder.flit.FwdTxnID() = details::PCFFwdTxnIDFromStashLPIDValid<config>(this->flit.FwdTxnID(), value);
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::StashLPIDValid>()>& SNPFlitBuilder<T, config, able>::SetStashLPIDValid(typename SNP<config>::stashlpidvalid_t value) noexcept requires is_field_applicable<T->fields->StashLPIDValid>
    {
        this->flit.FwdTxnID() = details::PCFFwdTxnIDFromStashLPIDValid<config>(this->flit.FwdTxnID(), value);
        return *this;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::vmidext_t SNPFlitBuilder<T, config, able>::VMIDExt() const noexcept requires is_field_applicable<T->fields->VMIDExt>
    {
        return details::PCFFwdTxnIDToVMIDExt<config>(this->flit.FwdTxnID());
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::VMIDExt>()> SNPFlitBuilder<T, config, able>::VMIDExt(typename SNP<config>::vmidext_t value) const noexcept requires is_field_applicable<T->fields->VMIDExt>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::VMIDExt>()> nextBuilder(*this);
        nextBuilder.flit.FwdTxnID() = details::PCFFwdTxnIDFromVMIDExt<config>(this->flit.FwdTxnID(), value);
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::VMIDExt>()>& SNPFlitBuilder<T, config, able>::SetVMIDExt(typename SNP<config>::vmidext_t value) noexcept requires is_field_applicable<T->fields->VMIDExt>
    {
        this->flit.FwdTxnID() = details::PCFFwdTxnIDFromVMIDExt<config>(this->flit.FwdTxnID(), value);
        return *this;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::opcode_t SNPFlitBuilder<T, config, able>::Opcode() const noexcept requires is_field_applicable<T->fields->Opcode>
    {
        return this->flit.Opcode();
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::Opcode>()> SNPFlitBuilder<T, config, able>::Opcode(typename SNP<config>::opcode_t value) const noexcept requires is_field_applicable<T->fields->Opcode>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::Opcode>()> nextBuilder(*this);
        nextBuilder.flit.Opcode() = value;
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::Opcode>()>& SNPFlitBuilder<T, config, able>::SetOpcode(typename SNP<config>::opcode_t value) noexcept requires is_field_applicable<T->fields->Opcode>
    {
        this->flit.Opcode() = value;
        return *this;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::addr_t SNPFlitBuilder<T, config, able>::Addr() const noexcept requires is_field_applicable<T->fields->Addr>
    {
        return this->flit.Addr();
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::Addr>()> SNPFlitBuilder<T, config, able>::Addr(typename SNP<config>::addr_t value) const noexcept requires is_field_applicable<T->fields->Addr>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::Addr>()> nextBuilder(*this);
        nextBuilder.flit.Addr() = value;
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::Addr>()>& SNPFlitBuilder<T, config, able>::SetAddr(typename SNP<config>::addr_t value) noexcept requires is_field_applicable<T->fields->Addr>
    {
        this->flit.Addr() = value;
        return *this;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::ns_t SNPFlitBuilder<T, config, able>::NS() const noexcept requires is_field_applicable<T->fields->NS>
    {
        return this->flit.NS();
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::NS>()> SNPFlitBuilder<T, config, able>::NS(typename SNP<config>::ns_t value) const noexcept requires is_field_applicable<T->fields->NS>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::NS>()> nextBuilder(*this);
        nextBuilder.flit.NS() = value;
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::NS>()>& SNPFlitBuilder<T, config, able>::SetNS(typename SNP<config>::ns_t value) noexcept requires is_field_applicable<T->fields->NS>
    {
        this->flit.NS() = value;
        return *this;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::donotgotosd_t SNPFlitBuilder<T, config, able>::DoNotGoToSD() const noexcept requires is_field_applicable<T->fields->DoNotGoToSD>
    {
        return this->flit.DoNotGoToSD();
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::DoNotGoToSD>()> SNPFlitBuilder<T, config, able>::DoNotGoToSD(typename SNP<config>::donotgotosd_t value) const noexcept requires is_field_applicable<T->fields->DoNotGoToSD>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::DoNotGoToSD>()> nextBuilder(*this);
        nextBuilder.flit.DoNotGoToSD() = value;
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::DoNotGoToSD>()>& SNPFlitBuilder<T, config, able>::SetDoNotGoToSD(typename SNP<config>::donotgotosd_t value) noexcept requires is_field_applicable<T->fields->DoNotGoToSD>
    {
        this->flit.DoNotGoToSD() = value;
        return *this;
    }

#if defined(CHI_ISSUE_B_ENABLE) || defined(CHI_ISSUE_C_ENABLE)
    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::donotdatapull_t SNPFlitBuilder<T, config, able>::DoNotDataPull() const noexcept requires is_field_applicable<T->fields->DoNotDataPull>
    {
        return PCFDoNotGoToSDToDoNotDataPull<config>(this->flit.DoNotGoToSD());
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::DoNotDataPull>()> SNPFlitBuilder<T, config, able>::DoNotDataPull(typename SNP<config>::donotdatapull_t value) const noexcept requires is_field_applicable<T->fields->DoNotDataPull>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::DoNotDataPull>()> nextBuilder(*this);
        nextBuilder.flit.DoNotGoToSD() = PCFDoNotGoToSDFromDoNotDataPull<config>(this->flit.DoNotGoToSD(), value);
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::DoNotDataPull>()>& SNPFlitBuilder<T, config, able>::SetDoNotDataPull(typename SNP<config>::donotdatapull_t value) noexcept requires is_field_applicable<T->fields->DoNotDataPull>
    {
        this->flit.DoNotGoToSD() = PCFDoNotGoToSDFromDoNotDataPull<config>(this->flit.DoNotGoToSD(), value);
        return *this;
    }
#endif

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::rettosrc_t SNPFlitBuilder<T, config, able>::RetToSrc() const noexcept requires is_field_applicable<T->fields->RetToSrc>
    {
        return this->flit.RetToSrc();
    }
    
    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::RetToSrc>()> SNPFlitBuilder<T, config, able>::RetToSrc(typename SNP<config>::rettosrc_t value) const noexcept requires is_field_applicable<T->fields->RetToSrc>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::RetToSrc>()> nextBuilder(*this);
        nextBuilder.flit.RetToSrc() = value;
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::RetToSrc>()>& SNPFlitBuilder<T, config, able>::SetRetToSrc(typename SNP<config>::rettosrc_t value) noexcept requires is_field_applicable<T->fields->RetToSrc>
    {
        this->flit.RetToSrc() = value;
        return *this;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::tracetag_t SNPFlitBuilder<T, config, able>::TraceTag() const noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        return this->flit.TraceTag();
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::TraceTag>()> SNPFlitBuilder<T, config, able>::TraceTag(typename SNP<config>::tracetag_t value) const noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::TraceTag>()> nextBuilder(*this);
        nextBuilder.flit.TraceTag() = value;
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::TraceTag>()>& SNPFlitBuilder<T, config, able>::SetTraceTag(typename SNP<config>::tracetag_t value) noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        this->flit.TraceTag() = value;
        return *this;
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config>::mpam_t SNPFlitBuilder<T, config, able>::MPAM() const noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<SNP<config>>
    {
        return this->flit.MPAM();
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::MPAM>()> SNPFlitBuilder<T, config, able>::MPAM(typename SNP<config>::mpam_t value) const noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<SNP<config>>
    {
        SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::MPAM>()> nextBuilder(*this);
        nextBuilder.flit.MPAM() = value;
        return nextBuilder;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::NextSNPFlitBuildability<T, able, details::SNPFlitField::MPAM>()>& SNPFlitBuilder<T, config, able>::SetMPAM(typename SNP<config>::mpam_t value) noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<SNP<config>>
    {
        this->flit.MPAM() = value;
        return *this;
    }
#endif

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline consteval SNP<config> SNPFlitBuilder<T, config, able>::Eval() const noexcept requires details::is_buildable<able>
    {
        return this->flit;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config> SNPFlitBuilder<T, config, able>::Build() const noexcept requires details::is_buildable<able>
    {
        return this->flit;
    }
    
    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline consteval SNP<config> SNPFlitBuilder<T, config, able>::EvalUnsafe() const noexcept
    {
        return this->flit;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNP<config> SNPFlitBuilder<T, config, able>::BuildUnsafe() const noexcept
    {
        return this->flit;
    }

    template<SNPBuildTargetEnum T, SNPFlitConfigurationConcept config, details::SNPFlitBuildability able>
    inline constexpr SNPFlitBuilder<T, config, details::always_buildable_snp> SNPFlitBuilder<T, config, able>::Unsafe() const noexcept
    {
        return SNPFlitBuilder<T, config, details::always_buildable_snp>(*this);
    }
}


#endif // __CHI__CHI_UTIL_FLIT_BUILDER_SNP_*
