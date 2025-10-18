#pragma once
//
// Non-standard truncated integer templates
//
//

#include <concepts>
#include <type_traits>

#ifndef __BULLSEYE_UTILITY__NONSTDINT
#define __BULLSEYE_UTILITY__NONSTDINT


#include <cstdint>


/*
namespace BullsEye {
    */

    //
    typedef     unsigned char       uchar;
    typedef     unsigned short      ushort;
    typedef     unsigned int        uint;
    typedef     unsigned long       ulong;
    typedef     unsigned long long  ulonglong;


    //
    template<unsigned _Val, unsigned _Llsh>
    concept TL8 =   _Val > 0 && _Val <= 8   && _Llsh >= 0 && _Llsh < 8  && (_Val + _Llsh) <= 8;

    template<unsigned _Val, unsigned _Llsh>
    concept TL16 =  _Val > 0 && _Val <= 16  && _Llsh >= 0 && _Llsh < 16 && (_Val + _Llsh) <= 16;

    template<unsigned _Val, unsigned _Llsh>
    concept TL32 =  _Val > 0 && _Val <= 32  && _Llsh >= 0 && _Llsh < 32 && (_Val + _Llsh) <= 32;

    template<unsigned _Val, unsigned _Llsh>
    concept TL64 =  _Val > 0 && _Val <= 64  && _Llsh >= 0 && _Llsh < 64 && (_Val + _Llsh) <= 64;


    /*
    *   _Tsv : Type of signed value holder
    *   _Tuv : Type of unsigned value holder
    *   _L   : Applicating length of the truncated integer
    *   _Llsh: The lowest applicatable bit shift on value holder
    *   _R   : Whether applying reduced operation (strict bit width) on value holder
    */    
    //
    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh = 0, bool _R = false>
    struct _truncated_int_base;
    
    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh = 0, bool _R = false>
    struct _truncated_uint_base;


    //
    template<unsigned _Len, unsigned _Llsh = 0, bool _R = false> requires TL8<_Len, _Llsh>
    using truncated_int8_t      = _truncated_int_base<int8_t, uint8_t, _Len, _Llsh, _R>;

    template<unsigned _Len, unsigned _Llsh = 0, bool _R = false> requires TL8<_Len, _Llsh>
    using truncated_uint8_t     = _truncated_uint_base<int8_t, uint8_t, _Len, _Llsh, _R>;

    template<unsigned _Len, unsigned _Llsh = 0, bool _R = false> requires TL16<_Len, _Llsh>
    using truncated_int16_t     = _truncated_int_base<int16_t, uint16_t, _Len, _Llsh, _R>;

    template<unsigned _Len, unsigned _Llsh = 0, bool _R = false> requires TL16<_Len, _Llsh>
    using truncated_uint16_t    = _truncated_uint_base<int16_t, uint16_t, _Len, _Llsh, _R>;

    template<unsigned _Len, unsigned _Llsh = 0, bool _R = false> requires TL32<_Len, _Llsh>
    using truncated_int32_t     = _truncated_int_base<int32_t, uint32_t, _Len, _Llsh, _R>;

    template<unsigned _Len, unsigned _Llsh = 0, bool _R = false> requires TL32<_Len, _Llsh>
    using truncated_uint32_t    = _truncated_uint_base<int32_t, uint32_t, _Len, _Llsh, _R>;

    template<unsigned _Len, unsigned _Llsh = 0, bool _R = false> requires TL64<_Len, _Llsh>
    using truncated_int64_t     = _truncated_int_base<int64_t, uint64_t, _Len, _Llsh, _R>;

    template<unsigned _Len, unsigned _Llsh = 0, bool _R = false> requires TL64<_Len, _Llsh>
    using truncated_uint64_t    = _truncated_uint_base<int64_t, uint64_t, _Len, _Llsh, _R>;

    //
    using uint1_t               = truncated_uint8_t<1>;
    using uint2_t               = truncated_uint8_t<2>;
    using uint3_t               = truncated_uint8_t<3>;
    using uint4_t               = truncated_uint8_t<4>;
    using uint5_t               = truncated_uint8_t<5>;
    using uint6_t               = truncated_uint8_t<6>;
    using uint7_t               = truncated_uint8_t<7>;

    using uint9_t               = truncated_uint16_t<9>;
    using uint10_t              = truncated_uint16_t<10>;
    using uint11_t              = truncated_uint16_t<11>;
    using uint12_t              = truncated_uint16_t<12>;
    using uint13_t              = truncated_uint16_t<13>;
    using uint14_t              = truncated_uint16_t<14>;
    using uint15_t              = truncated_uint16_t<15>;

    using uint17_t              = truncated_uint32_t<17>;
    using uint18_t              = truncated_uint32_t<18>;
    using uint19_t              = truncated_uint32_t<19>;
    using uint20_t              = truncated_uint32_t<20>;
    using uint21_t              = truncated_uint32_t<21>;
    using uint22_t              = truncated_uint32_t<22>;
    using uint23_t              = truncated_uint32_t<23>;
    using uint24_t              = truncated_uint32_t<24>;
    using uint25_t              = truncated_uint32_t<25>;
    using uint26_t              = truncated_uint32_t<26>;
    using uint27_t              = truncated_uint32_t<27>;
    using uint28_t              = truncated_uint32_t<28>;
    using uint29_t              = truncated_uint32_t<29>;
    using uint30_t              = truncated_uint32_t<30>;
    using uint31_t              = truncated_uint32_t<31>;

    using uint33_t              = truncated_uint64_t<33>;
    using uint34_t              = truncated_uint64_t<34>;
    using uint35_t              = truncated_uint64_t<35>;
    using uint36_t              = truncated_uint64_t<36>;
    using uint37_t              = truncated_uint64_t<37>;
    using uint38_t              = truncated_uint64_t<38>;
    using uint39_t              = truncated_uint64_t<39>;
    using uint40_t              = truncated_uint64_t<40>;
    using uint41_t              = truncated_uint64_t<41>;
    using uint42_t              = truncated_uint64_t<42>;
    using uint43_t              = truncated_uint64_t<43>;
    using uint44_t              = truncated_uint64_t<44>;
    using uint45_t              = truncated_uint64_t<45>;
    using uint46_t              = truncated_uint64_t<46>;
    using uint47_t              = truncated_uint64_t<47>;
    using uint48_t              = truncated_uint64_t<48>;
    using uint49_t              = truncated_uint64_t<49>;
    using uint50_t              = truncated_uint64_t<50>;
    using uint51_t              = truncated_uint64_t<51>;
    using uint52_t              = truncated_uint64_t<52>;
    using uint53_t              = truncated_uint64_t<53>;
    using uint54_t              = truncated_uint64_t<54>;
    using uint55_t              = truncated_uint64_t<55>;
    using uint56_t              = truncated_uint64_t<56>;
    using uint57_t              = truncated_uint64_t<57>;
    using uint58_t              = truncated_uint64_t<58>;
    using uint59_t              = truncated_uint64_t<59>;
    using uint60_t              = truncated_uint64_t<60>;
    using uint61_t              = truncated_uint64_t<61>;
    using uint62_t              = truncated_uint64_t<62>;
    using uint63_t              = truncated_uint64_t<63>;

    //
    template<unsigned _Len, typename _TNA = void>
    struct uint_fit { using type = _TNA; };

    template<unsigned _Len, typename _TNA = void>
    using uint_fit_t = typename uint_fit<_Len, _TNA>::type;

    template<typename _TNA> struct uint_fit<1 , _TNA>    { using type = uint1_t; };
    template<typename _TNA> struct uint_fit<2 , _TNA>    { using type = uint2_t; };
    template<typename _TNA> struct uint_fit<3 , _TNA>    { using type = uint3_t; };
    template<typename _TNA> struct uint_fit<4 , _TNA>    { using type = uint4_t; };
    template<typename _TNA> struct uint_fit<5 , _TNA>    { using type = uint5_t; };
    template<typename _TNA> struct uint_fit<6 , _TNA>    { using type = uint6_t; };
    template<typename _TNA> struct uint_fit<7 , _TNA>    { using type = uint7_t; };
    template<typename _TNA> struct uint_fit<8 , _TNA>    { using type = uint8_t; };

    template<typename _TNA> struct uint_fit<9 , _TNA>    { using type = uint9_t; };
    template<typename _TNA> struct uint_fit<10, _TNA>   { using type = uint10_t; };
    template<typename _TNA> struct uint_fit<11, _TNA>   { using type = uint11_t; };
    template<typename _TNA> struct uint_fit<12, _TNA>   { using type = uint12_t; };
    template<typename _TNA> struct uint_fit<13, _TNA>   { using type = uint13_t; };
    template<typename _TNA> struct uint_fit<14, _TNA>   { using type = uint14_t; };
    template<typename _TNA> struct uint_fit<15, _TNA>   { using type = uint15_t; };
    template<typename _TNA> struct uint_fit<16, _TNA>   { using type = uint16_t; };

    template<typename _TNA> struct uint_fit<17, _TNA>   { using type = uint17_t; };
    template<typename _TNA> struct uint_fit<18, _TNA>   { using type = uint18_t; };
    template<typename _TNA> struct uint_fit<19, _TNA>   { using type = uint19_t; };
    template<typename _TNA> struct uint_fit<20, _TNA>   { using type = uint20_t; };
    template<typename _TNA> struct uint_fit<21, _TNA>   { using type = uint21_t; };
    template<typename _TNA> struct uint_fit<22, _TNA>   { using type = uint22_t; };
    template<typename _TNA> struct uint_fit<23, _TNA>   { using type = uint23_t; };
    template<typename _TNA> struct uint_fit<24, _TNA>   { using type = uint24_t; };
    template<typename _TNA> struct uint_fit<25, _TNA>   { using type = uint25_t; };
    template<typename _TNA> struct uint_fit<26, _TNA>   { using type = uint26_t; };
    template<typename _TNA> struct uint_fit<27, _TNA>   { using type = uint27_t; };
    template<typename _TNA> struct uint_fit<28, _TNA>   { using type = uint28_t; };
    template<typename _TNA> struct uint_fit<29, _TNA>   { using type = uint29_t; };
    template<typename _TNA> struct uint_fit<30, _TNA>   { using type = uint30_t; };
    template<typename _TNA> struct uint_fit<31, _TNA>   { using type = uint31_t; };
    template<typename _TNA> struct uint_fit<32, _TNA>   { using type = uint32_t; };

    template<typename _TNA> struct uint_fit<33, _TNA>   { using type = uint33_t; };
    template<typename _TNA> struct uint_fit<34, _TNA>   { using type = uint34_t; };
    template<typename _TNA> struct uint_fit<35, _TNA>   { using type = uint35_t; };
    template<typename _TNA> struct uint_fit<36, _TNA>   { using type = uint36_t; };
    template<typename _TNA> struct uint_fit<37, _TNA>   { using type = uint37_t; };
    template<typename _TNA> struct uint_fit<38, _TNA>   { using type = uint38_t; };
    template<typename _TNA> struct uint_fit<39, _TNA>   { using type = uint39_t; };
    template<typename _TNA> struct uint_fit<40, _TNA>   { using type = uint40_t; };
    template<typename _TNA> struct uint_fit<41, _TNA>   { using type = uint41_t; };
    template<typename _TNA> struct uint_fit<42, _TNA>   { using type = uint42_t; };
    template<typename _TNA> struct uint_fit<43, _TNA>   { using type = uint43_t; };
    template<typename _TNA> struct uint_fit<44, _TNA>   { using type = uint44_t; };
    template<typename _TNA> struct uint_fit<45, _TNA>   { using type = uint45_t; };
    template<typename _TNA> struct uint_fit<46, _TNA>   { using type = uint46_t; };
    template<typename _TNA> struct uint_fit<47, _TNA>   { using type = uint47_t; };
    template<typename _TNA> struct uint_fit<48, _TNA>   { using type = uint48_t; };
    template<typename _TNA> struct uint_fit<49, _TNA>   { using type = uint49_t; };
    template<typename _TNA> struct uint_fit<50, _TNA>   { using type = uint50_t; };
    template<typename _TNA> struct uint_fit<51, _TNA>   { using type = uint51_t; };
    template<typename _TNA> struct uint_fit<52, _TNA>   { using type = uint52_t; };
    template<typename _TNA> struct uint_fit<53, _TNA>   { using type = uint53_t; };
    template<typename _TNA> struct uint_fit<54, _TNA>   { using type = uint54_t; };
    template<typename _TNA> struct uint_fit<55, _TNA>   { using type = uint55_t; };
    template<typename _TNA> struct uint_fit<56, _TNA>   { using type = uint56_t; };
    template<typename _TNA> struct uint_fit<57, _TNA>   { using type = uint57_t; };
    template<typename _TNA> struct uint_fit<58, _TNA>   { using type = uint58_t; };
    template<typename _TNA> struct uint_fit<59, _TNA>   { using type = uint59_t; };
    template<typename _TNA> struct uint_fit<60, _TNA>   { using type = uint60_t; };
    template<typename _TNA> struct uint_fit<61, _TNA>   { using type = uint61_t; };
    template<typename _TNA> struct uint_fit<62, _TNA>   { using type = uint62_t; };
    template<typename _TNA> struct uint_fit<63, _TNA>   { using type = uint63_t; };
    template<typename _TNA> struct uint_fit<64, _TNA>   { using type = uint64_t; };

    //
    template<unsigned _Llsh = 0> using uint1_strict_t   = truncated_uint8_t<1, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint2_strict_t   = truncated_uint8_t<2, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint3_strict_t   = truncated_uint8_t<3, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint4_strict_t   = truncated_uint8_t<4, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint5_strict_t   = truncated_uint8_t<5, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint6_strict_t   = truncated_uint8_t<6, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint7_strict_t   = truncated_uint8_t<7, _Llsh, true>;
    template<unsigned _Llsh = 0> requires TL8<8, _Llsh> using uint8_strict_t   = uint8_t;

    template<unsigned _Llsh = 0> using uint9_strict_t   = truncated_uint16_t<9, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint10_strict_t  = truncated_uint16_t<10, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint11_strict_t  = truncated_uint16_t<11, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint12_strict_t  = truncated_uint16_t<12, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint13_strict_t  = truncated_uint16_t<13, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint14_strict_t  = truncated_uint16_t<14, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint15_strict_t  = truncated_uint16_t<15, _Llsh, true>;
    template<unsigned _Llsh = 0> requires TL16<16, _Llsh> using uint16_strict_t = uint16_t;

    template<unsigned _Llsh = 0> using uint17_strict_t  = truncated_uint32_t<17, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint18_strict_t  = truncated_uint32_t<18, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint19_strict_t  = truncated_uint32_t<19, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint20_strict_t  = truncated_uint32_t<20, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint21_strict_t  = truncated_uint32_t<21, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint22_strict_t  = truncated_uint32_t<22, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint23_strict_t  = truncated_uint32_t<23, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint24_strict_t  = truncated_uint32_t<24, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint25_strict_t  = truncated_uint32_t<25, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint26_strict_t  = truncated_uint32_t<26, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint27_strict_t  = truncated_uint32_t<27, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint28_strict_t  = truncated_uint32_t<28, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint29_strict_t  = truncated_uint32_t<29, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint30_strict_t  = truncated_uint32_t<30, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint31_strict_t  = truncated_uint32_t<31, _Llsh, true>;
    template<unsigned _Llsh = 0> requires TL32<32, _Llsh> using uint32_strict_t  = uint32_t;

    template<unsigned _Llsh = 0> using uint33_strict_t  = truncated_uint64_t<33, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint34_strict_t  = truncated_uint64_t<34, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint35_strict_t  = truncated_uint64_t<35, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint36_strict_t  = truncated_uint64_t<36, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint37_strict_t  = truncated_uint64_t<37, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint38_strict_t  = truncated_uint64_t<38, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint39_strict_t  = truncated_uint64_t<39, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint40_strict_t  = truncated_uint64_t<40, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint41_strict_t  = truncated_uint64_t<41, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint42_strict_t  = truncated_uint64_t<42, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint43_strict_t  = truncated_uint64_t<43, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint44_strict_t  = truncated_uint64_t<44, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint45_strict_t  = truncated_uint64_t<45, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint46_strict_t  = truncated_uint64_t<46, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint47_strict_t  = truncated_uint64_t<47, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint48_strict_t  = truncated_uint64_t<48, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint49_strict_t  = truncated_uint64_t<49, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint50_strict_t  = truncated_uint64_t<50, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint51_strict_t  = truncated_uint64_t<51, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint52_strict_t  = truncated_uint64_t<52, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint53_strict_t  = truncated_uint64_t<53, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint54_strict_t  = truncated_uint64_t<54, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint55_strict_t  = truncated_uint64_t<55, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint56_strict_t  = truncated_uint64_t<56, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint57_strict_t  = truncated_uint64_t<57, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint58_strict_t  = truncated_uint64_t<58, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint59_strict_t  = truncated_uint64_t<59, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint60_strict_t  = truncated_uint64_t<60, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint61_strict_t  = truncated_uint64_t<61, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint62_strict_t  = truncated_uint64_t<62, _Llsh, true>;
    template<unsigned _Llsh = 0> using uint63_strict_t  = truncated_uint64_t<63, _Llsh, true>;
    template<unsigned _Llsh = 0> requires TL64<64, _Llsh> using uint64_strict_t  = uint64_t;

    //
    template<unsigned _L, unsigned _Llsh = 0>
    struct uint_strict { using type = void; };
    
    template<unsigned _L, unsigned _Llsh = 0>
    using uint_strict_t = typename uint_strict<_L, _Llsh>::type;

    template<unsigned _Llsh> struct uint_strict<1, _Llsh>       { using type = uint1_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<2, _Llsh>       { using type = uint2_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<3, _Llsh>       { using type = uint3_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<4, _Llsh>       { using type = uint4_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<5, _Llsh>       { using type = uint5_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<6, _Llsh>       { using type = uint6_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<7, _Llsh>       { using type = uint7_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<8, _Llsh>       { using type = uint8_strict_t<_Llsh>; };

    template<unsigned _Llsh> struct uint_strict<9, _Llsh>       { using type = uint9_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<10, _Llsh>      { using type = uint10_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<11, _Llsh>      { using type = uint11_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<12, _Llsh>      { using type = uint12_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<13, _Llsh>      { using type = uint13_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<14, _Llsh>      { using type = uint14_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<15, _Llsh>      { using type = uint15_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<16, _Llsh>      { using type = uint16_strict_t<_Llsh>; };

    template<unsigned _Llsh> struct uint_strict<17, _Llsh>      { using type = uint17_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<18, _Llsh>      { using type = uint18_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<19, _Llsh>      { using type = uint19_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<20, _Llsh>      { using type = uint20_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<21, _Llsh>      { using type = uint21_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<22, _Llsh>      { using type = uint22_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<23, _Llsh>      { using type = uint23_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<24, _Llsh>      { using type = uint24_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<25, _Llsh>      { using type = uint25_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<26, _Llsh>      { using type = uint26_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<27, _Llsh>      { using type = uint27_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<28, _Llsh>      { using type = uint28_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<29, _Llsh>      { using type = uint29_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<30, _Llsh>      { using type = uint30_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<31, _Llsh>      { using type = uint31_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<32, _Llsh>      { using type = uint32_strict_t<_Llsh>; };

    template<unsigned _Llsh> struct uint_strict<33, _Llsh>      { using type = uint33_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<34, _Llsh>      { using type = uint34_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<35, _Llsh>      { using type = uint35_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<36, _Llsh>      { using type = uint36_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<37, _Llsh>      { using type = uint37_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<38, _Llsh>      { using type = uint38_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<39, _Llsh>      { using type = uint39_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<40, _Llsh>      { using type = uint40_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<41, _Llsh>      { using type = uint41_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<42, _Llsh>      { using type = uint42_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<43, _Llsh>      { using type = uint43_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<44, _Llsh>      { using type = uint44_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<45, _Llsh>      { using type = uint45_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<46, _Llsh>      { using type = uint46_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<47, _Llsh>      { using type = uint47_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<48, _Llsh>      { using type = uint48_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<49, _Llsh>      { using type = uint49_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<50, _Llsh>      { using type = uint50_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<51, _Llsh>      { using type = uint51_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<52, _Llsh>      { using type = uint52_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<53, _Llsh>      { using type = uint53_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<54, _Llsh>      { using type = uint54_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<55, _Llsh>      { using type = uint55_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<56, _Llsh>      { using type = uint56_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<57, _Llsh>      { using type = uint57_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<58, _Llsh>      { using type = uint58_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<59, _Llsh>      { using type = uint59_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<60, _Llsh>      { using type = uint60_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<61, _Llsh>      { using type = uint61_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<62, _Llsh>      { using type = uint62_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<63, _Llsh>      { using type = uint63_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct uint_strict<64, _Llsh>      { using type = uint64_strict_t<_Llsh>; };

    //
    template<unsigned _L, unsigned _Llsh = 0>
    using uint8n_strict_t       = truncated_uint8_t<_L, _Llsh, true>;

    template<unsigned _L, unsigned _Llsh = 0>
    using uint16n_strict_t      = truncated_uint16_t<_L, _Llsh, true>;

    template<unsigned _L, unsigned _Llsh = 0>
    using uint32n_strict_t      = truncated_uint32_t<_L, _Llsh, true>;

    template<unsigned _L, unsigned _Llsh = 0>
    using uint64n_strict_t      = truncated_uint64_t<_L, _Llsh, true>;

    template<unsigned _VL, unsigned _L, unsigned _Llsh = 0>
    struct uintfitn_strict { using type = void; };

    template<unsigned _VL, unsigned _L, unsigned _Llsh = 0>
    using uintfitn_strict_t = typename uintfitn_strict<_VL, _L, _Llsh>::type;

    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<1, _L, _Llsh>  { using type = uint8n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<2, _L, _Llsh>  { using type = uint8n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<3, _L, _Llsh>  { using type = uint8n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<4, _L, _Llsh>  { using type = uint8n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<5, _L, _Llsh>  { using type = uint8n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<6, _L, _Llsh>  { using type = uint8n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<7, _L, _Llsh>  { using type = uint8n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<8, _L, _Llsh>  { using type = uint8n_strict_t<_L, _Llsh>; };

    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<9, _L, _Llsh>  { using type = uint16n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<10, _L, _Llsh> { using type = uint16n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<11, _L, _Llsh> { using type = uint16n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<12, _L, _Llsh> { using type = uint16n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<13, _L, _Llsh> { using type = uint16n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<14, _L, _Llsh> { using type = uint16n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<15, _L, _Llsh> { using type = uint16n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<16, _L, _Llsh> { using type = uint16n_strict_t<_L, _Llsh>; };

    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<17, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<18, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<19, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<20, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<21, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<22, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<23, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<24, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<25, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<26, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<27, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<28, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<29, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<30, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<31, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<32, _L, _Llsh> { using type = uint32n_strict_t<_L, _Llsh>; };

    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<33, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<34, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<35, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<36, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<37, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<38, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<39, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<40, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<41, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<42, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<43, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<44, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<45, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<46, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<47, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<48, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<49, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<50, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<51, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<52, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<53, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<54, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<55, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<56, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<57, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<58, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<59, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<60, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<61, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<62, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<63, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct uintfitn_strict<64, _L, _Llsh> { using type = uint64n_strict_t<_L, _Llsh>; };


    //
    using int1_t                = truncated_int8_t<1>;
    using int2_t                = truncated_int8_t<2>;
    using int3_t                = truncated_int8_t<3>;
    using int4_t                = truncated_int8_t<4>;
    using int5_t                = truncated_int8_t<5>;
    using int6_t                = truncated_int8_t<6>;
    using int7_t                = truncated_int8_t<7>;

    using int9_t                = truncated_int16_t<9>;
    using int10_t               = truncated_int16_t<10>;
    using int11_t               = truncated_int16_t<11>;
    using int12_t               = truncated_int16_t<12>;
    using int13_t               = truncated_int16_t<13>;
    using int14_t               = truncated_int16_t<14>;
    using int15_t               = truncated_int16_t<15>;
    
    using int17_t               = truncated_int32_t<17>;
    using int18_t               = truncated_int32_t<18>;
    using int19_t               = truncated_int32_t<19>;
    using int20_t               = truncated_int32_t<20>;
    using int21_t               = truncated_int32_t<21>;
    using int22_t               = truncated_int32_t<22>;
    using int23_t               = truncated_int32_t<23>;
    using int24_t               = truncated_int32_t<24>;
    using int25_t               = truncated_int32_t<25>;
    using int26_t               = truncated_int32_t<26>;
    using int27_t               = truncated_int32_t<27>;
    using int28_t               = truncated_int32_t<28>;
    using int29_t               = truncated_int32_t<29>;
    using int30_t               = truncated_int32_t<30>;
    using int31_t               = truncated_int32_t<31>;

    using int33_t               = truncated_int64_t<33>;
    using int34_t               = truncated_int64_t<34>;
    using int35_t               = truncated_int64_t<35>;
    using int36_t               = truncated_int64_t<36>;
    using int37_t               = truncated_int64_t<37>;
    using int38_t               = truncated_int64_t<38>;
    using int39_t               = truncated_int64_t<39>;
    using int40_t               = truncated_int64_t<40>;
    using int41_t               = truncated_int64_t<41>;
    using int42_t               = truncated_int64_t<42>;
    using int43_t               = truncated_int64_t<43>;
    using int44_t               = truncated_int64_t<44>;
    using int45_t               = truncated_int64_t<45>;
    using int46_t               = truncated_int64_t<46>;
    using int47_t               = truncated_int64_t<47>;
    using int48_t               = truncated_int64_t<48>;
    using int49_t               = truncated_int64_t<49>;
    using int50_t               = truncated_int64_t<50>;
    using int51_t               = truncated_int64_t<51>;
    using int52_t               = truncated_int64_t<52>;
    using int53_t               = truncated_int64_t<53>;
    using int54_t               = truncated_int64_t<54>;
    using int55_t               = truncated_int64_t<55>;
    using int56_t               = truncated_int64_t<56>;
    using int57_t               = truncated_int64_t<57>;
    using int58_t               = truncated_int64_t<58>;
    using int59_t               = truncated_int64_t<59>;
    using int60_t               = truncated_int64_t<60>;
    using int61_t               = truncated_int64_t<61>;
    using int62_t               = truncated_int64_t<62>;
    using int63_t               = truncated_int64_t<63>;

    //
    template<unsigned _Len, typename _TNA = void>
    struct int_fit { using type = _TNA; };

    template<unsigned _Len>
    using int_fit_t = typename int_fit<_Len>::type;

    template<typename _TNA> struct int_fit<1 , _TNA>    { using type = int1_t; };
    template<typename _TNA> struct int_fit<2 , _TNA>    { using type = int2_t; };
    template<typename _TNA> struct int_fit<3 , _TNA>    { using type = int3_t; };
    template<typename _TNA> struct int_fit<4 , _TNA>    { using type = int4_t; };
    template<typename _TNA> struct int_fit<5 , _TNA>    { using type = int5_t; };
    template<typename _TNA> struct int_fit<6 , _TNA>    { using type = int6_t; };
    template<typename _TNA> struct int_fit<7 , _TNA>    { using type = int7_t; };
    template<typename _TNA> struct int_fit<8 , _TNA>    { using type = int8_t; };

    template<typename _TNA> struct int_fit<9 , _TNA>    { using type = int9_t; };
    template<typename _TNA> struct int_fit<10, _TNA>    { using type = int10_t; };
    template<typename _TNA> struct int_fit<11, _TNA>    { using type = int11_t; };
    template<typename _TNA> struct int_fit<12, _TNA>    { using type = int12_t; };
    template<typename _TNA> struct int_fit<13, _TNA>    { using type = int13_t; };
    template<typename _TNA> struct int_fit<14, _TNA>    { using type = int14_t; };
    template<typename _TNA> struct int_fit<15, _TNA>    { using type = int15_t; };
    template<typename _TNA> struct int_fit<16, _TNA>    { using type = int16_t; };

    template<typename _TNA> struct int_fit<17, _TNA>    { using type = int17_t; };
    template<typename _TNA> struct int_fit<18, _TNA>    { using type = int18_t; };
    template<typename _TNA> struct int_fit<19, _TNA>    { using type = int19_t; };
    template<typename _TNA> struct int_fit<20, _TNA>    { using type = int20_t; };
    template<typename _TNA> struct int_fit<21, _TNA>    { using type = int21_t; };
    template<typename _TNA> struct int_fit<22, _TNA>    { using type = int22_t; };
    template<typename _TNA> struct int_fit<23, _TNA>    { using type = int23_t; };
    template<typename _TNA> struct int_fit<24, _TNA>    { using type = int24_t; };
    template<typename _TNA> struct int_fit<25, _TNA>    { using type = int25_t; };
    template<typename _TNA> struct int_fit<26, _TNA>    { using type = int26_t; };
    template<typename _TNA> struct int_fit<27, _TNA>    { using type = int27_t; };
    template<typename _TNA> struct int_fit<28, _TNA>    { using type = int28_t; };
    template<typename _TNA> struct int_fit<29, _TNA>    { using type = int29_t; };
    template<typename _TNA> struct int_fit<30, _TNA>    { using type = int30_t; };
    template<typename _TNA> struct int_fit<31, _TNA>    { using type = int31_t; };
    template<typename _TNA> struct int_fit<32, _TNA>    { using type = int32_t; };

    template<typename _TNA> struct int_fit<33, _TNA>    { using type = int33_t; };
    template<typename _TNA> struct int_fit<34, _TNA>    { using type = int34_t; };
    template<typename _TNA> struct int_fit<35, _TNA>    { using type = int35_t; };
    template<typename _TNA> struct int_fit<36, _TNA>    { using type = int36_t; };
    template<typename _TNA> struct int_fit<37, _TNA>    { using type = int37_t; };
    template<typename _TNA> struct int_fit<38, _TNA>    { using type = int38_t; };
    template<typename _TNA> struct int_fit<39, _TNA>    { using type = int39_t; };
    template<typename _TNA> struct int_fit<40, _TNA>    { using type = int40_t; };
    template<typename _TNA> struct int_fit<41, _TNA>    { using type = int41_t; };
    template<typename _TNA> struct int_fit<42, _TNA>    { using type = int42_t; };
    template<typename _TNA> struct int_fit<43, _TNA>    { using type = int43_t; };
    template<typename _TNA> struct int_fit<44, _TNA>    { using type = int44_t; };
    template<typename _TNA> struct int_fit<45, _TNA>    { using type = int45_t; };
    template<typename _TNA> struct int_fit<46, _TNA>    { using type = int46_t; };
    template<typename _TNA> struct int_fit<47, _TNA>    { using type = int47_t; };
    template<typename _TNA> struct int_fit<48, _TNA>    { using type = int48_t; };
    template<typename _TNA> struct int_fit<49, _TNA>    { using type = int49_t; };
    template<typename _TNA> struct int_fit<50, _TNA>    { using type = int50_t; };
    template<typename _TNA> struct int_fit<51, _TNA>    { using type = int51_t; };
    template<typename _TNA> struct int_fit<52, _TNA>    { using type = int52_t; };
    template<typename _TNA> struct int_fit<53, _TNA>    { using type = int53_t; };
    template<typename _TNA> struct int_fit<54, _TNA>    { using type = int54_t; };
    template<typename _TNA> struct int_fit<55, _TNA>    { using type = int55_t; };
    template<typename _TNA> struct int_fit<56, _TNA>    { using type = int56_t; };
    template<typename _TNA> struct int_fit<57, _TNA>    { using type = int57_t; };
    template<typename _TNA> struct int_fit<58, _TNA>    { using type = int58_t; };
    template<typename _TNA> struct int_fit<59, _TNA>    { using type = int59_t; };
    template<typename _TNA> struct int_fit<60, _TNA>    { using type = int60_t; };
    template<typename _TNA> struct int_fit<61, _TNA>    { using type = int61_t; };
    template<typename _TNA> struct int_fit<62, _TNA>    { using type = int62_t; };
    template<typename _TNA> struct int_fit<63, _TNA>    { using type = int63_t; };
    template<typename _TNA> struct int_fit<64, _TNA>    { using type = int64_t; };

    //
    template<unsigned _Llsh = 0> using int1_strict_t    = truncated_int8_t<1, _Llsh, true>;
    template<unsigned _Llsh = 0> using int2_strict_t    = truncated_int8_t<2, _Llsh, true>;
    template<unsigned _Llsh = 0> using int3_strict_t    = truncated_int8_t<3, _Llsh, true>;
    template<unsigned _Llsh = 0> using int4_strict_t    = truncated_int8_t<4, _Llsh, true>;
    template<unsigned _Llsh = 0> using int5_strict_t    = truncated_int8_t<5, _Llsh, true>;
    template<unsigned _Llsh = 0> using int6_strict_t    = truncated_int8_t<6, _Llsh, true>;
    template<unsigned _Llsh = 0> using int7_strict_t    = truncated_int8_t<7, _Llsh, true>;
    template<unsigned _Llsh = 0> requires TL8<8, _Llsh> using int8_strict_t = int8_t;

    template<unsigned _Llsh = 0> using int9_strict_t    = truncated_int16_t<9, _Llsh, true>;
    template<unsigned _Llsh = 0> using int10_strict_t   = truncated_int16_t<10, _Llsh, true>;
    template<unsigned _Llsh = 0> using int11_strict_t   = truncated_int16_t<11, _Llsh, true>;
    template<unsigned _Llsh = 0> using int12_strict_t   = truncated_int16_t<12, _Llsh, true>;
    template<unsigned _Llsh = 0> using int13_strict_t   = truncated_int16_t<13, _Llsh, true>;
    template<unsigned _Llsh = 0> using int14_strict_t   = truncated_int16_t<14, _Llsh, true>;
    template<unsigned _Llsh = 0> using int15_strict_t   = truncated_int16_t<15, _Llsh, true>;
    template<unsigned _Llsh = 0> requires TL16<16, _Llsh> using int16_strict_t = int16_t;

    template<unsigned _Llsh = 0> using int17_strict_t   = truncated_int32_t<17, _Llsh, true>;
    template<unsigned _Llsh = 0> using int18_strict_t   = truncated_int32_t<18, _Llsh, true>;
    template<unsigned _Llsh = 0> using int19_strict_t   = truncated_int32_t<19, _Llsh, true>;
    template<unsigned _Llsh = 0> using int20_strict_t   = truncated_int32_t<20, _Llsh, true>;
    template<unsigned _Llsh = 0> using int21_strict_t   = truncated_int32_t<21, _Llsh, true>;
    template<unsigned _Llsh = 0> using int22_strict_t   = truncated_int32_t<22, _Llsh, true>;
    template<unsigned _Llsh = 0> using int23_strict_t   = truncated_int32_t<23, _Llsh, true>;
    template<unsigned _Llsh = 0> using int24_strict_t   = truncated_int32_t<24, _Llsh, true>;
    template<unsigned _Llsh = 0> using int25_strict_t   = truncated_int32_t<25, _Llsh, true>;
    template<unsigned _Llsh = 0> using int26_strict_t   = truncated_int32_t<26, _Llsh, true>;
    template<unsigned _Llsh = 0> using int27_strict_t   = truncated_int32_t<27, _Llsh, true>;
    template<unsigned _Llsh = 0> using int28_strict_t   = truncated_int32_t<28, _Llsh, true>;
    template<unsigned _Llsh = 0> using int29_strict_t   = truncated_int32_t<29, _Llsh, true>;
    template<unsigned _Llsh = 0> using int30_strict_t   = truncated_int32_t<30, _Llsh, true>;
    template<unsigned _Llsh = 0> using int31_strict_t   = truncated_int32_t<31, _Llsh, true>;
    template<unsigned _Llsh = 0> requires TL32<32, _Llsh> using int32_strict_t = int32_t;

    template<unsigned _Llsh = 0> using int33_strict_t   = truncated_int64_t<33, _Llsh, true>;
    template<unsigned _Llsh = 0> using int34_strict_t   = truncated_int64_t<34, _Llsh, true>;
    template<unsigned _Llsh = 0> using int35_strict_t   = truncated_int64_t<35, _Llsh, true>;
    template<unsigned _Llsh = 0> using int36_strict_t   = truncated_int64_t<36, _Llsh, true>;
    template<unsigned _Llsh = 0> using int37_strict_t   = truncated_int64_t<37, _Llsh, true>;
    template<unsigned _Llsh = 0> using int38_strict_t   = truncated_int64_t<38, _Llsh, true>;
    template<unsigned _Llsh = 0> using int39_strict_t   = truncated_int64_t<39, _Llsh, true>;
    template<unsigned _Llsh = 0> using int40_strict_t   = truncated_int64_t<40, _Llsh, true>;
    template<unsigned _Llsh = 0> using int41_strict_t   = truncated_int64_t<41, _Llsh, true>;
    template<unsigned _Llsh = 0> using int42_strict_t   = truncated_int64_t<42, _Llsh, true>;
    template<unsigned _Llsh = 0> using int43_strict_t   = truncated_int64_t<43, _Llsh, true>;
    template<unsigned _Llsh = 0> using int44_strict_t   = truncated_int64_t<44, _Llsh, true>;
    template<unsigned _Llsh = 0> using int45_strict_t   = truncated_int64_t<45, _Llsh, true>;
    template<unsigned _Llsh = 0> using int46_strict_t   = truncated_int64_t<46, _Llsh, true>;
    template<unsigned _Llsh = 0> using int47_strict_t   = truncated_int64_t<47, _Llsh, true>;
    template<unsigned _Llsh = 0> using int48_strict_t   = truncated_int64_t<48, _Llsh, true>;
    template<unsigned _Llsh = 0> using int49_strict_t   = truncated_int64_t<49, _Llsh, true>;
    template<unsigned _Llsh = 0> using int50_strict_t   = truncated_int64_t<50, _Llsh, true>;
    template<unsigned _Llsh = 0> using int51_strict_t   = truncated_int64_t<51, _Llsh, true>;
    template<unsigned _Llsh = 0> using int52_strict_t   = truncated_int64_t<52, _Llsh, true>;
    template<unsigned _Llsh = 0> using int53_strict_t   = truncated_int64_t<53, _Llsh, true>;
    template<unsigned _Llsh = 0> using int54_strict_t   = truncated_int64_t<54, _Llsh, true>;
    template<unsigned _Llsh = 0> using int55_strict_t   = truncated_int64_t<55, _Llsh, true>;
    template<unsigned _Llsh = 0> using int56_strict_t   = truncated_int64_t<56, _Llsh, true>;
    template<unsigned _Llsh = 0> using int57_strict_t   = truncated_int64_t<57, _Llsh, true>;
    template<unsigned _Llsh = 0> using int58_strict_t   = truncated_int64_t<58, _Llsh, true>;
    template<unsigned _Llsh = 0> using int59_strict_t   = truncated_int64_t<59, _Llsh, true>;
    template<unsigned _Llsh = 0> using int60_strict_t   = truncated_int64_t<60, _Llsh, true>;
    template<unsigned _Llsh = 0> using int61_strict_t   = truncated_int64_t<61, _Llsh, true>;
    template<unsigned _Llsh = 0> using int62_strict_t   = truncated_int64_t<62, _Llsh, true>;
    template<unsigned _Llsh = 0> using int63_strict_t   = truncated_int64_t<63, _Llsh, true>;
    template<unsigned _Llsh = 0> requires TL64<64, _Llsh> using int64_strict_t   = int64_t;

    //
    template<unsigned _L, unsigned _Llsh = 0>
    struct int_strict { using type = void; };

    template<unsigned _L, unsigned _Llsh = 0>
    using int_strict_t = typename int_strict<_L, _Llsh>::type;

    template<unsigned _Llsh> struct int_strict<1, _Llsh>        { using type = int1_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<2, _Llsh>        { using type = int2_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<3, _Llsh>        { using type = int3_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<4, _Llsh>        { using type = int4_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<5, _Llsh>        { using type = int5_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<6, _Llsh>        { using type = int6_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<7, _Llsh>        { using type = int7_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<8, _Llsh>        { using type = int8_strict_t<_Llsh>; };

    template<unsigned _Llsh> struct int_strict<9, _Llsh>        { using type = int9_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<10, _Llsh>       { using type = int10_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<11, _Llsh>       { using type = int11_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<12, _Llsh>       { using type = int12_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<13, _Llsh>       { using type = int13_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<14, _Llsh>       { using type = int14_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<15, _Llsh>       { using type = int15_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<16, _Llsh>       { using type = int16_strict_t<_Llsh>; };

    template<unsigned _Llsh> struct int_strict<17, _Llsh>       { using type = int17_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<18, _Llsh>       { using type = int18_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<19, _Llsh>       { using type = int19_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<20, _Llsh>       { using type = int20_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<21, _Llsh>       { using type = int21_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<22, _Llsh>       { using type = int22_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<23, _Llsh>       { using type = int23_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<24, _Llsh>       { using type = int24_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<25, _Llsh>       { using type = int25_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<26, _Llsh>       { using type = int26_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<27, _Llsh>       { using type = int27_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<28, _Llsh>       { using type = int28_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<29, _Llsh>       { using type = int29_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<30, _Llsh>       { using type = int30_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<31, _Llsh>       { using type = int31_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<32, _Llsh>       { using type = int32_strict_t<_Llsh>; };

    template<unsigned _Llsh> struct int_strict<33, _Llsh>       { using type = int33_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<34, _Llsh>       { using type = int34_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<35, _Llsh>       { using type = int35_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<36, _Llsh>       { using type = int36_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<37, _Llsh>       { using type = int37_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<38, _Llsh>       { using type = int38_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<39, _Llsh>       { using type = int39_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<40, _Llsh>       { using type = int40_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<41, _Llsh>       { using type = int41_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<42, _Llsh>       { using type = int42_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<43, _Llsh>       { using type = int43_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<44, _Llsh>       { using type = int44_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<45, _Llsh>       { using type = int45_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<46, _Llsh>       { using type = int46_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<47, _Llsh>       { using type = int47_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<48, _Llsh>       { using type = int48_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<49, _Llsh>       { using type = int49_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<50, _Llsh>       { using type = int50_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<51, _Llsh>       { using type = int51_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<52, _Llsh>       { using type = int52_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<53, _Llsh>       { using type = int53_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<54, _Llsh>       { using type = int54_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<55, _Llsh>       { using type = int55_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<56, _Llsh>       { using type = int56_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<57, _Llsh>       { using type = int57_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<58, _Llsh>       { using type = int58_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<59, _Llsh>       { using type = int59_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<60, _Llsh>       { using type = int60_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<61, _Llsh>       { using type = int61_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<62, _Llsh>       { using type = int62_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<63, _Llsh>       { using type = int63_strict_t<_Llsh>; };
    template<unsigned _Llsh> struct int_strict<64, _Llsh>       { using type = int64_strict_t<_Llsh>; };

    //
    template<unsigned _L, unsigned _Llsh = 0>
    using int8n_strict_t        = truncated_int8_t<_L, _Llsh, true>;

    template<unsigned _L, unsigned _Llsh = 0>
    using int16n_strict_t       = truncated_int16_t<_L, _Llsh, true>;

    template<unsigned _L, unsigned _Llsh = 0>
    using int32n_strict_t       = truncated_int32_t<_L, _Llsh, true>;

    template<unsigned _L, unsigned _Llsh = 0>
    using int64n_strict_t       = truncated_int64_t<_L, _Llsh, true>;
    
    template<unsigned _VL, unsigned _L, unsigned _Llsh = 0>
    struct intfitn_strict { using type = void; };

    template<unsigned _VL, unsigned _L, unsigned _Llsh = 0>
    using intfitn_strict_t = typename intfitn_strict<_VL, _L, _Llsh>::type;

    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<1, _L, _Llsh>   { using type = int8n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<2, _L, _Llsh>   { using type = int8n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<3, _L, _Llsh>   { using type = int8n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<4, _L, _Llsh>   { using type = int8n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<5, _L, _Llsh>   { using type = int8n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<6, _L, _Llsh>   { using type = int8n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<7, _L, _Llsh>   { using type = int8n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<8, _L, _Llsh>   { using type = int8n_strict_t<_L, _Llsh>; };

    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<9, _L, _Llsh>   { using type = int16n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<10, _L, _Llsh>  { using type = int16n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<11, _L, _Llsh>  { using type = int16n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<12, _L, _Llsh>  { using type = int16n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<13, _L, _Llsh>  { using type = int16n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<14, _L, _Llsh>  { using type = int16n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<15, _L, _Llsh>  { using type = int16n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<16, _L, _Llsh>  { using type = int16n_strict_t<_L, _Llsh>; };

    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<17, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<18, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<19, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<20, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<21, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<22, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<23, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<24, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<25, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<26, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<27, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<28, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<29, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<30, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<31, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<32, _L, _Llsh>  { using type = int32n_strict_t<_L, _Llsh>; };

    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<33, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<34, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<35, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<36, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<37, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<38, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<39, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<40, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<41, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<42, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<43, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<44, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<45, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<46, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<47, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<48, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<49, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<50, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<51, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<52, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<53, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<54, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<55, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<56, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<57, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<58, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<59, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<60, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<61, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<62, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<63, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };
    template<unsigned _L, unsigned _Llsh> struct intfitn_strict<64, _L, _Llsh>  { using type = int64n_strict_t<_L, _Llsh>; };


    //
    struct _truncated_bits {};


    //
    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    struct _truncated_uint_base : public _truncated_bits {
    public:
        static constexpr _Tuv       MASK            = (_Tuv(1) << _L) - 1;
        static constexpr unsigned   SIGN_SHIFT      = sizeof(_Tuv) * 8 - _L;

    public:
        static constexpr _Tuv       MAX             = MASK;
        static constexpr _Tuv       MIN             = 0;
        static constexpr unsigned   BITS            = _L;
        static constexpr size_t     HOLDER_BITS     = sizeof(_Tuv) * 8;

    private:
        _Tuv    val;

        static constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> make_raw(_Tsv val) noexcept;

    public:
        using value_type = _Tuv;

    public:
        constexpr _truncated_uint_base() noexcept = default;
        constexpr _truncated_uint_base(const _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>&) noexcept = default;
        constexpr _truncated_uint_base(const _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>&) noexcept;
        constexpr _truncated_uint_base(_Tuv val) noexcept;

        template<class _Tother>
        constexpr bool          operator==(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr bool          operator!=(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr bool          operator<(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr bool          operator>(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr bool          operator<=(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr bool          operator>=(const _Tother&) const noexcept;

        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator~() const noexcept;

        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator<<(int) const noexcept;
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator>>(int) const noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator&(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator|(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator^(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator+(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator-(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator*(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator/(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator%(const _Tother&) const noexcept;

        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator++() noexcept;
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator--() noexcept;

        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator++(int) noexcept;
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator--(int) noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator+=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator-=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator*=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator/=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator%=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator&=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator|=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator^=(const _Tother&) noexcept;

        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator<<=(int) noexcept;
        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  operator>>=(int) noexcept;

        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  to_unsigned() const noexcept;
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   to_signed() const noexcept;

        constexpr _Tuv          decay() const noexcept;

        constexpr operator      _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>() const noexcept;
        constexpr operator      _Tuv() const noexcept;
    };


    //
    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    struct _truncated_int_base : public _truncated_bits {
    private:
        static constexpr _Tuv       MASK            = (_Tuv(1) << _L) - 1;
        static constexpr unsigned   SIGN_SHIFT      = sizeof(_Tuv) * 8 - _L - _Llsh;

    public:
        static constexpr _Tuv       MAX             = MASK >> 1;
        static constexpr _Tuv       MIN             = (~MAX) & MASK;
        static constexpr unsigned   BITS            = _L;
        static constexpr size_t     HOLDER_BITS     = sizeof(_Tsv) * 8;

    private:
        _Tsv    val;

        static constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> make_raw(_Tsv val) noexcept;

    public:
        using value_type = _Tsv;

    public:
        constexpr _truncated_int_base() noexcept = default;
        constexpr _truncated_int_base(const _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>&) noexcept;
        constexpr _truncated_int_base(const _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>&) noexcept = default;
        constexpr _truncated_int_base(_Tsv val) noexcept;

        template<class _Tother>
        constexpr bool          operator==(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr bool          operator!=(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr bool          operator<(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr bool          operator>(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr bool          operator<=(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr bool          operator>=(const _Tother&) const noexcept;

        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator~() const noexcept;

        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator<<(int) const noexcept;
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator>>(int) const noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator&(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator|(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator^(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator+(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator-(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator*(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator/(const _Tother&) const noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator%(const _Tother&) const noexcept;

        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator++() noexcept;
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator--() noexcept;

        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator++(int) noexcept;
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator--(int) noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator+=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator-=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator*=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator/=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator%=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator&=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator|=(const _Tother&) noexcept;

        template<class _Tother>
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator^=(const _Tother&) noexcept;

        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator<<=(int) noexcept;
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   operator>>=(int) noexcept;

        constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>  to_unsigned() const noexcept;
        constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>   to_signed() const noexcept;

        constexpr _Tsv          decay() const noexcept;

        constexpr operator      _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>() const noexcept;
        constexpr operator      _Tsv() const noexcept;
    };

    
    //
    template<class T>
    concept _truncated_bits_concept = std::derived_from<T, _truncated_bits>;

    /*
    * bit_len_of<T>
    */
    template<class T>
    struct bit_len_of : std::integral_constant<unsigned, sizeof(T) * 8> {};

    template<_truncated_bits_concept T>
    struct bit_len_of<T> : std::integral_constant<unsigned, T::BITS> {};

    template<class T>
    inline constexpr unsigned bit_len_of_v = bit_len_of<T>::value;

    /*
    * is_same_leng<T, U>
    */
    template<class T, class U>
    struct is_same_len : std::integral_constant<bool, bit_len_of_v<T> == bit_len_of_v<U>> {};

    template<class T, class U>
    inline constexpr bool is_same_len_v = is_same_len<T, U>::value;


    /*
}
*/


// Implementation of: struct _truncated_uint_base
/*
namespace BullsEye {
    */
    //
    // _Tuv    val;
    //

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>
        ::_truncated_uint_base(const _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>& other) noexcept
        : val(static_cast<_Tuv>(other))
    { }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>
        ::_truncated_uint_base(_Tuv val) noexcept
    { 
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                this->val = val;
            else
                this->val = val << _Llsh;
        else;

        if constexpr (_Llsh == 0)
            this->val = val & MASK;
        else
            this->val = (val & MASK) << _Llsh;
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(_Tsv val) noexcept
    {
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> v;
        v.val = val;
        return v;
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr bool 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator==(const _Tother& other) const noexcept
    {
        return static_cast<_Tuv>(*this) == static_cast<_Tuv>(other);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr bool 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator!=(const _Tother& other) const noexcept
    {
        return static_cast<_Tuv>(*this) != static_cast<_Tuv>(other);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr bool 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator<(const _Tother& other) const noexcept
    {
        return static_cast<_Tuv>(*this) < static_cast<_Tuv>(other);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr bool 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator>(const _Tother& other) const noexcept
    {
        return static_cast<_Tuv>(*this) > static_cast<_Tuv>(other);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr bool 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator<=(const _Tother& other) const noexcept
    {
        return static_cast<_Tuv>(*this) <= static_cast<_Tuv>(other);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr bool 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator>=(const _Tother& other) const noexcept
    {
        return static_cast<_Tuv>(*this) >= static_cast<_Tuv>(other);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator~() const noexcept
    {
        if constexpr (!_R)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>(
                ~val
            );

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (~val & MASK) | (val & ~MASK)
            );
        else
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (~val & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator<<(int n) const noexcept
    {
        if constexpr (!_R)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val << n
            );

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (((val /*& MASK*/) << n) & MASK) | (val & ~MASK)
            );
        else
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (((val & (MASK << _Llsh)) << n) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator>>(int n) const noexcept
    {
        if constexpr (!_R)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val >> n
            );

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val & MASK) >> n) | (val & ~MASK)
            );
        else
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (((val & (MASK << _Llsh)) >> n) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator&(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val & static_cast<_Tuv>(other)
                );
            else
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val & (static_cast<_Tuv>(other) << _Llsh)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (val & static_cast<_Tuv>(other)) | (val & ~MASK)
            );
        else
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (val & (static_cast<_Tuv>(other) << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator|(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val | static_cast<_Tuv>(other)
                );
            else
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val | (static_cast<_Tuv>(other) << _Llsh)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val | (static_cast<_Tuv>(other) & MASK)
            );
        else
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val | ((static_cast<_Tuv>(other) & MASK) << _Llsh)
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator^(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val ^ static_cast<_Tuv>(other)
                );
            else
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val ^ (static_cast<_Tuv>(other) << _Llsh)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val ^ (static_cast<_Tuv>(other) & MASK)
            );
        else
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val ^ ((static_cast<_Tuv>(other) & MASK) << _Llsh)
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator+(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val + static_cast<_Tuv>(other)
                );
            else
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val + (static_cast<_Tuv>(other) << _Llsh)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val + static_cast<_Tuv>(other)) & MASK) | (val & ~MASK)  
            );
        else
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val + (static_cast<_Tuv>(other) << _Llsh)) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator-(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val - static_cast<_Tuv>(other)
                );
            else
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val - (static_cast<_Tuv>(other) << _Llsh)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val - static_cast<_Tuv>(other)) & MASK) | (val & ~MASK)  
            );
        else
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val - (static_cast<_Tuv>(other) << _Llsh)) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator*(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val * static_cast<_Tuv>(other)
                );
            else
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    (val & (MASK << _Llsh)) * static_cast<_Tuv>(other)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val * static_cast<_Tuv>(other)) & MASK) | (val & ~MASK)  
            );
        else
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (((val & (MASK << _Llsh)) * static_cast<_Tuv>(other)) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator/(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    (val & MASK) / static_cast<_Tuv>(other)
                );
            else
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    (((val & (MASK << _Llsh)) >> _Llsh) / static_cast<_Tuv>(other)) << _Llsh
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val & MASK) / static_cast<_Tuv>(other)) | (val & ~MASK)
            );
        else
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((((val & (MASK << _Llsh)) >> _Llsh) / static_cast<_Tuv>(other)) << _Llsh) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator%(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    (val & MASK) % static_cast<_Tuv>(other)
                );
            else
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    (((val & (MASK << _Llsh)) >> _Llsh) % static_cast<_Tuv>(other)) << _Llsh
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val & MASK) % static_cast<_Tuv>(other)) | (val & ~MASK)
            );
        else
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((((val & (MASK << _Llsh)) >> _Llsh) % static_cast<_Tuv>(other)) << _Llsh) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator++() noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    ++val
                );
            else
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val += (decltype(val)(1) << _Llsh)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val = ((val + decltype(val)(1)) & MASK) | (val & ~MASK)
            );
        else
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val = ((val + (decltype(val)(1) << _Llsh)) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator--() noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    --val
                );
            else
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val -= (decltype(val)(1) << _Llsh)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val = ((val - decltype(val)(1)) & MASK) | (val & ~MASK)
            );
        else
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val = ((val - (decltype(val)(1) << _Llsh)) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator++(int) noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val++
                );
            else
            {
                decltype(val) old_val = val;
                val = val + (decltype(val)(1) << _Llsh);
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    old_val
                );
            }
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
        {
            decltype(val) old_val = val;
            val = ((val + decltype(val)(1)) & MASK) | (val & ~MASK);
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                old_val
            );
        }
        else
        {
            decltype(val) old_val = val;
            val = ((val + (decltype(val)(1) << _Llsh)) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh));
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                old_val
            );
        }
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator--(int) noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val--
                );
            else
            {
                decltype(val) old_val = val;
                val = val - (decltype(val)(1) << _Llsh);
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    old_val
                );
            }
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
        {
            decltype(val) old_val = val;
            val = ((val - decltype(val)(1)) & MASK) | (val & ~MASK);
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                old_val
            );
        }
        else
        {
            decltype(val) old_val = val;
            val = ((val - (decltype(val)(1) << _Llsh)) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh));
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                old_val
            );
        }
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator=(const _Tother& other) noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val = static_cast<_Tuv>(other)
                );
            else
                return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val = static_cast<_Tuv>(other) << _Llsh
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val = (static_cast<_Tuv>(other) & MASK) | (val & ~MASK)
            );
        else
            return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val = ((static_cast<_Tuv>(other) & MASK) << _Llsh) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator+=(const _Tother& other) noexcept
    {
        return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this + other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator-=(const _Tother& other) noexcept
    {
        return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this - other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator*=(const _Tother& other) noexcept
    {
        return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this * other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator/=(const _Tother& other) noexcept
    {
        return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this / other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator%=(const _Tother& other) noexcept
    {
        return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this % other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator&=(const _Tother& other) noexcept
    {
        return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this & other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator|=(const _Tother& other) noexcept
    {
        return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this | other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator^=(const _Tother& other) noexcept
    {
        return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this ^ other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator<<=(int n) noexcept
    {
        return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this << n).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator>>=(int n) noexcept
    {
        return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this >> n).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::to_unsigned() const noexcept
    {
        return *this;
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::to_signed() const noexcept
    {
        return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>(*this);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    constexpr _Tuv _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::decay() const noexcept
    {
        return static_cast<_Tuv>(*this);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>() const noexcept
    {
        return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>(*this);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator _Tuv() const noexcept
    {
        if constexpr (_Llsh == 0)
            return val & MASK;
        else
            return (val >> _Llsh) & MASK;
    }
    /*
}
*/


// Implementation of: struct truncated_int_base
/*
namespace BullsEye {
    */
    //
    // _Tsv    val;
    //

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>
        ::_truncated_int_base(const _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>& other) noexcept
        : val(static_cast<_Tsv>(other))
    { }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>
        ::_truncated_int_base(_Tsv val) noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                this->val = val;
            else
                this->val = val << _Llsh;
        else;

        if constexpr (_Llsh == 0)
            this->val = val & MASK;
        else
            this->val = (val & MASK) << _Llsh;
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(_Tsv val) noexcept
    {
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> v;
        v.val = val;
        return v;
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr bool 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator==(const _Tother& other) const noexcept
    {
        return static_cast<_Tsv>(*this) == static_cast<_Tsv>(other);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr bool 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator!=(const _Tother& other) const noexcept
    {
        return static_cast<_Tsv>(*this) != static_cast<_Tsv>(other);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr bool 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator<(const _Tother& other) const noexcept
    {
        return static_cast<_Tsv>(*this) < static_cast<_Tsv>(other);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr bool 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator>(const _Tother& other) const noexcept
    {
        return static_cast<_Tsv>(*this) > static_cast<_Tsv>(other);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr bool 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator<=(const _Tother& other) const noexcept
    {
        return static_cast<_Tsv>(*this) <= static_cast<_Tsv>(other);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr bool 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator>=(const _Tother& other) const noexcept
    {
        return static_cast<_Tsv>(*this) >= static_cast<_Tsv>(other);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator~() const noexcept
    {
        if constexpr (!_R)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ~val
            );

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (~val & MASK) | (val & ~MASK)
            );
        else
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (~val & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator<<(int n) const noexcept
    {
        if constexpr (!_R)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val << n
            );

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (((val /*& MASK*/) << n) & MASK) | (val & ~MASK)
            );
        else
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (((val & (MASK << _Llsh)) << n) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator>>(int n) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    ((val << SIGN_SHIFT) >> SIGN_SHIFT) >> n
                );
            else
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    ((val << SIGN_SHIFT) >> SIGN_SHIFT) >> n
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((((val << SIGN_SHIFT) >> SIGN_SHIFT) >> n) & MASK) | (val & ~MASK)
            );
        else
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (((val << SIGN_SHIFT) >> SIGN_SHIFT) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator&(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val & static_cast<_Tsv>(other)
                );
            else
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val & (static_cast<_Tsv>(other) << _Llsh)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (val & static_cast<_Tsv>(other)) | (val & ~MASK)
            );
        else
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (val & (static_cast<_Tsv>(other) << _Llsh)) | (val & ~(MASK << _Llsh))
            );

        return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>(val & static_cast<_Tsv>(other));
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator|(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val | static_cast<_Tsv>(other)
                );
            else
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val | (static_cast<_Tsv>(other) << _Llsh)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val | (static_cast<_Tsv>(other) & MASK)
            );
        else
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val | ((static_cast<_Tsv>(other) & MASK) << _Llsh)
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator^(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val ^ static_cast<_Tsv>(other)
                );
            else
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val ^ (static_cast<_Tsv>(other) << _Llsh)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val ^ (static_cast<_Tsv>(other) & MASK)
            );
        else
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val ^ ((static_cast<_Tsv>(other) & MASK) << _Llsh)
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator+(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val + static_cast<_Tsv>(other)
                );
            else
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val + (static_cast<_Tsv>(other) << _Llsh)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val + static_cast<_Tsv>(other)) & MASK) | (val & ~MASK)
            );
        else
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val + (static_cast<_Tsv>(other) << _Llsh)) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator-(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val - static_cast<_Tsv>(other)
                );
            else
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val - (static_cast<_Tsv>(other) << _Llsh)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val - static_cast<_Tsv>(other)) & MASK) | (val & ~MASK)
            );
        else
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val - (static_cast<_Tsv>(other) << _Llsh)) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator*(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val * static_cast<_Tsv>(other)
                );
            else
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    (val & (MASK << _Llsh)) * static_cast<_Tsv>(other)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val * static_cast<_Tsv>(other)) & MASK) | (val & ~MASK)
            );
        else
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                (((val & (MASK << _Llsh)) * static_cast<_Tsv>(other)) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator/(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    (val & MASK) / static_cast<_Tsv>(other)
                );
            else
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    (((val & (MASK << _Llsh)) >> _Llsh) / static_cast<_Tsv>(other)) << _Llsh
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val & MASK) / static_cast<_Tsv>(other)) | (val & ~MASK)
            );
        else
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((((val & (MASK << _Llsh)) >> _Llsh) / static_cast<_Tsv>(other)) << _Llsh) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator%(const _Tother& other) const noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    (val & MASK) % static_cast<_Tsv>(other)
                );
            else
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    (((val & (MASK << _Llsh)) >> _Llsh) % static_cast<_Tsv>(other)) << _Llsh
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((val & MASK) % static_cast<_Tsv>(other)) | (val & ~MASK)
            );
        else
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                ((((val & (MASK << _Llsh)) >> _Llsh) % static_cast<_Tsv>(other)) << _Llsh) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator++() noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    ++val
                );
            else
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val += (decltype(val)(1) << _Llsh)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val = ((val + decltype(val)(1)) & MASK) | (val & ~MASK)
            );
        else
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val = ((val + (decltype(val)(1) << _Llsh)) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator--() noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    --val
                );
            else
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val -= (decltype(val)(1) << _Llsh)
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val = ((val - decltype(val)(1)) & MASK) | (val & ~MASK)
            );
        else
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val = ((val - (decltype(val)(1) << _Llsh)) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator++(int) noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val++
                );
            else
            {
                decltype(val) old_val = val;
                val = val + (decltype(val)(1) << _Llsh);
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    old_val
                );
            }
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
        {
            decltype(val) old_val = val;
            val = ((val + decltype(val)(1)) & MASK) | (val & ~MASK);
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                old_val
            );
        }
        else
        {
            decltype(val) old_val = val;
            val = ((val + (decltype(val)(1) << _Llsh)) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh));
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                old_val
            );
        }
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator--(int) noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val--
                );
            else
            {
                decltype(val) old_val = val;
                val = val - (decltype(val)(1) << _Llsh);
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    old_val
                );
            }
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
        {
            decltype(val) old_val = val;
            val = ((val - decltype(val)(1)) & MASK) | (val & ~MASK);
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                old_val
            );
        }
        else
        {
            decltype(val) old_val = val;
            val = ((val - (decltype(val)(1) << _Llsh)) & (MASK << _Llsh)) | (val & ~(MASK << _Llsh));
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                old_val
            );
        }
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator=(const _Tother& other) noexcept
    {
        if constexpr (!_R)
            if constexpr (_Llsh == 0)
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val = static_cast<_Tsv>(other)
                );
            else
                return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                    val = static_cast<_Tsv>(other) << _Llsh
                );
        else;

        // strict-reduced operation
        if constexpr (_Llsh == 0)
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val = (static_cast<_Tsv>(other) & MASK) | (val & ~MASK)
            );
        else
            return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(
                val = ((static_cast<_Tsv>(other) & MASK) << _Llsh) | (val & ~(MASK << _Llsh))
            );
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator+=(const _Tother& other) noexcept
    {
        return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this + other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator-=(const _Tother& other) noexcept
    {
        return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this - other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator*=(const _Tother& other) noexcept
    {
        return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this * other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator/=(const _Tother& other) noexcept
    {
        return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this / other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator%=(const _Tother& other) noexcept
    {
        return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this % other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator&=(const _Tother& other) noexcept
    {
        return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this & other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator|=(const _Tother& other) noexcept
    {
        return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this | other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    template<class _Tother>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator^=(const _Tother& other) noexcept
    {
        return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this ^ other).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator<<=(int n) noexcept
    {
        return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this << n).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> 
        _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator>>=(int n) noexcept
    {
        return _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(this->val = (*this >> n).val);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R> _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::to_unsigned() const noexcept
    {
        return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>::make_raw(*this);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R> _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::to_signed() const noexcept
    {
        return *this;
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    constexpr _Tsv _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::decay() const noexcept
    {
        return static_cast<_Tsv>(*this);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>() const noexcept
    {
        return _truncated_uint_base<_Tsv, _Tuv, _L, _Llsh, _R>(*this);
    }

    template<class _Tsv, class _Tuv, unsigned _L, unsigned _Llsh, bool _R>
    inline constexpr _truncated_int_base<_Tsv, _Tuv, _L, _Llsh, _R>::operator _Tsv() const noexcept
    {
        if constexpr (_Llsh == 0)
            return ((val << SIGN_SHIFT) >> SIGN_SHIFT) & MASK;
        else
            return (((val << SIGN_SHIFT) >> SIGN_SHIFT) >> _Llsh) & MASK;
    }
    /*
}
*/

#endif // BULLSEYE_NONSTDINT_HPP
