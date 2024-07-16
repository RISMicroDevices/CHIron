#pragma once

/*
#ifndef __BULLSEYE_SIMS_GRAVITY__UTILITY
#define __BULLSEYE_SIMS_GRAVITY__UTILITY
*/

#ifndef __CHILOG_COMMON__UTILITY
#define __CHILOG_COMMON__UTILITY

#include <sstream>
#include <iomanip>


/*
* Type Utility
*/
/*
namespace Gravity {
    */

    /*
    * as_pointer_if<B, T>: 
    *   - If B is true, as_pointer_if<B, T>::type = T*, 
    *     otherwise as_pointer_if<B, T>::type = T.
    *
    * as_pointer_if_t: Helper template of as_pointer_if<B, T>
    */
    //
    template<bool B, class T>
    struct as_pointer_if {
        using type = std::conditional_t<B, T*, T>;
    };

    template<bool B, class T>
    using as_pointer_if_t = typename as_pointer_if<B, T>::type;
    //


    /*
    * is_same_size<T, U>:
    *   - If sizeof(T) == sizeof(U), is_same_size<T, U>::value = true,
    *     otherwise is_same_size<T, U>::value = false.
    *
    * is_same_size_v: 
    *   Helper template of is_same_size<T, U>
    *
    * assert_same_size(...): 
    *   Asserts (static assert) that sizeof(T) == sizeof(U).
    */
    //
    template<class T, class U>
    struct is_same_size {
        static constexpr bool value = sizeof(T) == sizeof(U);
    };

    template<class T, class U>
    inline constexpr bool is_same_size_v = is_same_size<T, U>::value;

    template<class T, class U>
    inline constexpr void assert_same_size() noexcept
    {
        static_assert(is_same_size_v<T, U>, "structural size mismatch");
    }

    template<class T, class U>
    inline constexpr T& assert_same_size(T& passed_t) noexcept
    {
        static_assert(is_same_size_v<T, U>, "structural size mismatch");
        return passed_t;
    }

    template<class T, class U>
    inline constexpr T&& assert_same_size(T&& passed_t, const U& unused_u) noexcept
    {
        (void) unused_u;
        static_assert(is_same_size_v<T, U>, "structural size mismatch");
        // Take is easy :), it just forwards everything of 'passed_t', including type information.
        return std::forward<T>(passed_t);
    }
    //
/*
}
*/


/*
* String Utility
*/
/*
namespace Gravity {
    */

    /*
    * String Appender (builder pattern helper for std::ostringstream)
    * ----------------------------------------------------------------
    * Usage:
    *   StringAppender appender("Hello");
    *   appender.Append(", ").Append("World!").Append(123).ToString();
    * or
    *   StringAppender().Append("Hello, ").Append("World!").Append(123).ToString();
    *   StringAppender("Hello, ").Append("World!").Append(123).ToString();
    */
    class StringAppender {
    private:
        std::ostringstream  oss;

    public:
        inline StringAppender() noexcept {};
        inline ~StringAppender() noexcept {};

        template<class T>
        inline StringAppender(const T& value) noexcept 
        { oss << value; }

        template<class T, class... U>
        inline StringAppender(const T& value, const U&... args) noexcept 
        { oss << value; Append(args...); }

        inline StringAppender& Hex() noexcept
        { oss << std::hex; return *this; }

        inline StringAppender& Dec() noexcept
        { oss << std::dec; return *this; }

        inline StringAppender& Oct() noexcept
        { oss << std::oct; return *this; }

        inline StringAppender& Fixed() noexcept
        { oss << std::fixed; return *this; }

        inline StringAppender& Scientific() noexcept
        { oss << std::scientific; return *this; }
        
        inline StringAppender& HexFloat() noexcept
        { oss << std::hexfloat; return *this; }

        inline StringAppender& DefaultFloat() noexcept
        { oss << std::defaultfloat; return *this; }

        inline StringAppender& Base(int n) noexcept
        { oss << std::setbase(n); return *this; }

        template<class CharT>
        inline StringAppender& Fill(CharT c) noexcept
        { oss << std::setfill(c); return *this; }

        inline StringAppender& Precision(int n) noexcept
        { oss << std::setprecision(n); return *this; }

        inline StringAppender& NextWidth(int n) noexcept
        { oss << std::setw(n); return *this; }

        inline StringAppender& BoolAlpha() noexcept
        { oss << std::boolalpha; return *this; }

        inline StringAppender& NoBoolAlpha() noexcept
        { oss << std::noboolalpha; return *this; }

        inline StringAppender& ShowBase() noexcept
        { oss << std::showbase; return *this; }

        inline StringAppender& NoShowBase() noexcept
        { oss << std::noshowbase; return *this; }

        inline StringAppender& ShowPoint() noexcept
        { oss << std::showpoint; return *this; }

        inline StringAppender& NoShowPoint() noexcept
        { oss << std::noshowpoint; return *this; }

        inline StringAppender& ShowPos() noexcept
        { oss << std::showpos; return *this; }

        inline StringAppender& NoShowPos() noexcept
        { oss << std::noshowpos; return *this; }

        inline StringAppender& SkipWs() noexcept
        { oss << std::skipws; return *this; }

        inline StringAppender& NoSkipWs() noexcept
        { oss << std::noskipws; return *this; }

        inline StringAppender& Left() noexcept
        { oss << std::left; return *this; }

        inline StringAppender& Right() noexcept
        { oss << std::right; return *this; }

        inline StringAppender& Internal() noexcept
        { oss << std::internal; return *this; }

        inline StringAppender& NewLine() noexcept
        { oss << std::endl; return *this; }

        inline StringAppender& EndLine() noexcept
        { oss << std::endl; return *this; }

        template<class T>
        inline StringAppender& Append() noexcept 
        { return *this; }

        template<class T>
        inline StringAppender& Append(const T& value) noexcept 
        { oss << value; return *this; }

        template<class T, class... U>
        inline StringAppender& Append(const T& value, const U&... args) noexcept 
        { oss << value; return Append(args...); }

        template<class T>
        inline StringAppender& operator<<(const T& value) noexcept 
        { oss << value; return *this; }

        inline std::string ToString() const noexcept 
        { return oss.str(); }
    };

/*
}
*/


#endif // __BULLSEYE_SIMS_GRAVITY__UTILITY
