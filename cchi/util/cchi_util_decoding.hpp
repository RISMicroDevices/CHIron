#pragma once

#ifndef __CCHI__CHI_UTIL_DECODING
#define __CCHI__CHI_UTIL_DECODING

#include <variant>
#include <bitset>

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
                class DecoderBase<_Tflit, _Tcompanion>* host;
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

#endif // __CCHI__CHI_UTIL_DECODING
 