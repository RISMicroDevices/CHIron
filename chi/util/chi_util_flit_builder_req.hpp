//#pragma once

//#ifndef __CHI__CHI_UTIL_FLIT_BUILDER_REQ
//#define __CHI__CHI_UTIL_FLIT_BUILDER_REQ

#ifndef CHI_UTIL_FLIT_BUILDER_REQ__STANDALONE
#   define CHI_ISSUE_EB_ENABLE
#   include "chi_util_flit_builder_req_header.hpp"                      // IWYU pragma: keep
#endif

#include "chi_util_flit_builder/chi_util_flit_builder_common.hpp"       // IWYU pragma: keep
#include "chi_util_flit_builder/chi_util_flit_builder_req_details.hpp"  // IWYU pragma: keep


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_REQ_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_UTIL_FLIT_BUILDER_REQ_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_REQ_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_UTIL_FLIT_BUILDER_REQ_EB
#endif


/*
namespace CHI {
*/
    namespace Flits {

        // enumerations
        namespace REQBuildTarget {
            inline constexpr REQBuildTargetEnumBack ReqLCrdReturn           ("ReqLCrdReturn"                    , Opcodes::REQ::ReqLCrdReturn               , &Xact::RequestFieldMappings::ReqLCrdReturn                );
            inline constexpr REQBuildTargetEnumBack ReadShared              ("ReadShared"                       , Opcodes::REQ::ReadShared                  , &Xact::RequestFieldMappings::ReadShared                   );
            inline constexpr REQBuildTargetEnumBack ReadClean               ("ReadClean"                        , Opcodes::REQ::ReadClean                   , &Xact::RequestFieldMappings::ReadClean                    );
            inline constexpr REQBuildTargetEnumBack ReadOnce                ("ReadOnce"                         , Opcodes::REQ::ReadOnce                    , &Xact::RequestFieldMappings::ReadOnce                     );
            inline constexpr REQBuildTargetEnumBack ReadNoSnp               ("ReadNoSnp"                        , Opcodes::REQ::ReadNoSnp                   , &Xact::RequestFieldMappings::ReadNoSnp                    );
            inline constexpr REQBuildTargetEnumBack PCrdReturn              ("PCrdReturn"                       , Opcodes::REQ::PCrdReturn                  , &Xact::RequestFieldMappings::PCrdReturn                   );
            // 0x06
            inline constexpr REQBuildTargetEnumBack ReadUnique              ("ReadUnique"                       , Opcodes::REQ::ReadUnique                  , &Xact::RequestFieldMappings::ReadUnique                   );
            inline constexpr REQBuildTargetEnumBack CleanShared             ("CleanShared"                      , Opcodes::REQ::CleanShared                 , &Xact::RequestFieldMappings::CleanShared                  );
            inline constexpr REQBuildTargetEnumBack CleanInvalid            ("CleanInvalid"                     , Opcodes::REQ::CleanInvalid                , &Xact::RequestFieldMappings::CleanInvalid                 );
            inline constexpr REQBuildTargetEnumBack MakeInvalid             ("MakeInvalid"                      , Opcodes::REQ::MakeInvalid                 , &Xact::RequestFieldMappings::MakeInvalid                  );
            inline constexpr REQBuildTargetEnumBack CleanUnique             ("CleanUnique"                      , Opcodes::REQ::CleanUnique                 , &Xact::RequestFieldMappings::CleanUnique                  );
            inline constexpr REQBuildTargetEnumBack MakeUnique              ("MakeUnique"                       , Opcodes::REQ::MakeUnique                  , &Xact::RequestFieldMappings::MakeUnique                   );
            inline constexpr REQBuildTargetEnumBack Evict                   ("Evict"                            , Opcodes::REQ::Evict                       , &Xact::RequestFieldMappings::Evict                        );
            // 0x0E
            // 0x0F
            // 0x10
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr REQBuildTargetEnumBack ReadNoSnpSep            ("ReadNoSnpSep"                     , Opcodes::REQ::ReadNoSnpSep                , &Xact::RequestFieldMappings::ReadNoSnpSep                 );
#endif
            // 0x12
#ifdef CHI_ISSUE_EB_ENABLE
            inline constexpr REQBuildTargetEnumBack CleanSharedPersistSep   ("CleanSharedPersistSep"            , Opcodes::REQ::CleanSharedPersistSep       , &Xact::RequestFieldMappings::CleanSharedPersistSep        );
#endif
            inline constexpr REQBuildTargetEnumBack DVMOp                   ("DVMOp"                            , Opcodes::REQ::DVMOp                       , &Xact::RequestFieldMappings::DVMOp                        );
            inline constexpr REQBuildTargetEnumBack WriteEvictFull          ("WriteEvictFull"                   , Opcodes::REQ::WriteEvictFull              , &Xact::RequestFieldMappings::WriteEvictFull               );
            // 0x16
            inline constexpr REQBuildTargetEnumBack WriteCleanFull          ("WriteCleanFull"                   , Opcodes::REQ::WriteCleanFull              , &Xact::RequestFieldMappings::WriteCleanFull               );
            inline constexpr REQBuildTargetEnumBack WriteUniquePtl          ("WriteUniquePtl"                   , Opcodes::REQ::WriteUniquePtl              , &Xact::RequestFieldMappings::WriteUniquePtl               );
            inline constexpr REQBuildTargetEnumBack WriteUniqueFull         ("WriteUniqueFull"                  , Opcodes::REQ::WriteUniqueFull             , &Xact::RequestFieldMappings::WriteUniqueFull              );
            inline constexpr REQBuildTargetEnumBack WriteBackPtl            ("WriteBackPtl"                     , Opcodes::REQ::WriteBackPtl                , &Xact::RequestFieldMappings::WriteBackPtl                 );
            inline constexpr REQBuildTargetEnumBack WriteBackFull           ("WriteBackFull"                    , Opcodes::REQ::WriteBackFull               , &Xact::RequestFieldMappings::WriteBackFull                );
            inline constexpr REQBuildTargetEnumBack WriteNoSnpPtl           ("WriteNoSnpPtl"                    , Opcodes::REQ::WriteNoSnpPtl               , &Xact::RequestFieldMappings::WriteNoSnpPtl                );
            inline constexpr REQBuildTargetEnumBack WriteNoSnpFull          ("WriteNoSnpFull"                   , Opcodes::REQ::WriteNoSnpFull              , &Xact::RequestFieldMappings::WriteNoSnpFull               );
            // 0x1E
            // 0x1F
            inline constexpr REQBuildTargetEnumBack WriteUniqueFullStash        ("WriteUniqueFullStash"         , Opcodes::REQ::WriteUniqueFullStash        , &Xact::RequestFieldMappings::WriteUniqueFullStash         );
            inline constexpr REQBuildTargetEnumBack WriteUniquePtlStash         ("WriteUniquePtlStash"          , Opcodes::REQ::WriteUniquePtlStash         , &Xact::RequestFieldMappings::WriteUniquePtlStash          );
            inline constexpr REQBuildTargetEnumBack StashOnceShared             ("StashOnceShared"              , Opcodes::REQ::StashOnceShared             , &Xact::RequestFieldMappings::StashOnceShared              );
            inline constexpr REQBuildTargetEnumBack StashOnceUnique             ("StashOnceUnique"              , Opcodes::REQ::StashOnceUnique             , &Xact::RequestFieldMappings::StashOnceUnique              );
            inline constexpr REQBuildTargetEnumBack ReadOnceCleanInvalid        ("ReadOnceCleanInvalid"         , Opcodes::REQ::ReadOnceCleanInvalid        , &Xact::RequestFieldMappings::ReadOnceCleanInvalid         );
            inline constexpr REQBuildTargetEnumBack ReadOnceMakeInvalid         ("ReadOnceMakeInvalid"          , Opcodes::REQ::ReadOnceMakeInvalid         , &Xact::RequestFieldMappings::ReadOnceMakeInvalid          );
            inline constexpr REQBuildTargetEnumBack ReadNotSharedDirty          ("ReadNotSharedDirty"           , Opcodes::REQ::ReadNotSharedDirty          , &Xact::RequestFieldMappings::ReadNotSharedDirty           );
            inline constexpr REQBuildTargetEnumBack CleanSharedPersist          ("CleanSharedPersist"           , Opcodes::REQ::CleanSharedPersist          , &Xact::RequestFieldMappings::CleanSharedPersist           );
            inline constexpr REQBuildTargetEnumBack AtomicStore_ADD             ("AtomicStore::ADD"             , Opcodes::REQ::AtomicStore::ADD            , &Xact::RequestFieldMappings::AtomicStore                  );
            inline constexpr REQBuildTargetEnumBack AtomicStore_CLR             ("AtomicStore::CLR"             , Opcodes::REQ::AtomicStore::CLR            , &Xact::RequestFieldMappings::AtomicStore                  );
            inline constexpr REQBuildTargetEnumBack AtomicStore_EOR             ("AtomicStore::EOR"             , Opcodes::REQ::AtomicStore::EOR            , &Xact::RequestFieldMappings::AtomicStore                  );
            inline constexpr REQBuildTargetEnumBack AtomicStore_SET             ("AtomicStore::SET"             , Opcodes::REQ::AtomicStore::SET            , &Xact::RequestFieldMappings::AtomicStore                  );
            inline constexpr REQBuildTargetEnumBack AtomicStore_SMAX            ("AtomicStore::SMAX"            , Opcodes::REQ::AtomicStore::SMAX           , &Xact::RequestFieldMappings::AtomicStore                  );
            inline constexpr REQBuildTargetEnumBack AtomicStore_SMIN            ("AtomicStore::SMIN"            , Opcodes::REQ::AtomicStore::SMIN           , &Xact::RequestFieldMappings::AtomicStore                  );
            inline constexpr REQBuildTargetEnumBack AtomicStore_UMAX            ("AtomicStore::UMAX"            , Opcodes::REQ::AtomicStore::UMAX           , &Xact::RequestFieldMappings::AtomicStore                  );
            inline constexpr REQBuildTargetEnumBack AtomicStore_UMIN            ("AtomicStore::UMIN"            , Opcodes::REQ::AtomicStore::UMIN           , &Xact::RequestFieldMappings::AtomicStore                  );
            inline constexpr REQBuildTargetEnumBack AtomicLoad_ADD              ("AtomicLoad::ADD"              , Opcodes::REQ::AtomicLoad::ADD             , &Xact::RequestFieldMappings::AtomicLoad                   );
            inline constexpr REQBuildTargetEnumBack AtomicLoad_CLR              ("AtomicLoad::CLR"              , Opcodes::REQ::AtomicLoad::CLR             , &Xact::RequestFieldMappings::AtomicLoad                   );
            inline constexpr REQBuildTargetEnumBack AtomicLoad_EOR              ("AtomicLoad::EOR"              , Opcodes::REQ::AtomicLoad::EOR             , &Xact::RequestFieldMappings::AtomicLoad                   );
            inline constexpr REQBuildTargetEnumBack AtomicLoad_SET              ("AtomicLoad::SET"              , Opcodes::REQ::AtomicLoad::SET             , &Xact::RequestFieldMappings::AtomicLoad                   );
            inline constexpr REQBuildTargetEnumBack AtomicLoad_SMAX             ("AtomicLoad::SMAX"             , Opcodes::REQ::AtomicLoad::SMAX            , &Xact::RequestFieldMappings::AtomicLoad                   );
            inline constexpr REQBuildTargetEnumBack AtomicLoad_SMIN             ("AtomicLoad::SMIN"             , Opcodes::REQ::AtomicLoad::SMIN            , &Xact::RequestFieldMappings::AtomicLoad                   );
            inline constexpr REQBuildTargetEnumBack AtomicLoad_UMAX             ("AtomicLoad::UMAX"             , Opcodes::REQ::AtomicLoad::UMAX            , &Xact::RequestFieldMappings::AtomicLoad                   );
            inline constexpr REQBuildTargetEnumBack AtomicLoad_UMIN             ("AtomicLoad::UMIN"             , Opcodes::REQ::AtomicLoad::UMIN            , &Xact::RequestFieldMappings::AtomicLoad                   );
            inline constexpr REQBuildTargetEnumBack AtomicSwap                  ("AtomicSwap"                   , Opcodes::REQ::AtomicSwap                  , &Xact::RequestFieldMappings::AtomicSwap                   );
            inline constexpr REQBuildTargetEnumBack AtomicCompare               ("AtomicCompare"                , Opcodes::REQ::AtomicCompare               , &Xact::RequestFieldMappings::AtomicCompare                );
            inline constexpr REQBuildTargetEnumBack PrefetchTgt                 ("PrefetchTgt"                  , Opcodes::REQ::PrefetchTgt                 , &Xact::RequestFieldMappings::PrefetchTgt                  );
            // 0x3B - 0x3F
#ifdef CHI_ISSUE_EB_ENABLE
            // 0x40
            inline constexpr REQBuildTargetEnumBack MakeReadUnique              ("MakeReadUnique"               , Opcodes::REQ::MakeReadUnique              , &Xact::RequestFieldMappings::MakeReadUnique               );
            inline constexpr REQBuildTargetEnumBack WriteEvictOrEvict           ("WriteEvictOrEvict"            , Opcodes::REQ::WriteEvictOrEvict           , &Xact::RequestFieldMappings::WriteEvictOrEvict            );
            inline constexpr REQBuildTargetEnumBack WriteUniqueZero             ("WriteUniqueZero"              , Opcodes::REQ::WriteUniqueZero             , &Xact::RequestFieldMappings::WriteUniqueZero              );
            inline constexpr REQBuildTargetEnumBack WriteNoSnpZero              ("WriteNoSnpZero"               , Opcodes::REQ::WriteNoSnpZero              , &Xact::RequestFieldMappings::WriteNoSnpZero               );
            // 0x45
            // 0x46
            inline constexpr REQBuildTargetEnumBack StashOnceSepShared          ("StashOnceSepShared"           , Opcodes::REQ::StashOnceSepShared          , &Xact::RequestFieldMappings::StashOnceSepShared           );
            inline constexpr REQBuildTargetEnumBack StashOnceSepUnique          ("StashOnceSepUnique"           , Opcodes::REQ::StashOnceSepUnique          , &Xact::RequestFieldMappings::StashOnceSepUnique           );
            // 0x49 - 0x4B
            inline constexpr REQBuildTargetEnumBack ReadPreferUnique            ("ReadPreferUnique"             , Opcodes::REQ::ReadPreferUnique            , &Xact::RequestFieldMappings::ReadPreferUnique             );
            // 0x4D - 0x4F
            inline constexpr REQBuildTargetEnumBack WriteNoSnpFullCleanSh       ("WriteNoSnpFullCleanSh"        , Opcodes::REQ::WriteNoSnpFullCleanSh       , &Xact::RequestFieldMappings::WriteNoSnpFullCleanSh        );
            inline constexpr REQBuildTargetEnumBack WriteNoSnpFullCleanInv      ("WriteNoSnpFullCleanInv"       , Opcodes::REQ::WriteNoSnpFullCleanInv      , &Xact::RequestFieldMappings::WriteNoSnpFullCleanInv       );
            inline constexpr REQBuildTargetEnumBack WriteNoSnpFullCleanShPerSep ("WriteNoSnpFullCleanShPerSep"  , Opcodes::REQ::WriteNoSnpFullCleanShPerSep , &Xact::RequestFieldMappings::WriteNoSnpFullCleanShPerSep  );
            // 0x53
            inline constexpr REQBuildTargetEnumBack WriteUniqueFullCleanSh      ("WriteUniqueFullCleanSh"       , Opcodes::REQ::WriteUniqueFullCleanSh      , &Xact::RequestFieldMappings::WriteUniqueFullCleanSh       );
            // 0x55
            inline constexpr REQBuildTargetEnumBack WriteUniqueFullCleanShPerSep("WriteUniqueFullCleanShPerSep" , Opcodes::REQ::WriteUniqueFullCleanShPerSep, &Xact::RequestFieldMappings::WriteUniqueFullCleanShPerSep );
            // 0x57
            inline constexpr REQBuildTargetEnumBack WriteBackFullCleanSh        ("WriteBackFullCleanSh"         , Opcodes::REQ::WriteBackFullCleanSh        , &Xact::RequestFieldMappings::WriteBackFullCleanSh         );
            inline constexpr REQBuildTargetEnumBack WriteBackFullCleanInv       ("WriteBackFullCleanInv"        , Opcodes::REQ::WriteBackFullCleanInv       , &Xact::RequestFieldMappings::WriteBackFullCleanInv        );
            inline constexpr REQBuildTargetEnumBack WriteBackFullCleanShPerSep  ("WriteBackFullCleanShPerSep"   , Opcodes::REQ::WriteBackFullCleanShPerSep  , &Xact::RequestFieldMappings::WriteBackFullCleanShPerSep   );
            // 0x5B
            inline constexpr REQBuildTargetEnumBack WriteCleanFullCleanSh       ("WriteCleanFullCleanSh"        , Opcodes::REQ::WriteCleanFullCleanSh       , &Xact::RequestFieldMappings::WriteCleanFullCleanSh        );
            // 0x5D
            inline constexpr REQBuildTargetEnumBack WriteCleanFullCleanShPerSep ("WriteCleanFullCleanShPerSep"  , Opcodes::REQ::WriteCleanFullCleanShPerSep , &Xact::RequestFieldMappings::WriteCleanFullCleanShPerSep  );
            // 0x5F
            inline constexpr REQBuildTargetEnumBack WriteNoSnpPtlCleanSh        ("WriteNoSnpPtlCleanSh"         , Opcodes::REQ::WriteNoSnpPtlCleanSh        , &Xact::RequestFieldMappings::WriteNoSnpPtlCleanSh         );
            inline constexpr REQBuildTargetEnumBack WriteNoSnpPtlCleanInv       ("WriteNoSnpPtlCleanInv"        , Opcodes::REQ::WriteNoSnpPtlCleanInv       , &Xact::RequestFieldMappings::WriteNoSnpPtlCleanInv        );
            inline constexpr REQBuildTargetEnumBack WriteNoSnpPtlCleanShPerSep  ("WriteNoSnpPtlCleanShPerSep"   , Opcodes::REQ::WriteNoSnpPtlCleanShPerSep  , &Xact::RequestFieldMappings::WriteNoSnpPtlCleanShPerSep   );
            // 0x63
            inline constexpr REQBuildTargetEnumBack WriteUniquePtlCleanSh       ("WriteUniquePtlCleanSh"        , Opcodes::REQ::WriteUniquePtlCleanSh       , &Xact::RequestFieldMappings::WriteUniquePtlCleanSh        );
            // 0x65
            inline constexpr REQBuildTargetEnumBack WriteUniquePtlCleanShPerSep ("WriteUniquePtlCleanShPerSep"  , Opcodes::REQ::WriteUniquePtlCleanShPerSep , &Xact::RequestFieldMappings::WriteUniquePtlCleanShPerSep  );
            // 0x67 - 0x7F
#endif
        }

        // REQ builders
        template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able = details::MakeREQFlitBuildability<T, config>()>
        class REQFlitBuilder
        {
            template<REQBuildTargetEnum, REQFlitConfigurationConcept, details::REQFlitBuildability>
            friend class REQFlitBuilder;

        protected:
            REQ<config>     flit    = { 0 };

        public:
            constexpr REQFlitBuilder() noexcept;

            template<details::REQFlitBuildability nextAble>
            constexpr REQFlitBuilder(const REQFlitBuilder<T, config, nextAble>&) noexcept;

        public:
            constexpr REQ<config>::qos_t                QoS() const noexcept requires is_field_applicable<T->fields->QoS>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::QoS>()>
                                                        QoS(typename REQ<config>::qos_t qos) const noexcept requires is_field_applicable<T->fields->QoS>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::QoS>()>&
                                                        SetQoS(typename REQ<config>::qos_t qos) noexcept requires is_field_applicable<T->fields->QoS>;

            constexpr REQ<config>::tgtid_t              TgtID() const noexcept requires is_field_applicable<T->fields->TgtID>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TgtID>()>
                                                        TgtID(typename REQ<config>::tgtid_t tgtID) const noexcept requires is_field_applicable<T->fields->TgtID>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TgtID>()>&
                                                        SetTgtID(typename REQ<config>::tgtid_t tgtID) noexcept requires is_field_applicable<T->fields->TgtID>;

            constexpr REQ<config>::srcid_t              SrcID() const noexcept requires is_field_applicable<T->fields->SrcID>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SrcID>()>
                                                        SrcID(typename REQ<config>::srcid_t srcID) const noexcept requires is_field_applicable<T->fields->SrcID>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SrcID>()>&
                                                        SetSrcID(typename REQ<config>::srcid_t srcID) noexcept requires is_field_applicable<T->fields->SrcID>;

            constexpr REQ<config>::txnid_t              TxnID() const noexcept requires is_field_applicable<T->fields->TxnID>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TxnID>()>
                                                        TxnID(typename REQ<config>::txnid_t txnID) const noexcept requires is_field_applicable<T->fields->TxnID>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TxnID>()>&
                                                        SetTxnID(typename REQ<config>::txnid_t txnID) noexcept requires is_field_applicable<T->fields->TxnID>;

            constexpr REQ<config>::returnnid_t          ReturnNID() const noexcept requires is_field_applicable<T->fields->ReturnNID>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ReturnNID>()>
                                                        ReturnNID(typename REQ<config>::returnnid_t returnNID) const noexcept requires is_field_applicable<T->fields->ReturnNID>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ReturnNID>()>&
                                                        SetReturnNID(typename REQ<config>::returnnid_t returnNID) noexcept requires is_field_applicable<T->fields->ReturnNID>;

            constexpr REQ<config>::stashnid_t           StashNID() const noexcept requires is_field_applicable<T->fields->StashNID>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashNID>()>
                                                        StashNID(typename REQ<config>::stashnid_t stashNID) const noexcept requires is_field_applicable<T->fields->StashNID>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashNID>()>&
                                                        SetStashNID(typename REQ<config>::stashnid_t stashNID) noexcept requires is_field_applicable<T->fields->StashNID>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr REQ<config>::slcrephint_t         SLCRepHint() const noexcept requires is_field_applicable<T->fields->SLCRepHint>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SLCRepHint>()>
                                                        SLCRepHint(typename REQ<config>::slcrephint_t slcRepHint) const noexcept requires is_field_applicable<T->fields->SLCRepHint>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SLCRepHint>()>&
                                                        SetSLCRepHint(typename REQ<config>::slcrephint_t slcRepHint) noexcept requires is_field_applicable<T->fields->SLCRepHint>;
#endif

            constexpr REQ<config>::stashnidvalid_t      StashNIDValid() const noexcept requires is_field_applicable<T->fields->StashNIDValid>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashNIDValid>()>
                                                        StashNIDValid(typename REQ<config>::stashnidvalid_t stashNIDValid) const noexcept requires is_field_applicable<T->fields->StashNIDValid>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashNIDValid>()>&
                                                        SetStashNIDValid(typename REQ<config>::stashnidvalid_t stashNIDValid) noexcept requires is_field_applicable<T->fields->StashNIDValid>;

            constexpr REQ<config>::endian_t             Endian() const noexcept requires is_field_applicable<T->fields->Endian>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Endian>()>
                                                        Endian(typename REQ<config>::endian_t endian) const noexcept requires is_field_applicable<T->fields->Endian>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Endian>()>&
                                                        SetEndian(typename REQ<config>::endian_t endian) noexcept requires is_field_applicable<T->fields->Endian>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr REQ<config>::deep_t               Deep() const noexcept requires is_field_applicable<T->fields->Deep>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Deep>()>
                                                        Deep(typename REQ<config>::deep_t deep) const noexcept requires is_field_applicable<T->fields->Deep>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Deep>()>&
                                                        SetDeep(typename REQ<config>::deep_t deep) noexcept requires is_field_applicable<T->fields->Deep>;
#endif

            constexpr REQ<config>::returntxnid_t        ReturnTxnID() const noexcept requires is_field_applicable<T->fields->ReturnTxnID>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ReturnTxnID>()>
                                                        ReturnTxnID(typename REQ<config>::returntxnid_t returnTxnID) const noexcept requires is_field_applicable<T->fields->ReturnTxnID>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ReturnTxnID>()>&
                                                        SetReturnTxnID(typename REQ<config>::returntxnid_t returnTxnID) noexcept requires is_field_applicable<T->fields->ReturnTxnID>;

            constexpr REQ<config>::stashlpidvalid_t     StashLPIDValid() const noexcept requires is_field_applicable<T->fields->StashLPIDValid>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashLPIDValid>()>
                                                        StashLPIDValid(typename REQ<config>::stashlpidvalid_t stashLPIDValid) const noexcept requires is_field_applicable<T->fields->StashLPIDValid>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashLPIDValid>()>&
                                                        SetStashLPIDValid(typename REQ<config>::stashlpidvalid_t stashLPIDValid) noexcept requires is_field_applicable<T->fields->StashLPIDValid>;

            constexpr REQ<config>::stashlpid_t          StashLPID() const noexcept requires is_field_applicable<T->fields->StashLPID>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashLPID>()>
                                                        StashLPID(typename REQ<config>::stashlpid_t stashLPID) const noexcept requires is_field_applicable<T->fields->StashLPID>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashLPID>()>&
                                                        SetStashLPID(typename REQ<config>::stashlpid_t stashLPID) noexcept requires is_field_applicable<T->fields->StashLPID>;

            constexpr REQ<config>::opcode_t             Opcode() const noexcept requires is_field_applicable<T->fields->Opcode>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Opcode>()>
                                                        Opcode(typename REQ<config>::opcode_t opcode) const noexcept requires is_field_applicable<T->fields->Opcode>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Opcode>()>&
                                                        SetOpcode(typename REQ<config>::opcode_t opcode) noexcept requires is_field_applicable<T->fields->Opcode>;

            constexpr REQ<config>::ssize_t              Size() const noexcept requires is_field_applicable<T->fields->Size>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Size>()>
                                                        Size(typename REQ<config>::ssize_t size) const noexcept requires is_field_applicable<T->fields->Size>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Size>()>&
                                                        SetSize(typename REQ<config>::ssize_t size) noexcept requires is_field_applicable<T->fields->Size>;

            constexpr REQ<config>::addr_t               Addr() const noexcept requires is_field_applicable<T->fields->Addr>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Addr>()>
                                                        Addr(typename REQ<config>::addr_t addr) const noexcept requires is_field_applicable<T->fields->Addr>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Addr>()>&
                                                        SetAddr(typename REQ<config>::addr_t addr) noexcept requires is_field_applicable<T->fields->Addr>;

            constexpr REQ<config>::ns_t                 NS() const noexcept requires is_field_applicable<T->fields->NS>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::NS>()>
                                                        NS(typename REQ<config>::ns_t ns) const noexcept requires is_field_applicable<T->fields->NS>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::NS>()>&
                                                        SetNS(typename REQ<config>::ns_t ns) noexcept requires is_field_applicable<T->fields->NS>;

            constexpr REQ<config>::likelyshared_t       LikelyShared() const noexcept requires is_field_applicable<T->fields->LikelyShared>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LikelyShared>()>
                                                        LikelyShared(typename REQ<config>::likelyshared_t likelyShared) const noexcept requires is_field_applicable<T->fields->LikelyShared>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LikelyShared>()>&
                                                        SetLikelyShared(typename REQ<config>::likelyshared_t likelyShared) noexcept requires is_field_applicable<T->fields->LikelyShared>;

            constexpr REQ<config>::allowretry_t         AllowRetry() const noexcept requires is_field_applicable<T->fields->AllowRetry>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::AllowRetry>()>
                                                        AllowRetry(typename REQ<config>::allowretry_t allowRetry) const noexcept requires is_field_applicable<T->fields->AllowRetry>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::AllowRetry>()>&
                                                        SetAllowRetry(typename REQ<config>::allowretry_t allowRetry) noexcept requires is_field_applicable<T->fields->AllowRetry>;

            constexpr REQ<config>::order_t              Order() const noexcept requires is_field_applicable<T->fields->Order>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Order>()>
                                                        Order(typename REQ<config>::order_t order) const noexcept requires is_field_applicable<T->fields->Order>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Order>()>&
                                                        SetOrder(typename REQ<config>::order_t order) noexcept requires is_field_applicable<T->fields->Order>;

            constexpr REQ<config>::pcrdtype_t           PCrdType() const noexcept requires is_field_applicable<T->fields->PCrdType>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::PCrdType>()>
                                                        PCrdType(typename REQ<config>::pcrdtype_t pCrdType) const noexcept requires is_field_applicable<T->fields->PCrdType>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::PCrdType>()>&
                                                        SetPCrdType(typename REQ<config>::pcrdtype_t pCrdType) noexcept requires is_field_applicable<T->fields->PCrdType>;

            constexpr REQ<config>::memattr_t            MemAttr() const noexcept requires is_memattr_applicable<T>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::MemAttr>()>
                                                        MemAttr(typename REQ<config>::memattr_t memAttr) const noexcept requires is_memattr_applicable<T>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::MemAttr>()>&
                                                        SetMemAttr(typename REQ<config>::memattr_t memAttr) noexcept requires is_memattr_applicable<T>;

            constexpr REQ<config>::snpattr_t            SnpAttr() const noexcept requires is_field_applicable<T->fields->SnpAttr>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SnpAttr>()>
                                                        SnpAttr(typename REQ<config>::snpattr_t snpAttr) const noexcept requires is_field_applicable<T->fields->SnpAttr>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SnpAttr>()>&
                                                        SetSnpAttr(typename REQ<config>::snpattr_t snpAttr) noexcept requires is_field_applicable<T->fields->SnpAttr>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr REQ<config>::dodwt_t              DoDWT() const noexcept requires is_field_applicable<T->fields->DoDWT>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::DoDWT>()>
                                                        DoDWT(typename REQ<config>::dodwt_t doDWT) const noexcept requires is_field_applicable<T->fields->DoDWT>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::DoDWT>()>&
                                                        SetDoDWT(typename REQ<config>::dodwt_t doDWT) noexcept requires is_field_applicable<T->fields->DoDWT>;
#endif

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr REQ<config>::pgroupid_t           PGroupID() const noexcept requires is_field_applicable<T->fields->PGroupID>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::PGroupID>()>
                                                        PGroupID(typename REQ<config>::pgroupid_t pGroupID) const noexcept requires is_field_applicable<T->fields->PGroupID>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::PGroupID>()>&
                                                        SetPGroupID(typename REQ<config>::pgroupid_t pGroupID) noexcept requires is_field_applicable<T->fields->PGroupID>;

            constexpr REQ<config>::stashgroupid_t       StashGroupID() const noexcept requires is_field_applicable<T->fields->StashGroupID>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashGroupID>()>
                                                        StashGroupID(typename REQ<config>::stashgroupid_t stashGroupID) const noexcept requires is_field_applicable<T->fields->StashGroupID>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashGroupID>()>&
                                                        SetStashGroupID(typename REQ<config>::stashgroupid_t stashGroupID) noexcept requires is_field_applicable<T->fields->StashGroupID>;

            constexpr REQ<config>::taggroupid_t         TagGroupID() const noexcept requires is_field_applicable<T->fields->TagGroupID>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TagGroupID>()>
                                                        TagGroupID(typename REQ<config>::taggroupid_t tagGroupID) const noexcept requires is_field_applicable<T->fields->TagGroupID>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TagGroupID>()>&
                                                        SetTagGroupID(typename REQ<config>::taggroupid_t tagGroupID) noexcept requires is_field_applicable<T->fields->TagGroupID>;
#endif

            constexpr REQ<config>::lpid_t               LPID() const noexcept requires is_field_applicable<T->fields->LPID>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LPID>()>
                                                        LPID(typename REQ<config>::lpid_t lpid) const noexcept requires is_field_applicable<T->fields->LPID>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LPID>()>&
                                                        SetLPID(typename REQ<config>::lpid_t lpid) noexcept requires is_field_applicable<T->fields->LPID>;

            constexpr REQ<config>::excl_t               Excl() const noexcept requires is_field_applicable<T->fields->Excl>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Excl>()>
                                                        Excl(typename REQ<config>::excl_t excl) const noexcept requires is_field_applicable<T->fields->Excl>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Excl>()>&
                                                        SetExcl(typename REQ<config>::excl_t excl) noexcept requires is_field_applicable<T->fields->Excl>;

            constexpr REQ<config>::snoopme_t            SnoopMe() const noexcept requires is_field_applicable<T->fields->SnoopMe>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SnoopMe>()>
                                                        SnoopMe(typename REQ<config>::snoopme_t snoopMe) const noexcept requires is_field_applicable<T->fields->SnoopMe>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SnoopMe>()>&
                                                        SetSnoopMe(typename REQ<config>::snoopme_t snoopMe) noexcept requires is_field_applicable<T->fields->SnoopMe>;

            constexpr REQ<config>::expcompack_t         ExpCompAck() const noexcept requires is_field_applicable<T->fields->ExpCompAck>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ExpCompAck>()>
                                                        ExpCompAck(typename REQ<config>::expcompack_t expCompAck) const noexcept requires is_field_applicable<T->fields->ExpCompAck>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ExpCompAck>()>&
                                                        SetExpCompAck(typename REQ<config>::expcompack_t expCompAck) noexcept requires is_field_applicable<T->fields->ExpCompAck>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr REQ<config>::tagop_t              TagOp() const noexcept requires is_field_applicable<T->fields->TagOp>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TagOp>()>
                                                        TagOp(typename REQ<config>::tagop_t tagOp) const noexcept requires is_field_applicable<T->fields->TagOp>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TagOp>()>&
                                                        SetTagOp(typename REQ<config>::tagop_t tagOp) noexcept requires is_field_applicable<T->fields->TagOp>;
#endif

            constexpr REQ<config>::tracetag_t           TraceTag() const noexcept requires is_field_applicable<T->fields->TraceTag>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TraceTag>()>
                                                        TraceTag(typename REQ<config>::tracetag_t traceTag) const noexcept requires is_field_applicable<T->fields->TraceTag>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TraceTag>()>&
                                                        SetTraceTag(typename REQ<config>::tracetag_t traceTag) noexcept requires is_field_applicable<T->fields->TraceTag>;

#ifdef CHI_ISSUE_EB_ENABLE
            constexpr REQ<config>::mpam_t               MPAM() const noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<REQ<config>>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::MPAM>()>
                                                        MPAM(typename REQ<config>::mpam_t mpam) const noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<REQ<config>>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::MPAM>()>&
                                                        SetMPAM(typename REQ<config>::mpam_t mpam) noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<REQ<config>>;
#endif

            constexpr REQ<config>::rsvdc_t              RSVDC() const noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<REQ<config>>;
            consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::RSVDC>()>
                                                        RSVDC(typename REQ<config>::rsvdc_t rsvdc) const noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<REQ<config>>;
            constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::RSVDC>()>&
                                                        SetRSVDC(typename REQ<config>::rsvdc_t rsvdc) noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<REQ<config>>;

            //            
            consteval REQ<config>                       Eval() const noexcept requires details::is_buildable<able>;
            constexpr REQ<config>                       Build() const noexcept requires details::is_buildable<able>;

            consteval REQ<config>                       EvalPartial() const noexcept;
            constexpr REQ<config>                       BuildPartial() const noexcept;
        };

        template<REQBuildTargetEnum T, REQFlitConfigurationConcept config>
        consteval REQFlitBuilder<T, config, details::MakeREQFlitBuildability<T, config>()> EvalREQFlitBuilder() noexcept;
        
        template<REQBuildTargetEnum T, REQFlitConfigurationConcept config>
        constexpr REQFlitBuilder<T, config, details::MakeREQFlitBuildability<T, config>()> MakeREQFlitBuilder() noexcept;
    }

/*
}
*/


// Implementation of: class REQFlitBuilder
namespace /*CHI::*/Flits {

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
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

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    template<details::REQFlitBuildability nextAble>
    inline constexpr REQFlitBuilder<T, config, able>::REQFlitBuilder(const REQFlitBuilder<T, config, nextAble>& obj) noexcept
    {
        this->flit = obj.flit;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQ<config>::qos_t REQFlitBuilder<T, config, able>::QoS() const noexcept requires is_field_applicable<T->fields->QoS>
    {
        return flit.QoS();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::QoS>()>
            REQFlitBuilder<T, config, able>::QoS(typename REQ<config>::qos_t qos) const noexcept requires is_field_applicable<T->fields->QoS>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::QoS>()>(*this);
        builder.flit.QoS() = qos;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::QoS>()>&
            REQFlitBuilder<T, config, able>::SetQoS(typename REQ<config>::qos_t qos) noexcept requires is_field_applicable<T->fields->QoS>
    {
        this->flit.QoS() = qos;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::QoS>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::tgtid_t REQFlitBuilder<T, config, able>::TgtID() const noexcept requires is_field_applicable<T->fields->TgtID>
    {
        return flit.TgtID();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TgtID>()>
            REQFlitBuilder<T, config, able>::TgtID(typename REQ<config>::tgtid_t tgtID) const noexcept requires is_field_applicable<T->fields->TgtID>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TgtID>()>(*this);
        builder.flit.TgtID() = tgtID;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TgtID>()>&
            REQFlitBuilder<T, config, able>::SetTgtID(typename REQ<config>::tgtid_t tgtID) noexcept requires is_field_applicable<T->fields->TgtID>
    {
        this->flit.TgtID() = tgtID;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TgtID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::srcid_t REQFlitBuilder<T, config, able>::SrcID() const noexcept requires is_field_applicable<T->fields->SrcID>
    {
        return flit.SrcID();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SrcID>()>
            REQFlitBuilder<T, config, able>::SrcID(typename REQ<config>::srcid_t srcID) const noexcept requires is_field_applicable<T->fields->SrcID>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SrcID>()>(*this);
        builder.flit.SrcID() = srcID;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SrcID>()>&
            REQFlitBuilder<T, config, able>::SetSrcID(typename REQ<config>::srcid_t srcID) noexcept requires is_field_applicable<T->fields->SrcID>
    {
        this->flit.SrcID() = srcID;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SrcID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::txnid_t REQFlitBuilder<T, config, able>::TxnID() const noexcept requires is_field_applicable<T->fields->TxnID>
    {
        return flit.TxnID();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TxnID>()>
            REQFlitBuilder<T, config, able>::TxnID(typename REQ<config>::txnid_t txnID) const noexcept requires is_field_applicable<T->fields->TxnID>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TxnID>()>(*this);
        builder.flit.TxnID() = txnID;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TxnID>()>&
            REQFlitBuilder<T, config, able>::SetTxnID(typename REQ<config>::txnid_t txnID) noexcept requires is_field_applicable<T->fields->TxnID>
    {
        this->flit.TxnID() = txnID;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TxnID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::returnnid_t REQFlitBuilder<T, config, able>::ReturnNID() const noexcept requires is_field_applicable<T->fields->ReturnNID>
    {
        return flit.ReturnNID();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ReturnNID>()>
            REQFlitBuilder<T, config, able>::ReturnNID(typename REQ<config>::returnnid_t returnNID) const noexcept requires is_field_applicable<T->fields->ReturnNID>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ReturnNID>()>(*this);
        builder.flit.ReturnNID() = returnNID;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ReturnNID>()>&
            REQFlitBuilder<T, config, able>::SetReturnNID(typename REQ<config>::returnnid_t returnNID) noexcept requires is_field_applicable<T->fields->ReturnNID>
    {
        this->flit.ReturnNID() = returnNID;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ReturnNID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::stashnid_t REQFlitBuilder<T, config, able>::StashNID() const noexcept requires is_field_applicable<T->fields->StashNID>
    {
        return details::PCFReturnNIDToStashNID<config>(flit.ReturnNID());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashNID>()>
            REQFlitBuilder<T, config, able>::StashNID(typename REQ<config>::stashnid_t stashNID) const noexcept requires is_field_applicable<T->fields->StashNID>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashNID>()>(*this);
        builder.flit.ReturnNID() = details::PCFReturnNIDFromStashNID<config>(builder.flit.ReturnNID(), stashNID);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashNID>()>&
            REQFlitBuilder<T, config, able>::SetStashNID(typename REQ<config>::stashnid_t stashNID) noexcept requires is_field_applicable<T->fields->StashNID>
    {
        this->flit.ReturnNID() = details::PCFReturnNIDFromStashNID<config>(this->flit.ReturnNID(), stashNID);
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashNID>()>*>(this);
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::slcrephint_t REQFlitBuilder<T, config, able>::SLCRepHint() const noexcept requires is_field_applicable<T->fields->SLCRepHint>
    {
        return details::PCFReturnNIDToSLCRepHint<config>(flit.ReturnNID());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SLCRepHint>()>
            REQFlitBuilder<T, config, able>::SLCRepHint(typename REQ<config>::slcrephint_t slcRepHint) const noexcept requires is_field_applicable<T->fields->SLCRepHint>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SLCRepHint>()>(*this);
        builder.flit.ReturnNID() = details::PCFReturnNIDFromSLCRepHint<config>(builder.flit.ReturnNID(), slcRepHint);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SLCRepHint>()>&
            REQFlitBuilder<T, config, able>::SetSLCRepHint(typename REQ<config>::slcrephint_t slcRepHint) noexcept requires is_field_applicable<T->fields->SLCRepHint>
    {
        this->flit.ReturnNID() = details::PCFReturnNIDFromSLCRepHint<config>(this->flit.ReturnNID(), slcRepHint);
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SLCRepHint>()>*>(this);
    }
#endif

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::stashnidvalid_t REQFlitBuilder<T, config, able>::StashNIDValid() const noexcept requires is_field_applicable<T->fields->StashNIDValid>
    {
        return flit.StashNIDValid();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashNIDValid>()>
            REQFlitBuilder<T, config, able>::StashNIDValid(typename REQ<config>::stashnidvalid_t stashNIDValid) const noexcept requires is_field_applicable<T->fields->StashNIDValid>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashNIDValid>()>(*this);
        builder.flit.StashNIDValid() = stashNIDValid;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashNIDValid>()>&
            REQFlitBuilder<T, config, able>::SetStashNIDValid(typename REQ<config>::stashnidvalid_t stashNIDValid) noexcept requires is_field_applicable<T->fields->StashNIDValid>
    {
        this->flit.StashNIDValid() = stashNIDValid;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashNIDValid>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::endian_t REQFlitBuilder<T, config, able>::Endian() const noexcept requires is_field_applicable<T->fields->Endian>
    {
        return details::PCFStashNIDValidToEndian<config>(flit.StashNIDValid());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Endian>()>
            REQFlitBuilder<T, config, able>::Endian(typename REQ<config>::endian_t endian) const noexcept requires is_field_applicable<T->fields->Endian>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Endian>()>(*this);
        builder.flit.StashNIDValid() = details::PCFStashNIDValidFromEndian<config>(builder.flit.StashNIDValid(), endian);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Endian>()>&
            REQFlitBuilder<T, config, able>::SetEndian(typename REQ<config>::endian_t endian) noexcept requires is_field_applicable<T->fields->Endian>
    {
        this->flit.StashNIDValid() = details::PCFStashNIDValidFromEndian<config>(this->flit.StashNIDValid(), endian);
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Endian>()>*>(this);
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::deep_t REQFlitBuilder<T, config, able>::Deep() const noexcept requires is_field_applicable<T->fields->Deep>
    {
        return details::PCFStashNIDValidToDeep<config>(flit.StashNIDValid());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Deep>()>
            REQFlitBuilder<T, config, able>::Deep(typename REQ<config>::deep_t deep) const noexcept requires is_field_applicable<T->fields->Deep>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Deep>()>(*this);
        builder.flit.StashNIDValid() = details::PCFStashNIDValidFromDeep<config>(builder.flit.StashNIDValid(), deep);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Deep>()>&
            REQFlitBuilder<T, config, able>::SetDeep(typename REQ<config>::deep_t deep) noexcept requires is_field_applicable<T->fields->Deep>
    {
        this->flit.StashNIDValid() = details::PCFStashNIDValidFromDeep<config>(this->flit.StashNIDValid(), deep);
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Deep>()>*>(this);
    }
#endif

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::returntxnid_t REQFlitBuilder<T, config, able>::ReturnTxnID() const noexcept requires is_field_applicable<T->fields->ReturnTxnID>
    {
        return flit.ReturnTxnID();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ReturnTxnID>()>
            REQFlitBuilder<T, config, able>::ReturnTxnID(typename REQ<config>::returntxnid_t returnTxnID) const noexcept requires is_field_applicable<T->fields->ReturnTxnID>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ReturnTxnID>()>(*this);
        builder.flit.ReturnTxnID() = returnTxnID;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ReturnTxnID>()>&
            REQFlitBuilder<T, config, able>::SetReturnTxnID(typename REQ<config>::returntxnid_t returnTxnID) noexcept requires is_field_applicable<T->fields->ReturnTxnID>
    {
        this->flit.ReturnTxnID() = returnTxnID;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ReturnTxnID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::stashlpidvalid_t REQFlitBuilder<T, config, able>::StashLPIDValid() const noexcept requires is_field_applicable<T->fields->StashLPIDValid>
    {
        return details::PCFReturnTxnIDToStashLPIDValid<config>(flit.ReturnTxnID());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashLPIDValid>()>
            REQFlitBuilder<T, config, able>::StashLPIDValid(typename REQ<config>::stashlpidvalid_t stashLPIDValid) const noexcept requires is_field_applicable<T->fields->StashLPIDValid>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashLPIDValid>()>(*this);
        builder.flit.ReturnTxnID() = details::PCFReturnTxnIDFromStashLPIDValid<config>(builder.flit.ReturnTxnID(), stashLPIDValid);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashLPIDValid>()>&
            REQFlitBuilder<T, config, able>::SetStashLPIDValid(typename REQ<config>::stashlpidvalid_t stashLPIDValid) noexcept requires is_field_applicable<T->fields->StashLPIDValid>
    {
        this->flit.ReturnTxnID() = details::PCFReturnTxnIDFromStashLPIDValid<config>(this->flit.ReturnTxnID(), stashLPIDValid);
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashLPIDValid>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::stashlpid_t REQFlitBuilder<T, config, able>::StashLPID() const noexcept requires is_field_applicable<T->fields->StashLPID>
    {
        return details::PCFReturnTxnIDToStashLPID<config>(flit.ReturnTxnID());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashLPID>()>
            REQFlitBuilder<T, config, able>::StashLPID(typename REQ<config>::stashlpid_t stashLPID) const noexcept requires is_field_applicable<T->fields->StashLPID>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashLPID>()>(*this);
        builder.flit.ReturnTxnID() = details::PCFReturnTxnIDFromStashLPID<config>(builder.flit.ReturnTxnID(), stashLPID);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashLPID>()>&
            REQFlitBuilder<T, config, able>::SetStashLPID(typename REQ<config>::stashlpid_t stashLPID) noexcept requires is_field_applicable<T->fields->StashLPID>
    {
        this->flit.ReturnTxnID() = details::PCFReturnTxnIDFromStashLPID<config>(this->flit.ReturnTxnID(), stashLPID);
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashLPID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::opcode_t REQFlitBuilder<T, config, able>::Opcode() const noexcept requires is_field_applicable<T->fields->Opcode>
    {
        return flit.Opcode();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Opcode>()>
            REQFlitBuilder<T, config, able>::Opcode(typename REQ<config>::opcode_t opcode) const noexcept requires is_field_applicable<T->fields->Opcode>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Opcode>()>(*this);
        builder.flit.Opcode() = opcode;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Opcode>()>&
            REQFlitBuilder<T, config, able>::SetOpcode(typename REQ<config>::opcode_t opcode) noexcept requires is_field_applicable<T->fields->Opcode>
    {
        this->flit.Opcode() = opcode;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Opcode>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::ssize_t REQFlitBuilder<T, config, able>::Size() const noexcept requires is_field_applicable<T->fields->Size>
    {
        return flit.Size();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Size>()>
            REQFlitBuilder<T, config, able>::Size(typename REQ<config>::ssize_t size) const noexcept requires is_field_applicable<T->fields->Size>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Size>()>(*this);
        builder.flit.Size() = size;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Size>()>&
            REQFlitBuilder<T, config, able>::SetSize(typename REQ<config>::ssize_t size) noexcept requires is_field_applicable<T->fields->Size>
    {
        this->flit.Size() = size;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Size>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::addr_t REQFlitBuilder<T, config, able>::Addr() const noexcept requires is_field_applicable<T->fields->Addr>
    {
        return flit.Addr();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Addr>()>
            REQFlitBuilder<T, config, able>::Addr(typename REQ<config>::addr_t addr) const noexcept requires is_field_applicable<T->fields->Addr>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Addr>()>(*this);
        builder.flit.Addr() = addr;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Addr>()>&
            REQFlitBuilder<T, config, able>::SetAddr(typename REQ<config>::addr_t addr) noexcept requires is_field_applicable<T->fields->Addr>
    {
        this->flit.Addr() = addr;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Addr>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::ns_t REQFlitBuilder<T, config, able>::NS() const noexcept requires is_field_applicable<T->fields->NS>
    {
        return flit.NS();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::NS>()>
            REQFlitBuilder<T, config, able>::NS(typename REQ<config>::ns_t ns) const noexcept requires is_field_applicable<T->fields->NS>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::NS>()>(*this);
        builder.flit.NS() = ns;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::NS>()>&
            REQFlitBuilder<T, config, able>::SetNS(typename REQ<config>::ns_t ns) noexcept requires is_field_applicable<T->fields->NS>
    {
        this->flit.NS() = ns;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::NS>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::likelyshared_t REQFlitBuilder<T, config, able>::LikelyShared() const noexcept requires is_field_applicable<T->fields->LikelyShared>
    {
        return flit.LikelyShared();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LikelyShared>()>
            REQFlitBuilder<T, config, able>::LikelyShared(typename REQ<config>::likelyshared_t likelyShared) const noexcept requires is_field_applicable<T->fields->LikelyShared>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LikelyShared>()>(*this);
        builder.flit.LikelyShared() = likelyShared;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LikelyShared>()>&
            REQFlitBuilder<T, config, able>::SetLikelyShared(typename REQ<config>::likelyshared_t likelyShared) noexcept requires is_field_applicable<T->fields->LikelyShared>
    {
        this->flit.LikelyShared() = likelyShared;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LikelyShared>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::allowretry_t REQFlitBuilder<T, config, able>::AllowRetry() const noexcept requires is_field_applicable<T->fields->AllowRetry>
    {
        return flit.AllowRetry();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::AllowRetry>()>
            REQFlitBuilder<T, config, able>::AllowRetry(typename REQ<config>::allowretry_t allowRetry) const noexcept requires is_field_applicable<T->fields->AllowRetry>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::AllowRetry>()>(*this);
        builder.flit.AllowRetry() = allowRetry;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::AllowRetry>()>&
            REQFlitBuilder<T, config, able>::SetAllowRetry(typename REQ<config>::allowretry_t allowRetry) noexcept requires is_field_applicable<T->fields->AllowRetry>
    {
        this->flit.AllowRetry() = allowRetry;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::AllowRetry>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::order_t REQFlitBuilder<T, config, able>::Order() const noexcept requires is_field_applicable<T->fields->Order>
    {
        return flit.Order();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Order>()>
            REQFlitBuilder<T, config, able>::Order(typename REQ<config>::order_t order) const noexcept requires is_field_applicable<T->fields->Order>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Order>()>(*this);
        builder.flit.Order() = order;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Order>()>&
            REQFlitBuilder<T, config, able>::SetOrder(typename REQ<config>::order_t order) noexcept requires is_field_applicable<T->fields->Order>
    {
        this->flit.Order() = order;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Order>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::pcrdtype_t REQFlitBuilder<T, config, able>::PCrdType() const noexcept requires is_field_applicable<T->fields->PCrdType>
    {
        return flit.PCrdType();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::PCrdType>()>
            REQFlitBuilder<T, config, able>::PCrdType(typename REQ<config>::pcrdtype_t pCrdType) const noexcept requires is_field_applicable<T->fields->PCrdType>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::PCrdType>()>(*this);
        builder.flit.PCrdType() = pCrdType;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::PCrdType>()>&
            REQFlitBuilder<T, config, able>::SetPCrdType(typename REQ<config>::pcrdtype_t pCrdType) noexcept requires is_field_applicable<T->fields->PCrdType>
    {
        this->flit.PCrdType() = pCrdType;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::PCrdType>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::memattr_t REQFlitBuilder<T, config, able>::MemAttr() const noexcept requires is_memattr_applicable<T>
    {
        return flit.MemAttr();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::MemAttr>()>
            REQFlitBuilder<T, config, able>::MemAttr(typename REQ<config>::memattr_t memAttr) const noexcept requires is_memattr_applicable<T>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::MemAttr>()>(*this);
        builder.flit.MemAttr() = memAttr;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::MemAttr>()>&
            REQFlitBuilder<T, config, able>::SetMemAttr(typename REQ<config>::memattr_t memAttr) noexcept requires is_memattr_applicable<T>
    {
        this->flit.MemAttr() = memAttr;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::MemAttr>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::snpattr_t REQFlitBuilder<T, config, able>::SnpAttr() const noexcept requires is_field_applicable<T->fields->SnpAttr>
    {
        return flit.SnpAttr();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SnpAttr>()>
            REQFlitBuilder<T, config, able>::SnpAttr(typename REQ<config>::snpattr_t snpAttr) const noexcept requires is_field_applicable<T->fields->SnpAttr>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SnpAttr>()>(*this);
        builder.flit.SnpAttr() = snpAttr;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SnpAttr>()>&
            REQFlitBuilder<T, config, able>::SetSnpAttr(typename REQ<config>::snpattr_t snpAttr) noexcept requires is_field_applicable<T->fields->SnpAttr>
    {
        this->flit.SnpAttr() = snpAttr;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SnpAttr>()>*>(this);
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::dodwt_t REQFlitBuilder<T, config, able>::DoDWT() const noexcept requires is_field_applicable<T->fields->DoDWT>
    {
        return details::PCFSnpAttrToDoDWT<config>(flit.SnpAttr());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::DoDWT>()>
            REQFlitBuilder<T, config, able>::DoDWT(typename REQ<config>::dodwt_t doDWT) const noexcept requires is_field_applicable<T->fields->DoDWT>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::DoDWT>()>(*this);
        builder.flit.SnpAttr() = details::PCFSnpAttrFromDoDWT<config>(builder.flit.SnpAttr(), doDWT);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::DoDWT>()>&
            REQFlitBuilder<T, config, able>::SetDoDWT(typename REQ<config>::dodwt_t doDWT) noexcept requires is_field_applicable<T->fields->DoDWT>
    {
        this->flit.SnpAttr() = details::PCFSnpAttrFromDoDWT<config>(this->flit.SnpAttr(), doDWT);
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::DoDWT>()>*>(this);
    }
#endif

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::pgroupid_t REQFlitBuilder<T, config, able>::PGroupID() const noexcept requires is_field_applicable<T->fields->PGroupID>
    {
        return flit.PGroupID();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::PGroupID>()>
            REQFlitBuilder<T, config, able>::PGroupID(typename REQ<config>::pgroupid_t pGroupID) const noexcept requires is_field_applicable<T->fields->PGroupID>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::PGroupID>()>(*this);
        builder.flit.PGroupID() = pGroupID;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::PGroupID>()>&
            REQFlitBuilder<T, config, able>::SetPGroupID(typename REQ<config>::pgroupid_t pGroupID) noexcept requires is_field_applicable<T->fields->PGroupID>
    {
        this->flit.PGroupID() = pGroupID;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::PGroupID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::stashgroupid_t REQFlitBuilder<T, config, able>::StashGroupID() const noexcept requires is_field_applicable<T->fields->StashGroupID>
    {
        return details::PCFPGroupIDToStashGroupID<config>(flit.PGroupID());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashGroupID>()>
            REQFlitBuilder<T, config, able>::StashGroupID(typename REQ<config>::stashgroupid_t stashGroupID) const noexcept requires is_field_applicable<T->fields->StashGroupID>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashGroupID>()>(*this);
        builder.flit.PGroupID() = details::PCFPGroupIDFromStashGroupID<config>(builder.flit.PGroupID(), stashGroupID);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashGroupID>()>&
            REQFlitBuilder<T, config, able>::SetStashGroupID(typename REQ<config>::stashgroupid_t stashGroupID) noexcept requires is_field_applicable<T->fields->StashGroupID>
    {
        this->flit.PGroupID() = details::PCFPGroupIDFromStashGroupID<config>(this->flit.PGroupID(), stashGroupID);
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::StashGroupID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::taggroupid_t REQFlitBuilder<T, config, able>::TagGroupID() const noexcept requires is_field_applicable<T->fields->TagGroupID>
    {
        return details::PCFPGroupIDToTagGroupID<config>(flit.PGroupID());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TagGroupID>()>
            REQFlitBuilder<T, config, able>::TagGroupID(typename REQ<config>::taggroupid_t tagGroupID) const noexcept requires is_field_applicable<T->fields->TagGroupID>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TagGroupID>()>(*this);
        builder.flit.PGroupID() = details::PCFPGroupIDFromTagGroupID<config>(builder.flit.PGroupID(), tagGroupID);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TagGroupID>()>&
            REQFlitBuilder<T, config, able>::SetTagGroupID(typename REQ<config>::taggroupid_t tagGroupID) noexcept requires is_field_applicable<T->fields->TagGroupID>
    {
        this->flit.PGroupID() = details::PCFPGroupIDFromTagGroupID<config>(this->flit.PGroupID(), tagGroupID);
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TagGroupID>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::lpid_t REQFlitBuilder<T, config, able>::LPID() const noexcept requires is_field_applicable<T->fields->LPID>
    {
        return details::PCFPGroupIDToLPID<config>(flit.PGroupID());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LPID>()>
            REQFlitBuilder<T, config, able>::LPID(typename REQ<config>::lpid_t lpid) const noexcept requires is_field_applicable<T->fields->LPID>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LPID>()>(*this);
        builder.flit.PGroupID() = details::PCFPGroupIDFromLPID<config>(builder.flit.PGroupID(), lpid);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LPID>()>&
            REQFlitBuilder<T, config, able>::SetLPID(typename REQ<config>::lpid_t lpid) noexcept requires is_field_applicable<T->fields->LPID>
    {
        this->flit.PGroupID() = details::PCFPGroupIDFromLPID<config>(this->flit.PGroupID(), lpid);
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LPID>()>*>(this);
    }

#else
    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::lpid_t REQFlitBuilder<T, config, able>::LPID() const noexcept requires is_field_applicable<T->fields->LPID>
    {
        return flit.LPID();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LPID>()>
            REQFlitBuilder<T, config, able>::LPID(typename REQ<config>::lpid_t lpid) const noexcept requires is_field_applicable<T->fields->LPID>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LPID>()>(*this);
        builder.flit.LPID() = lpid;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LPID>()>&
            REQFlitBuilder<T, config, able>::SetLPID(typename REQ<config>::lpid_t lpid) noexcept requires is_field_applicable<T->fields->LPID>
    {
        this->flit.LPID() = lpid;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::LPID>()>*>(this);
    }
#endif

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::excl_t REQFlitBuilder<T, config, able>::Excl() const noexcept requires is_field_applicable<T->fields->Excl>
    {
        return flit.Excl();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Excl>()>
            REQFlitBuilder<T, config, able>::Excl(typename REQ<config>::excl_t excl) const noexcept requires is_field_applicable<T->fields->Excl>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Excl>()>(*this);
        builder.flit.Excl() = excl;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Excl>()>&
            REQFlitBuilder<T, config, able>::SetExcl(typename REQ<config>::excl_t excl) noexcept requires is_field_applicable<T->fields->Excl>
    {
        this->flit.Excl() = excl;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::Excl>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::snoopme_t REQFlitBuilder<T, config, able>::SnoopMe() const noexcept requires is_field_applicable<T->fields->SnoopMe>
    {
        return details::PCFExclToSnoopMe<config>(flit.Excl());
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SnoopMe>()>
            REQFlitBuilder<T, config, able>::SnoopMe(typename REQ<config>::snoopme_t snoopMe) const noexcept requires is_field_applicable<T->fields->SnoopMe>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SnoopMe>()>(*this);
        builder.flit.Excl() = details::PCFExclFromSnoopMe<config>(builder.flit.Excl(), snoopMe);
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SnoopMe>()>&
            REQFlitBuilder<T, config, able>::SetSnoopMe(typename REQ<config>::snoopme_t snoopMe) noexcept requires is_field_applicable<T->fields->SnoopMe>
    {
        this->flit.Excl() = details::PCFExclFromSnoopMe<config>(this->flit.Excl(), snoopMe);
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::SnoopMe>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::expcompack_t REQFlitBuilder<T, config, able>::ExpCompAck() const noexcept requires is_field_applicable<T->fields->ExpCompAck>
    {
        return flit.ExpCompAck();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ExpCompAck>()>
            REQFlitBuilder<T, config, able>::ExpCompAck(typename REQ<config>::expcompack_t expCompAck) const noexcept requires is_field_applicable<T->fields->ExpCompAck>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ExpCompAck>()>(*this);
        builder.flit.ExpCompAck() = expCompAck;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ExpCompAck>()>&
            REQFlitBuilder<T, config, able>::SetExpCompAck(typename REQ<config>::expcompack_t expCompAck) noexcept requires is_field_applicable<T->fields->ExpCompAck>
    {
        this->flit.ExpCompAck() = expCompAck;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::ExpCompAck>()>*>(this);
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::tagop_t REQFlitBuilder<T, config, able>::TagOp() const noexcept requires is_field_applicable<T->fields->TagOp>
    {
        return flit.TagOp();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TagOp>()>
            REQFlitBuilder<T, config, able>::TagOp(typename REQ<config>::tagop_t tagOp) const noexcept requires is_field_applicable<T->fields->TagOp>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TagOp>()>(*this);
        builder.flit.TagOp() = tagOp;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TagOp>()>&
            REQFlitBuilder<T, config, able>::SetTagOp(typename REQ<config>::tagop_t tagOp) noexcept requires is_field_applicable<T->fields->TagOp>
    {
        this->flit.TagOp() = tagOp;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TagOp>()>*>(this);
    }
#endif

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::tracetag_t REQFlitBuilder<T, config, able>::TraceTag() const noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        return flit.TraceTag();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TraceTag>()>
            REQFlitBuilder<T, config, able>::TraceTag(typename REQ<config>::tracetag_t traceTag) const noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TraceTag>()>(*this);
        builder.flit.TraceTag() = traceTag;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TraceTag>()>&
            REQFlitBuilder<T, config, able>::SetTraceTag(typename REQ<config>::tracetag_t traceTag) noexcept requires is_field_applicable<T->fields->TraceTag>
    {
        this->flit.TraceTag() = traceTag;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::TraceTag>()>*>(this);
    }

#ifdef CHI_ISSUE_EB_ENABLE
    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::mpam_t REQFlitBuilder<T, config, able>::MPAM() const noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<REQ<config>>
    {
        return flit.MPAM();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::MPAM>()>
            REQFlitBuilder<T, config, able>::MPAM(typename REQ<config>::mpam_t mpam) const noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<REQ<config>>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::MPAM>()>(*this);
        builder.flit.MPAM() = mpam;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::MPAM>()>&
            REQFlitBuilder<T, config, able>::SetMPAM(typename REQ<config>::mpam_t mpam) noexcept requires is_field_applicable<T->fields->MPAM> && has_mpam<REQ<config>>
    {
        this->flit.MPAM() = mpam;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::MPAM>()>*>(this);
    }
#endif

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr typename REQ<config>::rsvdc_t REQFlitBuilder<T, config, able>::RSVDC() const noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<REQ<config>>
    {
        return flit.RSVDC();
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::RSVDC>()>
            REQFlitBuilder<T, config, able>::RSVDC(typename REQ<config>::rsvdc_t rsvdc) const noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<REQ<config>>
    {
        auto builder = REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::RSVDC>()>(*this);
        builder.flit.RSVDC() = rsvdc;
        return builder;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::RSVDC>()>&
            REQFlitBuilder<T, config, able>::SetRSVDC(typename REQ<config>::rsvdc_t rsvdc) noexcept requires is_field_applicable<T->fields->RSVDC> && has_rsvdc<REQ<config>>
    {
        this->flit.RSVDC() = rsvdc;
        return *reinterpret_cast<REQFlitBuilder<T, config, details::NextREQFlitBuildability<T, able, details::REQFlitField::RSVDC>()>*>(this);
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQ<config> REQFlitBuilder<T, config, able>::Eval() const noexcept requires details::is_buildable<able>
    {
        return flit;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQ<config> REQFlitBuilder<T, config, able>::Build() const noexcept requires details::is_buildable<able>
    {
        return flit;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline consteval REQ<config> REQFlitBuilder<T, config, able>::EvalPartial() const noexcept
    {
        return flit;
    }

    template<REQBuildTargetEnum T, REQFlitConfigurationConcept config, details::REQFlitBuildability able>
    inline constexpr REQ<config> REQFlitBuilder<T, config, able>::BuildPartial() const noexcept
    {
        return flit;
    }
}


#endif // __CHI__CHI_UTIL_FLIT_BUILDER_REQ_*

//#endif // __CHI__CHI_UTIL_FLIT_BUILDER_REQ
