#pragma once

#ifndef __CCHI__CCHI_PROTOCOL_ENCODING
#define __CCHI__CCHI_PROTOCOL_ENCODING

#include <climits>
#include <array>

#include "cchi_protocol_flits.hpp"


namespace CCHI {

    namespace Opcodes::REQ {

        /*
        See REQ channel opcodes.
        */

        // Opcode type
        using type = Flits::REQ<>::opcode_t;

        // Opcodes
        constexpr type      StashShared                 = 0x00;
        constexpr type      StashUnique                 = 0x01;
        constexpr type      ReadNoSnp                   = 0x02;
        constexpr type      ReadOnce                    = 0x03;
        constexpr type      ReadShared                  = 0x04;
    //  *Reserved*                                      = 0x05;
    //  *Reserved*                                      = 0x06;
    //  *Reserved*                                      = 0x07;
        constexpr type      WriteNoSnpPtl               = 0x08;
        constexpr type      WriteNoSnpFull              = 0x09;
        constexpr type      WriteUniquePtl              = 0x0A;
        constexpr type      WriteUniqueFull             = 0x0B;
        constexpr type      CleanShared                 = 0x0C;
        constexpr type      CleanInvalid                = 0x0D;
        constexpr type      MakeInvalid                 = 0x0E;
    //  *Reserved*                                      = 0x0F;
        constexpr type      ReadUnique                  = 0x10;
    //  *Reserved*                                      = 0x11;
        constexpr type      MakeUnique                  = 0x12;
    //  *Reserved*                                      = 0x13;
    //  *Reserved*                                      = 0x14;
    //  *Reserved*                                      = 0x15;
    //  *Reserved*                                      = 0x16;
    //  *Reserved*                                      = 0x17;
    //  *Reserved*                                      = 0x18;
    //  *Reserved*                                      = 0x19;
    //  *Reserved*                                      = 0x1A;
    //  *Reserved*                                      = 0x1B;
    //  *Reserved*                                      = 0x1C;
    //  *Reserved*                                      = 0x1D;
        constexpr type      EvictBack                   = 0x1E;
        constexpr type      EvictClean                  = 0x1F;
    //  AtomicLoad                                      = 0x20;
    //                                                ... 0x27;
    //  AtomicStore                                     = 0x28;
    //                                                ... 0x2F;
        constexpr type      AtomicSwap                  = 0x30;
        constexpr type      AtomicCompare               = 0x31;
    //                                                  = 0x32;
    //                                                ... 0x3F;
        
        namespace AtomicStore {

#           define SubAtomicStore(code) 0b100##code
            constexpr type  ADD                     = SubAtomicStore(000);
            constexpr type  CLR                     = SubAtomicStore(001);
            constexpr type  EOR                     = SubAtomicStore(010);
            constexpr type  SET                     = SubAtomicStore(011);
            constexpr type  SMAX                    = SubAtomicStore(100);
            constexpr type  SMIN                    = SubAtomicStore(101);
            constexpr type  UMAX                    = SubAtomicStore(110);
            constexpr type  UMIN                    = SubAtomicStore(111);
#           undef SubAtomicStore

            inline constexpr bool Is(type val) { return (val & 0b111000) == 0b100000; }
        }

        namespace AtomicLoad {

#           define SubAtomicLoad(code) 0b101##code
            constexpr type  ADD                     = SubAtomicLoad(000);
            constexpr type  CLR                     = SubAtomicLoad(001);
            constexpr type  EOR                     = SubAtomicLoad(010);
            constexpr type  SET                     = SubAtomicLoad(011);
            constexpr type  SMAX                    = SubAtomicLoad(100);
            constexpr type  SMIN                    = SubAtomicLoad(101);
            constexpr type  UMAX                    = SubAtomicLoad(110);
            constexpr type  UMIN                    = SubAtomicLoad(111);
#           undef SubAtomicLoad

            inline constexpr bool Is(type val) { return (val & 0b111000) == 0b101000; }
        }

        /*
        Opcodes applicable for Type 1 components.
        */
        namespace Type1 {

            //==============================================================
            constexpr type  StashShared                 = REQ::StashShared;
            constexpr type  StashUnique                 = REQ::StashUnique;
            //--------------------------------------------------------------
            constexpr type  ReadNoSnp                   = REQ::ReadNoSnp;
            constexpr type  ReadOnce                    = REQ::ReadOnce;
            //--------------------------------------------------------------
            constexpr type  ReadShared                  = REQ::ReadShared;
            //--------------------------------------------------------------
            constexpr type  WriteNoSnpPtl               = REQ::WriteNoSnpPtl;
            constexpr type  WriteNoSnpFull              = REQ::WriteNoSnpFull;
            constexpr type  WriteUniquePtl              = REQ::WriteUniquePtl;
            constexpr type  WriteUniqueFull             = REQ::WriteUniqueFull;
            constexpr type  CleanShared                 = REQ::CleanShared;
            constexpr type  CleanInvalid                = REQ::CleanInvalid;
            constexpr type  MakeInvalid                 = REQ::MakeInvalid;
            //--------------------------------------------------------------
            constexpr type  ReadUnique                  = REQ::ReadUnique;
            constexpr type  MakeUnique                  = REQ::MakeUnique; 
            //--------------------------------------------------------------
            constexpr type  EvictBack                   = REQ::EvictBack;
            constexpr type  EvictClean                  = REQ::EvictClean;
            //--------------------------------------------------------------
            namespace       AtomicLoad                  = REQ::AtomicLoad;  // NOLINT: misc-unused-alias-decls
            namespace       AtomicStore                 = REQ::AtomicStore; // NOLINT: misc-unused-alias-decls
            constexpr type  AtomicSwap                  = REQ::AtomicSwap;
            constexpr type  AtomicCompare               = REQ::AtomicCompare;
            //==============================================================
        }

        /*
        Opcodes applicable for Type 2 components.
        */
        namespace Type2 {

            //==============================================================
            constexpr type  StashShared                 = REQ::StashShared;
            constexpr type  StashUnique                 = REQ::StashUnique;
            //--------------------------------------------------------------
            constexpr type  ReadNoSnp                   = REQ::ReadNoSnp;
            constexpr type  ReadOnce                    = REQ::ReadOnce;
            //--------------------------------------------------------------
            constexpr type  ReadShared                  = REQ::ReadShared;
            //==============================================================
        }

        /*
        Opcodes applicable for Type 3 components.
        */
        namespace Type3 {

            //==============================================================
            constexpr type  StashShared                 = REQ::StashShared;
            constexpr type  StashUnique                 = REQ::StashUnique;
            //--------------------------------------------------------------
            constexpr type  ReadNoSnp                   = REQ::ReadNoSnp;
            constexpr type  ReadOnce                    = REQ::ReadOnce;
            //--------------------------------------------------------------
            constexpr type  WriteNoSnpPtl               = REQ::WriteNoSnpPtl;
            constexpr type  WriteNoSnpFull              = REQ::WriteNoSnpFull;
            constexpr type  WriteUniquePtl              = REQ::WriteUniquePtl;
            constexpr type  WriteUniqueFull             = REQ::WriteUniqueFull;
            constexpr type  CleanShared                 = REQ::CleanShared;
            constexpr type  CleanInvalid                = REQ::CleanInvalid;
            constexpr type  MakeInvalid                 = REQ::MakeInvalid;
            //==============================================================

            // *NOTICE: For Type 3 components, CMO operations were possible to be supported with mitigation
            //          that consider CompStash as CompCMO on CMO transactions, since the CMO transactions
            //          often prefer to be fully tracked.
            //          And this could also be applied to Type 5 components if expanding Opcode field width.
        }

        /*
        Opcodes applicable for Type 4 components.
        */
        namespace Type4 {

            //==============================================================
            constexpr type  StashShared                 = REQ::StashShared;
            constexpr type  StashUnique                 = REQ::StashUnique;
            //--------------------------------------------------------------
            constexpr type  ReadNoSnp                   = REQ::ReadNoSnp;
            constexpr type  ReadOnce                    = REQ::ReadOnce;
            //==============================================================
        }

        /*
        Opcodes applicable for Type 5 components.
        */
        namespace Type5 {

            //==============================================================
            constexpr type  StashShared                 = REQ::StashShared;
            constexpr type  StashUnique                 = REQ::StashUnique;
            //==============================================================
        }
    }


    namespace Opcodes::SNP {

        /*
        See SNP channel opcodes.
        */

        // Opcode type
        using type = Flits::SNP<>::opcode_t;

        // Opcodes
        constexpr type      SnpMakeInvalid              = 0x00;
        constexpr type      SnpToInvalid                = 0x01;
        constexpr type      SnpToShared                 = 0x02;
        constexpr type      SnpToClean                  = 0x03;

        /*
        Opcodes applicable for Type 1 components.
        */
        namespace Type1 {

            //==============================================================
            constexpr type  SnpMakeInvalid              = SNP::SnpMakeInvalid;
            constexpr type  SnpToInvalid                = SNP::SnpToInvalid;
            constexpr type  SnpToShared                 = SNP::SnpToShared;
            constexpr type  SnpToClean                  = SNP::SnpToClean;
            //==============================================================
        }

        /*
        Opcodes applicable for Type 2 components.
        */
        namespace Type2 {

            //==============================================================
            constexpr type  SnpMakeInvalid              = SNP::SnpMakeInvalid;
            //==============================================================
        }
    }


    namespace Opcodes::EVT {

        /*
        See EVT channel opcodes.
        */

        // Opcode type
        using type = Flits::EVT<>::opcode_t;

        // Opcodes
        constexpr type      Evict                       = 0x00;
        constexpr type      WriteBackFull               = 0x01;

        /*
        Opcodes applicable for Type 1 components.
        */
        namespace Type1 {
            
            //==============================================================
            constexpr type  Evict                       = EVT::Evict;
            constexpr type  WriteBackFull               = EVT::WriteBackFull;
            //==============================================================
        }

        /*
        Opcodes applicable for Type 2 components.
        */
        namespace Type2 {

            //==============================================================
            constexpr type  Evict                       = EVT::Evict;
            //==============================================================
        }
    }

    namespace Opcodes::DnRSP {

        /*
        See DnRSP channel opcodes.
        */

        // Opcode type
        using type = Flits::DnRSP<>::opcode_t;

        // Opcodes
        constexpr type      CompStash                   = 0x00;
        constexpr type      Comp                        = 0x01;
        constexpr type      DBIDResp                    = 0x02;
        constexpr type      CompDBIDResp                = 0x03;
        constexpr type      CompCMO                     = 0x04;
    //  *Reserved*          DBIDRespOrd                 = 0x05;
    //  *Reserved*                                      = 0x06;
    //  *Reserved*                                      = 0x07;

        /*
        Opcodes applicable for Type 1 components.
        */
        namespace Type1 {

            //==============================================================
            constexpr type  CompStash                   = DnRSP::CompStash;
            constexpr type  Comp                        = DnRSP::Comp;
            constexpr type  DBIDResp                    = DnRSP::DBIDResp;
            constexpr type  CompDBIDResp                = DnRSP::CompDBIDResp;
            //--------------------------------------------------------------
            constexpr type  CompCMO                     = DnRSP::CompCMO;
            //==============================================================
        }

        /*
        Opcodes applicable for Type 3 components.
        */
        namespace Type3 {

            //==============================================================
            constexpr type  CompStash                   = DnRSP::CompStash;
            constexpr type  Comp                        = DnRSP::Comp;
            constexpr type  DBIDResp                    = DnRSP::DBIDResp;
            constexpr type  CompDBIDResp                = DnRSP::CompDBIDResp;
            //==============================================================
        }

        /*
        Opcodes applicable for Type 5 components.
        */
        namespace Type5 {

            //==============================================================
            constexpr type  CompStash                   = DnRSP::CompStash;
            //==============================================================
        }
    }

    namespace Opcodes::UpRSP {

        /*
        See UpRSP channel opcodes.
        */

        // Opcode type
        using type = Flits::UpRSP<>::opcode_t;

        // Opcodes
        constexpr type      CompAck                     = 0x00;
        constexpr type      SnpResp                     = 0x01;

        /*
        Opcode applicable for Type 1 components.
        */
        namespace Type1 {

            //==============================================================
            constexpr type  CompAck                     = 0x00;
            constexpr type  SnpResp                     = 0x01;
            //==============================================================
        }

        /*
        Opcode applicable for Type 2 components.
        */
        namespace Type2 {

            //==============================================================
            constexpr type  CompAck                     = 0x00;
            constexpr type  SnpResp                     = 0x01;
            //==============================================================
        }

        /*
        Opcode applicable for Type 3 components.
        */
        namespace Type3 {

            //==============================================================
            constexpr type  CompAck                     = 0x00;
            //==============================================================
        }
    }

    namespace Opcodes::DnDAT {

        /*
        See DnDAT channel opcodes.
        */

        // Opcode type
        using type = Flits::DnDAT<>::opcode_t;

        // Opcodes
        constexpr type      CompData                    = 0x00;
    //  *Reserved*                                      = 0x01;

        /*
        Opcode applicable for Type 1 components.
        */
        namespace Type1 {

            //==============================================================
            constexpr type  CompData                    = 0x00;
            //==============================================================
        }

        /*
        Opcode applicable for Type 2 components.
        */
        namespace Type2 {

            //==============================================================
            constexpr type  CompData                    = 0x00;
            //==============================================================
        }

        /*
        Opcode applicable for Type 3 components.
        */
        namespace Type3 {

            //==============================================================
            constexpr type  CompData                    = 0x00;
            //==============================================================
        }

        /*
        Opcode applicable for Type 4 components.
        */
        namespace Type4 {

            //==============================================================
            constexpr type  CompData                    = 0x00;
            //==============================================================
        }
    }

    namespace Opcodes::UpDAT {

        /*
        See UpDAT channel opcodes.
        */

        // Opcode type
        using type = Flits::UpDAT<>::opcode_t;

        // Opcodes
        constexpr type      NonCopyBackWrData           = 0x00;
    //  *Reserved*                                      = 0x01;
        constexpr type      CopyBackWrData              = 0x02;
        constexpr type      SnpRespData                 = 0x03;

        /*
        Opcode applicable for Type 1 components.
        */
        namespace Type1 {

            //==============================================================
            constexpr type  NonCopyBackWrData           = UpDAT::NonCopyBackWrData;
            constexpr type  CopyBackWrData              = UpDAT::CopyBackWrData;
            constexpr type  SnpRespData                 = UpDAT::SnpRespData;
            //==============================================================
        }
        
        /*
        Opcode applicable for Type 3 components.
        */
        namespace Type3 {

            //==============================================================
            constexpr type  NonCopyBackWrData           = UpDAT::NonCopyBackWrData;
            //==============================================================
        }
    }


    namespace cchi_protocol_encoding::details {

        template<class T>
        class EnumerationUnsaturated {
        public:
            const char*     name;
            const int       value;
            const T* const  prev;

        public:
            inline constexpr EnumerationUnsaturated(const char* name, const int value, const T* const prev = nullptr) noexcept
            : name(name), value(value), prev(prev) { }

            inline constexpr operator int() const noexcept
            { return value; }

            inline constexpr operator const T*() const noexcept
            { return static_cast<const T*>(this); };

            inline constexpr bool operator==(const T& obj) const noexcept
            { return value == obj.value; }

            inline constexpr bool operator!=(const T& obj) const noexcept
            { return !(*this == obj); }

            inline constexpr bool IsValid() const noexcept
            { return value != INT_MIN; }
        };
    }


    //
    using Size = uint3_t;

    class SizeEnumBack : public cchi_protocol_encoding::details::EnumerationUnsaturated<SizeEnumBack> {
    public:
        inline constexpr SizeEnumBack(const char* name, const int value, const SizeEnumBack* const prev = nullptr) noexcept
        : EnumerationUnsaturated<SizeEnumBack>(name, value, prev) { }
    };

    using SizeEnum = const SizeEnumBack*;

    namespace Sizes {

        // Size type
        using type = Size;

        // Size encodings
        inline constexpr type   B1      = 0b000;
        inline constexpr type   B2      = 0b001;
        inline constexpr type   B4      = 0b010;
        inline constexpr type   B8      = 0b011;
        inline constexpr type   B16     = 0b100;
        inline constexpr type   B32     = 0b101;
        inline constexpr type   B64     = 0b110;

        //
        namespace Enum {
            inline constexpr SizeEnumBack   Invalid ("Invalid",  INT_MIN);

            inline constexpr SizeEnumBack   B1      ("1 Byte"   , Sizes::B1);
            inline constexpr SizeEnumBack   B2      ("2 Bytes"  , Sizes::B2 , B1);
            inline constexpr SizeEnumBack   B4      ("4 Bytes"  , Sizes::B4 , B2);
            inline constexpr SizeEnumBack   B8      ("8 Bytes"  , Sizes::B8 , B4);
            inline constexpr SizeEnumBack   B16     ("16 Bytes" , Sizes::B16, B8);
            inline constexpr SizeEnumBack   B32     ("32 Bytes" , Sizes::B32, B16);
            inline constexpr SizeEnumBack   B64     ("64 Bytes" , Sizes::B64, B32);
        }

        inline constexpr SizeEnum ToEnum(Size size) noexcept;
        inline constexpr bool IsValid(Size size) noexcept;
    }


    //
    using Resp = uint3_t;

    class RespEnumBack : public cchi_protocol_encoding::details::EnumerationUnsaturated<RespEnumBack> {
    private:
        const bool isPD;

    public:
        inline constexpr RespEnumBack(const char* name, const int value, const bool isPD, const RespEnumBack* const prev = nullptr) noexcept
        : EnumerationUnsaturated<RespEnumBack>(name, value, prev), isPD(isPD) { }

    public:
        inline constexpr bool IsPD() const noexcept { return isPD; }
    };

    using RespEnum = const RespEnumBack*;

    namespace Resps {

        // Resp type
        using type = Resp;

        // Resp encodings
        inline constexpr type   I       = 0b000;
        inline constexpr type   SC      = 0b001;
        inline constexpr type   UC      = 0b010;
    //                                  = 0b011;
        inline constexpr type   I_PD    = 0b100;
        inline constexpr type   SC_PD   = 0b101;
        inline constexpr type   UC_PD   = 0b110;
    //                                  = 0b111;

        //
        namespace Enum {
            inline constexpr RespEnumBack   Invalid ("Invalid",  INT_MIN, false);

            inline constexpr RespEnumBack   I       ("I"      , Resps::I     , false);
            inline constexpr RespEnumBack   SC      ("SC"     , Resps::SC    , false, I);
            inline constexpr RespEnumBack   UC      ("UC"     , Resps::UC    , false, SC);
            inline constexpr RespEnumBack   I_PD    ("I_PD"   , Resps::I_PD  , true , UC);
            inline constexpr RespEnumBack   SC_PD   ("SC_PD"  , Resps::SC_PD , true , I_PD);
            inline constexpr RespEnumBack   UC_PD   ("UC_PD"  , Resps::UC_PD , true , SC_PD);
        }

        inline constexpr RespEnum ToEnum(Resp resp) noexcept;
        inline constexpr bool IsValid(Resp resp) noexcept;
    }
}


// Implementation of enumeration table constevals
namespace CCHI::cchi_protocol_encoding::details {

    template<class T, size_t Bits>
    requires std::is_convertible_v<const T*, const EnumerationUnsaturated<T>*>
    inline consteval std::array<const T*, 1 << Bits> NextElement(const T* E, std::array<const T*, 1 << Bits> A) noexcept
    {
        A[E->value] = E;

        if (!E->prev)
            return A;
        else
            return NextElement<T, Bits>(*E->prev, A);
    }

    template<class T, size_t Bits, const T* First, const T* Invalid>
    requires std::is_convertible_v<const T*, const EnumerationUnsaturated<T>*>
    inline consteval std::array<const T*, 1 << Bits> GetTable() noexcept
    {
        std::array<const T*, 1 << Bits> A;
        for (auto& E : A)
            E = Invalid;

        return NextElement<T, Bits>(First, A);
    }
}


// Implementation of Sizes enumeration functions
namespace CCHI::Sizes {

    namespace Enum::details {
        inline constexpr std::array<SizeEnum, 1 << Size::BITS> TABLE =
            cchi_protocol_encoding::details::GetTable<SizeEnumBack, Size::BITS, Enum::B1, Enum::Invalid>();
    }

    inline constexpr SizeEnum ToEnum(Size size) noexcept
    {
        return Enum::details::TABLE[size];
    }

    inline constexpr bool IsValid(Size size) noexcept
    {
        return ToEnum(size)->IsValid();
    }
}


// Implementation of Resps enumeration functions
namespace CCHI::Resps {

    namespace Enum::details {
        inline constexpr std::array<RespEnum, 1 << Resp::BITS> TABLE =
            cchi_protocol_encoding::details::GetTable<RespEnumBack, Resp::BITS, Enum::I, Enum::Invalid>();
    }

    inline constexpr RespEnum ToEnum(Resp resp) noexcept
    {
        return Enum::details::TABLE[resp];
    }

    inline constexpr bool IsValid(Resp resp) noexcept
    {
        return ToEnum(resp)->IsValid();
    }
}


#endif // __CCHI__CCHI_PROTOCOL_ENCODING
