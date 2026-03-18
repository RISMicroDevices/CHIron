//#pragma once

//#ifndef __CHI__CHI_UTIL_FLIT_BUILDER
//#define __CHI__CHI_UTIL_FLIT_BUILDER

#ifndef CHI_UTIL_FLIT_BUILDER__STANDALONE
#   define CHI_ISSUE_EB_ENABLE
#   include "chi_util_flit_builder_header.hpp"          // IWYU pragma: keep
#   include "../xact/chi_xact_field_eb.hpp"             // IWYU pragma: keep
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_EB
#endif


/*
namespace CHI {
*/
    namespace Flits {

        // enumerations
        class REQBuildTargetEnumBack {
        public:
            const char*                     name;
            const int                       opcode;
            const Xact::RequestFieldMapping fields;

        public:
            inline constexpr REQBuildTargetEnumBack(const char* name, const int opcode, const Xact::RequestFieldMapping fields) noexcept
            : name(name), opcode(opcode), fields(fields) {}

        public:
            inline constexpr operator int() const noexcept
            { return opcode; }

            inline constexpr operator const REQBuildTargetEnumBack*() const noexcept
            { return this; }

            inline constexpr bool operator==(const REQBuildTargetEnumBack& obj) const noexcept
            { return opcode == obj.opcode; }
            
            inline constexpr bool operator!=(const REQBuildTargetEnumBack& obj) const noexcept
            { return !(*this == obj); }
        };

        using REQBuildTargetEnum = const REQBuildTargetEnumBack*;

        namespace REQBuildTarget {
            inline constexpr REQBuildTargetEnumBack     ReqLCrdReturn   ("ReqLCrdReturn", Opcodes::REQ::ReqLCrdReturn, &Xact::RequestFieldMappings::ReqLCrdReturn);
            // TODO
        }


        // concepts
        template<Xact::FieldConvention conv>
        concept is_field_applicable = Xact::FieldTrait::IsApplicable(conv);

        template<REQBuildTargetEnum T>
        concept is_memattr_applicable = Xact::FieldTrait::IsApplicable(T->fields->EWA)
                                     || Xact::FieldTrait::IsApplicable(T->fields->Device)
                                     || Xact::FieldTrait::IsApplicable(T->fields->Cacheable)
                                     || Xact::FieldTrait::IsApplicable(T->fields->Allocate);

        template<class T>
        concept has_mpam = T::hasMPAM;

        template<class T>
        concept has_rsvdc = T::hasRSVDC;

        
        // constevals
        template<Xact::FieldConvention conv>
        consteval uint64_t GetDefaultFieldValue() noexcept;

        template<Xact::FieldConvention conv>
        consteval bool HasDefaultFieldValue() noexcept;

        template<Xact::FieldConvention conv>
        consteval bool IsFieldValueStableOrInapplicable() noexcept;

        template<REQBuildTargetEnum T>
        consteval MemAttr GetDefaultMemAttrValue() noexcept;


        // REQ builders
        enum class REQFlitField {
            QoS,
            TgtID,
            SrcID,
            TxnID,
            ReturnNID,
            StashNID,
#ifdef CHI_ISSUE_EB_ENABLE
            SLCRepHint,
#endif
            StashNIDValid,
            Endian,
#ifdef CHI_ISSUE_EB_ENABLE
            Deep,
#endif
            ReturnTxnID,
            StashLPIDValid,
            StashLPID,
            Opcode,
            Size,
            Addr,
            NS,
            LikelyShared,
            AllowRetry,
            Order,
            PCrdType,
            MemAttr,
            SnpAttr,
#ifdef CHI_ISSUE_EB_ENABLE
            DoDWT,
#endif
            LPID,
#ifdef CHI_ISSUE_EB_ENABLE
            PGroupID,
            StashGroupID,
            TagGroupID,
#endif
            Excl,
            SnoopMe,
            ExpCompAck,
#ifdef CHI_ISSUE_EB_ENABLE
            TagOp,
#endif
            TraceTag,
#ifdef CHI_ISSUE_EB_ENABLE
            MPAM,
#endif
            RSVDC
        };

        struct REQFlitBuildability {
            bool QoS            = false;
            bool TgtID          = false;
            bool SrcID          = false;
            bool TxnID          = false;
            bool ReturnNID      = false;
            bool StashNID       = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool SLCRepHint     = false;
#endif
            bool StashNIDValid  = false;
            bool Endian         = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool Deep           = false;
#endif
            bool ReturnTxnID    = false;
            bool StashLPIDValid = false;
            bool StashLPID      = false;
            bool Opcode         = false;
            bool Size           = false;
            bool Addr           = false;
            bool NS             = false;
            bool LikelyShared   = false;
            bool AllowRetry     = false;
            bool Order          = false;
            bool PCrdType       = false;
            bool MemAttr        = false;
            bool SnpAttr        = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool DoDWT          = false;
#endif
            bool LPID           = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool PGroupID       = false;
            bool StashGroupID   = false;
            bool TagGroupID     = false;
#endif
            bool Excl           = false;
            bool SnoopMe        = false;
            bool ExpCompAck     = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool TagOp          = false;
#endif
            bool TraceTag       = false;
#ifdef CHI_ISSUE_EB_ENABLE
            bool MPAM           = false;
#endif
            bool RSVDC          = false;
        };

        template<REQFlitBuildability able>
        concept is_buildable = able.QoS
                            && able.TgtID
                            && able.SrcID
                            && able.TxnID
                            && able.ReturnNID
                            && able.StashNID
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.SLCRepHint
#endif
                            && able.StashNIDValid
                            && able.Endian
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.Deep
#endif
                            && able.ReturnTxnID
                            && able.StashLPIDValid
                            && able.StashLPID
                            && able.Opcode
                            && able.Size
                            && able.Addr
                            && able.NS
                            && able.LikelyShared
                            && able.AllowRetry
                            && able.Order
                            && able.PCrdType
                            && able.MemAttr
                            && able.SnpAttr
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.DoDWT
#endif
                            && able.LPID
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.PGroupID
                            && able.StashGroupID
                            && able.TagGroupID
#endif
                            && able.Excl
                            && able.SnoopMe
                            && able.ExpCompAck
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.TagOp
#endif
                            && able.TraceTag
#ifdef CHI_ISSUE_EB_ENABLE
                            && able.MPAM
#endif
                            && able.RSVDC
                            ;

        template<REQBuildTargetEnum T, REQFlitConfigurationConcept config>
        consteval REQFlitBuildability MakeREQFlitBuildability() noexcept;

        template<REQBuildTargetEnum T, REQFlitBuildability prevAble, REQFlitField field>
        consteval REQFlitBuildability NextREQFlitBuildability() noexcept;

        template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able = MakeREQFlitBuildability<T, config>()>
        class REQFlitBuilder
        {
            template<REQBuildTargetEnum thatT, REQFlitConfigurationConcept thatConfig, REQFlitBuildability thatAble>
            friend class REQFlitBuilder;

        protected:
            REQ<config>     flit    = { 0 };

        public:
            constexpr REQFlitBuilder() noexcept;

            template<REQFlitBuildability nextAble>
            constexpr REQFlitBuilder(const REQFlitBuilder<T, config, nextAble>&) noexcept;

        public:
            constexpr REQ<config>::qos_t                QoS() const noexcept requires is_field_applicable<T->fields->QoS>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::QoS>()>
                                                        QoS(typename REQ<config>::qos_t qos) const noexcept requires is_field_applicable<T->fields->QoS>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::QoS>()>&
                                                        SetQoS(typename REQ<config>::qos_t qos) noexcept requires is_field_applicable<T->fields->QoS>;

            constexpr REQ<config>::tgtid_t              TgtID() const noexcept requires is_field_applicable<T->fields->TgtID>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TgtID>()>
                                                        TgtID(typename REQ<config>::tgtid_t tgtID) const noexcept requires is_field_applicable<T->fields->TgtID>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TgtID>()>&
                                                        SetTgtID(typename REQ<config>::tgtid_t tgtID) noexcept requires is_field_applicable<T->fields->TgtID>;

            constexpr REQ<config>::srcid_t              SrcID() const noexcept requires is_field_applicable<T->fields->SrcID>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SrcID>()>
                                                        SrcID(typename REQ<config>::srcid_t srcID) const noexcept requires is_field_applicable<T->fields->SrcID>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SrcID>()>&
                                                        SetSrcID(typename REQ<config>::srcid_t srcID) noexcept requires is_field_applicable<T->fields->SrcID>;

            constexpr REQ<config>::txnid_t              TxnID() const noexcept requires is_field_applicable<T->fields->TxnID>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TxnID>()>
                                                        TxnID(typename REQ<config>::txnid_t txnID) const noexcept requires is_field_applicable<T->fields->TxnID>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TxnID>()>&
                                                        SetTxnID(typename REQ<config>::txnid_t txnID) noexcept requires is_field_applicable<T->fields->TxnID>;

            constexpr REQ<config>::returnnid_t          ReturnNID() const noexcept requires is_field_applicable<T->fields->ReturnNID>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ReturnNID>()>
                                                        ReturnNID(typename REQ<config>::returnnid_t returnNID) const noexcept requires is_field_applicable<T->fields->ReturnNID>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ReturnNID>()>&
                                                        SetReturnNID(typename REQ<config>::returnnid_t returnNID) noexcept requires is_field_applicable<T->fields->ReturnNID>;

            constexpr REQ<config>::stashnid_t           StashNID() const noexcept requires is_field_applicable<T->fields->StashNID>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashNID>()>
                                                        StashNID(typename REQ<config>::stashnid_t stashNID) const noexcept requires is_field_applicable<T->fields->StashNID>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashNID>()>&
                                                        SetStashNID(typename REQ<config>::stashnid_t stashNID) noexcept requires is_field_applicable<T->fields->StashNID>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr REQ<config>::slcrephint_t         SLCRepHint() const noexcept requires is_field_applicable<T->fields->SLCRepHint>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SLCRepHint>()>
                                                        SLCRepHint(typename REQ<config>::slcrephint_t slcRepHint) const noexcept requires is_field_applicable<T->fields->SLCRepHint>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SLCRepHint>()>&
                                                        SetSLCRepHint(typename REQ<config>::slcrephint_t slcRepHint) noexcept requires is_field_applicable<T->fields->SLCRepHint>;
#endif

            constexpr REQ<config>::stashnidvalid_t      StashNIDValid() const noexcept requires is_field_applicable<T->fields->StashNIDValid>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashNIDValid>()>
                                                        StashNIDValid(typename REQ<config>::stashnidvalid_t stashNIDValid) const noexcept requires is_field_applicable<T->fields->StashNIDValid>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashNIDValid>()>&
                                                        SetStashNIDValid(typename REQ<config>::stashnidvalid_t stashNIDValid) noexcept requires is_field_applicable<T->fields->StashNIDValid>;

            constexpr REQ<config>::endian_t             Endian() const noexcept requires is_field_applicable<T->fields->Endian>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Endian>()>
                                                        Endian(typename REQ<config>::endian_t endian) const noexcept requires is_field_applicable<T->fields->Endian>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Endian>()>&
                                                        SetEndian(typename REQ<config>::endian_t endian) noexcept requires is_field_applicable<T->fields->Endian>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr REQ<config>::deep_t               Deep() const noexcept requires is_field_applicable<T->fields->Deep>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Deep>()>
                                                        Deep(typename REQ<config>::deep_t deep) const noexcept requires is_field_applicable<T->fields->Deep>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Deep>()>&
                                                        SetDeep(typename REQ<config>::deep_t deep) noexcept requires is_field_applicable<T->fields->Deep>;
#endif

            constexpr REQ<config>::returntxnid_t        ReturnTxnID() const noexcept requires is_field_applicable<T->fields->ReturnTxnID>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ReturnTxnID>()>
                                                        ReturnTxnID(typename REQ<config>::returntxnid_t returnTxnID) const noexcept requires is_field_applicable<T->fields->ReturnTxnID>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ReturnTxnID>()>&
                                                        SetReturnTxnID(typename REQ<config>::returntxnid_t returnTxnID) noexcept requires is_field_applicable<T->fields->ReturnTxnID>;

            constexpr REQ<config>::stashlpidvalid_t     StashLPIDValid() const noexcept requires is_field_applicable<T->fields->StashLPIDValid>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashLPIDValid>()>
                                                        StashLPIDValid(typename REQ<config>::stashlpidvalid_t stashLPIDValid) const noexcept requires is_field_applicable<T->fields->StashLPIDValid>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashLPIDValid>()>&
                                                        SetStashLPIDValid(typename REQ<config>::stashlpidvalid_t stashLPIDValid) noexcept requires is_field_applicable<T->fields->StashLPIDValid>;

            constexpr REQ<config>::stashlpid_t          StashLPID() const noexcept requires is_field_applicable<T->fields->StashLPID>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashLPID>()>
                                                        StashLPID(typename REQ<config>::stashlpid_t stashLPID) const noexcept requires is_field_applicable<T->fields->StashLPID>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashLPID>()>&
                                                        SetStashLPID(typename REQ<config>::stashlpid_t stashLPID) noexcept requires is_field_applicable<T->fields->StashLPID>;

            constexpr REQ<config>::opcode_t             Opcode() const noexcept requires is_field_applicable<T->fields->Opcode>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Opcode>()>
                                                        Opcode(typename REQ<config>::opcode_t opcode) const noexcept requires is_field_applicable<T->fields->Opcode>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Opcode>()>&
                                                        SetOpcode(typename REQ<config>::opcode_t opcode) noexcept requires is_field_applicable<T->fields->Opcode>;

            constexpr REQ<config>::ssize_t              Size() const noexcept requires is_field_applicable<T->fields->Size>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Size>()>
                                                        Size(typename REQ<config>::ssize_t size) const noexcept requires is_field_applicable<T->fields->Size>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Size>()>&
                                                        SetSize(typename REQ<config>::ssize_t size) noexcept requires is_field_applicable<T->fields->Size>;

            constexpr REQ<config>::addr_t               Addr() const noexcept requires is_field_applicable<T->fields->Addr>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Addr>()>
                                                        Addr(typename REQ<config>::addr_t addr) const noexcept requires is_field_applicable<T->fields->Addr>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Addr>()>&
                                                        SetAddr(typename REQ<config>::addr_t addr) noexcept requires is_field_applicable<T->fields->Addr>;

            constexpr REQ<config>::ns_t                 NS() const noexcept requires is_field_applicable<T->fields->NS>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::NS>()>
                                                        NS(typename REQ<config>::ns_t ns) const noexcept requires is_field_applicable<T->fields->NS>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::NS>()>&
                                                        SetNS(typename REQ<config>::ns_t ns) noexcept requires is_field_applicable<T->fields->NS>;

            constexpr REQ<config>::likelyshared_t       LikelyShared() const noexcept requires is_field_applicable<T->fields->LikelyShared>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LikelyShared>()>
                                                        LikelyShared(typename REQ<config>::likelyshared_t likelyShared) const noexcept requires is_field_applicable<T->fields->LikelyShared>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LikelyShared>()>&
                                                        SetLikelyShared(typename REQ<config>::likelyshared_t likelyShared) noexcept requires is_field_applicable<T->fields->LikelyShared>;

            constexpr REQ<config>::allowretry_t         AllowRetry() const noexcept requires is_field_applicable<T->fields->AllowRetry>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::AllowRetry>()>
                                                        AllowRetry(typename REQ<config>::allowretry_t allowRetry) const noexcept requires is_field_applicable<T->fields->AllowRetry>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::AllowRetry>()>&
                                                        SetAllowRetry(typename REQ<config>::allowretry_t allowRetry) noexcept requires is_field_applicable<T->fields->AllowRetry>;

            constexpr REQ<config>::order_t              Order() const noexcept requires is_field_applicable<T->fields->Order>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Order>()>
                                                        Order(typename REQ<config>::order_t order) const noexcept requires is_field_applicable<T->fields->Order>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Order>()>&
                                                        SetOrder(typename REQ<config>::order_t order) noexcept requires is_field_applicable<T->fields->Order>;

            constexpr REQ<config>::pcrdtype_t           PCrdType() const noexcept requires is_field_applicable<T->fields->PCrdType>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::PCrdType>()>
                                                        PCrdType(typename REQ<config>::pcrdtype_t pCrdType) const noexcept requires is_field_applicable<T->fields->PCrdType>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::PCrdType>()>&
                                                        SetPCrdType(typename REQ<config>::pcrdtype_t pCrdType) noexcept requires is_field_applicable<T->fields->PCrdType>;

            constexpr REQ<config>::memattr_t            MemAttr() const noexcept requires is_memattr_applicable<T>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::MemAttr>()>
                                                        MemAttr(typename REQ<config>::memattr_t memAttr) const noexcept requires is_memattr_applicable<T>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::MemAttr>()>&
                                                        SetMemAttr(typename REQ<config>::memattr_t memAttr) noexcept requires is_memattr_applicable<T>;

            constexpr REQ<config>::snpattr_t            SnpAttr() const noexcept requires is_field_applicable<T->fields->SnpAttr>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SnpAttr>()>
                                                        SnpAttr(typename REQ<config>::snpattr_t snpAttr) const noexcept requires is_field_applicable<T->fields->SnpAttr>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SnpAttr>()>&
                                                        SetSnpAttr(typename REQ<config>::snpattr_t snpAttr) noexcept requires is_field_applicable<T->fields->SnpAttr>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr REQ<config>::dodwt_t              DoDWT() const noexcept requires is_field_applicable<T->fields->DoDWT>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::DoDWT>()>
                                                        DoDWT(typename REQ<config>::dodwt_t doDWT) const noexcept requires is_field_applicable<T->fields->DoDWT>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::DoDWT>()>&
                                                        SetDoDWT(typename REQ<config>::dodwt_t doDWT) noexcept requires is_field_applicable<T->fields->DoDWT>;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr REQ<config>::pgroupid_t           PGroupID() const noexcept requires is_field_applicable<T->fields->PGroupID>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::PGroupID>()>
                                                        PGroupID(typename REQ<config>::pgroupid_t pGroupID) const noexcept requires is_field_applicable<T->fields->PGroupID>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::PGroupID>()>&
                                                        SetPGroupID(typename REQ<config>::pgroupid_t pGroupID) noexcept requires is_field_applicable<T->fields->PGroupID>;

            constexpr REQ<config>::stashgroupid_t       StashGroupID() const noexcept requires is_field_applicable<T->fields->StashGroupID>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashGroupID>()>
                                                        StashGroupID(typename REQ<config>::stashgroupid_t stashGroupID) const noexcept requires is_field_applicable<T->fields->StashGroupID>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashGroupID>()>&
                                                        SetStashGroupID(typename REQ<config>::stashgroupid_t stashGroupID) noexcept requires is_field_applicable<T->fields->StashGroupID>;

            constexpr REQ<config>::taggroupid_t         TagGroupID() const noexcept requires is_field_applicable<T->fields->TagGroupID>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TagGroupID>()>
                                                        TagGroupID(typename REQ<config>::taggroupid_t tagGroupID) const noexcept requires is_field_applicable<T->fields->TagGroupID>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TagGroupID>()>&
                                                        SetTagGroupID(typename REQ<config>::taggroupid_t tagGroupID) noexcept requires is_field_applicable<T->fields->TagGroupID>;
#endif

            constexpr REQ<config>::lpid_t               LPID() const noexcept requires is_field_applicable<T->fields->LPID>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LPID>()>
                                                        LPID(typename REQ<config>::lpid_t lpid) const noexcept requires is_field_applicable<T->fields->LPID>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LPID>()>&
                                                        SetLPID(typename REQ<config>::lpid_t lpid) noexcept requires is_field_applicable<T->fields->LPID>;

            constexpr REQ<config>::excl_t               Excl() const noexcept requires is_field_applicable<T->fields->Excl>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Excl>()>
                                                        Excl(typename REQ<config>::excl_t excl) const noexcept requires is_field_applicable<T->fields->Excl>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Excl>()>&
                                                        SetExcl(typename REQ<config>::excl_t excl) noexcept requires is_field_applicable<T->fields->Excl>;

            constexpr REQ<config>::snoopme_t            SnoopMe() const noexcept requires is_field_applicable<T->fields->SnoopMe>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SnoopMe>()>
                                                        SnoopMe(typename REQ<config>::snoopme_t snoopMe) const noexcept requires is_field_applicable<T->fields->SnoopMe>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SnoopMe>()>&
                                                        SetSnoopMe(typename REQ<config>::snoopme_t snoopMe) noexcept requires is_field_applicable<T->fields->SnoopMe>;

            constexpr REQ<config>::expcompack_t         ExpCompAck() const noexcept requires is_field_applicable<T->fields->ExpCompAck>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ExpCompAck>()>
                                                        ExpCompAck(typename REQ<config>::expcompack_t expCompAck) const noexcept requires is_field_applicable<T->fields->ExpCompAck>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ExpCompAck>()>&
                                                        SetExpCompAck(typename REQ<config>::expcompack_t expCompAck) noexcept requires is_field_applicable<T->fields->ExpCompAck>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr REQ<config>::tagop_t              TagOp() const noexcept requires is_field_applicable<T->fields->TagOp>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TagOp>()>
                                                        TagOp(typename REQ<config>::tagop_t tagOp) const noexcept requires is_field_applicable<T->fields->TagOp>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TagOp>()>&
                                                        SetTagOp(typename REQ<config>::tagop_t tagOp) noexcept requires is_field_applicable<T->fields->TagOp>;
#endif

            constexpr REQ<config>::tracetag_t           TraceTag() const noexcept requires is_field_applicable<T->fields->TraceTag>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TraceTag>()>
                                                        TraceTag(typename REQ<config>::tracetag_t traceTag) const noexcept requires is_field_applicable<T->fields->TraceTag>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TraceTag>()>&
                                                        SetTraceTag(typename REQ<config>::tracetag_t traceTag) noexcept requires is_field_applicable<T->fields->TraceTag>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr REQ<config>::mpam_t               MPAM() const noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<REQ<config>>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::MPAM>()>
                                                        MPAM(typename REQ<config>::mpam_t mpam) const noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<REQ<config>>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::MPAM>()>&
                                                        SetMPAM(typename REQ<config>::mpam_t mpam) noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<REQ<config>>;
#endif

            constexpr REQ<config>::rsvdc_t              RSVDC() const noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<REQ<config>>;
            consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::RSVDC>()>
                                                        RSVDC(typename REQ<config>::rsvdc_t rsvdc) const noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<REQ<config>>;
            constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::RSVDC>()>&
                                                        SetRSVDC(typename REQ<config>::rsvdc_t rsvdc) noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<REQ<config>>;

            //            
            consteval REQ<config>                       Eval() const noexcept requires is_buildable<able>;
            constexpr REQ<config>                       Build() const noexcept requires is_buildable<able>;

            consteval REQ<config>                       EvalPartial() const noexcept;
            constexpr REQ<config>                       BuildPartial() const noexcept;
        };
        
        template<REQBuildTargetEnum T, REQFlitConfigurationConcept config>
        consteval REQFlitBuilder<T, config, MakeREQFlitBuildability<T, config>()> MakeREQFlitBuilder() noexcept;

        // patching common field writes
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::returnnid_t PCFReturnNIDFromStashNID(typename REQ<config>::returnnid_t, typename REQ<config>::stashnid_t) noexcept;

#ifdef CHI_ISSUE_EB_ENABLE
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::returnnid_t PCFReturnNIDFromSLCRepHint(typename REQ<config>::returnnid_t, typename REQ<config>::slcrephint_t) noexcept;
#endif

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::stashnidvalid_t PCFStashNIDValidFromEndian(typename REQ<config>::stashnidvalid_t, typename REQ<config>::endian_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::stashnidvalid_t PCFStashNIDValidFromDeep(typename REQ<config>::stashnidvalid_t, typename REQ<config>::deep_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::returntxnid_t PCFReturnTxnIDFromStashLPIDValid(typename REQ<config>::returntxnid_t, typename REQ<config>::stashlpidvalid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::returntxnid_t PCFReturnTxnIDFromStashLPID(typename REQ<config>::returntxnid_t, typename REQ<config>::stashlpid_t) noexcept;

#ifdef CHI_ISSUE_EB_ENABLE
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::snpattr_t PCFSnpAttrFromDoDWT(typename REQ<config>::snpattr_t, typename REQ<config>::dodwt_t) noexcept;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromStashGroupID(typename REQ<config>::pgroupid_t, typename REQ<config>::stashgroupid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromTagGroupID(typename REQ<config>::pgroupid_t, typename REQ<config>::taggroupid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromLPID(typename REQ<config>::pgroupid_t, typename REQ<config>::lpid_t) noexcept;
#endif

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::excl_t PCFExclFromSnoopMe(typename REQ<config>::excl_t, typename REQ<config>::snoopme_t) noexcept;
    
        // patching common field reads
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::stashnid_t PCFReturnNIDToStashNID(typename REQ<config>::returnnid_t) noexcept;

#ifdef CHI_ISSUE_EB_ENABLE
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::slcrephint_t PCFReturnNIDToSLCRepHint(typename REQ<config>::returnnid_t) noexcept;
#endif

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::endian_t PCFStashNIDValidToEndian(typename REQ<config>::stashnidvalid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::deep_t PCFStashNIDValidToDeep(typename REQ<config>::stashnidvalid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::stashlpidvalid_t PCFReturnTxnIDToStashLPIDValid(typename REQ<config>::returntxnid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::stashlpid_t PCFReturnTxnIDToStashLPID(typename REQ<config>::returntxnid_t) noexcept;

#ifdef CHI_ISSUE_EB_ENABLE
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::dodwt_t PCFSnpAttrToDoDWT(typename REQ<config>::snpattr_t) noexcept;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::stashgroupid_t PCFPGroupIDToStashGroupID(typename REQ<config>::pgroupid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::taggroupid_t PCFPGroupIDToTagGroupID(typename REQ<config>::pgroupid_t) noexcept;

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::lpid_t PCFPGroupIDToLPID(typename REQ<config>::pgroupid_t) noexcept;
#endif

        template<REQFlitConfigurationConcept config>
        constexpr typename REQ<config>::snoopme_t PCFExclToSnoopMe(typename REQ<config>::excl_t) noexcept;
    }

/*
}
*/


// Implementation of constevals
namespace /*CHI::*/Flits {

    template<Xact::FieldConvention conv>
    inline consteval uint64_t GetDefaultFieldValue() noexcept
    {
        if constexpr (conv == Xact::FieldConvention::A0)
            return 0;
        else if constexpr (conv == Xact::FieldConvention::A1)
            return 1;
        else if constexpr (conv == Xact::FieldConvention::I0)
            return 0;
        else if constexpr (conv == Xact::FieldConvention::X)
            return 0; // can take any value here, leave it 0 as default
        else if constexpr (conv == Xact::FieldConvention::Y)
            return 0; // applicable, leave to 0 as default
        else if constexpr (conv == Xact::FieldConvention::B8)
            return Sizes::B8;
        else if constexpr (conv == Xact::FieldConvention::B64)
            return Sizes::B64;
        else if constexpr (conv == Xact::FieldConvention::S)
            return 0; // do not set default value
        else if constexpr (conv == Xact::FieldConvention::D)
            return 0; // associated with NS field, leave it 0 as default
        else
            return 0;
    }

    template<Xact::FieldConvention conv>
    inline consteval bool HasDefaultFieldValue() noexcept
    {
        if constexpr (conv == Xact::FieldConvention::X)
            return false;
        else if constexpr (conv == Xact::FieldConvention::Y)
            return false;
        else if constexpr (conv == Xact::FieldConvention::S)
            return false;
        else
            return true;
    }

    template<Xact::FieldConvention conv>
    inline consteval bool IsFieldValueStableOrInapplicable() noexcept
    {
        if constexpr (conv == Xact::FieldConvention::A0)
            return true;
        else if constexpr (conv == Xact::FieldConvention::A1)
            return true;
        else if constexpr (conv == Xact::FieldConvention::I0)
            return true;
        else if constexpr (conv == Xact::FieldConvention::X)
            return true;
        else if constexpr (conv == Xact::FieldConvention::B8)
            return true;
        else if constexpr (conv == Xact::FieldConvention::B64)
            return true;
        else if constexpr (conv == Xact::FieldConvention::S)
            return true;
        else if constexpr (conv == Xact::FieldConvention::D)
            return true;
        else
            return false;
    }

    template<REQBuildTargetEnum T>
    inline consteval MemAttr GetDefaultMemAttrValue() noexcept
    {
        MemAttr attr = 0;
        if constexpr (GetDefaultFieldValue<T->fields->EWA>())       attr |= MemAttrs::EWA;
        if constexpr (GetDefaultFieldValue<T->fields->Device>())    attr |= MemAttrs::Device;
        if constexpr (GetDefaultFieldValue<T->fields->Cacheable>()) attr |= MemAttrs::Cacheable;
        if constexpr (GetDefaultFieldValue<T->fields->Allocate>())  attr |= MemAttrs::Allocate;
        return attr;
    }
}


// Implementation of common field patching write functions
namespace /*CHI::*/Flits {

    #define _PCF_WRITE(src, srcVal, dst, dstVal) \
        (  (uint64_t(dstVal) & ~(((0x1ULL << REQ<config>::src##_WIDTH) - 1) << (REQ<config>::src##_LSB - REQ<config>::dst##_LSB))) \
        | ((uint64_t(srcVal) & ((0x1ULL << REQ<config>::src##_WIDTH) - 1)) << (REQ<config>::src##_LSB - REQ<config>::dst##_LSB)))

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::returnnid_t PCFReturnNIDFromStashNID(typename REQ<config>::returnnid_t dst, typename REQ<config>::stashnid_t src) noexcept
    { return _PCF_WRITE(STASHNID, src, RETURNNID, dst); }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::returnnid_t PCFReturnNIDFromSLCRepHint(typename REQ<config>::returnnid_t dst, typename REQ<config>::slcrephint_t src) noexcept
    { return _PCF_WRITE(SLCREPHINT, src, RETURNNID, dst); }
#endif

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::stashnidvalid_t PCFStashNIDValidFromEndian(typename REQ<config>::stashnidvalid_t dst, typename REQ<config>::endian_t src) noexcept
    { return _PCF_WRITE(ENDIAN, src, STASHNIDVALID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::stashnidvalid_t PCFStashNIDValidFromDeep(typename REQ<config>::stashnidvalid_t dst, typename REQ<config>::deep_t src) noexcept
    { return _PCF_WRITE(DEEP, src, STASHNIDVALID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::returntxnid_t PCFReturnTxnIDFromStashLPIDValid(typename REQ<config>::returntxnid_t dst, typename REQ<config>::stashlpidvalid_t src) noexcept
    { return _PCF_WRITE(STASHLPIDVALID, src, RETURNTXNID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::returntxnid_t PCFReturnTxnIDFromStashLPID(typename REQ<config>::returntxnid_t dst, typename REQ<config>::stashlpid_t src) noexcept
    { return _PCF_WRITE(STASHLPID, src, RETURNTXNID, dst); }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::snpattr_t PCFSnpAttrFromDoDWT(typename REQ<config>::snpattr_t dst, typename REQ<config>::dodwt_t src) noexcept
    { return _PCF_WRITE(DODWT, src, SNPATTR, dst); }
#endif

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromStashGroupID(typename REQ<config>::pgroupid_t dst, typename REQ<config>::stashgroupid_t src) noexcept
    { return _PCF_WRITE(STASHGROUPID, src, PGROUPID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromTagGroupID(typename REQ<config>::pgroupid_t dst, typename REQ<config>::taggroupid_t src) noexcept
    { return _PCF_WRITE(TAGGROUPID, src, PGROUPID, dst); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::pgroupid_t PCFPGroupIDFromLPID(typename REQ<config>::pgroupid_t dst, typename REQ<config>::lpid_t src) noexcept
    { return _PCF_WRITE(LPID, src, PGROUPID, dst); }
#endif

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::excl_t PCFExclFromSnoopMe(typename REQ<config>::excl_t dst, typename REQ<config>::snoopme_t src) noexcept
    { return _PCF_WRITE(SNOOPME, src, EXCL, dst); }    

    #undef _PCF_WRITE
}

// Implementation of common field patching read functions
namespace /*CHI::*/Flits {

#define _PCF_READ(dst, src, srcVal) \
    ((uint64_t(srcVal) & (((1ULL << REQ<config>::dst##_WIDTH) - 1) << (REQ<config>::dst##_LSB - REQ<config>::src##_LSB))) \
        >> (REQ<config>::dst##_LSB - REQ<config>::src##_LSB))

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::stashnid_t PCFReturnNIDToStashNID(typename REQ<config>::returnnid_t src) noexcept
    { return _PCF_READ(STASHNID, RETURNNID, src); }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::slcrephint_t PCFReturnNIDToSLCRepHint(typename REQ<config>::returnnid_t src) noexcept
    { return _PCF_READ(SLCREPHINT, RETURNNID, src); }
#endif

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::endian_t PCFStashNIDValidToEndian(typename REQ<config>::stashnidvalid_t src) noexcept
    { return _PCF_READ(ENDIAN, STASHNIDVALID, src); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::deep_t PCFStashNIDValidToDeep(typename REQ<config>::stashnidvalid_t src) noexcept
    { return _PCF_READ(DEEP, STASHNIDVALID, src); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::stashlpidvalid_t PCFReturnTxnIDToStashLPIDValid(typename REQ<config>::returntxnid_t src) noexcept
    { return _PCF_READ(STASHLPIDVALID, RETURNTXNID, src); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::stashlpid_t PCFReturnTxnIDToStashLPID(typename REQ<config>::returntxnid_t src) noexcept
    { return _PCF_READ(STASHLPID, RETURNTXNID, src); }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::dodwt_t PCFSnpAttrToDoDWT(typename REQ<config>::snpattr_t src) noexcept
    { return _PCF_READ(DODWT, SNPATTR, src); }
#endif

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::stashgroupid_t PCFPGroupIDToStashGroupID(typename REQ<config>::pgroupid_t src) noexcept
    { return _PCF_READ(STASHGROUPID, PGROUPID, src); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::taggroupid_t PCFPGroupIDToTagGroupID(typename REQ<config>::pgroupid_t src) noexcept
    { return _PCF_READ(TAGGROUPID, PGROUPID, src); }

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::lpid_t PCFPGroupIDToLPID(typename REQ<config>::pgroupid_t src) noexcept
    { return _PCF_READ(LPID, PGROUPID, src); }
#endif

    template<REQFlitConfigurationConcept config>
    inline constexpr typename REQ<config>::snoopme_t PCFExclToSnoopMe(typename REQ<config>::excl_t src) noexcept
    { return _PCF_READ(SNOOPME, EXCL, src); }

#undef _PCF_READ
}


// Implementation of: class REQFlitBuilder
namespace /*CHI::*/Flits {

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, able>::REQFlitBuilder() noexcept
    {
        REQ<config> flit = { 0 };

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
        _FIELD_INIT(ReturnNID       );
    //  _FIELD_INIT(StashNID        );
        _FIELD_INIT_PCF(StashNID        , ReturnNID);
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT_PCF(SLCRepHint      , ReturnNID);
#endif
        _FIELD_INIT(StashNIDValid   );
        _FIELD_INIT_PCF(Endian          , StashNIDValid);
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT_PCF(Deep            , StashNIDValid);
#endif
        _FIELD_INIT(ReturnTxnID     );
        _FIELD_INIT_PCF(StashLPIDValid  , ReturnTxnID);
        _FIELD_INIT_PCF(StashLPID       , ReturnTxnID);
        _FIELD_INIT(Opcode          );
        _FIELD_INIT(Size            );
        _FIELD_INIT(Addr            );
        _FIELD_INIT(NS              );
        _FIELD_INIT(LikelyShared    );
        _FIELD_INIT(AllowRetry      );
        _FIELD_INIT(Order           );
        _FIELD_INIT(PCrdType        );
    //  _FIELD_INIT(MemAttr         );
        flit.MemAttr() = GetDefaultMemAttrValue<T>();
        _FIELD_INIT(SnpAttr         );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT_PCF(DoDWT           , SnpAttr);
#endif
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT(PGroupID        );
        _FIELD_INIT_PCF(StashGroupID    , PGroupID);
        _FIELD_INIT_PCF(TagGroupID      , PGroupID);
        _FIELD_INIT_PCF(LPID            , PGroupID);
#endif
#ifdef CHI_ISSUE_B_ENABLE
        _FIELD_INIT(LPID            );
#endif
        _FIELD_INIT(Excl            );
        _FIELD_INIT_PCF(SnoopMe         , Excl);
        _FIELD_INIT(ExpCompAck      );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT(TagOp           );
#endif
        _FIELD_INIT(TraceTag        );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_INIT_OPT(MPAM);
#endif
        _FIELD_INIT_OPT(RSVDC);

#undef _INIT_IF
#undef _FIELD_INIT
#undef _FIELD_INIT_PCF
#undef _FIELD_INIT_OPT

        this->flit = flit;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    template<REQFlitBuildability nextAble>
    inline constexpr REQFlitBuilder<T, config, able>::REQFlitBuilder(const REQFlitBuilder<T, config, nextAble>& obj) noexcept
    {
        this->flit = obj.flit;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQ<config>::qos_t REQFlitBuilder<T, config, able>::QoS() const noexcept requires is_field_applicable<T->fields->QoS>
    {
        return flit.QoS();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::QoS>()>
            REQFlitBuilder<T, config, able>::QoS(typename REQ<config>::qos_t qos) const noexcept requires is_field_applicable<T->fields->QoS>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::QoS>()>(*this);
        builder.flit.QoS() = qos;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::QoS>()>&
            REQFlitBuilder<T, config, able>::SetQoS(typename REQ<config>::qos_t qos) noexcept requires is_field_applicable<T->fields->QoS>
    {
        this->flit.QoS() = qos;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::QoS>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::tgtid_t REQFlitBuilder<T, config, able>::TgtID() const noexcept requires is_field_applicable<T->fields->TgtID>
    {
        return flit.TgtID();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TgtID>()>
            REQFlitBuilder<T, config, able>::TgtID(typename REQ<config>::tgtid_t tgtID) const noexcept requires is_field_applicable<T->fields->TgtID>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TgtID>()>(*this);
        builder.flit.TgtID() = tgtID;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TgtID>()>&
            REQFlitBuilder<T, config, able>::SetTgtID(typename REQ<config>::tgtid_t tgtID) noexcept requires is_field_applicable<T->fields->TgtID>
    {
        this->flit.TgtID() = tgtID;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TgtID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::srcid_t REQFlitBuilder<T, config, able>::SrcID() const noexcept requires is_field_applicable<T->fields->SrcID>
    {
        return flit.SrcID();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SrcID>()>
            REQFlitBuilder<T, config, able>::SrcID(typename REQ<config>::srcid_t srcID) const noexcept requires is_field_applicable<T->fields->SrcID>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SrcID>()>(*this);
        builder.flit.SrcID() = srcID;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SrcID>()>&
            REQFlitBuilder<T, config, able>::SetSrcID(typename REQ<config>::srcid_t srcID) noexcept requires is_field_applicable<T->fields->SrcID>
    {
        this->flit.SrcID() = srcID;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SrcID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::txnid_t REQFlitBuilder<T, config, able>::TxnID() const noexcept requires is_field_applicable<T->fields->TxnID>
    {
        return flit.TxnID();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TxnID>()>
            REQFlitBuilder<T, config, able>::TxnID(typename REQ<config>::txnid_t txnID) const noexcept requires is_field_applicable<T->fields->TxnID>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TxnID>()>(*this);
        builder.flit.TxnID() = txnID;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TxnID>()>&
            REQFlitBuilder<T, config, able>::SetTxnID(typename REQ<config>::txnid_t txnID) noexcept requires is_field_applicable<T->fields->TxnID>
    {
        this->flit.TxnID() = txnID;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TxnID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::returnnid_t REQFlitBuilder<T, config, able>::ReturnNID() const noexcept requires is_field_applicable<T->fields->ReturnNID>
    {
        return flit.ReturnNID();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ReturnNID>()>
            REQFlitBuilder<T, config, able>::ReturnNID(typename REQ<config>::returnnid_t returnNID) const noexcept requires is_field_applicable<T->fields->ReturnNID>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ReturnNID>()>(*this);
        builder.flit.ReturnNID() = returnNID;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ReturnNID>()>&
            REQFlitBuilder<T, config, able>::SetReturnNID(typename REQ<config>::returnnid_t returnNID) noexcept requires is_field_applicable<T->fields->ReturnNID>
    {
        this->flit.ReturnNID() = returnNID;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ReturnNID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::stashnid_t REQFlitBuilder<T, config, able>::StashNID() const noexcept requires is_field_applicable<T->fields->StashNID>
    {
        return PCFReturnNIDToStashNID<config>(flit.ReturnNID());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashNID>()>
            REQFlitBuilder<T, config, able>::StashNID(typename REQ<config>::stashnid_t stashNID) const noexcept requires is_field_applicable<T->fields->StashNID>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashNID>()>(*this);
        builder.flit.ReturnNID() = PCFReturnNIDFromStashNID<config>(builder.flit.ReturnNID(), stashNID);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashNID>()>&
            REQFlitBuilder<T, config, able>::SetStashNID(typename REQ<config>::stashnid_t stashNID) noexcept requires is_field_applicable<T->fields->StashNID>
    {
        this->flit.ReturnNID() = PCFReturnNIDFromStashNID<config>(this->flit.ReturnNID(), stashNID);
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashNID>()>*>(this);
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::slcrephint_t REQFlitBuilder<T, config, able>::SLCRepHint() const noexcept requires is_field_applicable<T->fields->SLCRepHint>
    {
        return PCFReturnNIDToSLCRepHint<config>(flit.ReturnNID());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SLCRepHint>()>
            REQFlitBuilder<T, config, able>::SLCRepHint(typename REQ<config>::slcrephint_t slcRepHint) const noexcept requires is_field_applicable<T->fields->SLCRepHint>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SLCRepHint>()>(*this);
        builder.flit.ReturnNID() = PCFReturnNIDFromSLCRepHint<config>(builder.flit.ReturnNID(), slcRepHint);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SLCRepHint>()>&
            REQFlitBuilder<T, config, able>::SetSLCRepHint(typename REQ<config>::slcrephint_t slcRepHint) noexcept requires is_field_applicable<T->fields->SLCRepHint>
    {
        this->flit.ReturnNID() = PCFReturnNIDFromSLCRepHint<config>(this->flit.ReturnNID(), slcRepHint);
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SLCRepHint>()>*>(this);
    }
#endif

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::stashnidvalid_t REQFlitBuilder<T, config, able>::StashNIDValid() const noexcept requires is_field_applicable<T->fields->StashNIDValid>
    {
        return flit.StashNIDValid();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashNIDValid>()>
            REQFlitBuilder<T, config, able>::StashNIDValid(typename REQ<config>::stashnidvalid_t stashNIDValid) const noexcept requires is_field_applicable<T->fields->StashNIDValid>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashNIDValid>()>(*this);
        builder.flit.StashNIDValid() = stashNIDValid;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashNIDValid>()>&
            REQFlitBuilder<T, config, able>::SetStashNIDValid(typename REQ<config>::stashnidvalid_t stashNIDValid) noexcept requires is_field_applicable<T->fields->StashNIDValid>
    {
        this->flit.StashNIDValid() = stashNIDValid;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashNIDValid>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::endian_t REQFlitBuilder<T, config, able>::Endian() const noexcept requires is_field_applicable<T->fields->Endian>
    {
        return PCFStashNIDValidToEndian<config>(flit.StashNIDValid());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Endian>()>
            REQFlitBuilder<T, config, able>::Endian(typename REQ<config>::endian_t endian) const noexcept requires is_field_applicable<T->fields->Endian>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Endian>()>(*this);
        builder.flit.StashNIDValid() = PCFStashNIDValidFromEndian<config>(builder.flit.StashNIDValid(), endian);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Endian>()>&
            REQFlitBuilder<T, config, able>::SetEndian(typename REQ<config>::endian_t endian) noexcept requires is_field_applicable<T->fields->Endian>
    {
        this->flit.StashNIDValid() = PCFStashNIDValidFromEndian<config>(this->flit.StashNIDValid(), endian);
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Endian>()>*>(this);
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::deep_t REQFlitBuilder<T, config, able>::Deep() const noexcept requires is_field_applicable<T->fields->Deep>
    {
        return PCFStashNIDValidToDeep<config>(flit.StashNIDValid());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Deep>()>
            REQFlitBuilder<T, config, able>::Deep(typename REQ<config>::deep_t deep) const noexcept requires is_field_applicable<T->fields->Deep>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Deep>()>(*this);
        builder.flit.StashNIDValid() = PCFStashNIDValidFromDeep<config>(builder.flit.StashNIDValid(), deep);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Deep>()>&
            REQFlitBuilder<T, config, able>::SetDeep(typename REQ<config>::deep_t deep) noexcept requires is_field_applicable<T->fields->Deep>
    {
        this->flit.StashNIDValid() = PCFStashNIDValidFromDeep<config>(this->flit.StashNIDValid(), deep);
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Deep>()>*>(this);
    }
#endif

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::returntxnid_t REQFlitBuilder<T, config, able>::ReturnTxnID() const noexcept requires is_field_applicable<T->fields->ReturnTxnID>
    {
        return flit.ReturnTxnID();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ReturnTxnID>()>
            REQFlitBuilder<T, config, able>::ReturnTxnID(typename REQ<config>::returntxnid_t returnTxnID) const noexcept requires is_field_applicable<T->fields->ReturnTxnID>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ReturnTxnID>()>(*this);
        builder.flit.ReturnTxnID() = returnTxnID;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ReturnTxnID>()>&
            REQFlitBuilder<T, config, able>::SetReturnTxnID(typename REQ<config>::returntxnid_t returnTxnID) noexcept requires is_field_applicable<T->fields->ReturnTxnID>
    {
        this->flit.ReturnTxnID() = returnTxnID;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ReturnTxnID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::stashlpidvalid_t REQFlitBuilder<T, config, able>::StashLPIDValid() const noexcept requires is_field_applicable<T->fields->StashLPIDValid>
    {
        return PCFReturnTxnIDToStashLPIDValid<config>(flit.ReturnTxnID());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashLPIDValid>()>
            REQFlitBuilder<T, config, able>::StashLPIDValid(typename REQ<config>::stashlpidvalid_t stashLPIDValid) const noexcept requires is_field_applicable<T->fields->StashLPIDValid>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashLPIDValid>()>(*this);
        builder.flit.ReturnTxnID() = PCFReturnTxnIDFromStashLPIDValid<config>(builder.flit.ReturnTxnID(), stashLPIDValid);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashLPIDValid>()>&
            REQFlitBuilder<T, config, able>::SetStashLPIDValid(typename REQ<config>::stashlpidvalid_t stashLPIDValid) noexcept requires is_field_applicable<T->fields->StashLPIDValid>
    {
        this->flit.ReturnTxnID() = PCFReturnTxnIDFromStashLPIDValid<config>(this->flit.ReturnTxnID(), stashLPIDValid);
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashLPIDValid>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::stashlpid_t REQFlitBuilder<T, config, able>::StashLPID() const noexcept requires is_field_applicable<T->fields->StashLPID>
    {
        return PCFReturnTxnIDToStashLPID<config>(flit.ReturnTxnID());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashLPID>()>
            REQFlitBuilder<T, config, able>::StashLPID(typename REQ<config>::stashlpid_t stashLPID) const noexcept requires is_field_applicable<T->fields->StashLPID>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashLPID>()>(*this);
        builder.flit.ReturnTxnID() = PCFReturnTxnIDFromStashLPID<config>(builder.flit.ReturnTxnID(), stashLPID);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashLPID>()>&
            REQFlitBuilder<T, config, able>::SetStashLPID(typename REQ<config>::stashlpid_t stashLPID) noexcept requires is_field_applicable<T->fields->StashLPID>
    {
        this->flit.ReturnTxnID() = PCFReturnTxnIDFromStashLPID<config>(this->flit.ReturnTxnID(), stashLPID);
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashLPID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::opcode_t REQFlitBuilder<T, config, able>::Opcode() const noexcept requires is_field_applicable<T->fields->Opcode>
    {
        return flit.Opcode();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Opcode>()>
            REQFlitBuilder<T, config, able>::Opcode(typename REQ<config>::opcode_t opcode) const noexcept requires is_field_applicable<T->fields->Opcode>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Opcode>()>(*this);
        builder.flit.Opcode() = opcode;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Opcode>()>&
            REQFlitBuilder<T, config, able>::SetOpcode(typename REQ<config>::opcode_t opcode) noexcept requires is_field_applicable<T->fields->Opcode>
    {
        this->flit.Opcode() = opcode;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Opcode>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::ssize_t REQFlitBuilder<T, config, able>::Size() const noexcept requires is_field_applicable<T->fields->Size>
    {
        return flit.Size();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Size>()>
            REQFlitBuilder<T, config, able>::Size(typename REQ<config>::ssize_t size) const noexcept requires is_field_applicable<T->fields->Size>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Size>()>(*this);
        builder.flit.Size() = size;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Size>()>&
            REQFlitBuilder<T, config, able>::SetSize(typename REQ<config>::ssize_t size) noexcept requires is_field_applicable<T->fields->Size>
    {
        this->flit.Size() = size;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Size>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::addr_t REQFlitBuilder<T, config, able>::Addr() const noexcept requires is_field_applicable<T->fields->Addr>
    {
        return flit.Addr();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Addr>()>
            REQFlitBuilder<T, config, able>::Addr(typename REQ<config>::addr_t addr) const noexcept requires is_field_applicable<T->fields->Addr>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Addr>()>(*this);
        builder.flit.Addr() = addr;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Addr>()>&
            REQFlitBuilder<T, config, able>::SetAddr(typename REQ<config>::addr_t addr) noexcept requires is_field_applicable<T->fields->Addr>
    {
        this->flit.Addr() = addr;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Addr>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::ns_t REQFlitBuilder<T, config, able>::NS() const noexcept requires is_field_applicable<T->fields->NS>
    {
        return flit.NS();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::NS>()>
            REQFlitBuilder<T, config, able>::NS(typename REQ<config>::ns_t ns) const noexcept requires is_field_applicable<T->fields->NS>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::NS>()>(*this);
        builder.flit.NS() = ns;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::NS>()>&
            REQFlitBuilder<T, config, able>::SetNS(typename REQ<config>::ns_t ns) noexcept requires is_field_applicable<T->fields->NS>
    {
        this->flit.NS() = ns;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::NS>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::likelyshared_t REQFlitBuilder<T, config, able>::LikelyShared() const noexcept requires is_field_applicable<T->fields->LikelyShared>
    {
        return flit.LikelyShared();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LikelyShared>()>
            REQFlitBuilder<T, config, able>::LikelyShared(typename REQ<config>::likelyshared_t likelyShared) const noexcept requires is_field_applicable<T->fields->LikelyShared>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LikelyShared>()>(*this);
        builder.flit.LikelyShared() = likelyShared;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LikelyShared>()>&
            REQFlitBuilder<T, config, able>::SetLikelyShared(typename REQ<config>::likelyshared_t likelyShared) noexcept requires is_field_applicable<T->fields->LikelyShared>
    {
        this->flit.LikelyShared() = likelyShared;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LikelyShared>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::allowretry_t REQFlitBuilder<T, config, able>::AllowRetry() const noexcept requires is_field_applicable<T->fields->AllowRetry>
    {
        return flit.AllowRetry();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::AllowRetry>()>
            REQFlitBuilder<T, config, able>::AllowRetry(typename REQ<config>::allowretry_t allowRetry) const noexcept requires is_field_applicable<T->fields->AllowRetry>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::AllowRetry>()>(*this);
        builder.flit.AllowRetry() = allowRetry;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::AllowRetry>()>&
            REQFlitBuilder<T, config, able>::SetAllowRetry(typename REQ<config>::allowretry_t allowRetry) noexcept requires is_field_applicable<T->fields->AllowRetry>
    {
        this->flit.AllowRetry() = allowRetry;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::AllowRetry>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::order_t REQFlitBuilder<T, config, able>::Order() const noexcept requires is_field_applicable<T->fields->Order>
    {
        return flit.Order();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Order>()>
            REQFlitBuilder<T, config, able>::Order(typename REQ<config>::order_t order) const noexcept requires is_field_applicable<T->fields->Order>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Order>()>(*this);
        builder.flit.Order() = order;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Order>()>&
            REQFlitBuilder<T, config, able>::SetOrder(typename REQ<config>::order_t order) noexcept requires is_field_applicable<T->fields->Order>
    {
        this->flit.Order() = order;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Order>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::pcrdtype_t REQFlitBuilder<T, config, able>::PCrdType() const noexcept requires is_field_applicable<T->fields->PCrdType>
    {
        return flit.PCrdType();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::PCrdType>()>
            REQFlitBuilder<T, config, able>::PCrdType(typename REQ<config>::pcrdtype_t pCrdType) const noexcept requires is_field_applicable<T->fields->PCrdType>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::PCrdType>()>(*this);
        builder.flit.PCrdType() = pCrdType;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::PCrdType>()>&
            REQFlitBuilder<T, config, able>::SetPCrdType(typename REQ<config>::pcrdtype_t pCrdType) noexcept requires is_field_applicable<T->fields->PCrdType>
    {
        this->flit.PCrdType() = pCrdType;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::PCrdType>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::memattr_t REQFlitBuilder<T, config, able>::MemAttr() const noexcept requires is_memattr_applicable<T>
    {
        return flit.MemAttr();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::MemAttr>()>
            REQFlitBuilder<T, config, able>::MemAttr(typename REQ<config>::memattr_t memAttr) const noexcept requires is_memattr_applicable<T>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::MemAttr>()>(*this);
        builder.flit.MemAttr() = memAttr;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::MemAttr>()>&
            REQFlitBuilder<T, config, able>::SetMemAttr(typename REQ<config>::memattr_t memAttr) noexcept requires is_memattr_applicable<T>
    {
        this->flit.MemAttr() = memAttr;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::MemAttr>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::snpattr_t REQFlitBuilder<T, config, able>::SnpAttr() const noexcept requires is_field_applicable<T->fields->SnpAttr>
    {
        return flit.SnpAttr();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SnpAttr>()>
            REQFlitBuilder<T, config, able>::SnpAttr(typename REQ<config>::snpattr_t snpAttr) const noexcept requires is_field_applicable<T->fields->SnpAttr>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SnpAttr>()>(*this);
        builder.flit.SnpAttr() = snpAttr;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SnpAttr>()>&
            REQFlitBuilder<T, config, able>::SetSnpAttr(typename REQ<config>::snpattr_t snpAttr) noexcept requires is_field_applicable<T->fields->SnpAttr>
    {
        this->flit.SnpAttr() = snpAttr;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SnpAttr>()>*>(this);
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::dodwt_t REQFlitBuilder<T, config, able>::DoDWT() const noexcept requires is_field_applicable<T->fields->DoDWT>
    {
        return PCFSnpAttrToDoDWT<config>(flit.SnpAttr());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::DoDWT>()>
            REQFlitBuilder<T, config, able>::DoDWT(typename REQ<config>::dodwt_t doDWT) const noexcept requires is_field_applicable<T->fields->DoDWT>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::DoDWT>()>(*this);
        builder.flit.SnpAttr() = PCFSnpAttrFromDoDWT<config>(builder.flit.SnpAttr(), doDWT);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::DoDWT>()>&
            REQFlitBuilder<T, config, able>::SetDoDWT(typename REQ<config>::dodwt_t doDWT) noexcept requires is_field_applicable<T->fields->DoDWT>
    {
        this->flit.SnpAttr() = PCFSnpAttrFromDoDWT<config>(this->flit.SnpAttr(), doDWT);
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::DoDWT>()>*>(this);
    }
#endif

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::pgroupid_t REQFlitBuilder<T, config, able>::PGroupID() const noexcept requires is_field_applicable<T->fields->PGroupID>
    {
        return flit.PGroupID();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::PGroupID>()>
            REQFlitBuilder<T, config, able>::PGroupID(typename REQ<config>::pgroupid_t pGroupID) const noexcept requires is_field_applicable<T->fields->PGroupID>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::PGroupID>()>(*this);
        builder.flit.PGroupID() = pGroupID;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::PGroupID>()>&
            REQFlitBuilder<T, config, able>::SetPGroupID(typename REQ<config>::pgroupid_t pGroupID) noexcept requires is_field_applicable<T->fields->PGroupID>
    {
        this->flit.PGroupID() = pGroupID;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::PGroupID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::stashgroupid_t REQFlitBuilder<T, config, able>::StashGroupID() const noexcept requires is_field_applicable<T->fields->StashGroupID>
    {
        return PCFPGroupIDToStashGroupID<config>(flit.PGroupID());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashGroupID>()>
            REQFlitBuilder<T, config, able>::StashGroupID(typename REQ<config>::stashgroupid_t stashGroupID) const noexcept requires is_field_applicable<T->fields->StashGroupID>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashGroupID>()>(*this);
        builder.flit.PGroupID() = PCFPGroupIDFromStashGroupID<config>(builder.flit.PGroupID(), stashGroupID);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashGroupID>()>&
            REQFlitBuilder<T, config, able>::SetStashGroupID(typename REQ<config>::stashgroupid_t stashGroupID) noexcept requires is_field_applicable<T->fields->StashGroupID>
    {
        this->flit.PGroupID() = PCFPGroupIDFromStashGroupID<config>(this->flit.PGroupID(), stashGroupID);
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::StashGroupID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::taggroupid_t REQFlitBuilder<T, config, able>::TagGroupID() const noexcept requires is_field_applicable<T->fields->TagGroupID>
    {
        return PCFPGroupIDToTagGroupID<config>(flit.PGroupID());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TagGroupID>()>
            REQFlitBuilder<T, config, able>::TagGroupID(typename REQ<config>::taggroupid_t tagGroupID) const noexcept requires is_field_applicable<T->fields->TagGroupID>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TagGroupID>()>(*this);
        builder.flit.PGroupID() = PCFPGroupIDFromTagGroupID<config>(builder.flit.PGroupID(), tagGroupID);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TagGroupID>()>&
            REQFlitBuilder<T, config, able>::SetTagGroupID(typename REQ<config>::taggroupid_t tagGroupID) noexcept requires is_field_applicable<T->fields->TagGroupID>
    {
        this->flit.PGroupID() = PCFPGroupIDFromTagGroupID<config>(this->flit.PGroupID(), tagGroupID);
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TagGroupID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::lpid_t REQFlitBuilder<T, config, able>::LPID() const noexcept requires is_field_applicable<T->fields->LPID>
    {
        return PCFPGroupIDToLPID<config>(flit.PGroupID());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LPID>()>
            REQFlitBuilder<T, config, able>::LPID(typename REQ<config>::lpid_t lpid) const noexcept requires is_field_applicable<T->fields->LPID>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LPID>()>(*this);
        builder.flit.PGroupID() = PCFPGroupIDFromLPID<config>(builder.flit.PGroupID(), lpid);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LPID>()>&
            REQFlitBuilder<T, config, able>::SetLPID(typename REQ<config>::lpid_t lpid) noexcept requires is_field_applicable<T->fields->LPID>
    {
        this->flit.PGroupID() = PCFPGroupIDFromLPID<config>(this->flit.PGroupID(), lpid);
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LPID>()>*>(this);
    }

#else
    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::lpid_t REQFlitBuilder<T, config, able>::LPID() const noexcept requires is_field_applicable<T->fields->LPID>
    {
        return flit.LPID();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LPID>()>
            REQFlitBuilder<T, config, able>::LPID(typename REQ<config>::lpid_t lpid) const noexcept requires is_field_applicable<T->fields->LPID>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LPID>()>(*this);
        builder.flit.LPID() = lpid;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LPID>()>&
            REQFlitBuilder<T, config, able>::SetLPID(typename REQ<config>::lpid_t lpid) noexcept requires is_field_applicable<T->fields->LPID>
    {
        this->flit.LPID() = lpid;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::LPID>()>*>(this);
    }
#endif

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::excl_t REQFlitBuilder<T, config, able>::Excl() const noexcept requires is_field_applicable<T->fields->Excl>
    {
        return flit.Excl();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Excl>()>
            REQFlitBuilder<T, config, able>::Excl(typename REQ<config>::excl_t excl) const noexcept requires is_field_applicable<T->fields->Excl>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Excl>()>(*this);
        builder.flit.Excl() = excl;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Excl>()>&
            REQFlitBuilder<T, config, able>::SetExcl(typename REQ<config>::excl_t excl) noexcept requires is_field_applicable<T->fields->Excl>
    {
        this->flit.Excl() = excl;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::Excl>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::snoopme_t REQFlitBuilder<T, config, able>::SnoopMe() const noexcept requires is_field_applicable<T->fields->SnoopMe>
    {
        return PCFExclToSnoopMe<config>(flit.Excl());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SnoopMe>()>
            REQFlitBuilder<T, config, able>::SnoopMe(typename REQ<config>::snoopme_t snoopMe) const noexcept requires is_field_applicable<T->fields->SnoopMe>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SnoopMe>()>(*this);
        builder.flit.Excl() = PCFExclFromSnoopMe<config>(builder.flit.Excl(), snoopMe);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SnoopMe>()>&
            REQFlitBuilder<T, config, able>::SetSnoopMe(typename REQ<config>::snoopme_t snoopMe) noexcept requires is_field_applicable<T->fields->SnoopMe>
    {
        this->flit.Excl() = PCFExclFromSnoopMe<config>(this->flit.Excl(), snoopMe);
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::SnoopMe>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::expcompack_t REQFlitBuilder<T, config, able>::ExpCompAck() const noexcept requires is_field_applicable<T->fields->ExpCompAck>
    {
        return flit.ExpCompAck();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ExpCompAck>()>
            REQFlitBuilder<T, config, able>::ExpCompAck(typename REQ<config>::expcompack_t expCompAck) const noexcept requires is_field_applicable<T->fields->ExpCompAck>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ExpCompAck>()>(*this);
        builder.flit.ExpCompAck() = expCompAck;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ExpCompAck>()>&
            REQFlitBuilder<T, config, able>::SetExpCompAck(typename REQ<config>::expcompack_t expCompAck) noexcept requires is_field_applicable<T->fields->ExpCompAck>
    {
        this->flit.ExpCompAck() = expCompAck;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::ExpCompAck>()>*>(this);
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::tagop_t REQFlitBuilder<T, config, able>::TagOp() const noexcept requires is_field_applicable<T->fields->TagOp>
    {
        return flit.TagOp();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TagOp>()>
            REQFlitBuilder<T, config, able>::TagOp(typename REQ<config>::tagop_t tagOp) const noexcept requires is_field_applicable<T->fields->TagOp>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TagOp>()>(*this);
        builder.flit.TagOp() = tagOp;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TagOp>()>&
            REQFlitBuilder<T, config, able>::SetTagOp(typename REQ<config>::tagop_t tagOp) noexcept requires is_field_applicable<T->fields->TagOp>
    {
        this->flit.TagOp() = tagOp;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TagOp>()>*>(this);
    }
#endif

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::tracetag_t REQFlitBuilder<T, config, able>::TraceTag() const noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        return flit.TraceTag();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TraceTag>()>
            REQFlitBuilder<T, config, able>::TraceTag(typename REQ<config>::tracetag_t traceTag) const noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TraceTag>()>(*this);
        builder.flit.TraceTag() = traceTag;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TraceTag>()>&
            REQFlitBuilder<T, config, able>::SetTraceTag(typename REQ<config>::tracetag_t traceTag) noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        this->flit.TraceTag() = traceTag;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::TraceTag>()>*>(this);
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::mpam_t REQFlitBuilder<T, config, able>::MPAM() const noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<REQ<config>>
    {
        return flit.MPAM();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::MPAM>()>
            REQFlitBuilder<T, config, able>::MPAM(typename REQ<config>::mpam_t mpam) const noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<REQ<config>>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::MPAM>()>(*this);
        builder.flit.MPAM() = mpam;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::MPAM>()>&
            REQFlitBuilder<T, config, able>::SetMPAM(typename REQ<config>::mpam_t mpam) noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<REQ<config>>
    {
        this->flit.MPAM() = mpam;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::MPAM>()>*>(this);
    }
#endif

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr typename REQ<config>::rsvdc_t REQFlitBuilder<T, config, able>::RSVDC() const noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<REQ<config>>
    {
        return flit.RSVDC();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::RSVDC>()>
            REQFlitBuilder<T, config, able>::RSVDC(typename REQ<config>::rsvdc_t rsvdc) const noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<REQ<config>>
    {
        auto builder = REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::RSVDC>()>(*this);
        builder.flit.RSVDC() = rsvdc;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::RSVDC>()>&
            REQFlitBuilder<T, config, able>::SetRSVDC(typename REQ<config>::rsvdc_t rsvdc) noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<REQ<config>>
    {
        this->flit.RSVDC() = rsvdc;
        return *reinterpret_cast<REQFlitBuilder<T, config, NextREQFlitBuildability<T, able, REQFlitField::RSVDC>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQ<config> REQFlitBuilder<T, config, able>::Eval() const noexcept requires is_buildable<able>
    {
        return flit;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQ<config> REQFlitBuilder<T, config, able>::Build() const noexcept requires is_buildable<able>
    {
        return flit;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline consteval REQ<config> REQFlitBuilder<T, config, able>::EvalPartial() const noexcept
    {
        return flit;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, REQFlitBuildability able>
    inline constexpr REQ<config> REQFlitBuilder<T, config, able>::BuildPartial() const noexcept
    {
        return flit;
    }
}


// Implementation of: template MakeREQFlitBuildability
namespace /*CHI::*/Flits {

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config>
    inline consteval REQFlitBuildability MakeREQFlitBuildability() noexcept
    {
#define _FIELD_ABLE(field) \
        able.field = IsFieldValueStableOrInapplicable<T->fields->field>();

#define _FIELD_ABLE_OPT(field) \
        if constexpr (REQ<config>::has##field) \
            _FIELD_ABLE(field) \
        else \
            able.field = true;

        REQFlitBuildability able;
        _FIELD_ABLE(QoS             );
        _FIELD_ABLE(TgtID           );
        _FIELD_ABLE(SrcID           );
        _FIELD_ABLE(TxnID           );
        _FIELD_ABLE(ReturnNID       );
        _FIELD_ABLE(StashNID        );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(SLCRepHint      );
#endif
        _FIELD_ABLE(StashNIDValid   );
        _FIELD_ABLE(Endian          );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(Deep            );
#endif
        _FIELD_ABLE(ReturnTxnID     );
        _FIELD_ABLE(StashLPIDValid  );
        _FIELD_ABLE(StashLPID       );
        _FIELD_ABLE(Opcode          );
        _FIELD_ABLE(Size            );
        _FIELD_ABLE(Addr            );
        _FIELD_ABLE(NS              );
        _FIELD_ABLE(LikelyShared    );
        _FIELD_ABLE(AllowRetry      );
        _FIELD_ABLE(Order           );
        _FIELD_ABLE(PCrdType        );
    //  _FIELD_ABLE(MemAttr         );
        able.MemAttr = IsFieldValueStableOrInapplicable<T->fields->Device>()
                    && IsFieldValueStableOrInapplicable<T->fields->Cacheable>()
                    && IsFieldValueStableOrInapplicable<T->fields->EWA>()
                    && IsFieldValueStableOrInapplicable<T->fields->Allocate>();
        _FIELD_ABLE(SnpAttr         );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(DoDWT           );
#endif
        _FIELD_ABLE(LPID            );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(PGroupID        );
        _FIELD_ABLE(StashGroupID    );
        _FIELD_ABLE(TagGroupID      );
#endif
        _FIELD_ABLE(Excl            );
        _FIELD_ABLE(SnoopMe         );
        _FIELD_ABLE(ExpCompAck      );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE(TagOp           );
#endif
        _FIELD_ABLE(TraceTag        );
#ifdef CHI_ISSUE_EB_ENABLE
        _FIELD_ABLE_OPT(MPAM            );
#endif
        
        _FIELD_ABLE_OPT(RSVDC           );

        // Opcode always initialized
        able.Opcode = true;

        return able;

#undef _FIELD_ABLE
#undef _FIELD_ABLE_OPT
    }
}

// Implementation of: template NextREQFlitBuildability
namespace /*CHI::*/Flits {

    template<REQBuildTargetEnum T, REQFlitBuildability prevAble, REQFlitField field>
    inline consteval REQFlitBuildability NextREQFlitBuildability() noexcept
    {
        REQFlitBuildability nextAble = prevAble;
        if constexpr (field == REQFlitField::QoS            ) nextAble.QoS              = true;
        if constexpr (field == REQFlitField::TgtID          ) nextAble.TgtID            = true;
        if constexpr (field == REQFlitField::SrcID          ) nextAble.SrcID            = true;
        if constexpr (field == REQFlitField::TxnID          ) nextAble.TxnID            = true;
        if constexpr (field == REQFlitField::ReturnNID      ) nextAble.ReturnNID        = true;
        if constexpr (field == REQFlitField::StashNID       ) nextAble.StashNID         = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == REQFlitField::SLCRepHint     ) nextAble.SLCRepHint       = true;
#endif
        if constexpr (field == REQFlitField::StashNIDValid  ) nextAble.StashNIDValid    = true;
        if constexpr (field == REQFlitField::Endian         ) nextAble.Endian           = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == REQFlitField::Deep           ) nextAble.Deep             = true;
#endif
        if constexpr (field == REQFlitField::ReturnTxnID    ) nextAble.ReturnTxnID      = true;
        if constexpr (field == REQFlitField::StashLPIDValid ) nextAble.StashLPIDValid   = true;
        if constexpr (field == REQFlitField::StashLPID      ) nextAble.StashLPID        = true;
        if constexpr (field == REQFlitField::Opcode         ) nextAble.Opcode           = true;
        if constexpr (field == REQFlitField::Size           ) nextAble.Size             = true;
        if constexpr (field == REQFlitField::Addr           ) nextAble.Addr             = true;
        if constexpr (field == REQFlitField::NS             ) nextAble.NS               = true;
        if constexpr (field == REQFlitField::LikelyShared   ) nextAble.LikelyShared     = true;
        if constexpr (field == REQFlitField::AllowRetry     ) nextAble.AllowRetry       = true;
        if constexpr (field == REQFlitField::Order          ) nextAble.Order            = true;
        if constexpr (field == REQFlitField::PCrdType       ) nextAble.PCrdType         = true;
        if constexpr (field == REQFlitField::MemAttr        ) nextAble.MemAttr          = true;
        if constexpr (field == REQFlitField::SnpAttr        ) nextAble.SnpAttr          = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == REQFlitField::DoDWT          ) nextAble.DoDWT            = true;
#endif
        if constexpr (field == REQFlitField::LPID           ) nextAble.LPID             = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == REQFlitField::PGroupID       ) nextAble.PGroupID         = true;
        if constexpr (field == REQFlitField::StashGroupID   ) nextAble.StashGroupID     = true;
        if constexpr (field == REQFlitField::TagGroupID     ) nextAble.TagGroupID       = true;
#endif
        if constexpr (field == REQFlitField::Excl           ) nextAble.Excl             = true;
        if constexpr (field == REQFlitField::SnoopMe        ) nextAble.SnoopMe          = true;
        if constexpr (field == REQFlitField::ExpCompAck     ) nextAble.ExpCompAck       = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == REQFlitField::TagOp          ) nextAble.TagOp            = true;
#endif
        if constexpr (field == REQFlitField::TraceTag       ) nextAble.TraceTag         = true;
#ifdef CHI_ISSUE_EB_ENABLE
        if constexpr (field == REQFlitField::MPAM           ) nextAble.MPAM             = true;
#endif
        if constexpr (field == REQFlitField::RSVDC          ) nextAble.RSVDC            = true;
        return nextAble;
    }
}


#endif

//#endif // __CHI__CHI_UTIL_FLIT_BUILDER
