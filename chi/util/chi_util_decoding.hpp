//#pragma once

//#ifndef __CHI__CHI_UTIL_DECODING
//#define __CHI__CHI_UTIL_DECODING


#ifndef CHI_UTIL_DECODING__STANDALONE
#   include "chi_util_decoding_header.hpp"
#   include "../spec/chi_protocol_encoding.hpp"
#endif


#if (!defined(CHI_ISSUE_B_ENABLE)  || !defined(__CHI__CHI_UTIL_DECODING_B)) \
 && (!defined(CHI_ISSUE_EB_ENABLE) || !defined(__CHI__CHI_UTIL_DECODING_EB))

#ifdef CHI_ISSUE_B_ENABLE
#   define __CHI__CHI_UTIL_DECODING_B
#endif
#ifdef CHI_ISSUE_EB_ENABLE
#   define __CHI__CHI_UTIL_DECODING_EB
#endif


/*
namespace CHI {
*/
    //
    namespace Opcodes {

        /*
        Opcode information container.
        */
        template<class _Topcode, class _Tcompanion = std::monostate>
        class OpcodeInfo {
        public:
            enum class Channel {
                REQ = 0,
                SNP,
                DAT,
                RSP
            };

        private:
            bool            valid;
            Channel         channel;
            _Topcode        opcode;
            const char*     name;
            _Tcompanion     companion;

        public:
            OpcodeInfo() noexcept;
            OpcodeInfo(Channel channel, _Topcode opcode, const char* name) noexcept;

            bool                IsValid() const noexcept;

            Channel             GetChannel() const noexcept;
            _Topcode            GetOpcode() const noexcept;
            const char*         GetName() const noexcept;

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
            using OpcodeInfo = OpcodeInfo<opcode_t, _Tcompanion>;

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

                const OpcodeInfo& Next() noexcept;
            };

            friend class OpcodeIterator;

        protected:
            OpcodeInfo  opcodes[OPCODE_SLOT_COUNT];
            OpcodeInfo  opcode_invalid;

        protected:
            std::bitset<OPCODE_SLOT_COUNT>  mask_RNF;
            std::bitset<OPCODE_SLOT_COUNT>  mask_RND;
            std::bitset<OPCODE_SLOT_COUNT>  mask_RNI;

            std::bitset<OPCODE_SLOT_COUNT>  mask_HNF;
            std::bitset<OPCODE_SLOT_COUNT>  mask_HNI;

            std::bitset<OPCODE_SLOT_COUNT>  mask_SNF;
            std::bitset<OPCODE_SLOT_COUNT>  mask_SNI;

            std::bitset<OPCODE_SLOT_COUNT>  mask_MN;

        public:
            inline virtual ~DecoderBase() noexcept;

        public:
            virtual OpcodeInfo&
                operator[](size_t index) noexcept;

            virtual const OpcodeInfo&
                operator[](size_t index) const noexcept;

        public:
            virtual OpcodeInfo&         GetInvalid() noexcept;
            virtual const OpcodeInfo&   GetInvalid() const noexcept;
            virtual OpcodeInfo&         Invalid() noexcept;
            virtual const OpcodeInfo&   Invalid() const noexcept;

        public:
            /*
            Opcode decode (for the entire opcode set)
            */
            virtual const OpcodeInfo&   Decode(typename _Tflit::opcode_t opcode) const noexcept;

        public:
            /*
            Opcode decode for RN-F
            */
            virtual const OpcodeInfo&   DecodeRNF(typename _Tflit::opcode_t opcode) const noexcept;

            /*
            Opcode decode for RN-D
            */
            virtual const OpcodeInfo&   DecodeRND(typename _Tflit::opcode_t opcode) const noexcept;

            /*
            Opcode decode for RN-I
            */
            virtual const OpcodeInfo&   DecodeRNI(typename _Tflit::opcode_t opcode) const noexcept;

            /*
            Opcode decode for HN-F
            */
            virtual const OpcodeInfo&   DecodeHNF(typename _Tflit::opcode_t opcode) const noexcept;

            /*
            Opcode decode for HN-I
            */
            virtual const OpcodeInfo&   DecodeHNI(typename _Tflit::opcode_t opcode) const noexcept;

            /*
            Opcode decode for SN-F
            */
            virtual const OpcodeInfo&   DecodeSNF(typename _Tflit::opcode_t opcode) const noexcept;

            /*
            Opcode decode for SN-I
            */
            virtual const OpcodeInfo&   DecodeSNI(typename _Tflit::opcode_t opcode) const noexcept;

            /*
            Opcode decode for MN
            */
            virtual const OpcodeInfo&   DecodeMN(typename _Tflit::opcode_t opcode) const noexcept;
            
        public:
            /*
            Opcode iteration (for the entire opcode set)
            */
            virtual OpcodeIterator Iterate() const noexcept;

        public:
            /*
            Opcode iteration for RN-F
            */
            virtual OpcodeIterator IterateRNF() const noexcept;

            /*
            Opcode iteration for RN-D
            */
            virtual OpcodeIterator IterateRND() const noexcept;

            /*
            Opcode iteration for RN-I
            */
            virtual OpcodeIterator IterateRNI() const noexcept;

            /*
            Opcode iteration for HN-F
            */
            virtual OpcodeIterator IterateHNF() const noexcept;

            /*
            Opcode iteration for HN-I
            */
            virtual OpcodeIterator IterateHNI() const noexcept;

            /*
            Opcode iteration for SN-F
            */
            virtual OpcodeIterator IterateSNF() const noexcept;

            /*
            Opcode iteration for SN-I
            */
            virtual OpcodeIterator IterateSNI() const noexcept;

            /*
            Opcode iteration for MN
            */
            virtual OpcodeIterator IterateMN() const noexcept;
        };

    
        //
        namespace REQ {

            /*
            REQ flit opcode decoder
            */
            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class Decoder : public DecoderBase<_Tflit, _Tcompanion> {
            public:
                Decoder() noexcept;
                virtual ~Decoder() noexcept;
            };
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
        using REQDecoder = REQ::Decoder<_Tflit, _Tcompanion>;

        //
        namespace SNP {

            /*
            SNP flit opcode decoder
            */
            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class Decoder : public DecoderBase<_Tflit, _Tcompanion> {
            public:
                Decoder() noexcept;
                virtual ~Decoder() noexcept;
            };
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
        using SNPDecoder = SNP::Decoder<_Tflit, _Tcompanion>;

        //
        namespace DAT {

            /*
            DAT flit opcode decoder
            */
            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class Decoder : public DecoderBase<_Tflit, _Tcompanion> {
            public:
                Decoder() noexcept;
                virtual ~Decoder() noexcept;
            };
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
        using DATDecoder = DAT::Decoder<_Tflit, _Tcompanion>;

        //
        namespace RSP {

            /*
            RSP flit opcode decoder
            */
            template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
            class Decoder : public DecoderBase<_Tflit, _Tcompanion> {
            public:
                Decoder() noexcept;
                virtual ~Decoder() noexcept;
            };
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
        using RSPDecoder = RSP::Decoder<_Tflit, _Tcompanion>;
    }

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
    using REQOpcodeDecoder = Opcodes::REQDecoder<_Tflit, _Tcompanion>;

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
    using SNPOpcodeDecoder = Opcodes::SNPDecoder<_Tflit, _Tcompanion>;

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
    using RSPOpcodeDecoder = Opcodes::RSPDecoder<_Tflit, _Tcompanion>;

    template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion = std::monostate>
    using DATOpcodeDecoder = Opcodes::DATDecoder<_Tflit, _Tcompanion>;
/*
}
*/


// Implementation of: class OpcodeInfo
/*
namespace CHI {
*/
    namespace Opcodes {
        /*
        bool            valid;
        Channel         channel;
        _Topcode        opcode;
        const char*     name;
        _Tcompanion     companion;
        */

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
/*
}
*/


// Implementation of: class DecoderBase
/*
namespace CHI {
*/
    namespace Opcodes {
        /*
        protected:
            static constexpr size_t     OPCODE_SLOT_COUNT = 1 << _Tflit::OPCODE_WIDTH;

        protected:
            OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>  opcodes[OPCODE_SLOT_COUNT];
            OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>  opcode_invalid;

        protected:
            std::bitset<OPCODE_SLOT_COUNT>                      mask_RNF;
            std::bitset<OPCODE_SLOT_COUNT>                      mask_RND;
            std::bitset<OPCODE_SLOT_COUNT>                      mask_RNI;

            std::bitset<OPCODE_SLOT_COUNT>                      mask_HNF;
            std::bitset<OPCODE_SLOT_COUNT>                      mask_HNI;

            std::bitset<OPCODE_SLOT_COUNT>                      mask_SNF;
            std::bitset<OPCODE_SLOT_COUNT>                      mask_SNI;

            std::bitset<OPCODE_SLOT_COUNT>                      mask_MN;
        */

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
            const OpcodeInfo& info = opcodes[opcode];

            if (!info.IsValid())
                return opcode_invalid;

            return info;
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
            DecoderBase<_Tflit, _Tcompanion>::DecodeRNF(typename _Tflit::opcode_t opcode) const noexcept
        {
            if (!mask_RNF[opcode])
                return opcode_invalid;

            return Decode(opcode);
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
            DecoderBase<_Tflit, _Tcompanion>::DecodeRND(typename _Tflit::opcode_t opcode) const noexcept
        {
            if (!mask_RND[opcode])
                return opcode_invalid;

            return Decode(opcode);
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
            DecoderBase<_Tflit, _Tcompanion>::DecodeRNI(typename _Tflit::opcode_t opcode) const noexcept
        {
            if (!mask_RNI[opcode])
                return opcode_invalid;

            return Decode(opcode);
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
            DecoderBase<_Tflit, _Tcompanion>::DecodeHNF(typename _Tflit::opcode_t opcode) const noexcept
        {
            if (!mask_HNF[opcode])
                return opcode_invalid;

            return Decode(opcode);
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
            DecoderBase<_Tflit, _Tcompanion>::DecodeHNI(typename _Tflit::opcode_t opcode) const noexcept
        {
            if (!mask_HNI[opcode])
                return opcode_invalid;

            return Decode(opcode);
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
            DecoderBase<_Tflit, _Tcompanion>::DecodeSNF(typename _Tflit::opcode_t opcode) const noexcept
        {
            if (!mask_SNF[opcode])
                return opcode_invalid;

            return Decode(opcode);
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
            DecoderBase<_Tflit, _Tcompanion>::DecodeSNI(typename _Tflit::opcode_t opcode) const noexcept
        {
            if (!mask_SNI[opcode])
                return opcode_invalid;

            return Decode(opcode);
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline const OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>&
            DecoderBase<_Tflit, _Tcompanion>::DecodeMN(typename _Tflit::opcode_t opcode) const noexcept
        {
            if (!mask_MN[opcode])
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
            DecoderBase<_Tflit, _Tcompanion>::IterateRNF() const noexcept
        {
            return OpcodeIterator(this, true, mask_RNF);
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator
            DecoderBase<_Tflit, _Tcompanion>::IterateRND() const noexcept
        {
            return OpcodeIterator(this, true, mask_RND);
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator
            DecoderBase<_Tflit, _Tcompanion>::IterateRNI() const noexcept
        {
            return OpcodeIterator(this, true, mask_RNI);
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator
            DecoderBase<_Tflit, _Tcompanion>::IterateHNF() const noexcept
        {
            return OpcodeIterator(this, true, mask_HNF);
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator
            DecoderBase<_Tflit, _Tcompanion>::IterateHNI() const noexcept
        {
            return OpcodeIterator(this, true, mask_HNI);
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator
            DecoderBase<_Tflit, _Tcompanion>::IterateSNF() const noexcept
        {
            return OpcodeIterator(this, true, mask_SNF);
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator
            DecoderBase<_Tflit, _Tcompanion>::IterateSNI() const noexcept
        {
            return OpcodeIterator(this, true, mask_SNI);
        }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline DecoderBase<_Tflit, _Tcompanion>::OpcodeIterator
            DecoderBase<_Tflit, _Tcompanion>::IterateMN() const noexcept
        {
            return OpcodeIterator(this, true, mask_MN);
        }
    }
/*
}
*/


// Implementation of: class DecoderBase::OpcodeIterator
/*
namespace CHI {
*/
    namespace Opcodes {
        /*
        DecoderBase<_Tflit, _Tcompanion>*   host;
        bool                                masked;
        std::bitset<OPCODE_SLOT_COUNT>      mask;
        size_t                              ptr;
        */

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
/*
}
*/


// Implementation of: class REQ::Decoder
/*
namespace CHI {
*/
    namespace Opcodes::REQ {

        #define OPCODE_INFO_SET(name) \
            this->opcodes[name] \
                = OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>( \
                    OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>::Channel::REQ, \
                    name, #name)

        #define OPCODE_MASK_SET(target, name) \
            this->mask_##target[name] = true

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline Decoder<_Tflit, _Tcompanion>::~Decoder() noexcept
        {}

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline Decoder<_Tflit, _Tcompanion>::Decoder() noexcept
        {
            OPCODE_INFO_SET(ReqLCrdReturn);                     // 0x00
            OPCODE_INFO_SET(ReadShared);                        // 0x01
            OPCODE_INFO_SET(ReadClean);                         // 0x02
            OPCODE_INFO_SET(ReadOnce);                          // 0x03
            OPCODE_INFO_SET(ReadNoSnp);                         // 0x04
            OPCODE_INFO_SET(PCrdReturn);                        // 0x05
                                                                // 0x06
            OPCODE_INFO_SET(ReadUnique);                        // 0x07
            OPCODE_INFO_SET(CleanShared);                       // 0x08
            OPCODE_INFO_SET(CleanInvalid);                      // 0x09
            OPCODE_INFO_SET(MakeInvalid);                       // 0x0A
            OPCODE_INFO_SET(CleanUnique);                       // 0x0B
            OPCODE_INFO_SET(MakeUnique);                        // 0x0C
            OPCODE_INFO_SET(Evict);                             // 0x0D
                                                                // 0x0E
                                                                // 0x0F
                                                                // 0x10
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_INFO_SET(ReadNoSnpSep);                      // 0x11
#endif
                                                                // 0x12
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_INFO_SET(CleanSharedPersistSep);             // 0x13
#endif
            OPCODE_INFO_SET(DVMOp);                             // 0x14
            OPCODE_INFO_SET(WriteEvictFull);                    // 0x15
                                                                // 0x16
            OPCODE_INFO_SET(WriteCleanFull);                    // 0x17
            OPCODE_INFO_SET(WriteUniquePtl);                    // 0x18
            OPCODE_INFO_SET(WriteUniqueFull);                   // 0x19
            OPCODE_INFO_SET(WriteBackPtl);                      // 0x1A
            OPCODE_INFO_SET(WriteBackFull);                     // 0x1B
            OPCODE_INFO_SET(WriteNoSnpPtl);                     // 0x1C
            OPCODE_INFO_SET(WriteNoSnpFull);                    // 0x1D
                                                                // 0x1E
                                                                // 0x1F
            OPCODE_INFO_SET(WriteUniqueFullStash);              // 0x20
            OPCODE_INFO_SET(WriteUniquePtlStash);               // 0x21
            OPCODE_INFO_SET(StashOnceShared);                   // 0x22
            OPCODE_INFO_SET(StashOnceUnique);                   // 0x23
            OPCODE_INFO_SET(ReadOnceCleanInvalid);              // 0x24
            OPCODE_INFO_SET(ReadOnceMakeInvalid);               // 0x25
            OPCODE_INFO_SET(ReadNotSharedDirty);                // 0x26
            OPCODE_INFO_SET(CleanSharedPersist);                // 0x27
            OPCODE_INFO_SET(AtomicStore::ADD);                  // 0x28
            OPCODE_INFO_SET(AtomicStore::CLR);                  // 0x29
            OPCODE_INFO_SET(AtomicStore::EOR);                  // 0x2A
            OPCODE_INFO_SET(AtomicStore::SET);                  // 0x2B
            OPCODE_INFO_SET(AtomicStore::SMAX);                 // 0x2C
            OPCODE_INFO_SET(AtomicStore::SMIN);                 // 0x2D
            OPCODE_INFO_SET(AtomicStore::UMAX);                 // 0x2E
            OPCODE_INFO_SET(AtomicStore::UMIN);                 // 0x2F
            OPCODE_INFO_SET(AtomicLoad::ADD);                   // 0x30
            OPCODE_INFO_SET(AtomicLoad::CLR);                   // 0x31
            OPCODE_INFO_SET(AtomicLoad::EOR);                   // 0x32
            OPCODE_INFO_SET(AtomicLoad::SET);                   // 0x33
            OPCODE_INFO_SET(AtomicLoad::SMAX);                  // 0x34
            OPCODE_INFO_SET(AtomicLoad::SMIN);                  // 0x35
            OPCODE_INFO_SET(AtomicLoad::UMAX);                  // 0x36
            OPCODE_INFO_SET(AtomicLoad::UMIN);                  // 0x37
            OPCODE_INFO_SET(AtomicSwap);                        // 0x38
            OPCODE_INFO_SET(AtomicCompare);                     // 0x39
            OPCODE_INFO_SET(PrefetchTgt);                       // 0x3A
                                                                // 0x3B
                                                                // 0x3C
                                                                // 0x3D
                                                                // 0x3E
                                                                // 0x3F
#ifdef CHI_ISSUE_EB_ENABLE
                                                                // 0x40 | 0x00
            OPCODE_INFO_SET(MakeReadUnique);                    // 0x40 | 0x01
            OPCODE_INFO_SET(WriteEvictOrEvict);                 // 0x40 | 0x02
            OPCODE_INFO_SET(WriteUniqueZero);                   // 0x40 | 0x03
            OPCODE_INFO_SET(WriteNoSnpZero);                    // 0x40 | 0x04
                                                                // 0x40 | 0x05
                                                                // 0x40 | 0x06
            OPCODE_INFO_SET(StashOnceSepShared);                // 0x40 | 0x07
            OPCODE_INFO_SET(StashOnceSepUnique);                // 0x40 | 0x08
                                                                // 0x40 | 0x09
                                                                // 0x40 | 0x0A
                                                                // 0x40 | 0x0B
            OPCODE_INFO_SET(ReadPreferUnique);                  // 0x40 | 0x0C
                                                                // 0x40 | 0x0D
                                                                // 0x40 | 0x0E
                                                                // 0x40 | 0x0F
            OPCODE_INFO_SET(WriteNoSnpFullCleanSh);             // 0x40 | 0x10
            OPCODE_INFO_SET(WriteNoSnpFullCleanInv);            // 0x40 | 0x11
            OPCODE_INFO_SET(WriteNoSnpFullCleanShPerSep);       // 0x40 | 0x12
                                                                // 0x40 | 0x13
            OPCODE_INFO_SET(WriteUniqueFullCleanSh);            // 0x40 | 0x14
                                                                // 0x40 | 0x15
            OPCODE_INFO_SET(WriteUniqueFullCleanShPerSep);      // 0x40 | 0x16
                                                                // 0x40 | 0x17
            OPCODE_INFO_SET(WriteBackFullCleanSh);              // 0x40 | 0x18
            OPCODE_INFO_SET(WriteBackFullCleanInv);             // 0x40 | 0x19
            OPCODE_INFO_SET(WriteBackFullCleanShPerSep);        // 0x40 | 0x1A
                                                                // 0x40 | 0x1B
            OPCODE_INFO_SET(WriteCleanFullCleanSh);             // 0x40 | 0x1C
                                                                // 0x40 | 0x1D
            OPCODE_INFO_SET(WriteCleanFullCleanShPerSep);       // 0x40 | 0x1E
                                                                // 0x40 | 0x1F
            OPCODE_INFO_SET(WriteNoSnpPtlCleanSh);              // 0x40 | 0x20
            OPCODE_INFO_SET(WriteNoSnpPtlCleanInv);             // 0x40 | 0x21
            OPCODE_INFO_SET(WriteNoSnpPtlCleanShPerSep);        // 0x40 | 0x22
                                                                // 0x40 | 0x23
            OPCODE_INFO_SET(WriteUniquePtlCleanSh);             // 0x40 | 0x24
                                                                // 0x40 | 0x25
            OPCODE_INFO_SET(WriteUniquePtlCleanShPerSep);       // 0x40 | 0x26
                                                                // 0x40 | 0x27
                                                                // ...
                                                                // 0x40 | 0x3F
#endif

            // RN-F mask
            //================================================================
            OPCODE_MASK_SET(RNF, ReqLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, ReadNoSnp);
            OPCODE_MASK_SET(RNF, WriteNoSnpFull);
            OPCODE_MASK_SET(RNF, WriteNoSnpPtl);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, WriteNoSnpZero);
            OPCODE_MASK_SET(HNF, ReadNoSnpSep);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, ReadClean);
            OPCODE_MASK_SET(RNF, ReadShared);
            OPCODE_MASK_SET(RNF, ReadNotSharedDirty);
            OPCODE_MASK_SET(RNF, ReadUnique);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, ReadPreferUnique);
            OPCODE_MASK_SET(RNF, MakeReadUnique);
#endif
            OPCODE_MASK_SET(RNF, CleanUnique);
            OPCODE_MASK_SET(RNF, MakeUnique);
            OPCODE_MASK_SET(RNF, Evict);
            OPCODE_MASK_SET(RNF, WriteBackPtl);
            OPCODE_MASK_SET(RNF, WriteBackFull);
            OPCODE_MASK_SET(RNF, WriteCleanFull);
            OPCODE_MASK_SET(RNF, WriteEvictFull);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, WriteEvictOrEvict);
#endif
            OPCODE_MASK_SET(RNF, ReadOnce);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, ReadOnceCleanInvalid);
            OPCODE_MASK_SET(RNF, ReadOnceMakeInvalid);
            OPCODE_MASK_SET(RNF, StashOnceUnique);
            OPCODE_MASK_SET(RNF, StashOnceShared);
            OPCODE_MASK_SET(RNF, StashOnceSepUnique);
            OPCODE_MASK_SET(RNF, StashOnceSepShared);
#endif
            OPCODE_MASK_SET(RNF, WriteUniqueFull);
            OPCODE_MASK_SET(RNF, WriteUniqueFullStash);
            OPCODE_MASK_SET(RNF, WriteUniquePtl);
            OPCODE_MASK_SET(RNF, WriteUniquePtlStash);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, WriteUniqueZero);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, CleanShared);
            OPCODE_MASK_SET(RNF, CleanSharedPersist);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, CleanSharedPersistSep);
#endif
            OPCODE_MASK_SET(RNF, CleanInvalid);
            OPCODE_MASK_SET(RNF, MakeInvalid);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, WriteUniquePtlCleanSh);
            OPCODE_MASK_SET(RNF, WriteUniquePtlCleanShPerSep);
            OPCODE_MASK_SET(RNF, WriteUniqueFullCleanSh);
            OPCODE_MASK_SET(RNF, WriteUniqueFullCleanShPerSep);
            OPCODE_MASK_SET(RNF, WriteBackFullCleanInv);
            OPCODE_MASK_SET(RNF, WriteBackFullCleanSh);
            OPCODE_MASK_SET(RNF, WriteBackFullCleanShPerSep);
            OPCODE_MASK_SET(RNF, WriteCleanFullCleanSh);
            OPCODE_MASK_SET(RNF, WriteCleanFullCleanShPerSep);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, WriteNoSnpPtlCleanInv);
            OPCODE_MASK_SET(RNF, WriteNoSnpPtlCleanSh);
            OPCODE_MASK_SET(RNF, WriteNoSnpPtlCleanShPerSep);
            OPCODE_MASK_SET(RNF, WriteNoSnpFullCleanInv);
            OPCODE_MASK_SET(RNF, WriteNoSnpFullCleanSh);
            OPCODE_MASK_SET(RNF, WriteNoSnpFullCleanShPerSep);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, DVMOp);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, PCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, AtomicStore::ADD);
            OPCODE_MASK_SET(RNF, AtomicStore::CLR);
            OPCODE_MASK_SET(RNF, AtomicStore::EOR);
            OPCODE_MASK_SET(RNF, AtomicStore::SET);
            OPCODE_MASK_SET(RNF, AtomicStore::SMAX);
            OPCODE_MASK_SET(RNF, AtomicStore::SMIN);
            OPCODE_MASK_SET(RNF, AtomicStore::UMAX);
            OPCODE_MASK_SET(RNF, AtomicStore::UMIN);
            OPCODE_MASK_SET(RNF, AtomicLoad::ADD);
            OPCODE_MASK_SET(RNF, AtomicLoad::CLR);
            OPCODE_MASK_SET(RNF, AtomicLoad::EOR);
            OPCODE_MASK_SET(RNF, AtomicLoad::SET);
            OPCODE_MASK_SET(RNF, AtomicLoad::SMAX);
            OPCODE_MASK_SET(RNF, AtomicLoad::SMIN);
            OPCODE_MASK_SET(RNF, AtomicLoad::UMAX);
            OPCODE_MASK_SET(RNF, AtomicLoad::UMIN);
            OPCODE_MASK_SET(RNF, AtomicSwap);
            OPCODE_MASK_SET(RNF, AtomicCompare);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, PrefetchTgt);
            //================================================================

            // RN-D mask
            //================================================================
            OPCODE_MASK_SET(RND, ReqLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, ReadNoSnp);
            OPCODE_MASK_SET(RND, WriteNoSnpFull);
            OPCODE_MASK_SET(RND, WriteNoSnpPtl);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RND, WriteNoSnpZero);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, ReadOnce);
            OPCODE_MASK_SET(RND, ReadOnceCleanInvalid);
            OPCODE_MASK_SET(RND, ReadOnceMakeInvalid);
            OPCODE_MASK_SET(RND, StashOnceUnique);
            OPCODE_MASK_SET(RND, StashOnceShared);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RND, StashOnceSepUnique);
            OPCODE_MASK_SET(RND, StashOnceSepShared);
#endif
            OPCODE_MASK_SET(RND, WriteUniqueFull);
            OPCODE_MASK_SET(RND, WriteUniqueFullStash);
            OPCODE_MASK_SET(RND, WriteUniquePtl);
            OPCODE_MASK_SET(RND, WriteUniquePtlStash);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RND, WriteUniqueZero);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, CleanShared);
            OPCODE_MASK_SET(RND, CleanSharedPersist);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RND, CleanSharedPersistSep);
#endif
            OPCODE_MASK_SET(RND, CleanInvalid);
            OPCODE_MASK_SET(RND, MakeInvalid);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RND, WriteNoSnpPtlCleanInv);
            OPCODE_MASK_SET(RND, WriteNoSnpPtlCleanSh);
            OPCODE_MASK_SET(RND, WriteNoSnpPtlCleanShPerSep);
            OPCODE_MASK_SET(RND, WriteNoSnpFullCleanInv);
            OPCODE_MASK_SET(RND, WriteNoSnpFullCleanSh);
            OPCODE_MASK_SET(RND, WriteNoSnpFullCleanShPerSep);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, DVMOp);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, PCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, AtomicStore::ADD);
            OPCODE_MASK_SET(RND, AtomicStore::CLR);
            OPCODE_MASK_SET(RND, AtomicStore::EOR);
            OPCODE_MASK_SET(RND, AtomicStore::SET);
            OPCODE_MASK_SET(RND, AtomicStore::SMAX);
            OPCODE_MASK_SET(RND, AtomicStore::SMIN);
            OPCODE_MASK_SET(RND, AtomicStore::UMAX);
            OPCODE_MASK_SET(RND, AtomicStore::UMIN);
            OPCODE_MASK_SET(RND, AtomicLoad::ADD);
            OPCODE_MASK_SET(RND, AtomicLoad::CLR);
            OPCODE_MASK_SET(RND, AtomicLoad::EOR);
            OPCODE_MASK_SET(RND, AtomicLoad::SET);
            OPCODE_MASK_SET(RND, AtomicLoad::SMAX);
            OPCODE_MASK_SET(RND, AtomicLoad::SMIN);
            OPCODE_MASK_SET(RND, AtomicLoad::UMAX);
            OPCODE_MASK_SET(RND, AtomicLoad::UMIN);
            OPCODE_MASK_SET(RND, AtomicSwap);
            OPCODE_MASK_SET(RND, AtomicCompare);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, PrefetchTgt);
            //================================================================

            // RN-I mask
            //================================================================
            OPCODE_MASK_SET(RNI, ReqLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNI, ReadNoSnp);
            OPCODE_MASK_SET(RNI, WriteNoSnpFull);
            OPCODE_MASK_SET(RNI, WriteNoSnpPtl);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNI, WriteNoSnpZero);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNI, ReadOnce);
            OPCODE_MASK_SET(RNI, ReadOnceCleanInvalid);
            OPCODE_MASK_SET(RNI, ReadOnceMakeInvalid);
            OPCODE_MASK_SET(RNI, StashOnceUnique);
            OPCODE_MASK_SET(RNI, StashOnceShared);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNI, StashOnceSepUnique);
            OPCODE_MASK_SET(RNI, StashOnceSepShared);
#endif
            OPCODE_MASK_SET(RNI, WriteUniqueFull);
            OPCODE_MASK_SET(RNI, WriteUniqueFullStash);
            OPCODE_MASK_SET(RNI, WriteUniquePtl);
            OPCODE_MASK_SET(RNI, WriteUniquePtlStash);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNI, WriteUniqueZero);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNI, CleanShared);
            OPCODE_MASK_SET(RNI, CleanSharedPersist);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNI, CleanSharedPersistSep);
#endif
            OPCODE_MASK_SET(RNI, CleanInvalid);
            OPCODE_MASK_SET(RNI, MakeInvalid);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNI, WriteNoSnpPtlCleanInv);
            OPCODE_MASK_SET(RNI, WriteNoSnpPtlCleanSh);
            OPCODE_MASK_SET(RNI, WriteNoSnpPtlCleanShPerSep);
            OPCODE_MASK_SET(RNI, WriteNoSnpFullCleanInv);
            OPCODE_MASK_SET(RNI, WriteNoSnpFullCleanSh);
            OPCODE_MASK_SET(RNI, WriteNoSnpFullCleanShPerSep);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNI, PCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNI, AtomicStore::ADD);
            OPCODE_MASK_SET(RNI, AtomicStore::CLR);
            OPCODE_MASK_SET(RNI, AtomicStore::EOR);
            OPCODE_MASK_SET(RNI, AtomicStore::SET);
            OPCODE_MASK_SET(RNI, AtomicStore::SMAX);
            OPCODE_MASK_SET(RNI, AtomicStore::SMIN);
            OPCODE_MASK_SET(RNI, AtomicStore::UMAX);
            OPCODE_MASK_SET(RNI, AtomicStore::UMIN);
            OPCODE_MASK_SET(RNI, AtomicLoad::ADD);
            OPCODE_MASK_SET(RNI, AtomicLoad::CLR);
            OPCODE_MASK_SET(RNI, AtomicLoad::EOR);
            OPCODE_MASK_SET(RNI, AtomicLoad::SET);
            OPCODE_MASK_SET(RNI, AtomicLoad::SMAX);
            OPCODE_MASK_SET(RNI, AtomicLoad::SMIN);
            OPCODE_MASK_SET(RNI, AtomicLoad::UMAX);
            OPCODE_MASK_SET(RNI, AtomicLoad::UMIN);
            OPCODE_MASK_SET(RNI, AtomicSwap);
            OPCODE_MASK_SET(RNI, AtomicCompare);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNI, PrefetchTgt);
            //================================================================

            // HN-F mask
            //================================================================
            OPCODE_MASK_SET(HNF, ReqLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, ReadNoSnp);
            OPCODE_MASK_SET(HNF, WriteNoSnpFull);
            OPCODE_MASK_SET(HNF, WriteNoSnpPtl);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, WriteNoSnpZero);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, ReadNoSnpSep);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, ReadClean);
            OPCODE_MASK_SET(HNF, ReadShared);
            OPCODE_MASK_SET(HNF, ReadNotSharedDirty);
            OPCODE_MASK_SET(HNF, ReadUnique);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, ReadPreferUnique);
            OPCODE_MASK_SET(HNF, MakeReadUnique);
#endif
            OPCODE_MASK_SET(HNF, CleanUnique);
            OPCODE_MASK_SET(HNF, MakeUnique);
            OPCODE_MASK_SET(HNF, Evict);
            OPCODE_MASK_SET(HNF, WriteBackFull);
            OPCODE_MASK_SET(HNF, WriteBackPtl);
            OPCODE_MASK_SET(HNF, WriteEvictFull);
            OPCODE_MASK_SET(HNF, WriteCleanFull);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, WriteEvictOrEvict);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, ReadOnce);
            OPCODE_MASK_SET(HNF, ReadOnceCleanInvalid);
            OPCODE_MASK_SET(HNF, ReadOnceMakeInvalid);
            OPCODE_MASK_SET(HNF, StashOnceUnique);
            OPCODE_MASK_SET(HNF, StashOnceShared);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, StashOnceSepUnique);
            OPCODE_MASK_SET(HNF, StashOnceSepShared);
#endif
            OPCODE_MASK_SET(HNF, WriteUniqueFull);
            OPCODE_MASK_SET(HNF, WriteUniqueFullStash);
            OPCODE_MASK_SET(HNF, WriteUniquePtl);
            OPCODE_MASK_SET(HNF, WriteUniquePtlStash);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, WriteUniqueZero);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, CleanShared);
            OPCODE_MASK_SET(HNF, CleanSharedPersist);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, CleanSharedPersistSep);
#endif
            OPCODE_MASK_SET(HNF, CleanInvalid);
            OPCODE_MASK_SET(HNF, MakeInvalid);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, WriteUniquePtlCleanSh);
            OPCODE_MASK_SET(HNF, WriteUniquePtlCleanShPerSep);
            OPCODE_MASK_SET(HNF, WriteUniqueFullCleanSh);
            OPCODE_MASK_SET(HNF, WriteUniqueFullCleanShPerSep);
            OPCODE_MASK_SET(HNF, WriteBackFullCleanInv);
            OPCODE_MASK_SET(HNF, WriteBackFullCleanSh);
            OPCODE_MASK_SET(HNF, WriteBackFullCleanShPerSep);
            OPCODE_MASK_SET(HNF, WriteCleanFullCleanSh);
            OPCODE_MASK_SET(HNF, WriteCleanFullCleanShPerSep);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, WriteNoSnpPtlCleanInv);
            OPCODE_MASK_SET(HNF, WriteNoSnpPtlCleanSh);
            OPCODE_MASK_SET(HNF, WriteNoSnpPtlCleanShPerSep);
            OPCODE_MASK_SET(HNF, WriteNoSnpFullCleanInv);
            OPCODE_MASK_SET(HNF, WriteNoSnpFullCleanSh);
            OPCODE_MASK_SET(HNF, WriteNoSnpFullCleanShPerSep);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, PCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, AtomicStore::ADD);
            OPCODE_MASK_SET(HNF, AtomicStore::CLR);
            OPCODE_MASK_SET(HNF, AtomicStore::EOR);
            OPCODE_MASK_SET(HNF, AtomicStore::SET);
            OPCODE_MASK_SET(HNF, AtomicStore::SMAX);
            OPCODE_MASK_SET(HNF, AtomicStore::SMIN);
            OPCODE_MASK_SET(HNF, AtomicStore::UMAX);
            OPCODE_MASK_SET(HNF, AtomicStore::UMIN);
            OPCODE_MASK_SET(HNF, AtomicLoad::ADD);
            OPCODE_MASK_SET(HNF, AtomicLoad::CLR);
            OPCODE_MASK_SET(HNF, AtomicLoad::EOR);
            OPCODE_MASK_SET(HNF, AtomicLoad::SET);
            OPCODE_MASK_SET(HNF, AtomicLoad::SMAX);
            OPCODE_MASK_SET(HNF, AtomicLoad::SMIN);
            OPCODE_MASK_SET(HNF, AtomicLoad::UMAX);
            OPCODE_MASK_SET(HNF, AtomicLoad::UMIN);
            OPCODE_MASK_SET(HNF, AtomicSwap);
            OPCODE_MASK_SET(HNF, AtomicCompare);
            //================================================================

            // HN-I mask
            //================================================================
            OPCODE_MASK_SET(HNI, ReqLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNI, ReadNoSnp);
            OPCODE_MASK_SET(HNI, WriteNoSnpFull);
            OPCODE_MASK_SET(HNI, WriteNoSnpPtl);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, WriteNoSnpZero);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNI, ReadClean);
            OPCODE_MASK_SET(HNI, ReadShared);
            OPCODE_MASK_SET(HNI, ReadNotSharedDirty);
            OPCODE_MASK_SET(HNI, ReadUnique);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, ReadPreferUnique);
            OPCODE_MASK_SET(HNI, MakeReadUnique);
#endif
            OPCODE_MASK_SET(HNI, CleanUnique);
            OPCODE_MASK_SET(HNI, MakeUnique);
            OPCODE_MASK_SET(HNI, Evict);
            OPCODE_MASK_SET(HNI, WriteBackFull);
            OPCODE_MASK_SET(HNI, WriteBackPtl);
            OPCODE_MASK_SET(HNI, WriteEvictFull);
            OPCODE_MASK_SET(HNI, WriteCleanFull);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, WriteEvictOrEvict);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNI, ReadOnce);
            OPCODE_MASK_SET(HNI, ReadOnceCleanInvalid);
            OPCODE_MASK_SET(HNI, ReadOnceMakeInvalid);
            OPCODE_MASK_SET(HNI, StashOnceUnique);
            OPCODE_MASK_SET(HNI, StashOnceShared);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, StashOnceSepUnique);
            OPCODE_MASK_SET(HNI, StashOnceSepShared);
#endif
            OPCODE_MASK_SET(HNI, WriteUniqueFull);
            OPCODE_MASK_SET(HNI, WriteUniqueFullStash);
            OPCODE_MASK_SET(HNI, WriteUniquePtl);
            OPCODE_MASK_SET(HNI, WriteUniquePtlStash);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, WriteUniqueZero);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNI, CleanShared);
            OPCODE_MASK_SET(HNI, CleanSharedPersist);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, CleanSharedPersistSep);
#endif
            OPCODE_MASK_SET(HNI, CleanInvalid);
            OPCODE_MASK_SET(HNI, MakeInvalid);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, WriteUniquePtlCleanSh);
            OPCODE_MASK_SET(HNI, WriteUniquePtlCleanShPerSep);
            OPCODE_MASK_SET(HNI, WriteUniqueFullCleanSh);
            OPCODE_MASK_SET(HNI, WriteUniqueFullCleanShPerSep);
            OPCODE_MASK_SET(HNI, WriteBackFullCleanInv);
            OPCODE_MASK_SET(HNI, WriteBackFullCleanSh);
            OPCODE_MASK_SET(HNI, WriteBackFullCleanShPerSep);
            OPCODE_MASK_SET(HNI, WriteCleanFullCleanSh);
            OPCODE_MASK_SET(HNI, WriteCleanFullCleanShPerSep);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, WriteNoSnpPtlCleanInv);
            OPCODE_MASK_SET(HNI, WriteNoSnpPtlCleanSh);
            OPCODE_MASK_SET(HNI, WriteNoSnpPtlCleanShPerSep);
            OPCODE_MASK_SET(HNI, WriteNoSnpFullCleanInv);
            OPCODE_MASK_SET(HNI, WriteNoSnpFullCleanSh);
            OPCODE_MASK_SET(HNI, WriteNoSnpFullCleanShPerSep);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNI, PCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNI, AtomicStore::ADD);
            OPCODE_MASK_SET(HNI, AtomicStore::CLR);
            OPCODE_MASK_SET(HNI, AtomicStore::EOR);
            OPCODE_MASK_SET(HNI, AtomicStore::SET);
            OPCODE_MASK_SET(HNI, AtomicStore::SMAX);
            OPCODE_MASK_SET(HNI, AtomicStore::SMIN);
            OPCODE_MASK_SET(HNI, AtomicStore::UMAX);
            OPCODE_MASK_SET(HNI, AtomicStore::UMIN);
            OPCODE_MASK_SET(HNI, AtomicLoad::ADD);
            OPCODE_MASK_SET(HNI, AtomicLoad::CLR);
            OPCODE_MASK_SET(HNI, AtomicLoad::EOR);
            OPCODE_MASK_SET(HNI, AtomicLoad::SET);
            OPCODE_MASK_SET(HNI, AtomicLoad::SMAX);
            OPCODE_MASK_SET(HNI, AtomicLoad::SMIN);
            OPCODE_MASK_SET(HNI, AtomicLoad::UMAX);
            OPCODE_MASK_SET(HNI, AtomicLoad::UMIN);
            OPCODE_MASK_SET(HNI, AtomicSwap);
            OPCODE_MASK_SET(HNI, AtomicCompare);
            //================================================================

            // SN-F mask
            //================================================================
            OPCODE_MASK_SET(SNF, ReqLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNF, ReadNoSnp);
            OPCODE_MASK_SET(SNF, WriteNoSnpFull);
            OPCODE_MASK_SET(SNF, WriteNoSnpPtl);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNF, WriteNoSnpZero);
            OPCODE_MASK_SET(SNF, ReadNoSnpSep);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNF, CleanShared);
            OPCODE_MASK_SET(SNF, CleanSharedPersist);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNF, CleanSharedPersistSep);
#endif
            OPCODE_MASK_SET(SNF, CleanInvalid);
            OPCODE_MASK_SET(SNF, MakeInvalid);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNF, WriteNoSnpPtlCleanInv);
            OPCODE_MASK_SET(SNF, WriteNoSnpPtlCleanSh);
            OPCODE_MASK_SET(SNF, WriteNoSnpPtlCleanShPerSep);
            OPCODE_MASK_SET(SNF, WriteNoSnpFullCleanInv);
            OPCODE_MASK_SET(SNF, WriteNoSnpFullCleanSh);
            OPCODE_MASK_SET(SNF, WriteNoSnpFullCleanShPerSep);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNF, PCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNF, AtomicStore::ADD);
            OPCODE_MASK_SET(SNF, AtomicStore::CLR);
            OPCODE_MASK_SET(SNF, AtomicStore::EOR);
            OPCODE_MASK_SET(SNF, AtomicStore::SET);
            OPCODE_MASK_SET(SNF, AtomicStore::SMAX);
            OPCODE_MASK_SET(SNF, AtomicStore::SMIN);
            OPCODE_MASK_SET(SNF, AtomicStore::UMAX);
            OPCODE_MASK_SET(SNF, AtomicStore::UMIN);
            OPCODE_MASK_SET(SNF, AtomicLoad::ADD);
            OPCODE_MASK_SET(SNF, AtomicLoad::CLR);
            OPCODE_MASK_SET(SNF, AtomicLoad::EOR);
            OPCODE_MASK_SET(SNF, AtomicLoad::SET);
            OPCODE_MASK_SET(SNF, AtomicLoad::SMAX);
            OPCODE_MASK_SET(SNF, AtomicLoad::SMIN);
            OPCODE_MASK_SET(SNF, AtomicLoad::UMAX);
            OPCODE_MASK_SET(SNF, AtomicLoad::UMIN);
            OPCODE_MASK_SET(SNF, AtomicSwap);
            OPCODE_MASK_SET(SNF, AtomicCompare);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNF, PrefetchTgt);
            //================================================================

            // SN-I mask
            //================================================================
            OPCODE_MASK_SET(SNI, ReqLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNI, ReadNoSnp);
            OPCODE_MASK_SET(SNI, WriteNoSnpFull);
            OPCODE_MASK_SET(SNI, WriteNoSnpPtl);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNI, WriteNoSnpZero);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNI, ReadNoSnpSep);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNI, CleanShared);
            OPCODE_MASK_SET(SNI, CleanSharedPersist);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNI, CleanSharedPersistSep);
#endif
            OPCODE_MASK_SET(SNI, CleanInvalid);
            OPCODE_MASK_SET(SNI, MakeInvalid);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNI, WriteNoSnpPtlCleanInv);
            OPCODE_MASK_SET(SNI, WriteNoSnpPtlCleanSh);
            OPCODE_MASK_SET(SNI, WriteNoSnpPtlCleanShPerSep);
            OPCODE_MASK_SET(SNI, WriteNoSnpFullCleanInv);
            OPCODE_MASK_SET(SNI, WriteNoSnpFullCleanSh);
            OPCODE_MASK_SET(SNI, WriteNoSnpFullCleanShPerSep);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNI, PCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNI, AtomicStore::ADD);
            OPCODE_MASK_SET(SNI, AtomicStore::CLR);
            OPCODE_MASK_SET(SNI, AtomicStore::EOR);
            OPCODE_MASK_SET(SNI, AtomicStore::SET);
            OPCODE_MASK_SET(SNI, AtomicStore::SMAX);
            OPCODE_MASK_SET(SNI, AtomicStore::SMIN);
            OPCODE_MASK_SET(SNI, AtomicStore::UMAX);
            OPCODE_MASK_SET(SNI, AtomicStore::UMIN);
            OPCODE_MASK_SET(SNI, AtomicLoad::ADD);
            OPCODE_MASK_SET(SNI, AtomicLoad::CLR);
            OPCODE_MASK_SET(SNI, AtomicLoad::EOR);
            OPCODE_MASK_SET(SNI, AtomicLoad::SET);
            OPCODE_MASK_SET(SNI, AtomicLoad::SMAX);
            OPCODE_MASK_SET(SNI, AtomicLoad::SMIN);
            OPCODE_MASK_SET(SNI, AtomicLoad::UMAX);
            OPCODE_MASK_SET(SNI, AtomicLoad::UMIN);
            OPCODE_MASK_SET(SNI, AtomicSwap);
            OPCODE_MASK_SET(SNI, AtomicCompare);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNI, PrefetchTgt);
            //================================================================

            // MN mask
            //================================================================
            OPCODE_MASK_SET(MN, ReqLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(MN, DVMOp);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(MN, PCrdReturn);
            //================================================================
        }

        #undef  OPCODE_INFO_SET
        #undef  OPCODE_MASK_SET
    }
/*
}
*/


// Implementation of: class SNP::Decoder
/*
namespace CHI {
*/
    namespace Opcodes::SNP {
        
        #define OPCODE_INFO_SET(name) \
            this->opcodes[name] \
                = OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>( \
                    OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>::Channel::SNP, \
                    name, #name)

        #define OPCODE_MASK_SET(target, name) \
            this->mask_##target[name] = true

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline Decoder<_Tflit, _Tcompanion>::~Decoder() noexcept
        { }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline Decoder<_Tflit, _Tcompanion>::Decoder() noexcept
        {
            OPCODE_INFO_SET(SnpLCrdReturn);                     // 0x00
            OPCODE_INFO_SET(SnpShared);                         // 0x01
            OPCODE_INFO_SET(SnpClean);                          // 0x02
            OPCODE_INFO_SET(SnpOnce);                           // 0x03
            OPCODE_INFO_SET(SnpNotSharedDirty);                 // 0x04
            OPCODE_INFO_SET(SnpUniqueStash);                    // 0x05
            OPCODE_INFO_SET(SnpMakeInvalidStash);               // 0x06
            OPCODE_INFO_SET(SnpUnique);                         // 0x07
            OPCODE_INFO_SET(SnpCleanShared);                    // 0x08
            OPCODE_INFO_SET(SnpCleanInvalid);                   // 0x09
            OPCODE_INFO_SET(SnpMakeInvalid);                    // 0x0A
            OPCODE_INFO_SET(SnpStashUnique);                    // 0x0B
            OPCODE_INFO_SET(SnpStashShared);                    // 0x0C
            OPCODE_INFO_SET(SnpDVMOp);                          // 0x0D
                                                                // 0x0E
                                                                // 0x0F
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_INFO_SET(SnpQuery);                          // 0x10
#endif  
            OPCODE_INFO_SET(SnpSharedFwd);                      // 0x11
            OPCODE_INFO_SET(SnpCleanFwd);                       // 0x12
            OPCODE_INFO_SET(SnpOnceFwd);                        // 0x13
            OPCODE_INFO_SET(SnpNotSharedDirtyFwd);              // 0x14
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_INFO_SET(SnpPreferUnique);                   // 0x15
            OPCODE_INFO_SET(SnpPreferUniqueFwd);                // 0x16
#endif
            OPCODE_INFO_SET(SnpUniqueFwd);                      // 0x17
                                                                // 0x18
                                                                // 0x19
                                                                // 0x1A
                                                                // 0x1B
                                                                // 0x1C
                                                                // 0x1D
                                                                // 0x1E
                                                                // 0x1F

            // RN-F mask
            //================================================================
            OPCODE_MASK_SET(RNF, SnpLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, SnpShared);
            OPCODE_MASK_SET(RNF, SnpClean);
            OPCODE_MASK_SET(RNF, SnpOnce);
            OPCODE_MASK_SET(RNF, SnpNotSharedDirty);
            OPCODE_MASK_SET(RNF, SnpUnique);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, SnpPreferUnique);
#endif
            OPCODE_MASK_SET(RNF, SnpCleanShared);
            OPCODE_MASK_SET(RNF, SnpCleanInvalid);
            OPCODE_MASK_SET(RNF, SnpMakeInvalid);
            OPCODE_MASK_SET(RNF, SnpSharedFwd);
            OPCODE_MASK_SET(RNF, SnpCleanFwd);
            OPCODE_MASK_SET(RNF, SnpOnceFwd);
            OPCODE_MASK_SET(RNF, SnpNotSharedDirtyFwd);
            OPCODE_MASK_SET(RNF, SnpUniqueFwd);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, SnpPreferUniqueFwd);
#endif
            OPCODE_MASK_SET(RNF, SnpUniqueStash);
            OPCODE_MASK_SET(RNF, SnpMakeInvalidStash);
            OPCODE_MASK_SET(RNF, SnpStashUnique);
            OPCODE_MASK_SET(RNF, SnpStashShared);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, SnpQuery);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, SnpDVMOp);
            //================================================================

            // RN-D mask
            //================================================================
            OPCODE_MASK_SET(RND, SnpLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, SnpDVMOp);
            //================================================================

            // RN-I mask
            //================================================================
            //================================================================

            // HN-F mask
            //================================================================
            OPCODE_MASK_SET(HNF, SnpLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, SnpShared);
            OPCODE_MASK_SET(HNF, SnpClean);
            OPCODE_MASK_SET(HNF, SnpOnce);
            OPCODE_MASK_SET(HNF, SnpNotSharedDirty);
            OPCODE_MASK_SET(HNF, SnpUnique);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, SnpPreferUnique);
#endif
            OPCODE_MASK_SET(HNF, SnpCleanShared);
            OPCODE_MASK_SET(HNF, SnpCleanInvalid);
            OPCODE_MASK_SET(HNF, SnpMakeInvalid);
            OPCODE_MASK_SET(HNF, SnpSharedFwd);
            OPCODE_MASK_SET(HNF, SnpCleanFwd);
            OPCODE_MASK_SET(HNF, SnpOnceFwd);
            OPCODE_MASK_SET(HNF, SnpNotSharedDirtyFwd);
            OPCODE_MASK_SET(HNF, SnpUniqueFwd);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, SnpPreferUniqueFwd);
#endif
            OPCODE_MASK_SET(HNF, SnpUniqueStash);
            OPCODE_MASK_SET(HNF, SnpMakeInvalidStash);
            OPCODE_MASK_SET(HNF, SnpStashUnique);
            OPCODE_MASK_SET(HNF, SnpStashShared);
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, SnpQuery);
#endif
            //================================================================

            // HN-I mask
            //================================================================
            //================================================================

            // SN-F mask
            //================================================================
            //================================================================

            // SN-I mask
            //================================================================
            //================================================================

            // MN mask
            //================================================================
            OPCODE_MASK_SET(MN, SnpLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(MN, SnpDVMOp);
            //================================================================
        }

        #undef OPCODE_INFO_SET
        #undef OPCODE_MASK_SET
    }
/*
}
*/


// Implementation of: class DAT::Decoder
/*
namespace CHI {
*/
    namespace Opcodes::DAT {

        #define OPCODE_INFO_SET(name) \
            this->opcodes[name] \
                = OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>( \
                    OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>::Channel::DAT, \
                    name, #name)

        #define OPCODE_MASK_SET(target, name) \
            this->mask_##target[name] = true

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline Decoder<_Tflit, _Tcompanion>::~Decoder() noexcept
        { }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline Decoder<_Tflit, _Tcompanion>::Decoder() noexcept
        {
            OPCODE_INFO_SET(DataLCrdReturn);                    // 0x00
            OPCODE_INFO_SET(SnpRespData);                       // 0x01
            OPCODE_INFO_SET(CopyBackWrData);                    // 0x02
            OPCODE_INFO_SET(NonCopyBackWrData);                 // 0x03
            OPCODE_INFO_SET(CompData);                          // 0x04
            OPCODE_INFO_SET(SnpRespDataPtl);                    // 0x05
            OPCODE_INFO_SET(SnpRespDataFwded);                  // 0x06
            OPCODE_INFO_SET(WriteDataCancel);                   // 0x07
#ifdef CHI_ISSUE_EB_ENABLE
                                                                // 0x08
                                                                // 0x09
                                                                // 0x0A
            OPCODE_INFO_SET(DataSepResp);                       // 0x0B
            OPCODE_INFO_SET(NCBWrDataCompAck);                  // 0x0C
                                                                // 0x0D
                                                                // 0x0E
                                                                // 0x0F
#endif

            // RN-F mask
            //================================================================
            OPCODE_MASK_SET(RNF, DataLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, CompData);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, DataSepResp);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, CopyBackWrData);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, WriteDataCancel);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, NonCopyBackWrData);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, NCBWrDataCompAck);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, SnpRespData);
            OPCODE_MASK_SET(RNF, SnpRespDataFwded);
            OPCODE_MASK_SET(RNF, SnpRespDataPtl);
            //================================================================

            // RN-D mask
            //================================================================
            OPCODE_MASK_SET(RND, DataLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, CompData);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RND, DataSepResp);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, WriteDataCancel);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, NonCopyBackWrData);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RND, NCBWrDataCompAck);
#endif
            //================================================================

            // RN-I mask
            //================================================================
            OPCODE_MASK_SET(RNI, DataLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNI, CompData);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNI, DataSepResp);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNI, WriteDataCancel);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNI, NonCopyBackWrData);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNI, NCBWrDataCompAck);
#endif
            //================================================================

            // HN-F mask
            //================================================================
            OPCODE_MASK_SET(HNF, DataLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, CompData);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, DataSepResp);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, CopyBackWrData);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, WriteDataCancel);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, NonCopyBackWrData);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, NCBWrDataCompAck);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, SnpRespData);
            OPCODE_MASK_SET(HNF, SnpRespDataFwded);
            OPCODE_MASK_SET(HNF, SnpRespDataPtl);
            //================================================================

            // HN-I mask
            //================================================================
            OPCODE_MASK_SET(HNI, DataLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNI, CompData);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, DataSepResp);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNI, CopyBackWrData);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNI, WriteDataCancel);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNI, NonCopyBackWrData);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, NCBWrDataCompAck);
#endif
            //================================================================

            // SN-F mask
            //================================================================
            OPCODE_MASK_SET(SNF, DataLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNF, CompData);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNF, DataSepResp);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNF, WriteDataCancel);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNF, NonCopyBackWrData);
            //================================================================

            // SN-I mask
            //================================================================
            OPCODE_MASK_SET(SNI, DataLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNI, CompData);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNI, DataSepResp);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNI, WriteDataCancel);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNI, NonCopyBackWrData);
            //================================================================

            // MN mask
            //================================================================
            OPCODE_MASK_SET(MN, DataLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(MN, NonCopyBackWrData);
            //================================================================

        }

        #undef OPCODE_INFO_SET
        #undef OPCODE_MASK_SET
    }
/*
}
*/


// Implementation of: class RSP::Decoder
/*
namespace CHI {
*/
    namespace Opcodes::RSP {

        #define OPCODE_INFO_SET(name) \
            this->opcodes[name] \
                = OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>( \
                    OpcodeInfo<typename _Tflit::opcode_t, _Tcompanion>::Channel::RSP, \
                    name, #name)

        #define OPCODE_MASK_SET(target, name) \
            this->mask_##target[name] = true

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline Decoder<_Tflit, _Tcompanion>::~Decoder() noexcept
        { }

        template<Flits::FlitOpcodeFormatConcept _Tflit, class _Tcompanion>
        inline Decoder<_Tflit, _Tcompanion>::Decoder() noexcept
        {
            OPCODE_INFO_SET(RespLCrdReturn);                    // 0x00
            OPCODE_INFO_SET(SnpResp);                           // 0x01
            OPCODE_INFO_SET(CompAck);                           // 0x02
            OPCODE_INFO_SET(RetryAck);                          // 0x03
            OPCODE_INFO_SET(Comp);                              // 0x04
            OPCODE_INFO_SET(CompDBIDResp);                      // 0x05
            OPCODE_INFO_SET(DBIDResp);                          // 0x06
            OPCODE_INFO_SET(PCrdGrant);                         // 0x07
            OPCODE_INFO_SET(ReadReceipt);                       // 0x08
            OPCODE_INFO_SET(SnpRespFwded);                      // 0x09
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_INFO_SET(TagMatch);                          // 0x0A
            OPCODE_INFO_SET(RespSepData);                       // 0x0B
            OPCODE_INFO_SET(Persist);                           // 0x0C
            OPCODE_INFO_SET(CompPersist);                       // 0x0D
            OPCODE_INFO_SET(DBIDRespOrd);                       // 0x0E
                                                                // 0x0F
            OPCODE_INFO_SET(StashDone);                         // 0x10
            OPCODE_INFO_SET(CompStashDone);                     // 0x11
                                                                // 0x12
                                                                // 0x13
            OPCODE_INFO_SET(CompCMO);                           // 0x14
                                                                // 0x15
                                                                // 0x16
                                                                // 0x17
                                                                // 0x18
                                                                // 0x19
                                                                // 0x1A
                                                                // 0x1B
                                                                // 0x1C
                                                                // 0x1D
                                                                // 0x1E
                                                                // 0x1F

            // RN-F mask
            //================================================================
            OPCODE_MASK_SET(RNF, RespLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, RetryAck);
            OPCODE_MASK_SET(RNF, PCrdGrant);
            OPCODE_MASK_SET(RNF, Comp);
            OPCODE_MASK_SET(RNF, CompDBIDResp);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, CompCMO);
#endif
            OPCODE_MASK_SET(RNF, ReadReceipt);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, RespSepData);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, DBIDResp);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, DBIDRespOrd);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, StashDone);
            OPCODE_MASK_SET(RNF, CompStashDone);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, TagMatch);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, Persist);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNF, CompPersist);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, CompAck);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, SnpResp);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNF, SnpRespFwded);
            //================================================================

            // RN-D mask
            //================================================================
            OPCODE_MASK_SET(RND, RespLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, RetryAck);
            OPCODE_MASK_SET(RND, PCrdGrant);
            OPCODE_MASK_SET(RND, Comp);
            OPCODE_MASK_SET(RND, CompDBIDResp);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RND, CompCMO);
#endif
            OPCODE_MASK_SET(RND, ReadReceipt);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RND, RespSepData);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, DBIDResp);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RND, DBIDRespOrd);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RND, StashDone);
            OPCODE_MASK_SET(RND, CompStashDone);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RND, TagMatch);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RND, Persist);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RND, CompPersist);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, CompAck);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RND, SnpResp);
            //================================================================

            // RN-I mask
            //================================================================
            OPCODE_MASK_SET(RNI, RespLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNI, RetryAck);
            OPCODE_MASK_SET(RNI, PCrdGrant);
            OPCODE_MASK_SET(RNI, Comp);
            OPCODE_MASK_SET(RNI, CompDBIDResp);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNI, CompCMO);
#endif
            OPCODE_MASK_SET(RNI, ReadReceipt);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNI, RespSepData);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNI, DBIDResp);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNI, DBIDRespOrd);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNI, StashDone);
            OPCODE_MASK_SET(RNI, CompStashDone);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNI, TagMatch);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNI, Persist);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(RNI, CompPersist);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(RNI, CompAck);
            //================================================================

            // HN-F mask
            //================================================================
            OPCODE_MASK_SET(HNF, RespLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, RetryAck);
            OPCODE_MASK_SET(HNF, PCrdGrant);
            OPCODE_MASK_SET(HNF, Comp);
            OPCODE_MASK_SET(HNF, CompDBIDResp);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, CompCMO);
#endif
            OPCODE_MASK_SET(HNF, ReadReceipt);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, RespSepData);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, DBIDResp);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, DBIDRespOrd);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, StashDone);
            OPCODE_MASK_SET(HNF, CompStashDone);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, TagMatch);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, Persist);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNF, CompPersist);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, CompAck);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, SnpResp);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNF, SnpRespFwded);
            //================================================================

            // HN-I mask
            //================================================================
            OPCODE_MASK_SET(HNI, RespLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNI, RetryAck);
            OPCODE_MASK_SET(HNI, PCrdGrant);
            OPCODE_MASK_SET(HNI, Comp);
            OPCODE_MASK_SET(HNI, CompDBIDResp);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, CompCMO);
#endif
            OPCODE_MASK_SET(HNI, ReadReceipt);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, RespSepData);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNI, DBIDResp);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, DBIDRespOrd);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, Persist);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(HNI, CompPersist);
#endif
            //----------------------------------------------------------------
            OPCODE_MASK_SET(HNI, CompAck);
            //================================================================

            // SN-F mask
            //================================================================
            OPCODE_MASK_SET(SNF, RespLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNF, RetryAck);
            OPCODE_MASK_SET(SNF, PCrdGrant);
            OPCODE_MASK_SET(SNF, Comp);
            OPCODE_MASK_SET(SNF, CompDBIDResp);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNF, CompCMO);
#endif
            OPCODE_MASK_SET(SNF, ReadReceipt);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNF, DBIDResp);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNF, TagMatch);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNF, Persist);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNF, CompPersist);
#endif
            //================================================================

            // SN-I mask
            //================================================================
            OPCODE_MASK_SET(SNI, RespLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNI, RetryAck);
            OPCODE_MASK_SET(SNI, PCrdGrant);
            OPCODE_MASK_SET(SNI, Comp);
            OPCODE_MASK_SET(SNI, CompDBIDResp);
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNI, CompCMO);
#endif
            OPCODE_MASK_SET(SNI, ReadReceipt);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(SNI, DBIDResp);
            //----------------------------------------------------------------
#ifdef  CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNI, Persist);
#endif
            //----------------------------------------------------------------
#ifdef CHI_ISSUE_EB_ENABLE
            OPCODE_MASK_SET(SNI, CompPersist);
#endif
            //================================================================

            // MN mask
            //================================================================
            OPCODE_MASK_SET(MN, RespLCrdReturn);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(MN, RetryAck);
            OPCODE_MASK_SET(MN, PCrdGrant);
            OPCODE_MASK_SET(MN, Comp);
            OPCODE_MASK_SET(MN, CompDBIDResp);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(MN, DBIDResp);
            //----------------------------------------------------------------
            OPCODE_MASK_SET(MN, SnpResp);
            //================================================================
#endif
        }
        
        #undef OPCODE_INFO_SET
        #undef OPCODE_MASK_SET
    }
/*
}
*/


#endif // __CHI__CHI_UTIL_DECODING_*

//#endif __CHI__CHI_UTIL_DECODING
