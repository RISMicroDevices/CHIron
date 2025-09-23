//#pragma once

//#ifndef __CHI__CHI_PROTOCOL_ENCODING
//#define __CHI__CHI_PROTOCOL_ENCODING

#ifndef CHI_PROTOCOL_ENCODING__STANDALONE
#   include "chi_protocol_encoding_header.hpp"         // IWYU pragma: keep
#   include "chi_protocol_flits.hpp"
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_PROTOCOL_ENCODING_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_PROTOCOL_ENCODING_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_PROTOCOL_ENCODING_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_PROTOCOL_ENCODING_EB
#endif


/*
namespace CHI {
*/

    //
    namespace Opcodes {

        namespace REQ {

            /*
            See REQ channel opcodes.
            */

            // Opcode type
            using type = Flits::REQ<>::opcode_t;

            // Opcodes
            constexpr type              ReqLCrdReturn                   = 0x00;
            constexpr type              ReadShared                      = 0x01;
            constexpr type              ReadClean                       = 0x02;
            constexpr type              ReadOnce                        = 0x03;
            constexpr type              ReadNoSnp                       = 0x04;
            constexpr type              PCrdReturn                      = 0x05;
//          *Reserved*                                                  = 0x06;
            constexpr type              ReadUnique                      = 0x07;
            constexpr type              CleanShared                     = 0x08;
            constexpr type              CleanInvalid                    = 0x09;
            constexpr type              MakeInvalid                     = 0x0A;
            constexpr type              CleanUnique                     = 0x0B;
            constexpr type              MakeUnique                      = 0x0C;
            constexpr type              Evict                           = 0x0D;
//          *Reserved* EOBarrier                                        = 0x0E;
//          *Reserved* ECBarrier                                        = 0x0F;
//          *Reserved*                                                  = 0x10;
#ifdef CHI_ISSUE_EB_ENABLE
            constexpr type              ReadNoSnpSep                    = 0x11;
#endif
//          *Reserved*                                                  = 0x12;
#ifdef CHI_ISSUE_EB_ENABLE
            constexpr type              CleanSharedPersistSep           = 0x13;
#endif
            constexpr type              DVMOp                           = 0x14;
            constexpr type              WriteEvictFull                  = 0x15;
//          *Reserved*                                                  = 0x16;
            constexpr type              WriteCleanFull                  = 0x17;
            constexpr type              WriteUniquePtl                  = 0x18;
            constexpr type              WriteUniqueFull                 = 0x19;
            constexpr type              WriteBackPtl                    = 0x1A;
            constexpr type              WriteBackFull                   = 0x1B;
            constexpr type              WriteNoSnpPtl                   = 0x1C;
            constexpr type              WriteNoSnpFull                  = 0x1D;
//          *Reserved*                                                  = 0x1E;
//          *Reserved*                                                  = 0x1F;
            constexpr type              WriteUniqueFullStash            = 0x20;
            constexpr type              WriteUniquePtlStash             = 0x21;
            constexpr type              StashOnceShared                 = 0x22;
            constexpr type              StashOnceUnique                 = 0x23;
            constexpr type              ReadOnceCleanInvalid            = 0x24;
            constexpr type              ReadOnceMakeInvalid             = 0x25;
            constexpr type              ReadNotSharedDirty              = 0x26;
            constexpr type              CleanSharedPersist              = 0x27;
//          AtomicStore                                                 = 0x28;
//                                                                    ... 0x2F;
//          AtomicLoad                                                  = 0x30 
//                                                                    ... 0x37;
            constexpr type              AtomicSwap                      = 0x38;
            constexpr type              AtomicCompare                   = 0x39;
            constexpr type              PrefetchTgt                     = 0x3A;
//          *Reserved*                                                  = 0x3B;
//                                                                    ... 0x3F;

#ifdef CHI_ISSUE_EB_ENABLE
//          *Reserved*                                                  = 0x00 | 0x40;
            constexpr type              MakeReadUnique                  = 0x01 | 0x40;
            constexpr type              WriteEvictOrEvict               = 0x02 | 0x40;
            constexpr type              WriteUniqueZero                 = 0x03 | 0x40;
            constexpr type              WriteNoSnpZero                  = 0x04 | 0x40;
//          *Reserved*                                                  = 0x05 | 0x40;
//                                                                    ... 0x06 | 0x40;
            constexpr type              StashOnceSepShared              = 0x07 | 0x40;
            constexpr type              StashOnceSepUnique              = 0x08 | 0x40;
//          *Reserved*                                                  = 0x09 | 0x40;
//                                                                    ... 0x0B | 0x40;
            constexpr type              ReadPreferUnique                = 0x0C | 0x40;
//          *Reserved*                                                  = 0x0D | 0x40;
//                                                                    ... 0x0F | 0x40;
            constexpr type              WriteNoSnpFullCleanSh           = 0x10 | 0x40;
            constexpr type              WriteNoSnpFullCleanInv          = 0x11 | 0x40;
            constexpr type              WriteNoSnpFullCleanShPerSep     = 0x12 | 0x40;
//          *Reserved*                                                  = 0x13 | 0x40;
            constexpr type              WriteUniqueFullCleanSh          = 0x14 | 0x40;
//          *Reserved*                                                  = 0x15 | 0x40;
            constexpr type              WriteUniqueFullCleanShPerSep    = 0x16 | 0x40;
//          *Reserved*                                                  = 0x17 | 0x40;
            constexpr type              WriteBackFullCleanSh            = 0x18 | 0x40;
            constexpr type              WriteBackFullCleanInv           = 0x19 | 0x40;
            constexpr type              WriteBackFullCleanShPerSep      = 0x1A | 0x40;
//          *Reserved*                                                  = 0x1B | 0x40;
            constexpr type              WriteCleanFullCleanSh           = 0x1C | 0x40;
//          *Reserved*                                                  = 0x1D | 0x40;
            constexpr type              WriteCleanFullCleanShPerSep     = 0x1E | 0x40;
//          *Reserved*                                                  = 0x1F | 0x40;
            constexpr type              WriteNoSnpPtlCleanSh            = 0x20 | 0x40;
            constexpr type              WriteNoSnpPtlCleanInv           = 0x21 | 0x40;
            constexpr type              WriteNoSnpPtlCleanShPerSep      = 0x22 | 0x40;
//          *Reserved*                                                  = 0x23 | 0x40;
            constexpr type              WriteUniquePtlCleanSh           = 0x24 | 0x40;
//          *Reserved*                                                  = 0x25 | 0x40;
            constexpr type              WriteUniquePtlCleanShPerSep     = 0x26 | 0x40;
//          *Reserved*                                                  = 0x27 | 0x40;
//                                                                    ... 0x3F | 0x40;
#endif

            namespace AtomicStore {

#               define SubAtomicStore(code) 0b101##code
                constexpr type          ADD                     = SubAtomicStore(000);
                constexpr type          CLR                     = SubAtomicStore(001);
                constexpr type          EOR                     = SubAtomicStore(010);
                constexpr type          SET                     = SubAtomicStore(011);
                constexpr type          SMAX                    = SubAtomicStore(100);
                constexpr type          SMIN                    = SubAtomicStore(101);
                constexpr type          UMAX                    = SubAtomicStore(110);
                constexpr type          UMIN                    = SubAtomicStore(111);
#               undef SubAtomicStore

                inline constexpr bool Is(type val) { return (val & 0b111000) == 0b101000; }
            };

            namespace AtomicLoad {

#               define SubAtomicLoad(code) 0b110##code
                constexpr type          ADD                     = SubAtomicLoad(000);
                constexpr type          CLR                     = SubAtomicLoad(001);
                constexpr type          EOR                     = SubAtomicLoad(010);
                constexpr type          SET                     = SubAtomicLoad(011);
                constexpr type          SMAX                    = SubAtomicLoad(100);
                constexpr type          SMIN                    = SubAtomicLoad(101);
                constexpr type          UMAX                    = SubAtomicLoad(110);
                constexpr type          UMIN                    = SubAtomicLoad(111);
#               undef SubAtomicLoad

                inline constexpr bool Is(type val) { return (val & 0b111000) == 0b110000; }
            };

            /*
            Opcodes permitted in RN. See page B-356.
            */
            namespace RN {
                
                //=============================================================================================================
                constexpr type          ReqLCrdReturn                   = REQ::ReqLCrdReturn;                   //
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          ReadNoSnp                       = REQ::ReadNoSnp;                       // from      RN
                constexpr type          WriteNoSnpFull                  = REQ::WriteNoSnpFull;                  // from      RN
                constexpr type          WriteNoSnpPtl                   = REQ::WriteNoSnpPtl;                   // from      RN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          WriteNoSnpZero                  = REQ::WriteNoSnpZero;                  // from      RN
#endif
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          ReadClean                       = REQ::ReadClean;                       // from      RN
                constexpr type          ReadShared                      = REQ::ReadShared;                      // from      RN
                constexpr type          ReadNotSharedDirty              = REQ::ReadNotSharedDirty;              // from      RN
                constexpr type          ReadUnique                      = REQ::ReadUnique;                      // from      RN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          ReadPreferUnique                = REQ::ReadPreferUnique;                // from      RN
                constexpr type          MakeReadUnique                  = REQ::MakeReadUnique;                  // from      RN
#endif
                constexpr type          CleanUnique                     = REQ::CleanUnique;                     // from      RN
                constexpr type          MakeUnique                      = REQ::MakeUnique;                      // from      RN
                constexpr type          Evict                           = REQ::Evict;                           // from      RN
                constexpr type          WriteBackFull                   = REQ::WriteBackFull;                   // from      RN
                constexpr type          WriteBackPtl                    = REQ::WriteBackPtl;                    // from      RN
                constexpr type          WriteEvictFull                  = REQ::WriteEvictFull;                  // from      RN
                constexpr type          WriteCleanFull                  = REQ::WriteCleanFull;                  // from      RN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          WriteEvictOrEvict               = REQ::WriteEvictOrEvict;               // from      RN
#endif
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          ReadOnce                        = REQ::ReadOnce;                        // from      RN
                constexpr type          ReadOnceCleanInvalid            = REQ::ReadOnceCleanInvalid;            // from      RN
                constexpr type          ReadOnceMakeInvalid             = REQ::ReadOnceMakeInvalid;             // from      RN
                constexpr type          StashOnceUnique                 = REQ::StashOnceUnique;                 // from      RN
                constexpr type          StashOnceShared                 = REQ::StashOnceShared;                 // from      RN
#ifdef CHI_ISSUE_EB_ENABLE  
                constexpr type          StashOnceSepUnique              = REQ::StashOnceSepUnique;              // from      RN
                constexpr type          StashOnceSepShared              = REQ::StashOnceSepShared;              // from      RN
#endif  
                constexpr type          WriteUniqueFull                 = REQ::WriteUniqueFull;                 // from      RN
                constexpr type          WriteUniqueFullStash            = REQ::WriteUniqueFullStash;            // from      RN
                constexpr type          WriteUniquePtl                  = REQ::WriteUniquePtl;                  // from      RN
                constexpr type          WriteUniquePtlStash             = REQ::WriteUniquePtlStash;             // from      RN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          WriteUniqueZero                 = REQ::WriteUniqueZero;                 // from      RN
#endif
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          CleanShared                     = REQ::CleanShared;                     // from      RN
                constexpr type          CleanSharedPersist              = REQ::CleanSharedPersist;              // from      RN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          CleanSharedPersistSep           = REQ::CleanSharedPersistSep;           // from      RN
#endif
                constexpr type          CleanInvalid                    = REQ::CleanInvalid;                    // from      RN
                constexpr type          MakeInvalid                     = REQ::MakeInvalid;                     // from      RN
                //-------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          WriteUniquePtlCleanSh           = REQ::WriteUniquePtlCleanSh;           // from      RN
                constexpr type          WriteUniquePtlCleanShPerSep     = REQ::WriteUniquePtlCleanShPerSep;     // from      RN
                constexpr type          WriteUniqueFullCleanSh          = REQ::WriteUniqueFullCleanSh;          // from      RN
                constexpr type          WriteUniqueFullCleanShPerSep    = REQ::WriteUniqueFullCleanShPerSep;    // from      RN
                constexpr type          WriteBackFullCleanInv           = REQ::WriteBackFullCleanInv;           // from      RN
                constexpr type          WriteBackFullCleanSh            = REQ::WriteBackFullCleanSh;            // from      RN
                constexpr type          WriteBackFullCleanShPerSep      = REQ::WriteBackFullCleanShPerSep;      // from      RN
                constexpr type          WriteCleanFullCleanSh           = REQ::WriteCleanFullCleanSh;           // from      RN
                constexpr type          WriteCleanFullCleanShPerSep     = REQ::WriteCleanFullCleanShPerSep;     // from      RN
#endif
                //-------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          WriteNoSnpPtlCleanInv           = REQ::WriteNoSnpPtlCleanInv;           // from      RN
                constexpr type          WriteNoSnpPtlCleanSh            = REQ::WriteNoSnpPtlCleanSh;            // from      RN
                constexpr type          WriteNoSnpPtlCleanShPerSep      = REQ::WriteNoSnpPtlCleanShPerSep;      // from      RN
                constexpr type          WriteNoSnpFullCleanInv          = REQ::WriteNoSnpFullCleanInv;          // from      RN
                constexpr type          WriteNoSnpFullCleanSh           = REQ::WriteNoSnpFullCleanSh;           // from      RN
                constexpr type          WriteNoSnpFullCleanShPerSep     = REQ::WriteNoSnpFullCleanShPerSep;     // from      RN
#endif
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          DVMOp                           = REQ::DVMOp;                           // from      RN
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          PCrdReturn                      = REQ::PCrdReturn;                      // from      RN
                //-------------------------------------------------------------------------------------------------------------
                namespace               AtomicStore                     = REQ::AtomicStore;                     // from      RN // NOLINT: misc-unused-alias-decls
                namespace               AtomicLoad                      = REQ::AtomicLoad;                      // from      RN // NOLINT: misc-unused-alias-decls
                constexpr type          AtomicSwap                      = REQ::AtomicSwap;                      // from      RN
                constexpr type          AtomicCompare                   = REQ::AtomicCompare;                   // from      RN
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          PrefetchTgt                     = REQ::PrefetchTgt;                     // from      RN
                //=============================================================================================================

                // Opcodes permitted in RN-F.
                namespace F {

                    //===============================================================================================================
                    constexpr type          ReqLCrdReturn                   = REQ::RN::ReqLCrdReturn;               //
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          ReadNoSnp                       = REQ::RN::ReadNoSnp;                   // from      RN-F
                    constexpr type          WriteNoSnpFull                  = REQ::RN::WriteNoSnpFull;              // from      RN-F
                    constexpr type          WriteNoSnpPtl                   = REQ::RN::WriteNoSnpPtl;               // from      RN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          WriteNoSnpZero                  = REQ::RN::WriteNoSnpZero;              // from      RN-F
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          ReadClean                       = REQ::RN::ReadClean;                   // from      RN-F
                    constexpr type          ReadShared                      = REQ::RN::ReadShared;                  // from      RN-F
                    constexpr type          ReadNotSharedDirty              = REQ::RN::ReadNotSharedDirty;          // from      RN-F
                    constexpr type          ReadUnique                      = REQ::RN::ReadUnique;                  // from      RN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          ReadPreferUnique                = REQ::RN::ReadPreferUnique;            // from      RN-F
                    constexpr type          MakeReadUnique                  = REQ::RN::MakeReadUnique;              // from      RN-F
#endif
                    constexpr type          CleanUnique                     = REQ::RN::CleanUnique;                 // from      RN-F
                    constexpr type          MakeUnique                      = REQ::RN::MakeUnique;                  // from      RN-F
                    constexpr type          Evict                           = REQ::RN::Evict;                       // from      RN-F
                    constexpr type          WriteBackFull                   = REQ::RN::WriteBackFull;               // from      RN-F
                    constexpr type          WriteBackPtl                    = REQ::RN::WriteBackPtl;                // from      RN-F
                    constexpr type          WriteEvictFull                  = REQ::RN::WriteEvictFull;              // from      RN-F
                    constexpr type          WriteCleanFull                  = REQ::RN::WriteCleanFull;              // from      RN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          WriteEvictOrEvict               = REQ::RN::WriteEvictOrEvict;           // from      RN-F
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          ReadOnce                        = REQ::RN::ReadOnce;                    // from      RN-F
                    constexpr type          ReadOnceCleanInvalid            = REQ::RN::ReadOnceCleanInvalid;        // from      RN-F
                    constexpr type          ReadOnceMakeInvalid             = REQ::RN::ReadOnceMakeInvalid;         // from      RN-F
                    constexpr type          StashOnceUnique                 = REQ::RN::StashOnceUnique;             // from      RN-F
                    constexpr type          StashOnceShared                 = REQ::RN::StashOnceShared;             // from      RN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          StashOnceSepUnique              = REQ::RN::StashOnceSepUnique;          // from      RN-F
                    constexpr type          StashOnceSepShared              = REQ::RN::StashOnceSepShared;          // from      RN-F
#endif
                    constexpr type          WriteUniqueFull                 = REQ::RN::WriteUniqueFull;             // from      RN-F
                    constexpr type          WriteUniqueFullStash            = REQ::RN::WriteUniqueFullStash;        // from      RN-F
                    constexpr type          WriteUniquePtl                  = REQ::RN::WriteUniquePtl;              // from      RN-F
                    constexpr type          WriteUniquePtlStash             = REQ::RN::WriteUniquePtlStash;         // from      RN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          WriteUniqueZero                 = REQ::RN::WriteUniqueZero;             // from      RN-F
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          CleanShared                     = REQ::RN::CleanShared;                 // from      RN-F
                    constexpr type          CleanSharedPersist              = REQ::RN::CleanSharedPersist;          // from      RN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          CleanSharedPersistSep           = REQ::RN::CleanSharedPersistSep;       // from      RN-F
#endif
                    constexpr type          CleanInvalid                    = REQ::RN::CleanInvalid;                // from      RN-F
                    constexpr type          MakeInvalid                     = REQ::RN::MakeInvalid;                 // from      RN-F
                    //---------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          WriteUniquePtlCleanSh           = REQ::RN::WriteUniquePtlCleanSh;       // from      RN-F
                    constexpr type          WriteUniquePtlCleanShPerSep     = REQ::RN::WriteUniquePtlCleanShPerSep; // from      RN-F
                    constexpr type          WriteUniqueFullCleanSh          = REQ::RN::WriteUniqueFullCleanSh;      // from      RN-F
                    constexpr type          WriteUniqueFullCleanShPerSep    = REQ::RN::WriteUniqueFullCleanShPerSep;// from      RN-F
                    constexpr type          WriteBackFullCleanInv           = REQ::RN::WriteBackFullCleanInv;       // from      RN-F
                    constexpr type          WriteBackFullCleanSh            = REQ::RN::WriteBackFullCleanSh;        // from      RN-F
                    constexpr type          WriteBackFullCleanShPerSep      = REQ::RN::WriteBackFullCleanShPerSep;  // from      RN-F
                    constexpr type          WriteCleanFullCleanSh           = REQ::RN::WriteCleanFullCleanSh;       // from      RN-F
                    constexpr type          WriteCleanFullCleanShPerSep     = REQ::RN::WriteCleanFullCleanShPerSep; // from      RN-F
#endif
                    //---------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          WriteNoSnpPtlCleanInv           = REQ::RN::WriteNoSnpPtlCleanInv;       // from      RN-F
                    constexpr type          WriteNoSnpPtlCleanSh            = REQ::RN::WriteNoSnpPtlCleanSh;        // from      RN-F
                    constexpr type          WriteNoSnpPtlCleanShPerSep      = REQ::RN::WriteNoSnpPtlCleanShPerSep;  // from      RN-F
                    constexpr type          WriteNoSnpFullCleanInv          = REQ::RN::WriteNoSnpFullCleanInv;      // from      RN-F
                    constexpr type          WriteNoSnpFullCleanSh           = REQ::RN::WriteNoSnpFullCleanSh;       // from      RN-F
                    constexpr type          WriteNoSnpFullCleanShPerSep     = REQ::RN::WriteNoSnpFullCleanShPerSep; // from      RN-F
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          DVMOp                           = REQ::RN::DVMOp;                       // from      RN-F
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          PCrdReturn                      = REQ::RN::PCrdReturn;                  // from      RN-F
                    //---------------------------------------------------------------------------------------------------------------
                    namespace               AtomicStore                     = REQ::RN::AtomicStore;                 // from      RN-F // NOLINT: misc-unused-alias-decls
                    namespace               AtomicLoad                      = REQ::RN::AtomicLoad;                  // from      RN-F // NOLINT: misc-unused-alias-decls
                    constexpr type          AtomicSwap                      = REQ::RN::AtomicSwap;                  // from      RN-F
                    constexpr type          AtomicCompare                   = REQ::RN::AtomicCompare;               // from      RN-F
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          PrefetchTgt                     = REQ::RN::PrefetchTgt;                 // from      RN-F
                    //===============================================================================================================
                };

                // Opcodes permitted in RN-D.
                namespace D {

                    //===============================================================================================================
                    constexpr type          ReqLCrdReturn                   = REQ::RN::ReqLCrdReturn;               //
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          ReadNoSnp                       = REQ::RN::ReadNoSnp;                   // from      RN-D
                    constexpr type          WriteNoSnpFull                  = REQ::RN::WriteNoSnpFull;              // from      RN-D
                    constexpr type          WriteNoSnpPtl                   = REQ::RN::WriteNoSnpPtl;               // from      RN-D
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          WriteNoSnpZero                  = REQ::RN::WriteNoSnpZero;              // from      RN-D
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          ReadOnce                        = REQ::RN::ReadOnce;                    // from      RN-D
                    constexpr type          ReadOnceCleanInvalid            = REQ::RN::ReadOnceCleanInvalid;        // from      RN-D
                    constexpr type          ReadOnceMakeInvalid             = REQ::RN::ReadOnceMakeInvalid;         // from      RN-D
                    constexpr type          StashOnceUnique                 = REQ::RN::StashOnceUnique;             // from      RN-D
                    constexpr type          StashOnceShared                 = REQ::RN::StashOnceShared;             // from      RN-D
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          StashOnceSepUnique              = REQ::RN::StashOnceSepUnique;          // from      RN-D
                    constexpr type          StashOnceSepShared              = REQ::RN::StashOnceSepShared;          // from      RN-D
#endif
                    constexpr type          WriteUniqueFull                 = REQ::RN::WriteUniqueFull;             // from      RN-D
                    constexpr type          WriteUniqueFullStash            = REQ::RN::WriteUniqueFullStash;        // from      RN-D
                    constexpr type          WriteUniquePtl                  = REQ::RN::WriteUniquePtl;              // from      RN-D
                    constexpr type          WriteUniquePtlStash             = REQ::RN::WriteUniquePtlStash;         // from      RN-D
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          WriteUniqueZero                 = REQ::RN::WriteUniqueZero;             // from      RN-D
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          CleanShared                     = REQ::RN::CleanShared;                 // from      RN-D
                    constexpr type          CleanSharedPersist              = REQ::RN::CleanSharedPersist;          // from      RN-D
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          CleanSharedPersistSep           = REQ::RN::CleanSharedPersistSep;       // from      RN-D
#endif
                    constexpr type          CleanInvalid                    = REQ::RN::CleanInvalid;                // from      RN-D
                    constexpr type          MakeInvalid                     = REQ::RN::MakeInvalid;                 // from      RN-D
                    //---------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          WriteNoSnpPtlCleanInv           = REQ::RN::WriteNoSnpPtlCleanInv;       // from      RN-D
                    constexpr type          WriteNoSnpPtlCleanSh            = REQ::RN::WriteNoSnpPtlCleanSh;        // from      RN-D
                    constexpr type          WriteNoSnpPtlCleanShPerSep      = REQ::RN::WriteNoSnpPtlCleanShPerSep;  // from      RN-D
                    constexpr type          WriteNoSnpFullCleanInv          = REQ::RN::WriteNoSnpFullCleanInv;      // from      RN-D
                    constexpr type          WriteNoSnpFullCleanSh           = REQ::RN::WriteNoSnpFullCleanSh;       // from      RN-D
                    constexpr type          WriteNoSnpFullCleanShPerSep     = REQ::RN::WriteNoSnpFullCleanShPerSep; // from      RN-D
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          DVMOp                           = REQ::RN::DVMOp;                       // from      RN-D
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          PCrdReturn                      = REQ::RN::PCrdReturn;                  // from      RN-D
                    namespace               AtomicStore                     = REQ::RN::AtomicStore;                 // from      RN-D // NOLINT: misc-unused-alias-decls
                    namespace               AtomicLoad                      = REQ::RN::AtomicLoad;                  // from      RN-D // NOLINT: misc-unused-alias-decls
                    constexpr type          AtomicSwap                      = REQ::RN::AtomicSwap;                  // from      RN-D
                    constexpr type          AtomicCompare                   = REQ::RN::AtomicCompare;               // from      RN-D
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          PrefetchTgt                     = REQ::RN::PrefetchTgt;                 // from      RN-D
                    //===============================================================================================================
                };

                // Opcodes permitted in RN-I.
                namespace I {

                    //===============================================================================================================
                    constexpr type          ReqLCrdReturn                   = REQ::RN::ReqLCrdReturn;               //
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          ReadNoSnp                       = REQ::RN::ReadNoSnp;                   // from      RN-I
                    constexpr type          WriteNoSnpFull                  = REQ::RN::WriteNoSnpFull;              // from      RN-I
                    constexpr type          WriteNoSnpPtl                   = REQ::RN::WriteNoSnpPtl;               // from      RN-I
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          WriteNoSnpZero                  = REQ::RN::WriteNoSnpZero;              // from      RN-I
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          ReadOnce                        = REQ::RN::ReadOnce;                    // from      RN-I
                    constexpr type          ReadOnceCleanInvalid            = REQ::RN::ReadOnceCleanInvalid;        // from      RN-I
                    constexpr type          ReadOnceMakeInvalid             = REQ::RN::ReadOnceMakeInvalid;         // from      RN-I
                    constexpr type          StashOnceUnique                 = REQ::RN::StashOnceUnique;             // from      RN-I
                    constexpr type          StashOnceShared                 = REQ::RN::StashOnceShared;             // from      RN-I
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          StashOnceSepUnique              = REQ::RN::StashOnceSepUnique;          // from      RN-I
                    constexpr type          StashOnceSepShared              = REQ::RN::StashOnceSepShared;          // from      RN-I
#endif
                    constexpr type          WriteUniqueFull                 = REQ::RN::WriteUniqueFull;             // from      RN-I
                    constexpr type          WriteUniqueFullStash            = REQ::RN::WriteUniqueFullStash;        // from      RN-I
                    constexpr type          WriteUniquePtl                  = REQ::RN::WriteUniquePtl;              // from      RN-I
                    constexpr type          WriteUniquePtlStash             = REQ::RN::WriteUniquePtlStash;         // from      RN-I
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          WriteUniqueZero                 = REQ::RN::WriteUniqueZero;             // from      RN-I
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          CleanShared                     = REQ::RN::CleanShared;                 // from      RN-I
                    constexpr type          CleanSharedPersist              = REQ::RN::CleanSharedPersist;          // from      RN-I
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          CleanSharedPersistSep           = REQ::RN::CleanSharedPersistSep;       // from      RN-I
#endif
                    constexpr type          CleanInvalid                    = REQ::RN::CleanInvalid;                // from      RN-I
                    constexpr type          MakeInvalid                     = REQ::RN::MakeInvalid;                 // from      RN-I
                    //---------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          WriteNoSnpPtlCleanInv           = REQ::RN::WriteNoSnpPtlCleanInv;       // from      RN-I
                    constexpr type          WriteNoSnpPtlCleanSh            = REQ::RN::WriteNoSnpPtlCleanSh;        // from      RN-I
                    constexpr type          WriteNoSnpPtlCleanShPerSep      = REQ::RN::WriteNoSnpPtlCleanShPerSep;  // from      RN-I
                    constexpr type          WriteNoSnpFullCleanInv          = REQ::RN::WriteNoSnpFullCleanInv;      // from      RN-I
                    constexpr type          WriteNoSnpFullCleanSh           = REQ::RN::WriteNoSnpFullCleanSh;       // from      RN-I
                    constexpr type          WriteNoSnpFullCleanShPerSep     = REQ::RN::WriteNoSnpFullCleanShPerSep; // from      RN-I
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          PCrdReturn                      = REQ::RN::PCrdReturn;                  // from      RN-I
                    //---------------------------------------------------------------------------------------------------------------
                    namespace               AtomicStore                     = REQ::RN::AtomicStore;                 // from      RN-I // NOLINT: misc-unused-alias-decls
                    namespace               AtomicLoad                      = REQ::RN::AtomicLoad;                  // from      RN-I // NOLINT: misc-unused-alias-decls
                    constexpr type          AtomicSwap                      = REQ::RN::AtomicSwap;                  // from      RN-I
                    constexpr type          AtomicCompare                   = REQ::RN::AtomicCompare;               // from      RN-I
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type          PrefetchTgt                     = REQ::RN::PrefetchTgt;                 // from      RN-I
                    //===============================================================================================================
                };
            };

            /*
            Opcodes permitted in HN. See page B-356.
            */
            namespace HN {

                //=============================================================================================================
                constexpr type          ReqLCrdReturn                   = REQ::ReqLCrdReturn;                   //
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          ReadNoSnp                       = REQ::ReadNoSnp;                       // from & to HN
                constexpr type          WriteNoSnpFull                  = REQ::WriteNoSnpFull;                  // from & to HN
                constexpr type          WriteNoSnpPtl                   = REQ::WriteNoSnpPtl;                   // from & to HN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          WriteNoSnpZero                  = REQ::WriteNoSnpZero;                  // from & to HN
#endif
                //-------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          ReadNoSnpSep                    = REQ::ReadNoSnpSep;                    // from      HN
#endif
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          ReadClean                       = REQ::ReadClean;                       //        to HN
                constexpr type          ReadShared                      = REQ::ReadShared;                      //        to HN
                constexpr type          ReadNotSharedDirty              = REQ::ReadNotSharedDirty;              //        to HN
                constexpr type          ReadUnique                      = REQ::ReadUnique;                      //        to HN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          ReadPreferUnique                = REQ::ReadPreferUnique;                //        to HN
                constexpr type          MakeReadUnique                  = REQ::MakeReadUnique;                  //        to HN
#endif
                constexpr type          CleanUnique                     = REQ::CleanUnique;                     //        to HN
                constexpr type          MakeUnique                      = REQ::MakeUnique;                      //        to HN
                constexpr type          Evict                           = REQ::Evict;                           //        to HN
                constexpr type          WriteBackFull                   = REQ::WriteBackFull;                   //        to HN
                constexpr type          WriteBackPtl                    = REQ::WriteBackPtl;                    //        to HN
                constexpr type          WriteEvictFull                  = REQ::WriteEvictFull;                  //        to HN
                constexpr type          WriteCleanFull                  = REQ::WriteCleanFull;                  //        to HN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          WriteEvictOrEvict               = REQ::WriteEvictOrEvict;               //        to HN
#endif
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          ReadOnce                        = REQ::ReadOnce;                        //        to HN
                constexpr type          ReadOnceCleanInvalid            = REQ::ReadOnceCleanInvalid;            //        to HN
                constexpr type          ReadOnceMakeInvalid             = REQ::ReadOnceMakeInvalid;             //        to HN
                constexpr type          StashOnceUnique                 = REQ::StashOnceUnique;                 //        to HN
                constexpr type          StashOnceShared                 = REQ::StashOnceShared;                 //        to HN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          StashOnceSepUnique              = REQ::StashOnceSepUnique;              //        to HN
                constexpr type          StashOnceSepShared              = REQ::StashOnceSepShared;              //        to HN
#endif
                constexpr type          WriteUniqueFull                 = REQ::WriteUniqueFull;                 //        to HN
                constexpr type          WriteUniqueFullStash            = REQ::WriteUniqueFullStash;            //        to HN
                constexpr type          WriteUniquePtl                  = REQ::WriteUniquePtl;                  //        to HN
                constexpr type          WriteUniquePtlStash             = REQ::WriteUniquePtlStash;             //        to HN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          WriteUniqueZero                 = REQ::WriteUniqueZero;                 //        to HN
#endif
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          CleanShared                     = REQ::CleanShared;                     // from & to HN
                constexpr type          CleanSharedPersist              = REQ::CleanSharedPersist;              // from & to HN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          CleanSharedPersistSep           = REQ::CleanSharedPersistSep;           // from & to HN
#endif
                constexpr type          CleanInvalid                    = REQ::CleanInvalid;                    // from & to HN
                constexpr type          MakeInvalid                     = REQ::MakeInvalid;                     // from & to HN
                //-------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          WriteUniquePtlCleanSh           = REQ::WriteUniquePtlCleanSh;           //        to HN
                constexpr type          WriteUniquePtlCleanShPerSep     = REQ::WriteUniquePtlCleanShPerSep;     //        to HN
                constexpr type          WriteUniqueFullCleanSh          = REQ::WriteUniqueFullCleanSh;          //        to HN
                constexpr type          WriteUniqueFullCleanShPerSep    = REQ::WriteUniqueFullCleanShPerSep;    //        to HN
                constexpr type          WriteBackFullCleanInv           = REQ::WriteBackFullCleanInv;           //        to HN
                constexpr type          WriteBackFullCleanSh            = REQ::WriteBackFullCleanSh;            //        to HN
                constexpr type          WriteBackFullCleanShPerSep      = REQ::WriteBackFullCleanShPerSep;      //        to HN
                constexpr type          WriteCleanFullCleanSh           = REQ::WriteCleanFullCleanSh;           //        to HN
                constexpr type          WriteCleanFullCleanShPerSep     = REQ::WriteCleanFullCleanShPerSep;     //        to HN
#endif
                //-------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          WriteNoSnpPtlCleanInv           = REQ::WriteNoSnpPtlCleanInv;           // from & to HN
                constexpr type          WriteNoSnpPtlCleanSh            = REQ::WriteNoSnpPtlCleanSh;            // from & to HN
                constexpr type          WriteNoSnpPtlCleanShPerSep      = REQ::WriteNoSnpPtlCleanShPerSep;      // from & to HN
                constexpr type          WriteNoSnpFullCleanInv          = REQ::WriteNoSnpFullCleanInv;          // from & to HN
                constexpr type          WriteNoSnpFullCleanSh           = REQ::WriteNoSnpFullCleanSh;           // from & to HN
                constexpr type          WriteNoSnpFullCleanShPerSep     = REQ::WriteNoSnpFullCleanShPerSep;     // from & to HN
#endif
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          PCrdReturn                      = REQ::PCrdReturn;                      // from & to HN
                //-------------------------------------------------------------------------------------------------------------
                namespace               AtomicStore                     = REQ::AtomicStore;                     // from & to HN // NOLINT: misc-unused-alias-decls
                namespace               AtomicLoad                      = REQ::AtomicLoad;                      // from & to HN // NOLINT: misc-unused-alias-decls
                constexpr type          AtomicSwap                      = REQ::AtomicSwap;                      // from & to HN
                constexpr type          AtomicCompare                   = REQ::AtomicCompare;                   // from & to HN
                //=============================================================================================================

                // Opcodes permitted in HN-F.
                namespace F {

                    //===============================================================================================================
                    constexpr type      ReqLCrdReturn                       = REQ::HN::ReqLCrdReturn;               //
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      ReadNoSnp                           = REQ::HN::ReadNoSnp;                   // from & to HN-F
                    constexpr type      WriteNoSnpFull                      = REQ::HN::WriteNoSnpFull;              // from & to HN-F
                    constexpr type      WriteNoSnpPtl                       = REQ::HN::WriteNoSnpPtl;               // from & to HN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      WriteNoSnpZero                      = REQ::HN::WriteNoSnpZero;              // from & to HN-F
#endif
                    //---------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      ReadNoSnpSep                        = REQ::HN::ReadNoSnpSep;                // from      HN-F
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      ReadClean                           = REQ::HN::ReadClean;                   //        to HN-F
                    constexpr type      ReadShared                          = REQ::HN::ReadShared;                  //        to HN-F
                    constexpr type      ReadNotSharedDirty                  = REQ::HN::ReadNotSharedDirty;          //        to HN-F
                    constexpr type      ReadUnique                          = REQ::HN::ReadUnique;                  //        to HN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      ReadPreferUnique                    = REQ::HN::ReadPreferUnique;            //        to HN-F
                    constexpr type      MakeReadUnique                      = REQ::HN::MakeReadUnique;              //        to HN-F
#endif
                    constexpr type      CleanUnique                         = REQ::HN::CleanUnique;                 //        to HN-F
                    constexpr type      MakeUnique                          = REQ::HN::MakeUnique;                  //        to HN-F
                    constexpr type      Evict                               = REQ::HN::Evict;                       //        to HN-F
                    constexpr type      WriteBackFull                       = REQ::HN::WriteBackFull;               //        to HN-F
                    constexpr type      WriteBackPtl                        = REQ::HN::WriteBackPtl;                //        to HN-F
                    constexpr type      WriteEvictFull                      = REQ::HN::WriteEvictFull;              //        to HN-F
                    constexpr type      WriteCleanFull                      = REQ::HN::WriteCleanFull;              //        to HN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      WriteEvictOrEvict                   = REQ::HN::WriteEvictOrEvict;           //        to HN-F
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      ReadOnce                            = REQ::HN::ReadOnce;                    //        to HN-F
                    constexpr type      ReadOnceCleanInvalid                = REQ::HN::ReadOnceCleanInvalid;        //        to HN-F
                    constexpr type      ReadOnceMakeInvalid                 = REQ::HN::ReadOnceMakeInvalid;         //        to HN-F
                    constexpr type      StashOnceUnique                     = REQ::HN::StashOnceUnique;             //        to HN-F
                    constexpr type      StashOnceShared                     = REQ::HN::StashOnceShared;             //        to HN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      StashOnceSepUnique                  = REQ::HN::StashOnceSepUnique;          //        to HN-F
                    constexpr type      StashOnceSepShared                  = REQ::HN::StashOnceSepShared;          //        to HN-F
#endif
                    constexpr type      WriteUniqueFull                     = REQ::HN::WriteUniqueFull;             //        to HN-F
                    constexpr type      WriteUniqueFullStash                = REQ::HN::WriteUniqueFullStash;        //        to HN-F
                    constexpr type      WriteUniquePtl                      = REQ::HN::WriteUniquePtl;              //        to HN-F
                    constexpr type      WriteUniquePtlStash                 = REQ::HN::WriteUniquePtlStash;         //        to HN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      WriteUniqueZero                     = REQ::HN::WriteUniqueZero;             //        to HN-F
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      CleanShared                         = REQ::HN::CleanShared;                 // from & to HN-F
                    constexpr type      CleanSharedPersist                  = REQ::HN::CleanSharedPersist;          // from & to HN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      CleanSharedPersistSep               = REQ::HN::CleanSharedPersistSep;       // from & to HN-F
#endif
                    constexpr type      CleanInvalid                        = REQ::HN::CleanInvalid;                // from & to HN-F
                    constexpr type      MakeInvalid                         = REQ::HN::MakeInvalid;                 // from & to HN-F
                    //---------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      WriteUniquePtlCleanSh               = REQ::HN::WriteUniquePtlCleanSh;       //        to HN-F
                    constexpr type      WriteUniquePtlCleanShPerSep         = REQ::HN::WriteUniquePtlCleanShPerSep; //        to HN-F
                    constexpr type      WriteUniqueFullCleanSh              = REQ::HN::WriteUniqueFullCleanSh;      //        to HN-F
                    constexpr type      WriteUniqueFullCleanShPerSep        = REQ::HN::WriteUniqueFullCleanShPerSep;//        to HN-F
                    constexpr type      WriteBackFullCleanInv               = REQ::HN::WriteBackFullCleanInv;       //        to HN-F
                    constexpr type      WriteBackFullCleanSh                = REQ::HN::WriteBackFullCleanSh;        //        to HN-F
                    constexpr type      WriteBackFullCleanShPerSep          = REQ::HN::WriteBackFullCleanShPerSep;  //        to HN-F
                    constexpr type      WriteCleanFullCleanSh               = REQ::HN::WriteCleanFullCleanSh;       //        to HN-F
                    constexpr type      WriteCleanFullCleanShPerSep         = REQ::HN::WriteCleanFullCleanShPerSep; //        to HN-F
#endif
                    //---------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      WriteNoSnpPtlCleanInv               = REQ::HN::WriteNoSnpPtlCleanInv;       // from & to HN-F
                    constexpr type      WriteNoSnpPtlCleanSh                = REQ::HN::WriteNoSnpPtlCleanSh;        // from & to HN-F
                    constexpr type      WriteNoSnpPtlCleanShPerSep          = REQ::HN::WriteNoSnpPtlCleanShPerSep;  // from & to HN-F
                    constexpr type      WriteNoSnpFullCleanInv              = REQ::HN::WriteNoSnpFullCleanInv;      // from & to HN-F
                    constexpr type      WriteNoSnpFullCleanSh               = REQ::HN::WriteNoSnpFullCleanSh;       // from & to HN-F
                    constexpr type      WriteNoSnpFullCleanShPerSep         = REQ::HN::WriteNoSnpFullCleanShPerSep; // from & to HN-F
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      PCrdReturn                          = REQ::HN::PCrdReturn;                  // from & to HN-F
                    //---------------------------------------------------------------------------------------------------------------
                    namespace           AtomicStore                         = REQ::HN::AtomicStore;                 // from & to HN-F // NOLINT: misc-unused-alias-decls
                    namespace           AtomicLoad                          = REQ::HN::AtomicLoad;                  // from & to HN-F // NOLINT: misc-unused-alias-decls
                    constexpr type      AtomicSwap                          = REQ::HN::AtomicSwap;                  // from & to HN-F
                    constexpr type      AtomicCompare                       = REQ::HN::AtomicCompare;               // from & to HN-F
                    //===============================================================================================================
                };

                // Opcodes permitted in HN-I.
                namespace I {

                    //===============================================================================================================
                    constexpr type      ReqLCrdReturn                       = REQ::HN::ReqLCrdReturn;               //
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      ReadNoSnp                           = REQ::HN::ReadNoSnp;                   // from & to HN-I
                    constexpr type      WriteNoSnpFull                      = REQ::HN::WriteNoSnpFull;              // from & to HN-I
                    constexpr type      WriteNoSnpPtl                       = REQ::HN::WriteNoSnpPtl;               // from & to HN-I
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      WriteNoSnpZero                      = REQ::HN::WriteNoSnpZero;              // from & to HN-I
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      ReadClean                           = REQ::HN::ReadClean;                   //        to HN-I
                    constexpr type      ReadShared                          = REQ::HN::ReadShared;                  //        to HN-I
                    constexpr type      ReadNotSharedDirty                  = REQ::HN::ReadNotSharedDirty;          //        to HN-I
                    constexpr type      ReadUnique                          = REQ::HN::ReadUnique;                  //        to HN-I
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      ReadPreferUnique                    = REQ::HN::ReadPreferUnique;            //        to HN-I
                    constexpr type      MakeReadUnique                      = REQ::HN::MakeReadUnique;              //        to HN-I
#endif
                    constexpr type      CleanUnique                         = REQ::HN::CleanUnique;                 //        to HN-I
                    constexpr type      MakeUnique                          = REQ::HN::MakeUnique;                  //        to HN-I
                    constexpr type      Evict                               = REQ::HN::Evict;                       //        to HN-I
                    constexpr type      WriteBackFull                       = REQ::HN::WriteBackFull;               //        to HN-I
                    constexpr type      WriteBackPtl                        = REQ::HN::WriteBackPtl;                //        to HN-I
                    constexpr type      WriteEvictFull                      = REQ::HN::WriteEvictFull;              //        to HN-I
                    constexpr type      WriteCleanFull                      = REQ::HN::WriteCleanFull;              //        to HN-I
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      WriteEvictOrEvict                   = REQ::HN::WriteEvictOrEvict;           //        to HN-I
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      ReadOnce                            = REQ::HN::ReadOnce;                    //        to HN-I
                    constexpr type      ReadOnceCleanInvalid                = REQ::HN::ReadOnceCleanInvalid;        //        to HN-I
                    constexpr type      ReadOnceMakeInvalid                 = REQ::HN::ReadOnceMakeInvalid;         //        to HN-I
                    constexpr type      StashOnceUnique                     = REQ::HN::StashOnceUnique;             //        to HN-I
                    constexpr type      StashOnceShared                     = REQ::HN::StashOnceShared;             //        to HN-I
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      StashOnceSepUnique                  = REQ::HN::StashOnceSepUnique;          //        to HN-I
                    constexpr type      StashOnceSepShared                  = REQ::HN::StashOnceSepShared;          //        to HN-I
#endif
                    constexpr type      WriteUniqueFull                     = REQ::HN::WriteUniqueFull;             //        to HN-I
                    constexpr type      WriteUniqueFullStash                = REQ::HN::WriteUniqueFullStash;        //        to HN-I
                    constexpr type      WriteUniquePtl                      = REQ::HN::WriteUniquePtl;              //        to HN-I
                    constexpr type      WriteUniquePtlStash                 = REQ::HN::WriteUniquePtlStash;         //        to HN-I
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      WriteUniqueZero                     = REQ::HN::WriteUniqueZero;             //        to HN-I
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      CleanShared                         = REQ::HN::CleanShared;                 // from & to HN-I
                    constexpr type      CleanSharedPersist                  = REQ::HN::CleanSharedPersist;          // from & to HN-I
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      CleanSharedPersistSep               = REQ::HN::CleanSharedPersistSep;       // from & to HN-I
#endif
                    constexpr type      CleanInvalid                        = REQ::HN::CleanInvalid;                // from & to HN-I
                    constexpr type      MakeInvalid                         = REQ::HN::MakeInvalid;                 // from & to HN-I
                    //---------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      WriteUniquePtlCleanSh               = REQ::HN::WriteUniquePtlCleanSh;       //        to HN-I
                    constexpr type      WriteUniquePtlCleanShPerSep         = REQ::HN::WriteUniquePtlCleanShPerSep; //        to HN-I
                    constexpr type      WriteUniqueFullCleanSh              = REQ::HN::WriteUniqueFullCleanSh;      //        to HN-I
                    constexpr type      WriteUniqueFullCleanShPerSep        = REQ::HN::WriteUniqueFullCleanShPerSep;//        to HN-I
                    constexpr type      WriteBackFullCleanInv               = REQ::HN::WriteBackFullCleanInv;       //        to HN-I
                    constexpr type      WriteBackFullCleanSh                = REQ::HN::WriteBackFullCleanSh;        //        to HN-I
                    constexpr type      WriteBackFullCleanShPerSep          = REQ::HN::WriteBackFullCleanShPerSep;  //        to HN-I
                    constexpr type      WriteCleanFullCleanSh               = REQ::HN::WriteCleanFullCleanSh;       //        to HN-I
                    constexpr type      WriteCleanFullCleanShPerSep         = REQ::HN::WriteCleanFullCleanShPerSep; //        to HN-I
#endif
                    //---------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      WriteNoSnpPtlCleanInv               = REQ::HN::WriteNoSnpPtlCleanInv;       // from & to HN-I
                    constexpr type      WriteNoSnpPtlCleanSh                = REQ::HN::WriteNoSnpPtlCleanSh;        // from & to HN-I
                    constexpr type      WriteNoSnpPtlCleanShPerSep          = REQ::HN::WriteNoSnpPtlCleanShPerSep;  // from & to HN-I
                    constexpr type      WriteNoSnpFullCleanInv              = REQ::HN::WriteNoSnpFullCleanInv;      // from & to HN-I
                    constexpr type      WriteNoSnpFullCleanSh               = REQ::HN::WriteNoSnpFullCleanSh;       // from & to HN-I
                    constexpr type      WriteNoSnpFullCleanShPerSep         = REQ::HN::WriteNoSnpFullCleanShPerSep; // from & to HN-I
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      PCrdReturn                          = REQ::HN::PCrdReturn;                  // from & to HN-I
                    //---------------------------------------------------------------------------------------------------------------
                    namespace           AtomicStore                         = REQ::HN::AtomicStore;                 // from & to HN-I // NOLINT: misc-unused-alias-decls
                    namespace           AtomicLoad                          = REQ::HN::AtomicLoad;                  // from & to HN-I // NOLINT: misc-unused-alias-decls
                    constexpr type      AtomicSwap                          = REQ::HN::AtomicSwap;                  // from & to HN-I
                    constexpr type      AtomicCompare                       = REQ::HN::AtomicCompare;               // from & to HN-I
                    //===============================================================================================================
                };
            };

            /*
            Opcodes permitted in SN. See page B-356.
            */
            namespace SN {

                //=============================================================================================================
                constexpr type          ReqLCrdReturn                   = REQ::ReqLCrdReturn;                   //
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          ReadNoSnp                       = REQ::ReadNoSnp;                       //        to SN
                constexpr type          WriteNoSnpFull                  = REQ::WriteNoSnpFull;                  //        to SN
                constexpr type          WriteNoSnpPtl                   = REQ::WriteNoSnpPtl;                   //        to SN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          WriteNoSnpZero                  = REQ::WriteNoSnpZero;                  //        to SN
#endif
                //-------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          ReadNoSnpSep                    = REQ::ReadNoSnpSep;                    //        to SN
#endif
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          CleanShared                     = REQ::CleanShared;                     //        to SN
                constexpr type          CleanSharedPersist              = REQ::CleanSharedPersist;              //        to SN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          CleanSharedPersistSep           = REQ::CleanSharedPersistSep;           //        to SN
#endif
                constexpr type          CleanInvalid                    = REQ::CleanInvalid;                    //        to SN
                constexpr type          MakeInvalid                     = REQ::MakeInvalid;                     //        to SN
                //-------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          WriteNoSnpPtlCleanInv           = REQ::WriteNoSnpPtlCleanInv;           //        to SN
                constexpr type          WriteNoSnpPtlCleanSh            = REQ::WriteNoSnpPtlCleanSh;            //        to SN
                constexpr type          WriteNoSnpPtlCleanShPerSep      = REQ::WriteNoSnpPtlCleanShPerSep;      //        to SN
                constexpr type          WriteNoSnpFullCleanInv          = REQ::WriteNoSnpFullCleanInv;          //        to SN
                constexpr type          WriteNoSnpFullCleanSh           = REQ::WriteNoSnpFullCleanSh;           //        to SN
                constexpr type          WriteNoSnpFullCleanShPerSep     = REQ::WriteNoSnpFullCleanShPerSep;     //        to SN
#endif
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          PCrdReturn                      = REQ::PCrdReturn;                      //        to SN
                //-------------------------------------------------------------------------------------------------------------
                namespace               AtomicStore                     = REQ::AtomicStore;                     //        to SN // NOLINT: misc-unused-alias-decls
                namespace               AtomicLoad                      = REQ::AtomicLoad;                      //        to SN // NOLINT: misc-unused-alias-decls
                constexpr type          AtomicSwap                      = REQ::AtomicSwap;                      //        to SN
                constexpr type          AtomicCompare                   = REQ::AtomicCompare;                   //        to SN
                //-------------------------------------------------------------------------------------------------------------
                constexpr type          PrefetchTgt                     = REQ::PrefetchTgt;                     //        to SN
                //=============================================================================================================

                // Opcodes permitted in SN-F.
                namespace F {

                    //===============================================================================================================
                    constexpr type      ReqLCrdReturn                       = REQ::SN::ReqLCrdReturn;               //
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      ReadNoSnp                           = REQ::SN::ReadNoSnp;                   //        to SN-F
                    constexpr type      WriteNoSnpFull                      = REQ::SN::WriteNoSnpFull;              //        to SN-F
                    constexpr type      WriteNoSnpPtl                       = REQ::SN::WriteNoSnpPtl;               //        to SN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      WriteNoSnpZero                      = REQ::SN::WriteNoSnpZero;              //        to SN-F
#endif
                    //---------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      ReadNoSnpSep                        = REQ::SN::ReadNoSnpSep;                //        to SN-F
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      CleanShared                         = REQ::SN::CleanShared;                 //        to SN-F
                    constexpr type      CleanSharedPersist                  = REQ::SN::CleanSharedPersist;          //        to SN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      CleanSharedPersistSep               = REQ::SN::CleanSharedPersistSep;       //        to SN-F
#endif
                    constexpr type      CleanInvalid                        = REQ::SN::CleanInvalid;                //        to SN-F
                    constexpr type      MakeInvalid                         = REQ::SN::MakeInvalid;                 //        to SN-F
                    //---------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      WriteNoSnpPtlCleanInv               = REQ::SN::WriteNoSnpPtlCleanInv;       //        to SN-F
                    constexpr type      WriteNoSnpPtlCleanSh                = REQ::SN::WriteNoSnpPtlCleanSh;        //        to SN-F
                    constexpr type      WriteNoSnpPtlCleanShPerSep          = REQ::SN::WriteNoSnpPtlCleanShPerSep;  //        to SN-F
                    constexpr type      WriteNoSnpFullCleanInv              = REQ::SN::WriteNoSnpFullCleanInv;      //        to SN-F
                    constexpr type      WriteNoSnpFullCleanSh               = REQ::SN::WriteNoSnpFullCleanSh;       //        to SN-F
                    constexpr type      WriteNoSnpFullCleanShPerSep         = REQ::SN::WriteNoSnpFullCleanShPerSep; //        to SN-F
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      PCrdReturn                          = REQ::SN::PCrdReturn;                  //        to SN-F
                    //---------------------------------------------------------------------------------------------------------------
                    namespace           AtomicStore                         = REQ::SN::AtomicStore;                 //        to SN-F // NOLINT: misc-unused-alias-decls
                    namespace           AtomicLoad                          = REQ::SN::AtomicLoad;                  //        to SN-F // NOLINT: misc-unused-alias-decls
                    constexpr type      AtomicSwap                          = REQ::SN::AtomicSwap;                  //        to SN-F
                    constexpr type      AtomicCompare                       = REQ::SN::AtomicCompare;               //        to SN-F
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      PrefetchTgt                         = REQ::SN::PrefetchTgt;                 //        to SN-F
                    //===============================================================================================================
                };

                // Opcodes permitted in SN-I.
                namespace I {

                    //===============================================================================================================
                    constexpr type      ReqLCrdReturn                       = REQ::SN::ReqLCrdReturn;               //
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      ReadNoSnp                           = REQ::SN::ReadNoSnp;                   //        to SN-I
                    constexpr type      WriteNoSnpFull                      = REQ::SN::WriteNoSnpFull;              //        to SN-I
                    constexpr type      WriteNoSnpPtl                       = REQ::SN::WriteNoSnpPtl;               //        to SN-I
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      WriteNoSnpZero                      = REQ::SN::WriteNoSnpZero;              //        to SN-I
#endif
                    //---------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      ReadNoSnpSep                        = REQ::SN::ReadNoSnpSep;                //        to SN-I
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      CleanShared                         = REQ::SN::CleanShared;                 //        to SN-I
                    constexpr type      CleanSharedPersist                  = REQ::SN::CleanSharedPersist;          //        to SN-I
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      CleanSharedPersistSep               = REQ::SN::CleanSharedPersistSep;       //        to SN-I
#endif
                    constexpr type      CleanInvalid                        = REQ::SN::CleanInvalid;                //        to SN-I
                    constexpr type      MakeInvalid                         = REQ::SN::MakeInvalid;                 //        to SN-I
                    //---------------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      WriteNoSnpPtlCleanInv               = REQ::SN::WriteNoSnpPtlCleanInv;       //        to SN-I
                    constexpr type      WriteNoSnpPtlCleanSh                = REQ::SN::WriteNoSnpPtlCleanSh;        //        to SN-I
                    constexpr type      WriteNoSnpPtlCleanShPerSep          = REQ::SN::WriteNoSnpPtlCleanShPerSep;  //        to SN-I
                    constexpr type      WriteNoSnpFullCleanInv              = REQ::SN::WriteNoSnpFullCleanInv;      //        to SN-I
                    constexpr type      WriteNoSnpFullCleanSh               = REQ::SN::WriteNoSnpFullCleanSh;       //        to SN-I
                    constexpr type      WriteNoSnpFullCleanShPerSep         = REQ::SN::WriteNoSnpFullCleanShPerSep; //        to SN-I
#endif
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      PCrdReturn                          = REQ::SN::PCrdReturn;                  //        to SN-I
                    //---------------------------------------------------------------------------------------------------------------
                    namespace           AtomicStore                         = REQ::SN::AtomicStore;                 //        to SN-I // NOLINT: misc-unused-alias-decls
                    namespace           AtomicLoad                          = REQ::SN::AtomicLoad;                  //        to SN-I // NOLINT: misc-unused-alias-decls
                    constexpr type      AtomicSwap                          = REQ::SN::AtomicSwap;                  //        to SN-I
                    constexpr type      AtomicCompare                       = REQ::SN::AtomicCompare;               //        to SN-I
                    //---------------------------------------------------------------------------------------------------------------
                    constexpr type      PrefetchTgt                         = REQ::SN::PrefetchTgt;                 //        to SN-I
                    //===============================================================================================================
                };
            };

            /*
            Opcodes permitted in MN. See page B-356.
            */
            namespace MN {

                //=========================================================================================
                constexpr type          ReqLCrdReturn           = REQ::ReqLCrdReturn;       //
                //-----------------------------------------------------------------------------------------
                constexpr type          DVMOp                   = REQ::DVMOp;               //        to MN
                //-----------------------------------------------------------------------------------------
                constexpr type          PCrdReturn              = REQ::PCrdReturn;          //        to MN
                //=========================================================================================
            };
        };

        namespace SNP {

            /*
            See SNP channel opcodes on page 12-300.
            */

            // Opcode type
            using type = Flits::SNP<>::opcode_t;

            // Opcodes
            constexpr type              SnpLCrdReturn           = 0x00;
            constexpr type              SnpShared               = 0x01;
            constexpr type              SnpClean                = 0x02;
            constexpr type              SnpOnce                 = 0x03;
            constexpr type              SnpNotSharedDirty       = 0x04;
            constexpr type              SnpUniqueStash          = 0x05;
            constexpr type              SnpMakeInvalidStash     = 0x06;
            constexpr type              SnpUnique               = 0x07;
            constexpr type              SnpCleanShared          = 0x08;
            constexpr type              SnpCleanInvalid         = 0x09;
            constexpr type              SnpMakeInvalid          = 0x0A;
            constexpr type              SnpStashUnique          = 0x0B;
            constexpr type              SnpStashShared          = 0x0C;
            constexpr type              SnpDVMOp                = 0x0D;
//          *Reserved*                                          = 0x0E;
//                                                            ... 0x0F;
#ifdef CHI_ISSUE_EB_ENABLE
            constexpr type              SnpQuery                = 0x10;
#endif
            constexpr type              SnpSharedFwd            = 0x11;
            constexpr type              SnpCleanFwd             = 0x12;
            constexpr type              SnpOnceFwd              = 0x13;
            constexpr type              SnpNotSharedDirtyFwd    = 0x14;
#ifdef CHI_ISSUE_EB_ENABLE
            constexpr type              SnpPreferUnique         = 0x15;
            constexpr type              SnpPreferUniqueFwd      = 0x16;
#endif
            constexpr type              SnpUniqueFwd            = 0x17;
//          *Reserved*                                          = 0x18;
//                                                            ... 0x1F;

            /*
            Opcodes permitted in RN. See page B-358.
            */
            namespace RN {
                
                //=========================================================================================
                constexpr type          SnpLCrdReturn           = SNP::SnpLCrdReturn;       //
                //-----------------------------------------------------------------------------------------
                constexpr type          SnpShared               = SNP::SnpShared;           //        to RN
                constexpr type          SnpClean                = SNP::SnpClean;            //        to RN
                constexpr type          SnpOnce                 = SNP::SnpOnce;             //        to RN
                constexpr type          SnpNotSharedDirty       = SNP::SnpNotSharedDirty;   //        to RN
                constexpr type          SnpUnique               = SNP::SnpUnique;           //        to RN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          SnpPreferUnique         = SNP::SnpPreferUnique;     //        to RN
#endif
                constexpr type          SnpCleanShared          = SNP::SnpCleanShared;      //        to RN
                constexpr type          SnpCleanInvalid         = SNP::SnpCleanInvalid;     //        to RN
                constexpr type          SnpMakeInvalid          = SNP::SnpMakeInvalid;      //        to RN
                constexpr type          SnpSharedFwd            = SNP::SnpSharedFwd;        //        to RN
                constexpr type          SnpCleanFwd             = SNP::SnpCleanFwd;         //        to RN
                constexpr type          SnpOnceFwd              = SNP::SnpOnceFwd;          //        to RN
                constexpr type          SnpNotSharedDirtyFwd    = SNP::SnpNotSharedDirtyFwd;//        to RN
                constexpr type          SnpUniqueFwd            = SNP::SnpUniqueFwd;        //        to RN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          SnpPreferUniqueFwd      = SNP::SnpPreferUniqueFwd;  //        to RN
#endif
                constexpr type          SnpUniqueStash          = SNP::SnpUniqueStash;      //        to RN
                constexpr type          SnpMakeInvalidStash     = SNP::SnpMakeInvalidStash; //        to RN
                constexpr type          SnpStashUnique          = SNP::SnpStashUnique;      //        to RN
                constexpr type          SnpStashShared          = SNP::SnpStashShared;      //        to RN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          SnpQuery                = SNP::SnpQuery;            //        to RN
#endif
                //-----------------------------------------------------------------------------------------
                constexpr type          SnpDVMOp                = SNP::SnpDVMOp;            //        to RN
                //=========================================================================================

                // Opcodes permitted in RN-F.
                namespace F {

                    //===============================================================================================
                    constexpr type      SnpLCrdReturn               = SNP::RN::SnpLCrdReturn;       //
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      SnpShared                   = SNP::RN::SnpShared;           //        to RN-F
                    constexpr type      SnpClean                    = SNP::RN::SnpClean;            //        to RN-F
                    constexpr type      SnpOnce                     = SNP::RN::SnpOnce;             //        to RN-F
                    constexpr type      SnpNotSharedDirty           = SNP::RN::SnpNotSharedDirty;   //        to RN-F
                    constexpr type      SnpUnique                   = SNP::RN::SnpUnique;           //        to RN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      SnpPreferUnique             = SNP::RN::SnpPreferUnique;     //        to RN-F
#endif
                    constexpr type      SnpCleanShared              = SNP::RN::SnpCleanShared;      //        to RN-F
                    constexpr type      SnpCleanInvalid             = SNP::RN::SnpCleanInvalid;     //        to RN-F
                    constexpr type      SnpMakeInvalid              = SNP::RN::SnpMakeInvalid;      //        to RN-F
                    constexpr type      SnpSharedFwd                = SNP::RN::SnpSharedFwd;        //        to RN-F
                    constexpr type      SnpCleanFwd                 = SNP::RN::SnpCleanFwd;         //        to RN-F
                    constexpr type      SnpOnceFwd                  = SNP::RN::SnpOnceFwd;          //        to RN-F
                    constexpr type      SnpNotSharedDirtyFwd        = SNP::RN::SnpNotSharedDirtyFwd;//        to RN-F
                    constexpr type      SnpUniqueFwd                = SNP::RN::SnpUniqueFwd;        //        to RN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      SnpPreferUniqueFwd          = SNP::RN::SnpPreferUniqueFwd;  //        to RN-F
#endif
                    constexpr type      SnpUniqueStash              = SNP::RN::SnpUniqueStash;      //        to RN-F
                    constexpr type      SnpMakeInvalidStash         = SNP::RN::SnpMakeInvalidStash; //        to RN-F
                    constexpr type      SnpStashUnique              = SNP::RN::SnpStashUnique;      //        to RN-F
                    constexpr type      SnpStashShared              = SNP::RN::SnpStashShared;      //        to RN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      SnpQuery                    = SNP::RN::SnpQuery;            //        to RN-F
#endif
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      SnpDVMOp                    = SNP::RN::SnpDVMOp;            //        to RN-F
                    //===============================================================================================
                };

                // Opcodes permitted in RN-D.
                namespace D {

                    //===============================================================================================
                    constexpr type      SnpLCrdReturn               = SNP::RN::SnpLCrdReturn;       //
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      SnpDVMOp                    = SNP::RN::SnpDVMOp;            //        to RN-D
                    //===============================================================================================
                };

                // Opcodes permitted in RN-I.
                namespace I {
                };
            };

            /*
            Opcodes permitted in HN. See page B-358.
            */
            namespace HN {

                //=========================================================================================
                constexpr type          SnpLCrdReturn           = SNP::SnpLCrdReturn;       //
                //-----------------------------------------------------------------------------------------
                constexpr type          SnpShared               = SNP::SnpShared;           // from      HN
                constexpr type          SnpClean                = SNP::SnpClean;            // from      HN
                constexpr type          SnpOnce                 = SNP::SnpOnce;             // from      HN
                constexpr type          SnpNotSharedDirty       = SNP::SnpNotSharedDirty;   // from      HN
                constexpr type          SnpUnique               = SNP::SnpUnique;           // from      HN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          SnpPreferUnique         = SNP::SnpPreferUnique;     // from      HN
#endif
                constexpr type          SnpCleanShared          = SNP::SnpCleanShared;      // from      HN
                constexpr type          SnpCleanInvalid         = SNP::SnpCleanInvalid;     // from      HN
                constexpr type          SnpMakeInvalid          = SNP::SnpMakeInvalid;      // from      HN
                constexpr type          SnpSharedFwd            = SNP::SnpSharedFwd;        // from      HN
                constexpr type          SnpCleanFwd             = SNP::SnpCleanFwd;         // from      HN
                constexpr type          SnpOnceFwd              = SNP::SnpOnceFwd;          // from      HN
                constexpr type          SnpNotSharedDirtyFwd    = SNP::SnpNotSharedDirtyFwd;// from      HN
                constexpr type          SnpUniqueFwd            = SNP::SnpUniqueFwd;        // from      HN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          SnpPreferUniqueFwd      = SNP::SnpPreferUniqueFwd;  // from      HN
#endif
                constexpr type          SnpUniqueStash          = SNP::SnpUniqueStash;      // from      HN
                constexpr type          SnpMakeInvalidStash     = SNP::SnpMakeInvalidStash; // from      HN
                constexpr type          SnpStashUnique          = SNP::SnpStashUnique;      // from      HN
                constexpr type          SnpStashShared          = SNP::SnpStashShared;      // from      HN
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          SnpQuery                = SNP::SnpQuery;            // from      HN
#endif
                //=========================================================================================

                // Opcodes permitted in HN-F.
                namespace F {

                    //===============================================================================================
                    constexpr type      SnpLCrdReturn               = SNP::HN::SnpLCrdReturn;       //
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      SnpShared                   = SNP::HN::SnpShared;           // from      HN-F
                    constexpr type      SnpClean                    = SNP::HN::SnpClean;            // from      HN-F
                    constexpr type      SnpOnce                     = SNP::HN::SnpOnce;             // from      HN-F
                    constexpr type      SnpNotSharedDirty           = SNP::HN::SnpNotSharedDirty;   // from      HN-F
                    constexpr type      SnpUnique                   = SNP::HN::SnpUnique;           // from      HN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      SnpPreferUnique             = SNP::HN::SnpPreferUnique;     // from      HN-F
#endif
                    constexpr type      SnpCleanShared              = SNP::HN::SnpCleanShared;      // from      HN-F
                    constexpr type      SnpCleanInvalid             = SNP::HN::SnpCleanInvalid;     // from      HN-F
                    constexpr type      SnpMakeInvalid              = SNP::HN::SnpMakeInvalid;      // from      HN-F
                    constexpr type      SnpSharedFwd                = SNP::HN::SnpSharedFwd;        // from      HN-F
                    constexpr type      SnpCleanFwd                 = SNP::HN::SnpCleanFwd;         // from      HN-F
                    constexpr type      SnpOnceFwd                  = SNP::HN::SnpOnceFwd;          // from      HN-F
                    constexpr type      SnpNotSharedDirtyFwd        = SNP::HN::SnpNotSharedDirtyFwd;// from      HN-F
                    constexpr type      SnpUniqueFwd                = SNP::HN::SnpUniqueFwd;        // from      HN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      SnpPreferUniqueFwd          = SNP::HN::SnpPreferUniqueFwd;  // from      HN-F
#endif
                    constexpr type      SnpUniqueStash              = SNP::HN::SnpUniqueStash;      // from      HN-F
                    constexpr type      SnpMakeInvalidStash         = SNP::HN::SnpMakeInvalidStash; // from      HN-F
                    constexpr type      SnpStashUnique              = SNP::HN::SnpStashUnique;      // from      HN-F
                    constexpr type      SnpStashShared              = SNP::HN::SnpStashShared;      // from      HN-F
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      SnpQuery                    = SNP::HN::SnpQuery;            // from      HN-F
#endif
                    //===============================================================================================
                };

                // Opcodes permitted in HN-I.
                namespace I {
                };
            };

            /*
            Opcodes permitted in SN. See page B-358.
            */
            namespace SN {

                // Opcodes permitted in SN-F.
                namespace F {
                };

                // Opcodes permitted in SN-I.
                namespace I {
                };
            };

            /*
            Opcodes permitted in MN. See page B-358.
            */
            namespace MN {

                //=============================================================================================
                constexpr type          SnpLCrdReturn               = SNP::SnpLCrdReturn;       //
                //---------------------------------------------------------------------------------------------
                constexpr type          SnpDVMOp                    = SNP::SnpDVMOp;            // from      MN
                //=============================================================================================
            };
        };

        namespace DAT {

            /*
            See DAT channel opcodes on page 12-301.
            */

            // Opcode type
            using type = Flits::DAT<>::opcode_t;

            // Opcodes
            constexpr type              DataLCrdReturn              = 0x00;
            constexpr type              SnpRespData                 = 0x01;
            constexpr type              CopyBackWrData              = 0x02;
            constexpr type              NonCopyBackWrData           = 0x03;
            constexpr type              CompData                    = 0x04;
            constexpr type              SnpRespDataPtl              = 0x05;
            constexpr type              SnpRespDataFwded            = 0x06;
            constexpr type              WriteDataCancel             = 0x07;
#ifdef CHI_ISSUE_EB_ENABLE
//          *Reserved*                                              = 0x08;
//                                                                ... 0x0A;
            constexpr type              DataSepResp                 = 0x0B;
            constexpr type              NCBWrDataCompAck            = 0x0C;
//          *Reserved*                                              = 0x0D;
//                                                                ... 0x0F;
#endif

            /*
            Opcodes permitted in RN. See page B-360.
            */
            namespace RN {

                //=================================================================================================
                constexpr type          DataLCrdReturn                  = DAT::DataLCrdReturn;      //
                //-------------------------------------------------------------------------------------------------
                constexpr type          CompData                        = DAT::CompData;            // from & to RN
                //-------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          DataSepResp                     = DAT::DataSepResp;         //        to RN
#endif
                //-------------------------------------------------------------------------------------------------
                constexpr type          CopyBackWrData                  = DAT::CopyBackWrData;      // from      RN
                //-------------------------------------------------------------------------------------------------
                constexpr type          WriteDataCancel                 = DAT::WriteDataCancel;     // from      RN
                //-------------------------------------------------------------------------------------------------
                constexpr type          NonCopyBackWrData               = DAT::NonCopyBackWrData;   // from      RN
                //-------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          NCBWrDataCompAck                = DAT::NCBWrDataCompAck;    // from      RN
#endif
                //-------------------------------------------------------------------------------------------------
                constexpr type          SnpRespData                     = DAT::SnpRespData;         // from      RN
                constexpr type          SnpRespDataFwded                = DAT::SnpRespDataFwded;    // from      RN
                constexpr type          SnpRespDataPtl                  = DAT::SnpRespDataPtl;      // from      RN
                //=================================================================================================

                // Opcodes permitted in RN-F.
                namespace F {

                    //=======================================================================================================
                    constexpr type      DataLCrdReturn                      = DAT::RN::DataLCrdReturn;      //
                    //-------------------------------------------------------------------------------------------------------
                    constexpr type      CompData                            = DAT::RN::CompData;            // from & to RN-F
                    //-------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      DataSepResp                         = DAT::RN::DataSepResp;         //        to RN-F
#endif
                    //-------------------------------------------------------------------------------------------------------
                    constexpr type      CopyBackWrData                      = DAT::RN::CopyBackWrData;      // from      RN-F
                    //-------------------------------------------------------------------------------------------------------
                    constexpr type      WriteDataCancel                     = DAT::RN::WriteDataCancel;     // from      RN-F
                    //-------------------------------------------------------------------------------------------------------
                    constexpr type      NonCopyBackWrData                   = DAT::RN::NonCopyBackWrData;   // from      RN-F
                    //-------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      NCBWrDataCompAck                    = DAT::RN::NCBWrDataCompAck;    // from      RN-F
#endif
                    //-------------------------------------------------------------------------------------------------------
                    constexpr type      SnpRespData                         = DAT::RN::SnpRespData;         // from      RN-F
                    constexpr type      SnpRespDataFwded                    = DAT::RN::SnpRespDataFwded;    // from      RN-F
                    constexpr type      SnpRespDataPtl                      = DAT::RN::SnpRespDataPtl;      // from      RN-F
                    //=======================================================================================================
                };

                // Opcodes permitted in RN-D.
                namespace D {

                    //=======================================================================================================
                    constexpr type      DataLCrdReturn                      = DAT::RN::DataLCrdReturn;      //
                    //-------------------------------------------------------------------------------------------------------
                    constexpr type      CompData                            = DAT::RN::CompData;            //        to RN-D
                    //-------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      DataSepResp                         = DAT::RN::DataSepResp;         //        to RN-D
#endif
                    //-------------------------------------------------------------------------------------------------------
                    constexpr type      WriteDataCancel                     = DAT::RN::WriteDataCancel;     // from      RN-D
                    //-------------------------------------------------------------------------------------------------------
                    constexpr type      NonCopyBackWrData                   = DAT::RN::NonCopyBackWrData;   // from      RN-D
                    //-------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      NCBWrDataCompAck                    = DAT::RN::NCBWrDataCompAck;    // from      RN-D
#endif
                    //=======================================================================================================
                };

                // Opcodes permitted in RN-I.
                namespace I {

                    //=======================================================================================================
                    constexpr type      DataLCrdReturn                      = DAT::RN::DataLCrdReturn;      //
                    //-------------------------------------------------------------------------------------------------------
                    constexpr type      CompData                            = DAT::RN::CompData;            //        to RN-I
                    //-------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      DataSepResp                         = DAT::RN::DataSepResp;         //        to RN-I
#endif
                    //-------------------------------------------------------------------------------------------------------
                    constexpr type      WriteDataCancel                     = DAT::RN::WriteDataCancel;     // from      RN-I
                    //-------------------------------------------------------------------------------------------------------
                    constexpr type      NonCopyBackWrData                   = DAT::RN::NonCopyBackWrData;   // from      RN-I
                    //-------------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      NCBWrDataCompAck                    = DAT::RN::NCBWrDataCompAck;    // from      RN-I
#endif
                    //=======================================================================================================
                };
            };

            /*
            Opcodes permitted in HN. See page B-360.
            */
            namespace HN {

                //=============================================================================================
                constexpr type          DataLCrdReturn              = DAT::DataLCrdReturn;      //
                //---------------------------------------------------------------------------------------------
                constexpr type          CompData                    = DAT::CompData;            // from & to HN
                //---------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          DataSepResp                 = DAT::DataSepResp;         // from & to HN
#endif
                //---------------------------------------------------------------------------------------------
                constexpr type          CopyBackWrData              = DAT::CopyBackWrData;      //        to HN
                //---------------------------------------------------------------------------------------------
                constexpr type          WriteDataCancel             = DAT::WriteDataCancel;     // from & to HN
                //---------------------------------------------------------------------------------------------
                constexpr type          NonCopyBackWrData           = DAT::NonCopyBackWrData;   // from & to HN
                //---------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          NCBWrDataCompAck            = DAT::NCBWrDataCompAck;    //        to HN
#endif
                //---------------------------------------------------------------------------------------------
                constexpr type          SnpRespData                 = DAT::SnpRespData;         //        to HN
                constexpr type          SnpRespDataFwded            = DAT::SnpRespDataFwded;    //        to HN
                constexpr type          SnpRespDataPtl              = DAT::SnpRespDataPtl;      //        to HN
                //=============================================================================================

                // Opcodes permitted in HN-F.
                namespace F {
                    
                    //===================================================================================================
                    constexpr type      DataLCrdReturn                  = DAT::HN::DataLCrdReturn;      //
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      CompData                        = DAT::HN::CompData;            // from & to HN-F
                    //---------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      DataSepResp                     = DAT::HN::DataSepResp;         // from & to HN-F
#endif
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      CopyBackWrData                  = DAT::HN::CopyBackWrData;      //        to HN-F
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      WriteDataCancel                 = DAT::HN::WriteDataCancel;     // from & to HN-F
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      NonCopyBackWrData               = DAT::HN::NonCopyBackWrData;   // from & to HN-F
                    //---------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      NCBWrDataCompAck                = DAT::HN::NCBWrDataCompAck;    //        to HN-F
#endif
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      SnpRespData                     = DAT::HN::SnpRespData;         //        to HN-F
                    constexpr type      SnpRespDataFwded                = DAT::HN::SnpRespDataFwded;    //        to HN-F
                    constexpr type      SnpRespDataPtl                  = DAT::HN::SnpRespDataPtl;      //        to HN-F
                    //===================================================================================================
                }

                // Opcodes permitted in HN-I.
                namespace I {

                    //===================================================================================================
                    constexpr type      DataLCrdReturn                  = DAT::HN::DataLCrdReturn;      //
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      CompData                        = DAT::HN::CompData;            // from & to HN-I
                    //---------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      DataSepResp                     = DAT::HN::DataSepResp;         // from & to HN-I
#endif
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      CopyBackWrData                  = DAT::HN::CopyBackWrData;      //        to HN-I
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      WriteDataCancel                 = DAT::HN::WriteDataCancel;     // from & to HN-I
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      NonCopyBackWrData               = DAT::HN::NonCopyBackWrData;   // from & to HN-I
                    //---------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      NCBWrDataCompAck                = DAT::HN::NCBWrDataCompAck;    //        to HN-I
#endif
                    //===================================================================================================
                }
            };

            /*
            Opcodes permitted in SN. See page B-360.
            */
            namespace SN {

                //=============================================================================================
                constexpr type          DataLCrdReturn              = DAT::DataLCrdReturn;      //
                //---------------------------------------------------------------------------------------------
                constexpr type          CompData                    = DAT::CompData;            // from      SN
                //---------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          DataSepResp                 = DAT::DataSepResp;         // from      SN
#endif
                //---------------------------------------------------------------------------------------------
                constexpr type          WriteDataCancel             = DAT::WriteDataCancel;     //        to SN
                //---------------------------------------------------------------------------------------------
                constexpr type          NonCopyBackWrData           = DAT::NonCopyBackWrData;   //        to SN
                //=============================================================================================

                // Opcodes permitted in SN-F.
                namespace F {

                    //===================================================================================================
                    constexpr type      DataLCrdReturn                  = DAT::SN::DataLCrdReturn;      //
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      CompData                        = DAT::SN::CompData;            // from      SN-F
                    //---------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      DataSepResp                     = DAT::SN::DataSepResp;         // from      SN-F
#endif
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      WriteDataCancel                 = DAT::SN::WriteDataCancel;     //        to SN-F
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      NonCopyBackWrData               = DAT::SN::NonCopyBackWrData;   //        to SN-F
                    //===================================================================================================
                }

                // Opcodes permitted in SN-I.
                namespace I {

                    //===================================================================================================
                    constexpr type      DataLCrdReturn                  = DAT::SN::DataLCrdReturn;      //
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      CompData                        = DAT::SN::CompData;            // from      SN-I
                    //---------------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      DataSepResp                     = DAT::SN::DataSepResp;         // from      SN-I
#endif
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      WriteDataCancel                 = DAT::SN::WriteDataCancel;     //        to SN-I
                    //---------------------------------------------------------------------------------------------------
                    constexpr type      NonCopyBackWrData               = DAT::SN::NonCopyBackWrData;   //        to SN-I
                    //===================================================================================================
                }
            };

            /*
            Opcodes permitted in MN. See page B-360.
            */
            namespace MN {

                //=============================================================================================
                constexpr type          DataLCrdReturn              = DAT::DataLCrdReturn;      //
                //---------------------------------------------------------------------------------------------
                constexpr type          NonCopyBackWrData           = DAT::NonCopyBackWrData;   //        to MN
                //=============================================================================================
            };
        };

        namespace RSP {

            /*
            See RSP channel opcodes on page 12-299.
            */

            // Opcode type
            using type = Flits::RSP<>::opcode_t;

            // Opcodes
            constexpr type              RespLCrdReturn          = 0x00;
            constexpr type              SnpResp                 = 0x01;
            constexpr type              CompAck                 = 0x02;
            constexpr type              RetryAck                = 0x03;
            constexpr type              Comp                    = 0x04;
            constexpr type              CompDBIDResp            = 0x05;
            constexpr type              DBIDResp                = 0x06;
            constexpr type              PCrdGrant               = 0x07;
            constexpr type              ReadReceipt             = 0x08;
            constexpr type              SnpRespFwded            = 0x09;
#ifdef CHI_ISSUE_EB_ENABLE
            constexpr type              TagMatch                = 0x0A;
            constexpr type              RespSepData             = 0x0B;
            constexpr type              Persist                 = 0x0C;
            constexpr type              CompPersist             = 0x0D;
            constexpr type              DBIDRespOrd             = 0x0E;
//          *Reserved*                                          = 0x0F;
            constexpr type              StashDone               = 0x10;
            constexpr type              CompStashDone           = 0x11;
//          *Reserved*                                          = 0x12;
//                                                            ... 0x13;
            constexpr type              CompCMO                 = 0x14;
//          *Reserved*                                          = 0x15;
//                                                            ... 0x1F;
#endif

            /*
            Opcodes permitted in RN. See page B-359.
            */
            namespace RN {

                //========================================================================================
                constexpr type          RespLCrdReturn          = RSP::RespLCrdReturn;      //
                //----------------------------------------------------------------------------------------
                constexpr type          RetryAck                = RSP::RetryAck;            //       to RN
                constexpr type          PCrdGrant               = RSP::PCrdGrant;           //       to RN
                constexpr type          Comp                    = RSP::Comp;                //       to RN
                constexpr type          CompDBIDResp            = RSP::CompDBIDResp;        //       to RN
                //----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          CompCMO                 = RSP::CompCMO;             //       to RN
#endif
                constexpr type          ReadReceipt             = RSP::ReadReceipt;         //       to RN
                //----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          RespSepData             = RSP::RespSepData;         //       to RN
#endif
                //----------------------------------------------------------------------------------------
                constexpr type          DBIDResp                = RSP::DBIDResp;            //       to RN
                //----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          DBIDRespOrd             = RSP::DBIDRespOrd;         //       to RN
#endif
                //----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          StashDone               = RSP::StashDone;           //       to RN
                constexpr type          CompStashDone           = RSP::CompStashDone;       //       to RN
#endif
                //----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          TagMatch                = RSP::TagMatch;            //       to RN
#endif
                //----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          Persist                 = RSP::Persist;             //       to RN
#endif
                //----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          CompPersist             = RSP::CompPersist;         //       to RN
#endif
                //----------------------------------------------------------------------------------------
                constexpr type          CompAck                 = RSP::CompAck;             // from     RN
                //----------------------------------------------------------------------------------------
                constexpr type          SnpResp                 = RSP::SnpResp;             // from     RN
                //----------------------------------------------------------------------------------------
                constexpr type          SnpRespFwded            = RSP::SnpRespFwded;        // from     RN
                //========================================================================================

                // Opcodes permitted in RN-F.
                namespace F {

                    //==========================================================================================
                    constexpr type          RespLCrdReturn          = RSP::RespLCrdReturn;      //
                    //------------------------------------------------------------------------------------------
                    constexpr type          RetryAck                = RSP::RetryAck;            //       to RN-F
                    constexpr type          PCrdGrant               = RSP::PCrdGrant;           //       to RN-F
                    constexpr type          Comp                    = RSP::Comp;                //       to RN-F
                    constexpr type          CompDBIDResp            = RSP::CompDBIDResp;        //       to RN-F
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          CompCMO                 = RSP::CompCMO;             //       to RN-F
#endif
                    constexpr type          ReadReceipt             = RSP::ReadReceipt;         //       to RN-F
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          RespSepData             = RSP::RespSepData;         //       to RN-F
#endif
                    //------------------------------------------------------------------------------------------
                    constexpr type          DBIDResp                = RSP::DBIDResp;            //       to RN-F
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          DBIDRespOrd             = RSP::DBIDRespOrd;         //       to RN-F
#endif
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          StashDone               = RSP::StashDone;           //       to RN-F
                    constexpr type          CompStashDone           = RSP::CompStashDone;       //       to RN-F
#endif
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          TagMatch                = RSP::TagMatch;            //       to RN-F
#endif
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          Persist                 = RSP::Persist;             //       to RN-F
#endif
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          CompPersist             = RSP::CompPersist;         //       to RN-F
#endif
                    //------------------------------------------------------------------------------------------
                    constexpr type          CompAck                 = RSP::CompAck;             // from     RN-F
                    //------------------------------------------------------------------------------------------
                    constexpr type          SnpResp                 = RSP::SnpResp;             // from     RN-F
                    //------------------------------------------------------------------------------------------
                    constexpr type          SnpRespFwded            = RSP::SnpRespFwded;        // from     RN-F
                    //==========================================================================================
                };

                // Opcodes permitted in RN-D.
                namespace D {

                    //==========================================================================================
                    constexpr type          RespLCrdReturn          = RSP::RespLCrdReturn;      //
                    //------------------------------------------------------------------------------------------
                    constexpr type          RetryAck                = RSP::RetryAck;            //       to RN-D
                    constexpr type          PCrdGrant               = RSP::PCrdGrant;           //       to RN-D
                    constexpr type          Comp                    = RSP::Comp;                //       to RN-D
                    constexpr type          CompDBIDResp            = RSP::CompDBIDResp;        //       to RN-D
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          CompCMO                 = RSP::CompCMO;             //       to RN-D
#endif
                    constexpr type          ReadReceipt             = RSP::ReadReceipt;         //       to RN-D
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          RespSepData             = RSP::RespSepData;         //       to RN-D
#endif
                    //------------------------------------------------------------------------------------------
                    constexpr type          DBIDResp                = RSP::DBIDResp;            //       to RN-D
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          DBIDRespOrd             = RSP::DBIDRespOrd;         //       to RN-D
#endif
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          StashDone               = RSP::StashDone;           //       to RN-D
                    constexpr type          CompStashDone           = RSP::CompStashDone;       //       to RN-D
#endif
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          TagMatch                = RSP::TagMatch;            //       to RN-D
#endif
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          Persist                 = RSP::Persist;             //       to RN-D
#endif
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          CompPersist             = RSP::CompPersist;         //       to RN-D
#endif
                    //------------------------------------------------------------------------------------------
                    constexpr type          CompAck                 = RSP::CompAck;             // from     RN-D
                    //------------------------------------------------------------------------------------------
                    constexpr type          SnpResp                 = RSP::SnpResp;             // from     RN-D
                    //==========================================================================================
                };

                // Opcodes permitted in RN-I.
                namespace I {

                    //==========================================================================================
                    constexpr type          RespLCrdReturn          = RSP::RespLCrdReturn;      //
                    //------------------------------------------------------------------------------------------
                    constexpr type          RetryAck                = RSP::RetryAck;            //       to RN-I
                    constexpr type          PCrdGrant               = RSP::PCrdGrant;           //       to RN-I
                    constexpr type          Comp                    = RSP::Comp;                //       to RN-I
                    constexpr type          CompDBIDResp            = RSP::CompDBIDResp;        //       to RN-I
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          CompCMO                 = RSP::CompCMO;             //       to RN-I
#endif
                    constexpr type          ReadReceipt             = RSP::ReadReceipt;         //       to RN-I
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          RespSepData             = RSP::RespSepData;         //       to RN-I
#endif
                    //------------------------------------------------------------------------------------------
                    constexpr type          DBIDResp                = RSP::DBIDResp;            //       to RN-I
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          DBIDRespOrd             = RSP::DBIDRespOrd;         //       to RN-I
#endif
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          StashDone               = RSP::StashDone;           //       to RN-I
                    constexpr type          CompStashDone           = RSP::CompStashDone;       //       to RN-I
#endif
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          TagMatch                = RSP::TagMatch;            //       to RN-I
#endif
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          Persist                 = RSP::Persist;             //       to RN-I
#endif
                    //------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type          CompPersist             = RSP::CompPersist;         //       to RN-I
#endif
                    //------------------------------------------------------------------------------------------
                    constexpr type          CompAck                 = RSP::CompAck;             // from     RN-I
                    //==========================================================================================
                };
            };

            /*
            Opcodes permitted in HN. See page B-359.
            */
            namespace HN {

                //=========================================================================================
                constexpr type          RespLCrdReturn          = RSP::RespLCrdReturn;      //
                //-----------------------------------------------------------------------------------------
                constexpr type          RetryAck                = RSP::RetryAck;            // from & to HN
                constexpr type          PCrdGrant               = RSP::PCrdGrant;           // from & to HN
                constexpr type          Comp                    = RSP::Comp;                // from & to HN
                constexpr type          CompDBIDResp            = RSP::CompDBIDResp;        // from & to HN
                //-----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          CompCMO                 = RSP::CompCMO;             // from & to HN
#endif
                constexpr type          ReadReceipt             = RSP::ReadReceipt;         // from & to HN
                //-----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          RespSepData             = RSP::RespSepData;         // from & to HN
#endif
                //-----------------------------------------------------------------------------------------
                constexpr type          DBIDResp                = RSP::DBIDResp;            // from & to HN
                //-----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          DBIDRespOrd             = RSP::DBIDRespOrd;         // from      HN
#endif
                //-----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          StashDone               = RSP::StashDone;           // from      HN
                constexpr type          CompStashDone           = RSP::CompStashDone;       // from      HN
#endif
                //-----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          TagMatch                = RSP::TagMatch;            // from & to HN
#endif
                //-----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          Persist                 = RSP::Persist;             // from & to HN
#endif
                //-----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          CompPersist             = RSP::CompPersist;         // from & to HN
#endif
                //-----------------------------------------------------------------------------------------
                constexpr type          CompAck                 = RSP::CompAck;             //        to HN
                //-----------------------------------------------------------------------------------------
                constexpr type          SnpResp                 = RSP::SnpResp;             //        to HN
                //-----------------------------------------------------------------------------------------
                constexpr type          SnpRespFwded            = RSP::SnpRespFwded;        //        to HN
                //=========================================================================================

                // Opcodes permitted in HN-F.
                namespace F {

                    //===============================================================================================
                    constexpr type      RespLCrdReturn              = RSP::HN::RespLCrdReturn;      //
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      RetryAck                    = RSP::HN::RetryAck;            // from & to HN-F
                    constexpr type      PCrdGrant                   = RSP::HN::PCrdGrant;           // from & to HN-F
                    constexpr type      Comp                        = RSP::HN::Comp;                // from & to HN-F
                    constexpr type      CompDBIDResp                = RSP::HN::CompDBIDResp;        // from & to HN-F
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      CompCMO                     = RSP::HN::CompCMO;             // from & to HN-F
#endif
                    constexpr type      ReadReceipt                 = RSP::HN::ReadReceipt;         // from & to HN-F
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      RespSepData                 = RSP::HN::RespSepData;         // from      HN-F
#endif
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      DBIDResp                    = RSP::HN::DBIDResp;            // from & to HN-F
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      DBIDRespOrd                 = RSP::HN::DBIDRespOrd;         // from      HN-F
#endif
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      StashDone                   = RSP::HN::StashDone;           // from      HN-F
                    constexpr type      CompStashDone               = RSP::HN::CompStashDone;       // from      HN-F
#endif
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      TagMatch                    = RSP::HN::TagMatch;            // from & to HN-F
#endif
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      Persist                     = RSP::HN::Persist;             // from & to HN-F
#endif
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      CompPersist                 = RSP::HN::CompPersist;         // from & to HN-F
#endif
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      CompAck                     = RSP::HN::CompAck;             //        to HN-F
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      SnpResp                     = RSP::HN::SnpResp;             //        to HN-F
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      SnpRespFwded                = RSP::HN::SnpRespFwded;        //        to HN-F
                    //===============================================================================================
                };

                // Opcodes permitted in HN-I.
                namespace I {

                    //===============================================================================================
                    constexpr type      RespLCrdReturn              = RSP::HN::RespLCrdReturn;      //
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      RetryAck                    = RSP::HN::RetryAck;            // from & to HN-I
                    constexpr type      PCrdGrant                   = RSP::HN::PCrdGrant;           // from & to HN-I
                    constexpr type      Comp                        = RSP::HN::Comp;                // from & to HN-I
                    constexpr type      CompDBIDResp                = RSP::HN::CompDBIDResp;        // from & to HN-I
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      CompCMO                     = RSP::HN::CompCMO;             // from & to HN-I
#endif
                    constexpr type      ReadReceipt                 = RSP::HN::ReadReceipt;         // from & to HN-I
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      RespSepData                 = RSP::HN::RespSepData;         // from      HN-I
#endif
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      DBIDResp                    = RSP::HN::DBIDResp;            // from & to HN-I
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      DBIDRespOrd                 = RSP::HN::DBIDRespOrd;         // from      HN-I
#endif
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      Persist                     = RSP::HN::Persist;             // from & to HN-I
#endif
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      CompPersist                 = RSP::HN::CompPersist;         // from & to HN-I
#endif
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      CompAck                     = RSP::HN::CompAck;             //        to HN-I
                    //===============================================================================================
                };
            };

            /*
            Opcodes permitted in SN. See page B-359.
            */
            namespace SN {

                //=========================================================================================
                constexpr type          RespLCrdReturn          = RSP::RespLCrdReturn;      //
                //-----------------------------------------------------------------------------------------
                constexpr type          RetryAck                = RSP::RetryAck;            // from      SN
                constexpr type          PCrdGrant               = RSP::PCrdGrant;           // from      SN
                constexpr type          Comp                    = RSP::Comp;                // from      SN
                constexpr type          CompDBIDResp            = RSP::CompDBIDResp;        // from      SN
                //-----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          CompCMO                 = RSP::CompCMO;             // from      SN
#endif
                constexpr type          ReadReceipt             = RSP::ReadReceipt;         // from      SN
                //-----------------------------------------------------------------------------------------
                constexpr type          DBIDResp                = RSP::DBIDResp;            // from      SN
                //-----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          TagMatch                = RSP::TagMatch;            // from      SN
#endif
                //-----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          Persist                 = RSP::Persist;             // from      SN
#endif
                //-----------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                constexpr type          CompPersist             = RSP::CompPersist;         // from      SN
#endif
                //=========================================================================================

                // Opcodes permitted in SN-F.
                namespace F {

                    //===============================================================================================
                    constexpr type      RespLCrdReturn              = RSP::SN::RespLCrdReturn;      //
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      RetryAck                    = RSP::SN::RetryAck;            // from      SN-F
                    constexpr type      PCrdGrant                   = RSP::SN::PCrdGrant;           // from      SN-F
                    constexpr type      Comp                        = RSP::SN::Comp;                // from      SN-F
                    constexpr type      CompDBIDResp                = RSP::SN::CompDBIDResp;        // from      SN-F
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      CompCMO                     = RSP::SN::CompCMO;             // from      SN-F
#endif
                    constexpr type      ReadReceipt                 = RSP::SN::ReadReceipt;         // from      SN-F
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      DBIDResp                    = RSP::SN::DBIDResp;            // from      SN-F
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      TagMatch                    = RSP::SN::TagMatch;            // from      SN-F
#endif
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      Persist                     = RSP::SN::Persist;             // from      SN-F
#endif
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      CompPersist                 = RSP::SN::CompPersist;         // from      SN-F
#endif
                    //===============================================================================================

                };

                // Opcodes permitted in SN-I.
                namespace I {

                    //===============================================================================================
                    constexpr type      RespLCrdReturn              = RSP::SN::RespLCrdReturn;      //
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      RetryAck                    = RSP::SN::RetryAck;            // from      SN-I
                    constexpr type      PCrdGrant                   = RSP::SN::PCrdGrant;           // from      SN-I
                    constexpr type      Comp                        = RSP::SN::Comp;                // from      SN-I
                    constexpr type      CompDBIDResp                = RSP::SN::CompDBIDResp;        // from      SN-I
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      CompCMO                     = RSP::SN::CompCMO;             // from      SN-I
#endif
                    constexpr type      ReadReceipt                 = RSP::SN::ReadReceipt;         // from      SN-I
                    //-----------------------------------------------------------------------------------------------
                    constexpr type      DBIDResp                    = RSP::SN::DBIDResp;            // from      SN-I
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      Persist                     = RSP::SN::Persist;             // from      SN-I
#endif
                    //-----------------------------------------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
                    constexpr type      CompPersist                 = RSP::SN::CompPersist;         // from      SN-I
#endif
                    //===============================================================================================
                };
            };

            /*
            Opcodes permitted in MN. See page B-359.
            */
            namespace MN {

                //=========================================================================================
                constexpr type          RespLCrdReturn          = RSP::RespLCrdReturn;      //
                //-----------------------------------------------------------------------------------------
                constexpr type          RetryAck                = RSP::RetryAck;            // from      MN
                constexpr type          PCrdGrant               = RSP::PCrdGrant;           // from      MN
                constexpr type          Comp                    = RSP::Comp;                // from      MN
                constexpr type          CompDBIDResp            = RSP::CompDBIDResp;        // from      MN
                //-----------------------------------------------------------------------------------------
                constexpr type          DBIDResp                = RSP::DBIDResp;            // from      MN
                //-----------------------------------------------------------------------------------------
                constexpr type          SnpResp                 = RSP::SnpResp;             //        to MN
                //=========================================================================================
            };
        };
    }


    //
    template<unsigned I>
    struct Size {
        static constexpr Flits::REQ<>::ssize_t value {/*BRACE required by GCC 11 on C++20, GCC is Wrong!*/};
    };

    template<> struct Size<1    > { static constexpr Flits::REQ<>::ssize_t value = 0b000; };
    template<> struct Size<2    > { static constexpr Flits::REQ<>::ssize_t value = 0b001; };
    template<> struct Size<4    > { static constexpr Flits::REQ<>::ssize_t value = 0b010; };
    template<> struct Size<8    > { static constexpr Flits::REQ<>::ssize_t value = 0b011; };
    template<> struct Size<16   > { static constexpr Flits::REQ<>::ssize_t value = 0b100; };
    template<> struct Size<32   > { static constexpr Flits::REQ<>::ssize_t value = 0b101; };
    template<> struct Size<64   > { static constexpr Flits::REQ<>::ssize_t value = 0b110; };
//  *Reserved*                                                                   = 0b111;


    namespace NS {

        /*
        Non-secure.
        */

        // NS type
        using type = uint1_t;

        // NS encodings
        constexpr type  SecureAccess        = 0b0;
        constexpr type  NonSecureAccess     = 0b1;
    };


    //
    namespace MemAttr {

        /*
        Memory Attributes.
        */

        // MemAttr type
        using type = uint4_t;

        // MemAttr fields
        constexpr type  EWA             = 0b0001;       // Early Write Acknowledge bit.
        constexpr type  Device          = 0b0010;       // Device bit.
        constexpr type  Cacheable       = 0b0100;       // Cacheable bit.
        constexpr type  Allocate        = 0b1000;       // Allocate hint bit.


        // MemAttr field encodings
        constexpr type  EWAPermitted            = 0b0001;
        constexpr type  EWANotPermitted         = 0b0000;

        constexpr type  DeviceMemory            = 0b0010;
        constexpr type  NormalMemory            = 0b0000;

        constexpr type  CacheableMemory         = 0b0100;
        constexpr type  NonCacheableMemory      = 0b0000;

        constexpr type  RecommendToAllocate     = 0b1000;
        constexpr type  RecommendToNotAllocate  = 0b0000;


        // MemAttr helpers
        inline constexpr type ExtractEWA(type memattr)          { return memattr & EWA; }
        inline constexpr type ExtractDevice(type memattr)       { return memattr & Device; }
        inline constexpr type ExtractCacheable(type memattr)    { return memattr & Cacheable; }
        inline constexpr type ExtractAllocate(type memattr)     { return memattr & Allocate; }


        //
        static_assert(is_same_len_v<type, Flits::REQ<>::memattr_t>);
    };


    // 
    namespace SnpAttr {

        // SnpAttr type
        using type = uint1_t;

        // SnpAttr field encodings
        constexpr type  NonSnoopable    = 0b0;
        constexpr type  Snoopable       = 0b1;

        //
        static_assert(is_same_len_v<type, Flits::REQ<>::snpattr_t>);
    };


    //
    namespace Endian {

        // Endian type
        using type = uint1_t;

        // Endian field encodings
        constexpr type  LittleEndian    = 0b0;
        constexpr type  BigEndian       = 0b1;

        //
        static_assert(is_same_len_v<type, Flits::REQ<>::endian_t>);
    };

    
    //
    namespace Order {

        // Order type
        using type = uint2_t;

        // Order field encodings
        constexpr type  NoOrdering          = 0b00;
        constexpr type  RequestAccepted     = 0b01;
        constexpr type  RequestOrder        = 0b10;
        constexpr type  EndpointOrder       = 0b11;

        //
        static_assert(is_same_len_v<type, Flits::REQ<>::order_t>);
    };


    //
    namespace RespErr {

        // RespErr type
        using type = uint2_t;

        // RespErr field encodings
        constexpr type  OK                  = 0b00;     // Normal OK
        constexpr type  EXOK                = 0b01;     // Exclusive OK
        constexpr type  DERR                = 0b10;     // Data Error
        constexpr type  NDERR               = 0b11;     // Non-data Error

        //
        static_assert(is_same_len_v<type, Flits::DAT<>::resperr_t>);
        static_assert(is_same_len_v<type, Flits::RSP<>::resperr_t>);
    };


    //
    namespace Resp {

        // Resp type
        using type = uint3_t;

        // Resp fields
        constexpr type PassDirty                        = 0b100;

        // Resp field encodings     
        // TODO: better classification and constraint expressions
        //======================================================
        constexpr type  CompData_I                      = 0b000;
#ifdef CHI_ISSUE_EB_ENABLE
        constexpr type  DataSepResp_I                   = 0b000;
#endif
        //------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
        constexpr type  RespSepData_I                   = 0b000;
#endif
        //------------------------------------------------------
        constexpr type  CompData_UC                     = 0b010;
#ifdef CHI_ISSUE_EB_ENABLE
        constexpr type  DataSepResp_UC                  = 0b010;
        constexpr type  RespSepData_UC                  = 0b010;
#endif
        //------------------------------------------------------
        constexpr type  CompData_SC                     = 0b001;
#ifdef CHI_ISSUE_EB_ENABLE
        constexpr type  DataSepResp_SC                  = 0b001;
        constexpr type  RespSepData_SC                  = 0b001;
#endif
        //------------------------------------------------------
        constexpr type  CompData_UD_PD                  = 0b110;
#ifdef CHI_ISSUE_EB_ENABLE
        constexpr type  DataSepResp_UD_PD               = 0b110;
        constexpr type  RespSepData_UD_PD               = 0b110;
#endif
        //------------------------------------------------------
        constexpr type  CompData_SD_PD                  = 0b111;
        //======================================================

        //======================================================
        constexpr type  Comp_I                          = 0b000;
        //------------------------------------------------------
        constexpr type  Comp_UC                         = 0b010;
        //------------------------------------------------------
        constexpr type  Comp_SC                         = 0b001;
        //------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
        constexpr type  Comp_UD_PD                      = 0b110;
#endif
        //======================================================

        //======================================================
        constexpr type  CopyBackWrData_I                = 0b000;    // DAT Opcode = 0x2
        //------------------------------------------------------
        constexpr type  CopyBackWrData_UC               = 0b010;    // DAT Opcode = 0x2
        //------------------------------------------------------
        constexpr type  CopyBackWrData_SC               = 0b001;    // DAT Opcode = 0x2
        //------------------------------------------------------
        constexpr type  CopyBackWrData_UD_PD            = 0b110;    // DAT Opcode = 0x2
        //------------------------------------------------------
        constexpr type  CopyBackWrData_SD_PD            = 0b111;    // DAT Opcode = 0x2
        //------------------------------------------------------
        constexpr type  NonCopyBackWrData               = 0b000;    // DAT Opcode = 0x3
        //------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
        constexpr type  NCBWrDataCompAck                = 0b000;    // DAT opcode = 0xC
        //------------------------------------------------------
        constexpr type  WriteDataCancel                 = 0b000;    // DAT opcode = 0x7
#endif
        //======================================================

        //======================================================
        constexpr type  SnpResp_I                       = 0b000;    // RSP Opcode = 0x1
        //------------------------------------------------------
        constexpr type  SnpResp_UC                      = 0b010;    // RSP Opcode = 0x1
        //------------------------------------------------------
        constexpr type  SnpResp_SC                      = 0b001;    // RSP Opcode = 0x1
        //------------------------------------------------------
        constexpr type  SnpResp_UD                      = 0b010;    // RSP Opcode = 0x1
        //------------------------------------------------------
        constexpr type  SnpResp_SD                      = 0b011;    // RSP Opcode = 0x1
        //======================================================

        //======================================================
        constexpr type  SnpResp_I_Fwded_I               = 0b000;    // RSP Opcode = 0x9, FwdState = 0b000
        //------------------------------------------------------
        constexpr type  SnpResp_I_Fwded_SC              = 0b000;    // RSP Opcode = 0x9, FwdState = 0b001
        //------------------------------------------------------
        constexpr type  SnpResp_I_Fwded_UC              = 0b000;    // RSP Opcode = 0x9, FwdState = 0b010
        //------------------------------------------------------
        constexpr type  SnpResp_I_Fwded_UD_PD           = 0b000;    // RSP Opcode = 0x9, FwdState = 0b110
        //------------------------------------------------------
        constexpr type  SnpResp_I_Fwded_SD_PD           = 0b000;    // RSP Opcode = 0x9, FwdState = 0b111
        //------------------------------------------------------
        constexpr type  SnpResp_SC_Fwded_I              = 0b001;    // RSP Opcode = 0x9, FwdState = 0b000
        //------------------------------------------------------
        constexpr type  SnpResp_SC_Fwded_SC             = 0b001;    // RSP Opcode = 0x9, FwdState = 0b001
        //------------------------------------------------------
        constexpr type  SnpResp_SC_Fwded_SD_PD          = 0b001;    // RSP Opcode = 0x9, FwdState = 0b111
        //------------------------------------------------------
        constexpr type  SnpResp_UC_Fwded_I              = 0b010;    // RSP Opcode = 0x9, FwdState = 0b000
        //------------------------------------------------------
        constexpr type  SnpResp_UD_Fwded_I              = 0b010;    // RSP Opcode = 0x9, FwdState = 0b000
        //------------------------------------------------------
        constexpr type  SnpResp_SD_Fwded_I              = 0b011;    // RSP Opcode = 0x9, FwdState = 0b000
        //------------------------------------------------------
        constexpr type  SnpResp_SD_Fwded_SC             = 0b011;    // RSP Opcode = 0x9, FwdState = 0b001
        //======================================================

        //======================================================
        constexpr type  SnpRespData_I                   = 0b000;    // DAT Opcode = 0x1
        //------------------------------------------------------
        constexpr type  SnpRespData_UC                  = 0b010;    // DAT Opcode = 0x1
        constexpr type  SnpRespData_UD                  = 0b010;    // DAT Opcode = 0x1
        //------------------------------------------------------
        constexpr type  SnpRespData_SC                  = 0b001;    // DAT Opcode = 0x1
        //------------------------------------------------------
        constexpr type  SnpRespData_SD                  = 0b011;    // DAT Opcode = 0x1
        //------------------------------------------------------
        constexpr type  SnpRespData_I_PD                = 0b100;    // DAT Opcode = 0x1
        //------------------------------------------------------
        constexpr type  SnpRespData_UC_PD               = 0b110;    // DAT Opcode = 0x1
        //------------------------------------------------------
        constexpr type  SnpRespData_SC_PD               = 0b101;    // DAT Opcode = 0x1
        //------------------------------------------------------
        constexpr type  SnpRespDataPtl_I_PD             = 0b100;    // DAT Opcode = 0x5
        //------------------------------------------------------
        constexpr type  SnpRespDataPtl_UD               = 0b010;    // DAT Opcode = 0x5
        //======================================================

        //======================================================
        constexpr type  SnpRespData_I_Fwded_SC          = 0b000;    // DAT Opcode = 0x6, FwdState = 0b001
        //------------------------------------------------------
        constexpr type  SnpRespData_I_Fwded_SD_PD       = 0b000;    // DAT Opcode = 0x6, FwdState = 0b111
        //------------------------------------------------------
        constexpr type  SnpRespData_SC_Fwded_SC         = 0b001;    // DAT Opcode = 0x6, FwdState = 0b001
        //------------------------------------------------------
        constexpr type  SnpRespData_SC_Fwded_SD_PD      = 0b001;    // DAT Opcode = 0x6, FwdState = 0b111
        //------------------------------------------------------
        constexpr type  SnpRespData_SD_Fwded_SC         = 0b011;    // DAT Opcode = 0x6, FwdState = 0b001
        //------------------------------------------------------
        constexpr type  SnpRespData_I_PD_Fwded_I        = 0b100;    // DAT Opcode = 0x6, FwdState = 0b000
        //------------------------------------------------------
        constexpr type  SnpRespData_I_PD_Fwded_SC       = 0b100;    // DAT Opcode = 0x6, FwdState = 0b001
        //------------------------------------------------------
        constexpr type  SnpRespData_SC_PD_Fwded_I       = 0b101;    // DAT Opcode = 0x6, FwdState = 0b000
        //------------------------------------------------------
        constexpr type  SnpRespData_SC_PD_Fwded_SC      = 0b101;    // DAT Opcode = 0x6, FwdState = 0b001
        //======================================================

        //
        static_assert(is_same_len_v<type, Flits::DAT<>::resp_t>);
        static_assert(is_same_len_v<type, Flits::RSP<>::resp_t>);
    };


    //
    namespace FwdState {

        // FwdState type
        using type = uint3_t;

        // FwdState field masks
        constexpr type  MASK_PassDity           = 0b100;
        constexpr type  MASK_FinalState         = 0b011;

        // FwdState field encodings
        constexpr type  PassDirty_NotDirty      = 0b100;
        constexpr type  PassDirty_Dirty         = 0b000;

        constexpr type  I                       = 0b000;
        constexpr type  SC                      = 0b001;
        constexpr type  UC                      = 0b010;
//      *Reserved*                              = 0b011;
//      *Reserved*                              = 0b100;
//      *Reserved*                              = 0b101;
        constexpr type  UD_PD                   = 0b110;
        constexpr type  SD_PD                   = 0b111;

        //
        static_assert(is_same_len_v<type, Flits::DAT<>::fwdstate_t>);
        static_assert(is_same_len_v<type, Flits::RSP<>::fwdstate_t>);
    };


    //
#ifdef CHI_ISSUE_B_ENABLE
    namespace DataPull {

        // DataPull type
        using type = uint3_t;

        // DataPull field encodings
        constexpr type  NoRead          = 0b000;
        constexpr type  Read            = 0b001;
//      *Reserved*                      = 0b010;
//                                    ... 0b111;

        //
        static_assert(is_same_len_v<type, Flits::DAT<>::datapull_t>);
        static_assert(is_same_len_v<type, Flits::RSP<>::datapull_t>);
    };
#endif


    //
#ifdef CHI_ISSUE_EB_ENABLE
    namespace TagOp {

        // TagOp type
        using type = uint2_t;

        // TagOp field encodings
        constexpr type  Invalid         = 0b00;
        constexpr type  Transfer        = 0b01;
        constexpr type  Update          = 0b10;
        constexpr type  MatchFetch      = 0b11;

        //
        static_assert(is_same_len_v<type, Flits::DAT<>::tagop_t>);
        static_assert(is_same_len_v<type, Flits::RSP<>::tagop_t>);
        static_assert(is_same_len_v<type, Flits::DAT<>::tagop_t>);
    };
#endif

/*
};
*/


#endif // __CHI__CHI_PROTOCOL_ENCODING_*

//#endif // __CHI__CHI_PROTOCOL_ENCODING
