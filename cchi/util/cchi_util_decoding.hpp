#pragma once

#ifndef __CCHI__CHI_UTIL_DECODING
#define __CCHI__CHI_UTIL_DECODING

#include <variant>
#include <bitset>

#include "../spec/cchi_protocol_encoding.hpp"
#include "../spec/cchi_protocol_flits.hpp"


namespace CCHI {

    namespace Opcodes {

        /*
        Opcode information container.
        */
        template<class _Topcode, class _Tcompanion = std::monostate>
        class OpcodeInfo {
        public:
            enum class Channel {
                EVT = 0,
                SNP,
                REQ,
                DnRSP,
                UpRSP,
                DnDAT,
                UpDAT
            };

        private:
            bool        valid;
            Channel     channel;
            _Topcode    opcode;
            const char* name;
            _Tcompanion companion;

        public:
            OpcodeInfo() noexcept;
            OpcodeInfo(Channel channel, _Topcode opcode, const char* name) noexcept;

            bool                IsValid() const noexcept;

            Channel             GetChannel() const noexcept;
            _Topcode            GetOpcode() const noexcept;
            const char*         GetName() const noexcept;
            const char*         GetName(const char* whenInvalid) const noexcept;

            void                SetCompanion(const _Tcompanion& companion) noexcept;
            _Tcompanion&        GetCompanion() noexcept;
            const _Tcompanion&  GetCompanion() const noexcept;
            
        public:
            operator bool() const noexcept;
        };

        /*
        Opcode information decoder.
        */
        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
        class DecoderBase {
        protected:
            static constexpr size_t     OPCODE_SLOT_COUNT = 1 << _Tflit::OPCODE_WIDTH;

        public:
            using opcode_t = _Tflit::opcode_t;
            using opcodeinfo_t = OpcodeInfo<opcode_t, _Tcompanion>;

        public:
            class OpcodeIterator {
            private:
                const DecoderBase<_Tflit, _Tcompanion>* host;
                bool                                    masked;
                std::bitset<OPCODE_SLOT_COUNT>          mask;
                size_t                                  ptr;

            public:
                OpcodeIterator(const DecoderBase<_Tflit, _Tcompanion>*  host) noexcept;

                OpcodeIterator(const DecoderBase<_Tflit, _Tcompanion>*  host,
                               bool                                     masked,
                               std::bitset<OPCODE_SLOT_COUNT>           mask) noexcept;

                const opcodeinfo_t& Next() noexcept;
            };

            friend class OpcodeIterator;

        protected:
            opcodeinfo_t    opcodes[OPCODE_SLOT_COUNT];
            opcodeinfo_t    opcode_invalid;

        protected:
            std::bitset<OPCODE_SLOT_COUNT>  mask_Type1;
            std::bitset<OPCODE_SLOT_COUNT>  mask_Type2;
            std::bitset<OPCODE_SLOT_COUNT>  mask_Type3;
            std::bitset<OPCODE_SLOT_COUNT>  mask_Type4;
            std::bitset<OPCODE_SLOT_COUNT>  mask_Type5;

        public:
            inline virtual ~DecoderBase() noexcept;

        public:
            virtual opcodeinfo_t&
                operator[](size_t index) noexcept;

            virtual const opcodeinfo_t&
                operator[](size_t index) const noexcept;

        public:
            virtual opcodeinfo_t&       GetInvalid() noexcept;
            virtual const opcodeinfo_t& GetInvalid() const noexcept;
            virtual opcodeinfo_t&       Invalid() noexcept;
            virtual const opcodeinfo_t& Invalid() const noexcept;
        
        public:
            /*
            Opcode decode (for the entire opcode set)
            */
            virtual const opcodeinfo_t& Decode(typename _Tflit::opcode_t opcode) const noexcept;

        public:
            /*
            Opcode decode for Type 1
            */
            virtual const opcodeinfo_t& DecodeType1(typename _Tflit::opcode_t opcode) const noexcept;

            /*
            Opcode decode for Type 2
            */
            virtual const opcodeinfo_t& DecodeType2(typename _Tflit::opcode_t opcode) const noexcept;

            /*
            Opcode decode for Type 3
            */
            virtual const opcodeinfo_t& DecodeType3(typename _Tflit::opcode_t opcode) const noexcept;

            /*
            Opcode decode for Type 4
            */
            virtual const opcodeinfo_t& DecodeType4(typename _Tflit::opcode_t opcode) const noexcept;

            /*
            Opcode decode for Type 5
            */
            virtual const opcodeinfo_t& DecodeType5(typename _Tflit::opcode_t opcode) const noexcept;

        public:
            /*
            Opcode iteration (for the entire opcode set)
            */
            virtual OpcodeIterator Iterate() const noexcept;

        public:
            /*
            Opcode iteration for Type 1
            */
            virtual OpcodeIterator IterateType1() const noexcept;

            /*
            Opcode iteration for Type 2
            */
            virtual OpcodeIterator IterateType2() const noexcept;

            /*
            Opcode iteration for Type 3
            */
            virtual OpcodeIterator IterateType3() const noexcept;

            /*
            Opcode iteration for Type 4
            */
            virtual OpcodeIterator IterateType4() const noexcept;

            /*
            Opcode iteration for Type 5
            */
            virtual OpcodeIterator IterateType5() const noexcept;
        };


        //
        namespace EVT {

            /*
            EVT flit opcode decoder
            */
            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class DecoderBaseEVT : public DecoderBase<_Tflit, _Tcompanion> {
            public:
                DecoderBaseEVT() noexcept;
                virtual ~DecoderBaseEVT() noexcept;
            };

            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class Decoder : public DecoderBaseEVT<_Tflit, _Tcompanion> { };

            template<Flits::FlitOpcodeFormatConcept _Tflit>
            class Decoder<_Tflit, std::monostate> : public DecoderBaseEVT<_Tflit, std::monostate> {
            public:
                inline static const Decoder<_Tflit, std::monostate> INSTANCE = Decoder<_Tflit, std::monostate>();
            };
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
        using EVTDecoder = EVT::Decoder<_Tflit, _Tcompanion>;

        //
        namespace SNP {

            /*
            SNP flit opcode decoder
            */
            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class DecoderBaseSNP : public DecoderBase<_Tflit, _Tcompanion> {
            public:
                DecoderBaseSNP() noexcept;
                virtual ~DecoderBaseSNP() noexcept;
            };

            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class Decoder : public DecoderBaseSNP<_Tflit, _Tcompanion> {  };

            template<Flits::FlitOpcodeFormatConcept _Tflit>
            class Decoder<_Tflit, std::monostate> : public DecoderBaseSNP<_Tflit, std::monostate> {
            public:
                inline static const Decoder<_Tflit, std::monostate> INSTANCE = Decoder<_Tflit, std::monostate>();
            };
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
        using SNPDecoder = SNP::Decoder<_Tflit, _Tcompanion>;

        //
        namespace REQ {

            /*
            REQ flit opcode decoder
            */
            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class DecoderBaseREQ : public DecoderBase<_Tflit, _Tcompanion> {
            public:
                DecoderBaseREQ() noexcept;
                virtual ~DecoderBaseREQ() noexcept;
            };

            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class Decoder : public DecoderBaseREQ<_Tflit, _Tcompanion> { };

            template<Flits::FlitOpcodeFormatConcept _Tflit>
            class Decoder<_Tflit, std::monostate> : public DecoderBaseREQ<_Tflit, std::monostate> {
            public:
                inline static const Decoder<_Tflit, std::monostate> INSTANCE = Decoder<_Tflit, std::monostate>();
            };
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
        using REQDecoder = REQ::Decoder<_Tflit, _Tcompanion>;

        //
        namespace DnRSP {

            /*
            DnRSP flit opcode decoder
            */
            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class DecoderBaseDnRSP : public DecoderBase<_Tflit, _Tcompanion> {
            public:
                DecoderBaseDnRSP() noexcept;
                virtual ~DecoderBaseDnRSP() noexcept;
            };

            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class Decoder : public DecoderBaseDnRSP<_Tflit, _Tcompanion> { };

            template<Flits::FlitOpcodeFormatConcept _Tflit>
            class Decoder<_Tflit, std::monostate> : public DecoderBaseDnRSP<_Tflit, std::monostate> {
            public:
                inline static const Decoder<_Tflit, std::monostate> INSTANCE = Decoder<_Tflit, std::monostate>();
            };
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
        using DnRSPDecoder = DnRSP::Decoder<_Tflit, _Tcompanion>;

        //
        namespace UpRSP {

            /*
            UpRSP flit opcode decoder
            */
            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class DecoderBaseUpRSP : public DecoderBase<_Tflit, _Tcompanion> {
            public:
                DecoderBaseUpRSP() noexcept;
                virtual ~DecoderBaseUpRSP() noexcept;
            };

            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class Decoder : public DecoderBaseUpRSP<_Tflit, _Tcompanion> { };

            template<Flits::FlitOpcodeFormatConcept _Tflit>
            class Decoder<_Tflit, std::monostate> : public DecoderBaseUpRSP<_Tflit, std::monostate> {
            public:
                inline static const Decoder<_Tflit, std::monostate> INSTANCE = Decoder<_Tflit, std::monostate>();
            };
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
        using UpRSPDecoder = UpRSP::Decoder<_Tflit, _Tcompanion>;

        //
        namespace DnDAT {

            /*
            DnDAT flit opcode decoder
            */
            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class DecoderBaseDnDAT : public DecoderBase<_Tflit, _Tcompanion> {
            public:
                DecoderBaseDnDAT() noexcept;
                virtual ~DecoderBaseDnDAT() noexcept;
            };

            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class Decoder : public DecoderBaseDnDAT<_Tflit, _Tcompanion> { };

            template<Flits::FlitOpcodeFormatConcept _Tflit>
            class Decoder<_Tflit, std::monostate> : public DecoderBaseDnDAT<_Tflit, std::monostate> {
            public:
                inline static const Decoder<_Tflit, std::monostate> INSTANCE = Decoder<_Tflit, std::monostate>();
            };
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
        using DnDATDecoder = DnDAT::Decoder<_Tflit, _Tcompanion>;

        //
        namespace UpDAT {

            /*
            UpDAT flit opcode decoder
            */
            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class DecoderBaseUpDAT : public DecoderBase<_Tflit, _Tcompanion> {
            public:
                DecoderBaseUpDAT() noexcept;
                virtual ~DecoderBaseUpDAT() noexcept;
            };

            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class Decoder : public DecoderBaseUpDAT<_Tflit, _Tcompanion> { };

            template<Flits::FlitOpcodeFormatConcept _Tflit>
            class Decoder<_Tflit, std::monostate> : public DecoderBaseUpDAT<_Tflit, std::monostate> {
            public:
                inline static const Decoder<_Tflit, std::monostate> INSTANCE = Decoder<_Tflit, std::monostate>();
            };
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
        using UpDATDecoder = UpDAT::Decoder<_Tflit, _Tcompanion>;
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
    using EVTOpcodeDecoder = Opcodes::EVTDecoder<_Tflit, _Tcompanion>;

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
    using SNPOpcodeDecoder = Opcodes::SNPDecoder<_Tflit, _Tcompanion>;

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
    using REQOpcodeDecoder = Opcodes::REQDecoder<_Tflit, _Tcompanion>;

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
    using DnRSPOpcodeDecoder = Opcodes::DnRSPDecoder<_Tflit, _Tcompanion>;

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
    using UpRSPOpcodeDecoder = Opcodes::UpRSPDecoder<_Tflit, _Tcompanion>;

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
    using DnDATOpcodeDecoder = Opcodes::DnDATDecoder<_Tflit, _Tcompanion>;

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
    using UpDATOpcodeDecoder = Opcodes::UpDATDecoder<_Tflit, _Tcompanion>;
}


// Implementation of: class OpcodeInfo
namespace CCHI::Opcodes {

    template<class _Topcode, class _Tcompanion>
    inline OpcodeInfo<_Topcode, _Tcompanion>::OpcodeInfo() noexcept
        : valid     (false)
        , channel   ()
        , opcode    ()
        , name      ("")
        , companion ()
    { }

    template<class _Topcode, class _Tcompanion>
    inline OpcodeInfo<_Topcode, _Tcompanion>::OpcodeInfo(Channel channel, _Topcode opcode, const char* name) noexcept
        : valid     (true)
        , channel   (channel)
        , opcode    (opcode)
        , name      (name)
        , companion ()
    { }

    template<class _Topcode, class _Tcompanion>
    inline bool OpcodeInfo<_Topcode, _Tcompanion>::IsValid() const noexcept
    {
        return valid;
    }

    template<class _Topcode, class _Tcompanion>
    inline OpcodeInfo<_Topcode, _Tcompanion>::Channel OpcodeInfo<_Topcode, _Tcompanion>::GetChannel() const noexcept
    {
        return channel;
    }

    template<class _Topcode, class _Tcompanion>
    inline _Topcode OpcodeInfo<_Topcode, _Tcompanion>::GetOpcode() const noexcept
    {
        return opcode;
    }

    template<class _Topcode, class _Tcompanion>
    inline const char* OpcodeInfo<_Topcode, _Tcompanion>::GetName() const noexcept
    {
        return name;
    }

    template<class _Topcode, class _Tcompanion>
    inline const char* OpcodeInfo<_Topcode, _Tcompanion>::GetName(const char* whenInvalid) const noexcept
    {
        return IsValid() ? name : whenInvalid;
    }

    template<class _Topcode, class _Tcompanion>
    inline void OpcodeInfo<_Topcode, _Tcompanion>::SetCompanion(const _Tcompanion& companion) noexcept
    {
        this->companion = companion;
    }

    template<class _Topcode, class _Tcompanion>
    inline _Tcompanion& OpcodeInfo<_Topcode, _Tcompanion>::GetCompanion() noexcept
    {
        return companion;
    }

    template<class _Topcode, class _Tcompanion>
    inline const _Tcompanion& OpcodeInfo<_Topcode, _Tcompanion>::GetCompanion() const noexcept
    {
        return companion;
    }

    template<class _Topcode, class _Tcompanion>
    inline OpcodeInfo<_Topcode, _Tcompanion>::operator bool() const noexcept
    {
        return IsValid();
    }
}


// Implementation of: class DecoderBase
namespace CCHI::Opcodes {

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBase<_Tflit, _Tcompanion>::~DecoderBase() noexcept
    { }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
        DecoderBase<_Tflit, _Tcompanion>::operator[](size_t index) noexcept
    {
        return opcodes[(typename _Tflit::opcode_t)(index)];
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
        DecoderBase<_Tflit, _Tcompanion>::operator[](size_t index) const noexcept
    {
        return opcodes[(typename _Tflit::opcode_t)(index)];
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
        DecoderBase<_Tflit, _Tcompanion>::GetInvalid() noexcept
    {
        return opcode_invalid;
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
        DecoderBase<_Tflit, _Tcompanion>::GetInvalid() const noexcept
    {
        return opcode_invalid;
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
        DecoderBase<_Tflit, _Tcompanion>::Invalid() noexcept
    {
        return GetInvalid();
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
        DecoderBase<_Tflit, _Tcompanion>::Invalid() const noexcept
    {
        return GetInvalid();
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
        DecoderBase<_Tflit, _Tcompanion>::Decode(typename _Tflit::opcode_t opcode) const noexcept
    {
        const opcodeinfo_t& info = opcodes[opcode];

        if (!info.IsValid())
            return opcode_invalid;

        return info;
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
        DecoderBase<_Tflit, _Tcompanion>::DecodeType1(typename _Tflit::opcode_t opcode) const noexcept
    {
        if (!mask_Type1[opcode])
            return opcode_invalid;

        return Decode(opcode);
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
        DecoderBase<_Tflit, _Tcompanion>::DecodeType2(typename _Tflit::opcode_t opcode) const noexcept
    {
        if (!mask_Type2[opcode])
            return opcode_invalid;

        return Decode(opcode);
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
        DecoderBase<_Tflit, _Tcompanion>::DecodeType3(typename _Tflit::opcode_t opcode) const noexcept
    {
        if (!mask_Type3[opcode])
            return opcode_invalid;

        return Decode(opcode);
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
        DecoderBase<_Tflit, _Tcompanion>::DecodeType4(typename _Tflit::opcode_t opcode) const noexcept
    {
        if (!mask_Type4[opcode])
            return opcode_invalid;

        return Decode(opcode);
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
        DecoderBase<_Tflit, _Tcompanion>::DecodeType5(typename _Tflit::opcode_t opcode) const noexcept
    {
        if (!mask_Type5[opcode])
            return opcode_invalid;

        return Decode(opcode);
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator
        DecoderBase<_Tflit, _Tcompanion>::Iterate() const noexcept
    {
        return OpcodeIterator(this);
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator
        DecoderBase<_Tflit, _Tcompanion>::IterateType1() const noexcept
    {
        return OpcodeIterator(this, true, mask_Type1);
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator
        DecoderBase<_Tflit, _Tcompanion>::IterateType2() const noexcept
    {
        return OpcodeIterator(this, true, mask_Type2);
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator
        DecoderBase<_Tflit, _Tcompanion>::IterateType3() const noexcept
    {
        return OpcodeIterator(this, true, mask_Type3);
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator
        DecoderBase<_Tflit, _Tcompanion>::IterateType4() const noexcept
    {
        return OpcodeIterator(this, true, mask_Type4);
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator
        DecoderBase<_Tflit, _Tcompanion>::IterateType5() const noexcept
    {
        return OpcodeIterator(this, true, mask_Type5);
    }
}


// Implementation of: class DecoderBase::OpcodeIterator
namespace CCHI::Opcodes {

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator::OpcodeIterator(
        const DecoderBase<_Tflit, _Tcompanion>* host
    ) noexcept
        : host      (host)
        , masked    (false)
        , mask      ()
        , ptr       (0)
    { }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator::OpcodeIterator(
        const DecoderBase<_Tflit, _Tcompanion>* host,
        bool                                    masked,
        std::bitset<OPCODE_SLOT_COUNT>          mask
    ) noexcept
        : host      (host)
        , masked    (masked)
        , mask      (mask)
        , ptr       (0)
    { }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
        DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator::Next() noexcept
    {
        while (ptr < OPCODE_SLOT_COUNT)
        {
            if ((!masked || mask[ptr]) && host->opcodes[ptr].IsValid())
                return host->opcodes[ptr++];

            ptr++;
        }

        return host->Invalid();
    }
}


// Implementation of: class EVT::Decoder
namespace CCHI::Opcodes::EVT {

    #define OPCODE_INFO_SET(name) \
        this->opcodes[CCHI::EVT::name] \
            = OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>( \
                OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>::Channel::EVT, \
                CCHI::EVT::name, #name)

    #define OPCODE_MASK_SET(target, name) \
        this->mask_##target[CCHI::EVT::name] = true

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBaseEVT<_Tflit, _Tcompanion>::~DecoderBaseEVT() noexcept
    {}

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBaseEVT<_Tflit, _Tcompanion>::DecoderBaseEVT() noexcept
    {
        OPCODE_INFO_SET(Evict);
        OPCODE_INFO_SET(WriteBackFull);

        // Type 1 mask
        //================================================================
        OPCODE_MASK_SET(Type1, Evict);
        OPCODE_MASK_SET(Type1, WriteBackFull);
        //================================================================

        // Type 2 mask
        //================================================================
        OPCODE_MASK_SET(Type2, Evict);
        //================================================================

        // Type 3 mask
        //================================================================
        //================================================================

        // Type 4 mask
        //================================================================
        //================================================================

        // Type 5 mask
        //================================================================
        //================================================================
    }

    #undef OPCODE_INFO_SET
    #undef OPCODE_MASK_SET
}


// Implementation of: class SNP::Decoder
namespace CCHI::Opcodes::SNP {

    #define OPCODE_INFO_SET(name) \
        this->opcodes[CCHI::SNP::name] \
            = OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>( \
                OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>::Channel::SNP, \
                CCHI::SNP::name, #name)

    #define OPCODE_MASK_SET(target, name) \
        this->mask_##target[CCHI::SNP::name] = true

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBaseSNP<_Tflit, _Tcompanion>::~DecoderBaseSNP() noexcept
    {}

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBaseSNP<_Tflit, _Tcompanion>::DecoderBaseSNP() noexcept
    {
        OPCODE_INFO_SET(SnpMakeInvalid);
        OPCODE_INFO_SET(SnpToInvalid);
        OPCODE_INFO_SET(SnpToShared);
        OPCODE_INFO_SET(SnpToClean);

        // Type 1 mask
        //================================================================
        OPCODE_MASK_SET(Type1, SnpMakeInvalid);
        OPCODE_MASK_SET(Type1, SnpToInvalid);
        OPCODE_MASK_SET(Type1, SnpToShared);
        OPCODE_MASK_SET(Type1, SnpToClean);
        //================================================================

        // Type 2 mask
        //================================================================
        OPCODE_MASK_SET(Type2, SnpMakeInvalid);
        //================================================================

        // Type 3 mask
        //================================================================
        //================================================================

        // Type 4 mask
        //================================================================
        //================================================================

        // Type 5 mask
        //================================================================
        //================================================================
    }

    #undef OPCODE_INFO_SET
    #undef OPCODE_MASK_SET
}


// Implementation of: class REQ::Decoder
namespace CCHI::Opcodes::REQ {

    #define OPCODE_INFO_SET(name) \
        this->opcodes[CCHI::REQ::name] \
            = OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>( \
                OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>::Channel::REQ, \
                CCHI::REQ::name, #name)

    #define OPCODE_MASK_SET(target, name) \
        this->mask_##target[CCHI::REQ::name] = true

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBaseREQ<_Tflit, _Tcompanion>::~DecoderBaseREQ() noexcept
    {}

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBaseREQ<_Tflit, _Tcompanion>::DecoderBaseREQ() noexcept
    {
        OPCODE_INFO_SET(StashShared);               // 0x00
        OPCODE_INFO_SET(StashUnique);               // 0x01
        OPCODE_INFO_SET(ReadNoSnp);                 // 0x02
        OPCODE_INFO_SET(ReadOnce);                  // 0x03
        OPCODE_INFO_SET(ReadShared);                // 0x04
                                                    // 0x05
                                                    // 0x06
                                                    // 0x07
        OPCODE_INFO_SET(WriteNoSnpPtl);             // 0x08
        OPCODE_INFO_SET(WriteNoSnpFull);            // 0x09
        OPCODE_INFO_SET(WriteUniquePtl);            // 0x0A
        OPCODE_INFO_SET(WriteUniqueFull);           // 0x0B
        OPCODE_INFO_SET(CleanShared);               // 0x0C
        OPCODE_INFO_SET(CleanInvalid);              // 0x0D
        OPCODE_INFO_SET(MakeInvalid);               // 0x0E
                                                    // 0x0F
        OPCODE_INFO_SET(ReadUnique);                // 0x10
                                                    // 0x11
        OPCODE_INFO_SET(MakeUnique);                // 0x12
                                                    // 0x13
                                                    // 0x14
                                                    // 0x15
                                                    // 0x16
                                                    // 0x17
                                                    // 0x18
                                                    // 0x19
                                                    // 0x1A
                                                    // 0x1B
                                                    // 0x1C
                                                    // 0x1D
        OPCODE_INFO_SET(EvictBack);                 // 0x1E
        OPCODE_INFO_SET(EvictClean);                // 0x1F
        OPCODE_INFO_SET(AtomicLoad::ADD);           // 0x20
        OPCODE_INFO_SET(AtomicLoad::CLR);           // 0x21
        OPCODE_INFO_SET(AtomicLoad::EOR);           // 0x22
        OPCODE_INFO_SET(AtomicLoad::SET);           // 0x23
        OPCODE_INFO_SET(AtomicLoad::SMAX);          // 0x24
        OPCODE_INFO_SET(AtomicLoad::SMIN);          // 0x25
        OPCODE_INFO_SET(AtomicLoad::UMAX);          // 0x26
        OPCODE_INFO_SET(AtomicLoad::UMIN);          // 0x27
        OPCODE_INFO_SET(AtomicStore::ADD);          // 0x28
        OPCODE_INFO_SET(AtomicStore::CLR);          // 0x29
        OPCODE_INFO_SET(AtomicStore::EOR);          // 0x2A
        OPCODE_INFO_SET(AtomicStore::SET);          // 0x2B
        OPCODE_INFO_SET(AtomicStore::SMAX);         // 0x2C
        OPCODE_INFO_SET(AtomicStore::SMIN);         // 0x2D
        OPCODE_INFO_SET(AtomicStore::UMAX);         // 0x2E
        OPCODE_INFO_SET(AtomicStore::UMIN);         // 0x2F
        OPCODE_INFO_SET(AtomicSwap);                // 0x30
        OPCODE_INFO_SET(AtomicCompare);             // 0x31
                                                    // 0x32
                                                    // 0x33
                                                    // 0x34
                                                    // 0x35
                                                    // 0x36
                                                    // 0x37
                                                    // 0x38
                                                    // 0x39
                                                    // 0x3A
                                                    // 0x3B
                                                    // 0x3C
                                                    // 0x3D
                                                    // 0x3E
                                                    // 0x3F

        // Type 1 mask
        //================================================================
        OPCODE_MASK_SET(Type1, StashShared);
        OPCODE_MASK_SET(Type1, StashUnique);
        //----------------------------------------------------------------
        OPCODE_MASK_SET(Type1, ReadNoSnp);
        OPCODE_MASK_SET(Type1, ReadOnce);
        //----------------------------------------------------------------
        OPCODE_MASK_SET(Type1, ReadShared);
        //----------------------------------------------------------------
        OPCODE_MASK_SET(Type1, WriteNoSnpPtl);
        OPCODE_MASK_SET(Type1, WriteNoSnpFull);
        OPCODE_MASK_SET(Type1, WriteUniquePtl);
        OPCODE_MASK_SET(Type1, WriteUniqueFull);
        OPCODE_MASK_SET(Type1, CleanShared);
        OPCODE_MASK_SET(Type1, CleanInvalid);
        OPCODE_MASK_SET(Type1, MakeInvalid);
        //----------------------------------------------------------------
        OPCODE_MASK_SET(Type1, ReadUnique);
        OPCODE_MASK_SET(Type1, MakeUnique);
        //----------------------------------------------------------------
        OPCODE_MASK_SET(Type1, EvictBack);
        OPCODE_MASK_SET(Type1, EvictClean);
        //----------------------------------------------------------------
        OPCODE_MASK_SET(Type1, AtomicLoad::ADD);
        OPCODE_MASK_SET(Type1, AtomicLoad::CLR);
        OPCODE_MASK_SET(Type1, AtomicLoad::EOR);
        OPCODE_MASK_SET(Type1, AtomicLoad::SET);
        OPCODE_MASK_SET(Type1, AtomicLoad::SMAX);
        OPCODE_MASK_SET(Type1, AtomicLoad::SMIN);
        OPCODE_MASK_SET(Type1, AtomicLoad::UMAX);
        OPCODE_MASK_SET(Type1, AtomicLoad::UMIN);
        OPCODE_MASK_SET(Type1, AtomicStore::ADD);
        OPCODE_MASK_SET(Type1, AtomicStore::CLR);
        OPCODE_MASK_SET(Type1, AtomicStore::EOR);
        OPCODE_MASK_SET(Type1, AtomicStore::SET);
        OPCODE_MASK_SET(Type1, AtomicStore::SMAX);
        OPCODE_MASK_SET(Type1, AtomicStore::SMIN);
        OPCODE_MASK_SET(Type1, AtomicStore::UMAX);
        OPCODE_MASK_SET(Type1, AtomicStore::UMIN);
        OPCODE_MASK_SET(Type1, AtomicSwap);
        OPCODE_MASK_SET(Type1, AtomicCompare);
        //================================================================

        // Type 2 mask
        //================================================================
        OPCODE_MASK_SET(Type2, StashShared);
        OPCODE_MASK_SET(Type2, StashUnique);
        //----------------------------------------------------------------
        OPCODE_MASK_SET(Type2, ReadNoSnp);
        OPCODE_MASK_SET(Type2, ReadOnce);
        //----------------------------------------------------------------
        OPCODE_MASK_SET(Type2, ReadShared);
        //================================================================

        // Type 3 mask
        //================================================================
        OPCODE_MASK_SET(Type3, StashShared);
        OPCODE_MASK_SET(Type3, StashUnique);
        //----------------------------------------------------------------
        OPCODE_MASK_SET(Type3, ReadNoSnp);
        OPCODE_MASK_SET(Type3, ReadOnce);
        //----------------------------------------------------------------
        OPCODE_MASK_SET(Type3, WriteNoSnpPtl);
        OPCODE_MASK_SET(Type3, WriteNoSnpFull);
        OPCODE_MASK_SET(Type3, WriteUniquePtl);
        OPCODE_MASK_SET(Type3, WriteUniqueFull);
        OPCODE_MASK_SET(Type3, CleanShared);
        OPCODE_MASK_SET(Type3, CleanInvalid);
        OPCODE_MASK_SET(Type3, MakeInvalid);
        //================================================================

        // Type 4 mask
        //================================================================
        OPCODE_MASK_SET(Type4, StashShared);
        OPCODE_MASK_SET(Type4, StashUnique);
        //----------------------------------------------------------------
        OPCODE_MASK_SET(Type4, ReadNoSnp);
        OPCODE_MASK_SET(Type4, ReadOnce);
        //================================================================

        // Type 5 mask
        //================================================================
        OPCODE_MASK_SET(Type5, StashShared);
        OPCODE_MASK_SET(Type5, StashUnique);
        //================================================================
    }

    #undef OPCODE_INFO_SET
    #undef OPCODE_MASK_SET
}


// Implementation of: class DnRSP::Decoder
namespace CCHI::Opcodes::DnRSP {

    #define OPCODE_INFO_SET(name) \
        this->opcodes[CCHI::DnRSP::name] \
            = OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>( \
                OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>::Channel::DnRSP, \
                CCHI::DnRSP::name, #name)

    #define OPCODE_MASK_SET(target, name) \
        this->mask_##target[CCHI::DnRSP::name] = true

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBaseDnRSP<_Tflit, _Tcompanion>::~DecoderBaseDnRSP() noexcept
    {}

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBaseDnRSP<_Tflit, _Tcompanion>::DecoderBaseDnRSP() noexcept
    {
        OPCODE_INFO_SET(CompStash);                 // 0x00
        OPCODE_INFO_SET(Comp);                      // 0x01
        OPCODE_INFO_SET(DBIDResp);                  // 0x02
        OPCODE_INFO_SET(CompDBIDResp);              // 0x03
        OPCODE_INFO_SET(CompCMO);                   // 0x04
                                                    // 0x05
                                                    // 0x06
                                                    // 0x07

        // Type 1 mask
        //================================================================
        OPCODE_MASK_SET(Type1, CompStash);
        OPCODE_MASK_SET(Type1, Comp);
        OPCODE_MASK_SET(Type1, DBIDResp);
        OPCODE_MASK_SET(Type1, CompDBIDResp);
        //----------------------------------------------------------------
        OPCODE_MASK_SET(Type1, CompCMO);
        //================================================================

        // Type 2 mask
        //================================================================
        //================================================================

        // Type 3 mask
        //================================================================
        OPCODE_MASK_SET(Type3, CompStash);
        OPCODE_MASK_SET(Type3, Comp);
        OPCODE_MASK_SET(Type3, DBIDResp);
        OPCODE_MASK_SET(Type3, CompDBIDResp);
        //================================================================

        // Type 4 mask
        //================================================================
        //================================================================

        // Type 5 mask
        //================================================================
        OPCODE_MASK_SET(Type5, CompStash);
        //================================================================
    }

    #undef OPCODE_INFO_SET
    #undef OPCODE_MASK_SET
}


// Implementation of: class UpRSP::Decoder
namespace CCHI::Opcodes::UpRSP {

    #define OPCODE_INFO_SET(name) \
        this->opcodes[CCHI::UpRSP::name] \
            = OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>( \
                OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>::Channel::UpRSP, \
                CCHI::UpRSP::name, #name)

    #define OPCODE_MASK_SET(target, name) \
        this->mask_##target[CCHI::UpRSP::name] = true

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBaseUpRSP<_Tflit, _Tcompanion>::~DecoderBaseUpRSP() noexcept
    {}

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
    inline DecoderBaseUpRSP<_Tflit, _Tcompanion>::DecoderBaseUpRSP() noexcept
    {
        OPCODE_INFO_SET(CompAck);
        OPCODE_INFO_SET(SnpResp);

        // Type 1 mask
        //================================================================
        OPCODE_MASK_SET(Type1, CompAck);
        OPCODE_MASK_SET(Type1, SnpResp);
        //================================================================

        // Type 2 mask
        //================================================================
        OPCODE_MASK_SET(Type2, CompAck);
        OPCODE_MASK_SET(Type2, SnpResp);
        //================================================================

        // Type 3 mask
        //================================================================
        OPCODE_MASK_SET(Type3, CompAck);
        //================================================================

        // Type 4 mask
        //================================================================
        //================================================================

        // Type 5 mask
        //================================================================
        //================================================================
    }

    #undef OPCODE_INFO_SET
    #undef OPCODE_MASK_SET
}


#endif // __CCHI__CHI_UTIL_DECODING
