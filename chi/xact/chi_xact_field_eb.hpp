//#pragma once

//#ifndef __CHI__CHI_XACT_FIELD
//#define __CHI__CHI_XACT_FIELD

#ifndef CHI_XACT_FIELD__STANDALONE
#   include "chi_xact_field_header.hpp"                 // IWYU pragma: keep
#   include "../spec/chi_protocol_encoding_header.hpp"  // IWYU pragma: keep
#   include "../spec/chi_protocol_encoding.hpp"         // IWYU pragma: keep
#endif


#if (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_XACT_FIELD_EB))

#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_XACT_FIELD_EB
#endif


#ifdef CHI_ISSUE_EB_ENABLE 

#include "chi_xact_field.hpp"

/*
namespace CHI {
*/
    namespace Xact {

        // Request Field
        class RequestFieldMappingBack {
        public:
            const FieldConvention   QoS;
            const FieldConvention   TgtID;
            const FieldConvention   SrcID;
            const FieldConvention   TxnID;
            const FieldConvention   Opcode;
            const FieldConvention   AllowRetry;
            const FieldConvention   PCrdType;
            const FieldConvention   RSVDC;
            const FieldConvention   TagOp;
            const FieldConvention   TraceTag;
            const FieldConvention   MPAM;
            const FieldConvention   Addr;
            const FieldConvention   NS;
            const FieldConvention   Size;
            const FieldConvention   Allocate;
            const FieldConvention   Cacheable;
            const FieldConvention   Device;
            const FieldConvention   EWA;
            const FieldConvention   SnpAttr;
            const FieldConvention   DoDWT;
            const FieldConvention   Order;
            const FieldConvention   LikelyShared;
            const FieldConvention   Excl;
            const FieldConvention   SnoopMe;
            const FieldConvention   ExpCompAck;
            const FieldConvention   LPID;
            const FieldConvention   TagGroupID;
            const FieldConvention   StashGroupID;
            const FieldConvention   PGroupID;
            const FieldConvention   ReturnNID;
            const FieldConvention   StashNID;
            const FieldConvention   SLCRepHint;
            const FieldConvention   StashNIDValid;
            const FieldConvention   Endian;
            const FieldConvention   Deep;
            const FieldConvention   ReturnTxnID;
            const FieldConvention   StashLPIDValid;
            const FieldConvention   StashLPID;

        public:
            inline constexpr RequestFieldMappingBack(
                const FieldConvention   QoS,
                const FieldConvention   TgtID,
                const FieldConvention   SrcID,
                const FieldConvention   TxnID,
                const FieldConvention   Opcode,
                const FieldConvention   AllowRetry,
                const FieldConvention   PCrdType,
                const FieldConvention   RSVDC,
                const FieldConvention   TagOp,
                const FieldConvention   TraceTag,
                const FieldConvention   MPAM,
                const FieldConvention   Addr,
                const FieldConvention   NS,
                const FieldConvention   Size,
                const FieldConvention   Allocate,
                const FieldConvention   Cacheable,
                const FieldConvention   Device,
                const FieldConvention   EWA,
                const FieldConvention   SnpAttr,
                const FieldConvention   DoDWT,
                const FieldConvention   Order,
                const FieldConvention   LikelyShared,
                const FieldConvention   Excl,
                const FieldConvention   SnoopMe,
                const FieldConvention   ExpCompAck,
                const FieldConvention   LPID,
                const FieldConvention   TagGroupID,
                const FieldConvention   StashGroupID,
                const FieldConvention   PGroupID,
                const FieldConvention   ReturnNID,
                const FieldConvention   StashNID,
                const FieldConvention   SLCRepHint,
                const FieldConvention   StashNIDValid,
                const FieldConvention   Endian,
                const FieldConvention   Deep,
                const FieldConvention   ReturnTxnID,
                const FieldConvention   StashLPIDValid,
                const FieldConvention   StashLPID
            ) noexcept
                : QoS               (QoS)
                , TgtID             (TgtID)
                , SrcID             (SrcID)
                , TxnID             (TxnID)
                , Opcode            (Opcode)
                , AllowRetry        (AllowRetry)
                , PCrdType          (PCrdType)
                , RSVDC             (RSVDC)
                , TagOp             (TagOp)
                , TraceTag          (TraceTag)
                , MPAM              (MPAM)
                , Addr              (Addr)
                , NS                (NS)
                , Size              (Size)
                , Allocate          (Allocate)
                , Cacheable         (Cacheable)
                , Device            (Device)
                , EWA               (EWA)
                , SnpAttr           (SnpAttr)
                , DoDWT             (DoDWT)
                , Order             (Order)
                , LikelyShared      (LikelyShared)
                , Excl              (Excl)
                , SnoopMe           (SnoopMe)
                , ExpCompAck        (ExpCompAck)
                , LPID              (LPID)
                , TagGroupID        (TagGroupID)
                , StashGroupID      (StashGroupID)
                , PGroupID          (PGroupID)
                , ReturnNID         (ReturnNID)
                , StashNID          (StashNID)
                , SLCRepHint        (SLCRepHint)
                , StashNIDValid     (StashNIDValid)
                , Endian            (Endian)
                , Deep              (Deep)
                , ReturnTxnID       (ReturnTxnID)
                , StashLPIDValid    (StashLPIDValid)
                , StashLPID         (StashLPID)
            { }
        };

        using RequestFieldMapping = const RequestFieldMappingBack*;

        namespace RequestFieldMappings {
            using enum FieldConvention;
            /* Read, Dataless and Miscellaneous */
            inline constexpr RequestFieldMappingBack ReqLCrdReturn                  (X , X , X , A0, Y , X , X , X , X , X , X , X , X , X  , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X );
            inline constexpr RequestFieldMappingBack PCrdReturn                     (Y , Y , Y , I0, Y , I0, Y , Y , I0, Y , I0, I0, I0, I0 , I0, I0, I0, I0, I0, S , I0, I0, I0, I0, A0, I0, I0, I0, I0, I0, I0, I0, I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack DVMOp                          (Y , Y , Y , Y , Y , Y , Y , Y , I0, Y , I0, Y , I0, B8 , I0, I0, I0, I0, Y , S , I0, I0, I0, I0, A0, Y , S , S , S , I0, I0, I0, I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack PrefetchTgt                    (Y , Y , Y , X , Y , A0, X , Y , Y , Y , Y , Y , Y , X  , X , X , X , X , X , X , X , X , X , S , I0, Y , S , S , S , I0, I0, I0, X , X , X , I0, I0, I0);
            inline constexpr RequestFieldMappingBack ReadNoSnp                      (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y  , Y , Y , Y , Y , A0, S , Y , A0, Y , S , Y , Y , S , S , S , Y , S , Y , I0, I0, I0, Y , S , S );
            inline constexpr RequestFieldMappingBack ReadNoSnpSep                   (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y  , Y , Y , Y , Y , A0, S , Y , A0, A0, S , A0, Y , S , S , S , Y , S , Y , I0, I0, I0, Y , S , S );
            inline constexpr RequestFieldMappingBack ReadOnce                       (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , Y , A0, A0, S , Y , Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack ReadOnceCleanInvalid           (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , Y , A0, A0, S , Y , Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack ReadOnceMakeInvalid            (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, A0, A1, A0, A1, A1, S , Y , A0, A0, S , Y , Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack ReadClean                      (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, Y , Y , S , A1, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack ReadNotSharedDirty             (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, Y , Y , S , A1, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack ReadShared                     (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, Y , Y , S , A1, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack ReadUnique                     (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, A0, A0, S , A1, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack ReadPreferUnique               (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, A0, Y , S , A1, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack MakeReadUnique                 (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, A0, Y , S , A1, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack CleanShared                    (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , Y , Y , Y , Y , S , I0, A0, A0, S , A0, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack CleanSharedPersist             (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , Y , Y , Y , Y , S , I0, A0, A0, S , A0, I0, I0, I0, I0, S , S , Y , S , S , Y , I0, I0, I0);
            inline constexpr RequestFieldMappingBack CleanSharedPersistSep          (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , Y , Y , Y , Y , S , I0, A0, A0, S , A0, S , S , S , Y , Y , S , Y , S , S , Y , I0, I0, I0);
            inline constexpr RequestFieldMappingBack CleanInvalid                   (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , Y , Y , Y , Y , S , I0, A0, A0, S , A0, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack MakeInvalid                    (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , Y , Y , Y , Y , S , I0, A0, A0, S , A0, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack CleanUnique                    (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, A0, Y , S , A1, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack MakeUnique                     (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, A0, A0, S , A1, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack Evict                          (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, A0, A1, A0, A1, A1, S , A0, A0, A0, S , A0, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            /* Write and Combined Write */
            inline constexpr RequestFieldMappingBack WriteNoSnpPtl                  (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y  , Y , Y , Y , Y , A0, Y , Y , A0, Y , S , Y , Y , Y , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteNoSnpPtlCleanInv          (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y  , Y , Y , Y , Y , A0, Y , Y , A0, A0, S , Y , Y , Y , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteNoSnpPtlCleanSh           (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y  , Y , Y , Y , Y , A0, Y , Y , A0, A0, S , Y , Y , Y , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteNoSnpPtlCleanShPerSep     (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y  , Y , Y , Y , Y , A0, Y , Y , A0, A0, S , Y , S , S , S , Y , S , S , Y , S , S , Y , I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteNoSnpFull                 (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , Y , Y , Y , A0, Y , Y , A0, Y , S , Y , Y , Y , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteNoSnpFullCleanInv         (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , Y , Y , Y , A0, Y , Y , A0, A0, S , Y , Y , Y , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteNoSnpFullCleanSh          (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , Y , Y , Y , A0, Y , Y , A0, A0, S , Y , Y , Y , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteNoSnpFullCleanShPerSep    (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , Y , Y , Y , A0, Y , Y , A0, A0, S , Y , S , S , S , Y , S , S , Y , S , S , Y , I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteNoSnpZero                 (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , Y , Y , Y , A0, A0, Y , A0, A0, S , A0, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteUniquePtlStash            (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y  , Y , A1, A0, A1, A1, S , Y , Y , A0, S , Y , Y , Y , S , S , S , Y , Y , Y , S , S , S , Y , Y );
            inline constexpr RequestFieldMappingBack WriteUniqueFullStash           (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , Y , Y , A0, S , Y , Y , Y , S , S , S , Y , Y , Y , S , S , S , Y , Y );
            inline constexpr RequestFieldMappingBack WriteUniquePtl                 (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y  , Y , A1, A0, A1, A1, S , Y , Y , A0, S , Y , Y , Y , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteUniquePtlCleanSh          (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y  , Y , A1, A0, A1, A1, S , Y , Y , A0, S , Y , Y , Y , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteUniquePtlCleanShPerSep    (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y  , Y , A1, A0, A1, A1, S , Y , Y , A0, S , Y , S , S , S , Y , S , S , Y , S , S , Y , I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteUniqueFull                (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , Y , Y , A0, S , Y , Y , Y , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteUniqueFullCleanSh         (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , Y , Y , A0, S , Y , Y , Y , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteUniqueFullCleanShPerSep   (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , Y , Y , A0, S , Y , S , S , S , Y , S , S , Y , S , S , Y , I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteUniqueZero                (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , Y , Y , A0, S , A0, Y , Y , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteBackPtl                   (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, A0, A0, S , A0, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteBackFull                  (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, Y , A0, S , A0, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteBackFullCleanInv          (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, Y , A0, S , A0, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteBackFullCleanSh           (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, Y , A0, S , A0, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteBackFullCleanShPerSep     (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, Y , A0, S , A0, S , S , S , Y , S , S , Y , S , S , Y , I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteCleanFull                 (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, Y , A0, S , A0, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteCleanFullCleanSh          (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, Y , A0, S , A0, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteCleanFullCleanShPerSep    (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, Y , A0, S , A0, S , S , S , Y , S , S , Y , S , S , Y , I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteEvictFull                 (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, A1, A1, A0, A1, A1, S , A0, Y , A0, S , A0, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            inline constexpr RequestFieldMappingBack WriteEvictOrEvict              (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, A1, A1, A0, A1, A1, S , A0, Y , A0, S , A1, Y , S , S , S , S , S , Y , I0, I0, I0, I0, I0, I0);
            /* Stash and Atomic */
            inline constexpr RequestFieldMappingBack StashOnceUnique                (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, Y , A0, S , A0, Y , S , S , S , S , Y , Y , Y , S , S , S , Y , Y );
            inline constexpr RequestFieldMappingBack StashOnceSepUnique             (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, Y , A0, S , A0, S , S , Y , S , S , Y , Y , Y , S , S , S , Y , Y );
            inline constexpr RequestFieldMappingBack StashOnceShared                (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, Y , A0, S , A0, Y , S , S , S , S , Y , Y , Y , S , S , S , Y , Y );
            inline constexpr RequestFieldMappingBack StashOnceSepShared             (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , B64, Y , A1, A0, A1, A1, S , A0, Y , A0, S , A0, S , S , Y , S , S , Y , Y , Y , S , S , S , Y , Y );
            inline constexpr RequestFieldMappingBack AtomicLoad                     (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y  , Y , Y , Y , Y , Y , S , Y , A0, S , Y , A0, Y , Y , S , S , Y , S , S , S , Y , S , Y , S , S );
            inline constexpr RequestFieldMappingBack AtomicStore                    (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y  , Y , Y , Y , Y , Y , S , Y , A0, S , Y , A0, Y , Y , S , S , Y , S , S , S , Y , S , X , S , S );
            inline constexpr RequestFieldMappingBack AtomicCompare                  (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y  , Y , Y , Y , Y , Y , S , Y , A0, S , Y , A0, Y , Y , S , S , Y , S , S , S , Y , S , Y , S , S );
            inline constexpr RequestFieldMappingBack AtomicSwap                     (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y  , Y , Y , Y , Y , Y , S , Y , A0, S , Y , A0, Y , Y , S , S , Y , S , S , S , Y , S , Y , S , S );
        };

        class RequestFieldMappingTable {
        private:
            RequestFieldMapping    table[1 << Flits::REQ<>::OPCODE_WIDTH];

        public:
            RequestFieldMappingTable() noexcept;

            RequestFieldMapping     Get(Flits::REQ<>::opcode_t opcode) const noexcept;
            RequestFieldMapping     Get(size_t index) const noexcept;
        };


        // Response Field
        class ResponseFieldMappingBack {
        public:
            const FieldConvention   QoS;
            const FieldConvention   TgtID;
            const FieldConvention   SrcID;
            const FieldConvention   TxnID;
            const FieldConvention   Opcode;
            const FieldConvention   RespErr;
            const FieldConvention   Resp;
            const FieldConvention   CBusy;
            const FieldConvention   DBID;
            const FieldConvention   TagGroupID;
            const FieldConvention   StashGroupID;
            const FieldConvention   PGroupID;
            const FieldConvention   PCrdType;
            const FieldConvention   FwdState;
            const FieldConvention   DataPull;
            const FieldConvention   TagOp;
            const FieldConvention   TraceTag;

        public:
            inline constexpr ResponseFieldMappingBack(
                const FieldConvention   QoS,
                const FieldConvention   TgtID,
                const FieldConvention   SrcID,
                const FieldConvention   TxnID,
                const FieldConvention   Opcode,
                const FieldConvention   RespErr,
                const FieldConvention   Resp,
                const FieldConvention   CBusy,
                const FieldConvention   DBID,
                const FieldConvention   TagGroupID,
                const FieldConvention   StashGroupID,
                const FieldConvention   PGroupID,
                const FieldConvention   PCrdType,
                const FieldConvention   FwdState,
                const FieldConvention   DataPull,
                const FieldConvention   TagOp,
                const FieldConvention   TraceTag
            ) noexcept
                : QoS           (QoS)
                , TgtID         (TgtID)
                , SrcID         (SrcID)
                , TxnID         (TxnID)
                , Opcode        (Opcode)
                , RespErr       (RespErr)
                , Resp          (Resp)
                , CBusy         (CBusy)
                , DBID          (DBID)
                , TagGroupID    (TagGroupID)
                , StashGroupID  (StashGroupID)
                , PGroupID      (PGroupID)
                , PCrdType      (PCrdType)
                , FwdState      (FwdState)
                , DataPull      (DataPull)
                , TagOp         (TagOp)
                , TraceTag      (TraceTag)
            { }
        };

        using ResponseFieldMapping = const ResponseFieldMappingBack*;

        namespace ResponseFieldMappings {
            using enum FieldConvention;
            /**/
            inline const ResponseFieldMappingBack RspLCrdReturn                     (X , X , X , A0, Y , X , X , X , X , X , X , X , X , X , X , X , X);
            inline const ResponseFieldMappingBack SnpResp                           (Y , Y , Y , Y , Y , Y , Y , Y , Y , S , S , S , I0, S , Y , I0, Y);
            inline const ResponseFieldMappingBack SnpRespFwded                      (Y , Y , Y , Y , Y , Y , Y , Y , X , S , S , S , I0, Y , S , I0, Y);
            inline const ResponseFieldMappingBack CompAck                           (Y , Y , Y , Y , Y , A0, I0, I0, X , S , S , S , I0, I0, I0, I0, Y);
            inline const ResponseFieldMappingBack RetryAck                          (Y , Y , Y , Y , Y , A0, I0, Y , X , S , S , S , Y , I0, I0, I0, Y);
            inline const ResponseFieldMappingBack Comp                              (Y , Y , Y , Y , Y , Y , Y , Y , Y , S , S , S , I0, I0, I0, Y , Y);
            inline const ResponseFieldMappingBack CompCMO                           (Y , Y , Y , Y , Y , Y , Y , Y , X , S , S , S , I0, I0, I0, I0, Y);
            inline const ResponseFieldMappingBack Persist                           (Y , Y , Y , I0, Y , Y , I0, Y , S , S , S , Y , I0, I0, I0, I0, X);
            inline const ResponseFieldMappingBack CompPersist                       (Y , Y , Y , Y , Y , Y , Y , Y , S , S , S , Y , I0, I0, I0, I0, Y);
            inline const ResponseFieldMappingBack StashDone                         (Y , Y , Y , I0, Y , Y , I0, Y , S , S , Y , S , I0, I0, I0, I0, X);
            inline const ResponseFieldMappingBack CompStashDone                     (Y , Y , Y , Y , Y , Y , Y , Y , S , S , Y , S , I0, I0, I0, I0, Y);
            inline const ResponseFieldMappingBack RespSepData                       (Y , Y , Y , Y , Y , Y , Y , Y , Y , S , S , S , I0, I0, I0, I0, Y);
            inline const ResponseFieldMappingBack CompDBIDResp                      (Y , Y , Y , Y , Y , Y , A0, Y , Y , S , S , S , I0, I0, I0, I0, Y);
            inline const ResponseFieldMappingBack DBIDResp                          (Y , Y , Y , Y , Y , A0, I0, Y , Y , S , S , S , I0, I0, I0, I0, Y);
            inline const ResponseFieldMappingBack DBIDRespOrd                       (Y , Y , Y , Y , Y , A0, I0, Y , Y , S , S , S , I0, I0, I0, I0, Y);
            inline const ResponseFieldMappingBack TagMatch                          (Y , Y , Y , I0, Y , Y , Y , Y , S , Y , S , S , I0, I0, I0, I0, X);
            inline const ResponseFieldMappingBack PCrdGrant                         (Y , Y , Y , I0, Y , A0, I0, Y , I0, I0, I0, I0, Y , I0, I0, I0, Y);
            inline const ResponseFieldMappingBack ReadReceipt                       (Y , Y , Y , Y , Y , A0, I0, Y , X , S , S , S , I0, I0, I0, I0, Y);
        };

        class ResponseFieldMappingTable {
        private:
            ResponseFieldMapping    table[1 << Flits::RSP<>::OPCODE_WIDTH];

        public:
            ResponseFieldMappingTable() noexcept;

            ResponseFieldMapping    Get(Flits::RSP<>::opcode_t opcode) const noexcept;
            ResponseFieldMapping    Get(size_t index) const noexcept;
        };


        // Data Field
        class DataFieldMappingBack {
        public:
            const FieldConvention   QoS;
            const FieldConvention   TgtID;
            const FieldConvention   SrcID;
            const FieldConvention   TxnID;
            const FieldConvention   Opcode;
            const FieldConvention   RespErr;
            const FieldConvention   Resp;
            const FieldConvention   CBusy;
            const FieldConvention   DBID;
            const FieldConvention   CCID;
            const FieldConvention   DataID;
            const FieldConvention   RSVDC;
            const FieldConvention   BE;
            const FieldConvention   Data;
            const FieldConvention   HomeNID;
            const FieldConvention   FwdState;
            const FieldConvention   DataPull;
            const FieldConvention   DataSource;
            const FieldConvention   TraceTag;
            const FieldConvention   DataCheck;
            const FieldConvention   Poison;
            const FieldConvention   TagOp;
            const FieldConvention   Tag;
            const FieldConvention   TU;

        public:
            inline constexpr DataFieldMappingBack(
                const FieldConvention   QoS,
                const FieldConvention   TgtID,
                const FieldConvention   SrcID,
                const FieldConvention   TxnID,
                const FieldConvention   Opcode,
                const FieldConvention   RespErr,
                const FieldConvention   Resp,
                const FieldConvention   CBusy,
                const FieldConvention   DBID,
                const FieldConvention   CCID,
                const FieldConvention   DataID,
                const FieldConvention   RSVDC,
                const FieldConvention   BE,
                const FieldConvention   Data,
                const FieldConvention   HomeNID,
                const FieldConvention   FwdState,
                const FieldConvention   DataPull,
                const FieldConvention   DataSource,
                const FieldConvention   TraceTag,
                const FieldConvention   DataCheck,
                const FieldConvention   Poison,
                const FieldConvention   TagOp,
                const FieldConvention   Tag,
                const FieldConvention   TU
            ) noexcept
                : QoS           (QoS)
                , TgtID         (TgtID)
                , SrcID         (SrcID)
                , TxnID         (TxnID)
                , Opcode        (Opcode)
                , RespErr       (RespErr)
                , Resp          (Resp)
                , CBusy         (CBusy)
                , DBID          (DBID)
                , CCID          (CCID)
                , DataID        (DataID)
                , RSVDC         (RSVDC)
                , BE            (BE)
                , Data          (Data)
                , HomeNID       (HomeNID)
                , FwdState      (FwdState)
                , DataPull      (DataPull)
                , DataSource    (DataSource)
                , TraceTag      (TraceTag)
                , DataCheck     (DataCheck)
                , Poison        (Poison)
                , TagOp         (TagOp)
                , Tag           (Tag)
                , TU            (TU)
            { };
        };

        using DataFieldMapping = const DataFieldMappingBack*;

        namespace DataFieldMappings {
            using enum FieldConvention;
            /**/
            inline constexpr DataFieldMappingBack DatLCrdReturn                     (X , X , X , A0, Y , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X , X);
            inline constexpr DataFieldMappingBack SnpRespData                       (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , I0, S , Y , Y , Y , Y , Y , Y , Y , Y);
            inline constexpr DataFieldMappingBack SnpRespDataFwded                  (Y , Y , Y , Y , Y , Y , Y , Y , X , Y , Y , Y , Y , Y , I0, Y , S , S , Y , Y , Y , Y , Y , Y);
            inline constexpr DataFieldMappingBack CopyBackWrData                    (Y , Y , Y , Y , Y , Y , Y , I0, X , Y , Y , Y , Y , Y , I0, I0, I0, I0, Y , Y , Y , Y , Y , Y);
            inline constexpr DataFieldMappingBack NonCopyBackWrData                 (Y , Y , Y , Y , Y , Y , Y , I0, X , Y , Y , Y , Y , Y , I0, I0, I0, I0, Y , Y , Y , Y , Y , Y);
            inline constexpr DataFieldMappingBack NCBWrDataCompAck                  (Y , Y , Y , Y , Y , Y , Y , I0, X , Y , Y , Y , Y , Y , I0, I0, I0, I0, Y , Y , Y , Y , Y , Y);
            inline constexpr DataFieldMappingBack CompData                          (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , X , Y , Y , S , S , Y , Y , Y , Y , Y , Y , Y);
            inline constexpr DataFieldMappingBack DataSepResp                       (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , X , Y , Y , S , S , Y , Y , Y , Y , Y , Y , Y);
            inline constexpr DataFieldMappingBack SnpRespDataPtl                    (Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , Y , I0, S , Y , Y , Y , Y , Y , Y , Y , Y);
            inline constexpr DataFieldMappingBack WriteDataCancel                   (Y , Y , Y , Y , Y , Y , Y , I0, X , Y , Y , Y , A0, Y , I0, I0, I0, I0, Y , Y , Y , Y , Y , Y);
        }

        class DataFieldMappingTable {
        private:
            DataFieldMapping    table[1 << Flits::DAT<>::OPCODE_WIDTH];

        public:
            DataFieldMappingTable() noexcept;

            DataFieldMapping    Get(Flits::DAT<>::opcode_t opcode) const noexcept;
            DataFieldMapping    Get(size_t index) const noexcept;
        };


        // Snoop Field
        class SnoopFieldMappingBack {
        public:
            const FieldConvention   QoS;
            const FieldConvention   SrcID;
            const FieldConvention   TxnID;
            const FieldConvention   Opcode;
            const FieldConvention   Addr;
            const FieldConvention   NS;
            const FieldConvention   FwdNID;
            const FieldConvention   FwdTxnID;
            const FieldConvention   StashLPIDValid;
            const FieldConvention   StashLPID;
            const FieldConvention   VMIDExt;
            const FieldConvention   DoNotGoToSD;
            const FieldConvention   RetToSrc;
            const FieldConvention   TraceTag;
            const FieldConvention   MPAM;

        public:
            inline constexpr SnoopFieldMappingBack(
                const FieldConvention   QoS,
                const FieldConvention   SrcID,
                const FieldConvention   TxnID,
                const FieldConvention   Opcode,
                const FieldConvention   Addr,
                const FieldConvention   NS,
                const FieldConvention   FwdNID,
                const FieldConvention   FwdTxnID,
                const FieldConvention   StashLPIDValid,
                const FieldConvention   StashLPID,
                const FieldConvention   VMIDExt,
                const FieldConvention   DoNotGoToSD,
                const FieldConvention   RetToSrc,
                const FieldConvention   TraceTag,
                const FieldConvention   MPAM
            ) noexcept
                : QoS           (QoS)
                , SrcID         (SrcID)
                , TxnID         (TxnID)
                , Opcode        (Opcode)
                , Addr          (Addr)
                , NS            (NS)
                , FwdNID        (FwdNID)
                , FwdTxnID      (FwdTxnID)
                , StashLPIDValid(StashLPIDValid)
                , StashLPID     (StashLPID)
                , VMIDExt       (VMIDExt)
                , DoNotGoToSD   (DoNotGoToSD)
                , RetToSrc      (RetToSrc)
                , TraceTag      (TraceTag)
                , MPAM          (MPAM)
            { }
        };

        using SnoopFieldMapping = const SnoopFieldMappingBack*;

        namespace SnoopFieldMappings {
            using enum FieldConvention;
            /**/
            inline constexpr SnoopFieldMappingBack SnpLCrdReturn                    (X , X , A0, Y , X , X , X , X , X , X , X , X , X , X , X );
            inline constexpr SnoopFieldMappingBack SnpShared                        (Y , Y , Y , Y , Y , Y , I0, I0, I0, I0, I0, Y , Y , Y , D );
            inline constexpr SnoopFieldMappingBack SnpClean                         (Y , Y , Y , Y , Y , Y , I0, I0, I0, I0, I0, Y , Y , Y , D );
            inline constexpr SnoopFieldMappingBack SnpOnce                          (Y , Y , Y , Y , Y , Y , I0, I0, I0, I0, I0, Y , Y , Y , D );
            inline constexpr SnoopFieldMappingBack SnpNotSharedDirty                (Y , Y , Y , Y , Y , Y , I0, I0, I0, I0, I0, Y , Y , Y , D );
            inline constexpr SnoopFieldMappingBack SnpUnique                        (Y , Y , Y , Y , Y , Y , I0, I0, I0, I0, I0, A1, Y , Y , D );
            inline constexpr SnoopFieldMappingBack SnpPreferUnique                  (Y , Y , Y , Y , Y , Y , I0, I0, I0, I0, I0, Y , Y , Y , D );
            inline constexpr SnoopFieldMappingBack SnpCleanShared                   (Y , Y , Y , Y , Y , Y , I0, I0, I0, I0, I0, A1, I0, Y , D );
            inline constexpr SnoopFieldMappingBack SnpCleanInvalid                  (Y , Y , Y , Y , Y , Y , I0, I0, I0, I0, I0, A1, I0, Y , D );
            inline constexpr SnoopFieldMappingBack SnpMakeInvalid                   (Y , Y , Y , Y , Y , Y , I0, I0, I0, I0, I0, A1, I0, Y , D );
            inline constexpr SnoopFieldMappingBack SnpSharedFwd                     (Y , Y , Y , Y , Y , Y , Y , Y , S , S , S , Y , Y , Y , D );
            inline constexpr SnoopFieldMappingBack SnpCleanFwd                      (Y , Y , Y , Y , Y , Y , Y , Y , S , S , S , Y , Y , Y , D );
            inline constexpr SnoopFieldMappingBack SnpOnceFwd                       (Y , Y , Y , Y , Y , Y , Y , Y , S , S , S , Y , I0, Y , D );
            inline constexpr SnoopFieldMappingBack SnpNotSharedDirtyFwd             (Y , Y , Y , Y , Y , Y , Y , Y , S , S , S , Y , Y , Y , D );
            inline constexpr SnoopFieldMappingBack SnpUniqueFwd                     (Y , Y , Y , Y , Y , Y , Y , Y , S , S , S , A1, I0, Y , D );
            inline constexpr SnoopFieldMappingBack SnpPreferUniqueFwd               (Y , Y , Y , Y , Y , Y , Y , Y , S , S , S , Y , Y , Y , D );
            inline constexpr SnoopFieldMappingBack SnpUniqueStash                   (Y , Y , Y , Y , Y , Y , I0, S , Y , Y , S , A1, I0, Y , Y );
            inline constexpr SnoopFieldMappingBack SnpMakeInvalidStash              (Y , Y , Y , Y , Y , Y , I0, S , Y , Y , S , A1, I0, Y , Y );
            inline constexpr SnoopFieldMappingBack SnpStashUnique                   (Y , Y , Y , Y , Y , Y , I0, S , Y , Y , S , A1, I0, Y , Y );
            inline constexpr SnoopFieldMappingBack SnpStashShared                   (Y , Y , Y , Y , Y , Y , I0, S , Y , Y , S , A1, I0, Y , Y );
            inline constexpr SnoopFieldMappingBack SnpQuery                         (Y , Y , Y , Y , Y , Y , I0, I0, I0, I0, I0, A1, I0, Y , D );
            inline constexpr SnoopFieldMappingBack SnpDVMOp                         (Y , Y , Y , Y , Y , I0, I0, S , S , S , Y , I0, I0, Y , I0);
        }

        class SnoopFieldMappingTable {
        private:
            SnoopFieldMapping   table[1 << Flits::SNP<>::OPCODE_WIDTH];

        public:
            SnoopFieldMappingTable() noexcept;

            SnoopFieldMapping   Get(Flits::SNP<>::opcode_t opcode) const noexcept;
            SnoopFieldMapping   Get(size_t index) const noexcept;
        };
    }
/*
}
*/


// Implementation of: class RequestFieldMappingTable
namespace /*CHI::*/Xact {

    inline RequestFieldMappingTable::RequestFieldMappingTable() noexcept
    {
        std::memset(table, 0, sizeof(RequestFieldMapping) * (1 << Flits::REQ<>::OPCODE_WIDTH));

        //
        table[Opcodes::REQ::ReqLCrdReturn               ] = &RequestFieldMappings::ReqLCrdReturn;
        table[Opcodes::REQ::PCrdReturn                  ] = &RequestFieldMappings::PCrdReturn;
        table[Opcodes::REQ::DVMOp                       ] = &RequestFieldMappings::DVMOp;
        table[Opcodes::REQ::PrefetchTgt                 ] = &RequestFieldMappings::PrefetchTgt;
        table[Opcodes::REQ::ReadNoSnp                   ] = &RequestFieldMappings::ReadNoSnp;
        table[Opcodes::REQ::ReadNoSnpSep                ] = &RequestFieldMappings::ReadNoSnpSep;
        table[Opcodes::REQ::ReadOnce                    ] = &RequestFieldMappings::ReadOnce;
        table[Opcodes::REQ::ReadOnceCleanInvalid        ] = &RequestFieldMappings::ReadOnceCleanInvalid;
        table[Opcodes::REQ::ReadOnceMakeInvalid         ] = &RequestFieldMappings::ReadOnceMakeInvalid;
        table[Opcodes::REQ::ReadClean                   ] = &RequestFieldMappings::ReadClean;
        table[Opcodes::REQ::ReadNotSharedDirty          ] = &RequestFieldMappings::ReadNotSharedDirty;
        table[Opcodes::REQ::ReadShared                  ] = &RequestFieldMappings::ReadShared;
        table[Opcodes::REQ::ReadUnique                  ] = &RequestFieldMappings::ReadUnique;
        table[Opcodes::REQ::ReadPreferUnique            ] = &RequestFieldMappings::ReadPreferUnique;
        table[Opcodes::REQ::MakeReadUnique              ] = &RequestFieldMappings::MakeReadUnique;
        table[Opcodes::REQ::CleanShared                 ] = &RequestFieldMappings::CleanShared;
        table[Opcodes::REQ::CleanSharedPersist          ] = &RequestFieldMappings::CleanSharedPersist;
        table[Opcodes::REQ::CleanSharedPersistSep       ] = &RequestFieldMappings::CleanSharedPersistSep;
        table[Opcodes::REQ::CleanInvalid                ] = &RequestFieldMappings::CleanInvalid;
        table[Opcodes::REQ::MakeInvalid                 ] = &RequestFieldMappings::MakeInvalid;
        table[Opcodes::REQ::CleanUnique                 ] = &RequestFieldMappings::CleanUnique;
        table[Opcodes::REQ::MakeUnique                  ] = &RequestFieldMappings::MakeUnique;
        table[Opcodes::REQ::Evict                       ] = &RequestFieldMappings::Evict;
        //
        table[Opcodes::REQ::WriteNoSnpPtl               ] = &RequestFieldMappings::WriteNoSnpPtl;
        table[Opcodes::REQ::WriteNoSnpPtlCleanInv       ] = &RequestFieldMappings::WriteNoSnpPtlCleanInv;
        table[Opcodes::REQ::WriteNoSnpPtlCleanSh        ] = &RequestFieldMappings::WriteNoSnpPtlCleanSh;
        table[Opcodes::REQ::WriteNoSnpPtlCleanShPerSep  ] = &RequestFieldMappings::WriteNoSnpPtlCleanShPerSep;
        table[Opcodes::REQ::WriteNoSnpFull              ] = &RequestFieldMappings::WriteNoSnpFull;
        table[Opcodes::REQ::WriteNoSnpFullCleanInv      ] = &RequestFieldMappings::WriteNoSnpFullCleanInv;
        table[Opcodes::REQ::WriteNoSnpFullCleanSh       ] = &RequestFieldMappings::WriteNoSnpFullCleanSh;
        table[Opcodes::REQ::WriteNoSnpFullCleanShPerSep ] = &RequestFieldMappings::WriteNoSnpFullCleanShPerSep;
        table[Opcodes::REQ::WriteNoSnpZero              ] = &RequestFieldMappings::WriteNoSnpZero;
        table[Opcodes::REQ::WriteUniquePtlStash         ] = &RequestFieldMappings::WriteUniquePtlStash;
        table[Opcodes::REQ::WriteUniqueFullStash        ] = &RequestFieldMappings::WriteUniqueFullStash;
        table[Opcodes::REQ::WriteUniquePtl              ] = &RequestFieldMappings::WriteUniquePtl;
        table[Opcodes::REQ::WriteUniquePtlCleanSh       ] = &RequestFieldMappings::WriteUniquePtlCleanSh;
        table[Opcodes::REQ::WriteUniquePtlCleanShPerSep ] = &RequestFieldMappings::WriteUniquePtlCleanShPerSep;
        table[Opcodes::REQ::WriteUniqueFull             ] = &RequestFieldMappings::WriteUniqueFull;
        table[Opcodes::REQ::WriteUniqueFullCleanSh      ] = &RequestFieldMappings::WriteUniqueFullCleanSh;
        table[Opcodes::REQ::WriteUniqueFullCleanShPerSep] = &RequestFieldMappings::WriteUniqueFullCleanShPerSep; 
        table[Opcodes::REQ::WriteUniqueZero             ] = &RequestFieldMappings::WriteUniqueZero;
        table[Opcodes::REQ::WriteBackPtl                ] = &RequestFieldMappings::WriteBackPtl;
        table[Opcodes::REQ::WriteBackFull               ] = &RequestFieldMappings::WriteBackFull;
        table[Opcodes::REQ::WriteBackFullCleanInv       ] = &RequestFieldMappings::WriteBackFullCleanInv;
        table[Opcodes::REQ::WriteBackFullCleanSh        ] = &RequestFieldMappings::WriteBackFullCleanSh;
        table[Opcodes::REQ::WriteBackFullCleanShPerSep  ] = &RequestFieldMappings::WriteBackFullCleanShPerSep;
        table[Opcodes::REQ::WriteCleanFull              ] = &RequestFieldMappings::WriteCleanFull;
        table[Opcodes::REQ::WriteCleanFullCleanSh       ] = &RequestFieldMappings::WriteCleanFullCleanSh;
        table[Opcodes::REQ::WriteCleanFullCleanShPerSep ] = &RequestFieldMappings::WriteCleanFullCleanShPerSep;
        table[Opcodes::REQ::WriteEvictFull              ] = &RequestFieldMappings::WriteEvictFull;
        table[Opcodes::REQ::WriteEvictOrEvict           ] = &RequestFieldMappings::WriteEvictOrEvict;
        //
        table[Opcodes::REQ::StashOnceUnique             ] = &RequestFieldMappings::StashOnceUnique;
        table[Opcodes::REQ::StashOnceSepUnique          ] = &RequestFieldMappings::StashOnceSepUnique;
        table[Opcodes::REQ::StashOnceShared             ] = &RequestFieldMappings::StashOnceShared;
        table[Opcodes::REQ::StashOnceSepShared          ] = &RequestFieldMappings::StashOnceSepShared;
        table[Opcodes::REQ::AtomicLoad::ADD             ] = &RequestFieldMappings::AtomicLoad;
        table[Opcodes::REQ::AtomicLoad::CLR             ] = &RequestFieldMappings::AtomicLoad;
        table[Opcodes::REQ::AtomicLoad::EOR             ] = &RequestFieldMappings::AtomicLoad;
        table[Opcodes::REQ::AtomicLoad::SET             ] = &RequestFieldMappings::AtomicLoad;
        table[Opcodes::REQ::AtomicLoad::SMAX            ] = &RequestFieldMappings::AtomicLoad;
        table[Opcodes::REQ::AtomicLoad::SMIN            ] = &RequestFieldMappings::AtomicLoad;
        table[Opcodes::REQ::AtomicLoad::UMAX            ] = &RequestFieldMappings::AtomicLoad;
        table[Opcodes::REQ::AtomicLoad::UMIN            ] = &RequestFieldMappings::AtomicLoad;
        table[Opcodes::REQ::AtomicStore::ADD            ] = &RequestFieldMappings::AtomicStore;
        table[Opcodes::REQ::AtomicStore::CLR            ] = &RequestFieldMappings::AtomicStore;
        table[Opcodes::REQ::AtomicStore::EOR            ] = &RequestFieldMappings::AtomicStore;
        table[Opcodes::REQ::AtomicStore::SET            ] = &RequestFieldMappings::AtomicStore;
        table[Opcodes::REQ::AtomicStore::SMAX           ] = &RequestFieldMappings::AtomicStore;
        table[Opcodes::REQ::AtomicStore::SMIN           ] = &RequestFieldMappings::AtomicStore;
        table[Opcodes::REQ::AtomicStore::UMAX           ] = &RequestFieldMappings::AtomicStore;
        table[Opcodes::REQ::AtomicStore::UMIN           ] = &RequestFieldMappings::AtomicStore;
        table[Opcodes::REQ::AtomicCompare               ] = &RequestFieldMappings::AtomicCompare;
        table[Opcodes::REQ::AtomicSwap                  ] = &RequestFieldMappings::AtomicSwap;
    }

    inline RequestFieldMapping RequestFieldMappingTable::Get(Flits::REQ<>::opcode_t opcode) const noexcept
    {
        return table[opcode];
    }

    inline RequestFieldMapping RequestFieldMappingTable::Get(size_t index) const noexcept
    {
        if (index < (1 << Flits::REQ<>::OPCODE_WIDTH))
            return table[index];

        return nullptr;
    }
}


// Implementation of: class ResponseFieldMappingTable
namespace /*CHI::*/Xact {

    inline ResponseFieldMappingTable::ResponseFieldMappingTable() noexcept
    {
        std::memset(table, 0, sizeof(ResponseFieldMapping) * (1 << Flits::RSP<>::OPCODE_WIDTH));

        //
        table[Opcodes::RSP::RespLCrdReturn              ] = &ResponseFieldMappings::RspLCrdReturn;
        table[Opcodes::RSP::SnpResp                     ] = &ResponseFieldMappings::SnpResp;
        table[Opcodes::RSP::SnpRespFwded                ] = &ResponseFieldMappings::SnpRespFwded;
        table[Opcodes::RSP::CompAck                     ] = &ResponseFieldMappings::CompAck;
        table[Opcodes::RSP::RetryAck                    ] = &ResponseFieldMappings::RetryAck;
        table[Opcodes::RSP::Comp                        ] = &ResponseFieldMappings::Comp;
        table[Opcodes::RSP::CompCMO                     ] = &ResponseFieldMappings::CompCMO;
        table[Opcodes::RSP::Persist                     ] = &ResponseFieldMappings::Persist;
        table[Opcodes::RSP::CompPersist                 ] = &ResponseFieldMappings::CompPersist;
        table[Opcodes::RSP::StashDone                   ] = &ResponseFieldMappings::StashDone;
        table[Opcodes::RSP::CompStashDone               ] = &ResponseFieldMappings::CompStashDone;
        table[Opcodes::RSP::RespSepData                 ] = &ResponseFieldMappings::RespSepData;
        table[Opcodes::RSP::CompDBIDResp                ] = &ResponseFieldMappings::CompDBIDResp;
        table[Opcodes::RSP::DBIDResp                    ] = &ResponseFieldMappings::DBIDResp;
        table[Opcodes::RSP::DBIDRespOrd                 ] = &ResponseFieldMappings::DBIDRespOrd;
        table[Opcodes::RSP::TagMatch                    ] = &ResponseFieldMappings::TagMatch;
        table[Opcodes::RSP::PCrdGrant                   ] = &ResponseFieldMappings::PCrdGrant;
        table[Opcodes::RSP::ReadReceipt                 ] = &ResponseFieldMappings::ReadReceipt;
    }

    inline ResponseFieldMapping ResponseFieldMappingTable::Get(Flits::RSP<>::opcode_t opcode) const noexcept
    {
        return table[opcode];
    }

    inline ResponseFieldMapping ResponseFieldMappingTable::Get(size_t index) const noexcept
    {
        if (index < (1 << Flits::RSP<>::OPCODE_WIDTH))
            return table[index];

        return nullptr;
    }
}


// Implementation of: class DataFieldMappingTable
namespace /*CHI::*/Xact {

    inline DataFieldMappingTable::DataFieldMappingTable() noexcept
    {
        std::memset(table, 0, sizeof(DataFieldMapping) * (1 << Flits::DAT<>::OPCODE_WIDTH));

        //
        table[Opcodes::DAT::DataLCrdReturn              ] = &DataFieldMappings::DatLCrdReturn;
        table[Opcodes::DAT::SnpRespData                 ] = &DataFieldMappings::SnpRespData;
        table[Opcodes::DAT::SnpRespDataFwded            ] = &DataFieldMappings::SnpRespDataFwded;
        table[Opcodes::DAT::CopyBackWrData              ] = &DataFieldMappings::CopyBackWrData;
        table[Opcodes::DAT::NonCopyBackWrData           ] = &DataFieldMappings::NonCopyBackWrData;
        table[Opcodes::DAT::NCBWrDataCompAck            ] = &DataFieldMappings::NCBWrDataCompAck;
        table[Opcodes::DAT::CompData                    ] = &DataFieldMappings::CompData;
        table[Opcodes::DAT::DataSepResp                 ] = &DataFieldMappings::DataSepResp;
        table[Opcodes::DAT::SnpRespDataPtl              ] = &DataFieldMappings::SnpRespDataPtl;
        table[Opcodes::DAT::WriteDataCancel             ] = &DataFieldMappings::WriteDataCancel;
    }

    inline DataFieldMapping DataFieldMappingTable::Get(Flits::DAT<>::opcode_t opcode) const noexcept
    {
        return table[opcode];
    }

    inline DataFieldMapping DataFieldMappingTable::Get(size_t index) const noexcept
    {
        if (index < (1 << Flits::DAT<>::OPCODE_WIDTH))
            return table[index];

        return nullptr;
    }
}


// Implementation of: class SnoopFieldMappingTable
namespace /*CHI::*/Xact {

    inline SnoopFieldMappingTable::SnoopFieldMappingTable() noexcept
    {
        std::memset(table, 0, sizeof(SnoopFieldMapping) * (1 << Flits::SNP<>::OPCODE_WIDTH));

        //
        table[Opcodes::SNP::SnpLCrdReturn               ] = &SnoopFieldMappings::SnpLCrdReturn;
        table[Opcodes::SNP::SnpShared                   ] = &SnoopFieldMappings::SnpShared;
        table[Opcodes::SNP::SnpClean                    ] = &SnoopFieldMappings::SnpClean;
        table[Opcodes::SNP::SnpOnce                     ] = &SnoopFieldMappings::SnpOnce;
        table[Opcodes::SNP::SnpNotSharedDirty           ] = &SnoopFieldMappings::SnpNotSharedDirty;
        table[Opcodes::SNP::SnpUnique                   ] = &SnoopFieldMappings::SnpUnique;
        table[Opcodes::SNP::SnpPreferUnique             ] = &SnoopFieldMappings::SnpPreferUnique;
        table[Opcodes::SNP::SnpCleanShared              ] = &SnoopFieldMappings::SnpCleanShared;
        table[Opcodes::SNP::SnpCleanInvalid             ] = &SnoopFieldMappings::SnpCleanInvalid;
        table[Opcodes::SNP::SnpMakeInvalid              ] = &SnoopFieldMappings::SnpMakeInvalid;
        table[Opcodes::SNP::SnpSharedFwd                ] = &SnoopFieldMappings::SnpSharedFwd;
        table[Opcodes::SNP::SnpCleanFwd                 ] = &SnoopFieldMappings::SnpCleanFwd;
        table[Opcodes::SNP::SnpOnceFwd                  ] = &SnoopFieldMappings::SnpOnceFwd;
        table[Opcodes::SNP::SnpNotSharedDirtyFwd        ] = &SnoopFieldMappings::SnpNotSharedDirtyFwd;
        table[Opcodes::SNP::SnpUniqueFwd                ] = &SnoopFieldMappings::SnpUniqueFwd;
        table[Opcodes::SNP::SnpPreferUniqueFwd          ] = &SnoopFieldMappings::SnpPreferUniqueFwd;
        table[Opcodes::SNP::SnpUniqueStash              ] = &SnoopFieldMappings::SnpUniqueStash;
        table[Opcodes::SNP::SnpMakeInvalidStash         ] = &SnoopFieldMappings::SnpMakeInvalidStash;
        table[Opcodes::SNP::SnpStashUnique              ] = &SnoopFieldMappings::SnpStashUnique;
        table[Opcodes::SNP::SnpStashShared              ] = &SnoopFieldMappings::SnpStashShared;
        table[Opcodes::SNP::SnpQuery                    ] = &SnoopFieldMappings::SnpQuery;
        table[Opcodes::SNP::SnpDVMOp                    ] = &SnoopFieldMappings::SnpDVMOp;
    }

    inline SnoopFieldMapping SnoopFieldMappingTable::Get(Flits::SNP<>::opcode_t opcode) const noexcept
    {
        return table[opcode];
    }

    inline SnoopFieldMapping SnoopFieldMappingTable::Get(size_t index) const noexcept
    {
        if (index < (1 << Flits::SNP<>::OPCODE_WIDTH))
            return table[index];

        return nullptr;
    }
}


#endif // CHI_ISSUE_EB_ENABLE

#endif // __CHI__CHI_XACT_FIELD_*

// #endif // __CHI__CHI_XACT_FIELD
