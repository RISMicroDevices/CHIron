#pragma once

#include <concepts>                             // IWYU pragma: keep
#include <cstring>
#include <unordered_map>

#include "../cohestra_interface.hpp"


namespace Cohestra {

    // TickPreHandshake (drive pins) -> Eval (clock=0) -> TickPostHandshake (sample + pop) -> Eval (clock=1)

    namespace details {

#define COHESTRA_V3_CCHI_TYPE1_PORT_REQUIREMENTS(PORT) \
        { t.cchi_t1p ## PORT ## _rxevt_ready } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxevt_valid } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxevt_bits_TxnID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxevt_bits_SrcID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxevt_bits_TgtID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxevt_bits_Opcode } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxevt_bits_Addr } -> std::convertible_to<uint64_t&>; \
        { t.cchi_t1p ## PORT ## _rxevt_bits_NS } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxevt_bits_MemAttr } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxevt_bits_TraceTag } -> std::convertible_to<uint8_t&>; \
        \
        { t.cchi_t1p ## PORT ## _rxreq_ready } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_valid } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_TxnID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_SrcID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_TgtID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_Opcode } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_Size } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_Addr } -> std::convertible_to<uint64_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_TagAlias } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_NS } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_Order } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_MemAttr } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_Excl } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_ExpCompData } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_TraceTag } -> std::convertible_to<uint8_t&>; \
        \
        { t.cchi_t1p ## PORT ## _txsnp_ready } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txsnp_valid } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txsnp_bits_TxnID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txsnp_bits_SrcID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txsnp_bits_TgtID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txsnp_bits_Opcode } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txsnp_bits_Addr } -> std::convertible_to<uint64_t&>; \
        { t.cchi_t1p ## PORT ## _txsnp_bits_NS } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txsnp_bits_TraceTag } -> std::convertible_to<uint8_t&>; \
        \
        { t.cchi_t1p ## PORT ## _txrsp_ready } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txrsp_valid } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txrsp_bits_TxnID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txrsp_bits_SrcID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txrsp_bits_TgtID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txrsp_bits_DBID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txrsp_bits_Opcode } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txrsp_bits_RespErr } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txrsp_bits_Resp } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txrsp_bits_CBusy } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txrsp_bits_TraceTag } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxrsp_ready } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxrsp_valid } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxrsp_bits_TxnID } -> std::convertible_to<uint8_t>; \
        { t.cchi_t1p ## PORT ## _rxrsp_bits_SrcID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxrsp_bits_TgtID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxrsp_bits_Opcode } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxrsp_bits_RespErr } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxrsp_bits_Resp } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxrsp_bits_TraceTag } -> std::convertible_to<uint8_t&>; \
        \
        { t.cchi_t1p ## PORT ## _txdat_ready } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txdat_valid } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txdat_bits_TxnID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txdat_bits_SrcID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txdat_bits_TgtID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txdat_bits_DBID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txdat_bits_Opcode } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txdat_bits_RespErr } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txdat_bits_Resp } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txdat_bits_DataSource } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txdat_bits_CBusy } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txdat_bits_DataID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txdat_bits_Data[0] } -> std::convertible_to<uint32_t&>; \
        { t.cchi_t1p ## PORT ## _txdat_bits_TraceTag } -> std::convertible_to<uint8_t&>; \
        \
        { t.cchi_t1p ## PORT ## _rxdat_ready } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxdat_valid } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxdat_bits_TxnID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxdat_bits_SrcID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxdat_bits_TgtID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxdat_bits_Opcode } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxdat_bits_RespErr } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxdat_bits_Resp } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxdat_bits_DataID } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxdat_bits_Data[0] } -> std::convertible_to<uint32_t&>; \
        { t.cchi_t1p ## PORT ## _rxdat_bits_BE } -> std::convertible_to<uint32_t&>; \
        { t.cchi_t1p ## PORT ## _rxdat_bits_TraceTag } -> std::convertible_to<uint8_t&>;

#define COHESTRA_V3_CCHI_TYPE1_UW_PREDICT_REQUIREMENTS(PORT) \
        { t.cchi_t1p ## PORT ## _rxreq_bits_WayValid } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_Way } -> std::convertible_to<uint8_t&>;

#define COHESTRA_V3_CCHI_TYPE1_UW_PERSIST_REQUIREMENTS(PORT) \
        { t.cchi_t1p ## PORT ## _rxevt_bits_WayValid } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxevt_bits_Way } -> std::convertible_to<uint8_t&>; \
        \
        { t.cchi_t1p ## PORT ## _rxreq_bits_WayValid } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _rxreq_bits_Way } -> std::convertible_to<uint8_t&>; \
        \
        { t.cchi_t1p ## PORT ## _txrsp_bits_WayValid } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txrsp_bits_Way } -> std::convertible_to<uint8_t&>; \
        \
        { t.cchi_t1p ## PORT ## _txdat_bits_WayValid } -> std::convertible_to<uint8_t&>; \
        { t.cchi_t1p ## PORT ## _txdat_bits_Way } -> std::convertible_to<uint8_t&>;

#define COHESTRA_V3_DECLARE_CCHI_TYPE1_PORT_CONCEPTS(PORT) \
        template<class T> \
        concept V3CCHIModuleT1P ## PORT = requires (T t) { \
            COHESTRA_V3_CCHI_TYPE1_PORT_REQUIREMENTS(PORT) \
        }; \
        template<class T> \
        concept V3CCHIModuleT1P ## PORT ## UWPredict = requires (T t) { \
            COHESTRA_V3_CCHI_TYPE1_UW_PREDICT_REQUIREMENTS(PORT) \
        }; \
        template<class T> \
        concept V3CCHIModuleT1P ## PORT ## UWPersist = requires (T t) { \
            COHESTRA_V3_CCHI_TYPE1_UW_PERSIST_REQUIREMENTS(PORT) \
        };

        COHESTRA_V3_DECLARE_CCHI_TYPE1_PORT_CONCEPTS(0)
        COHESTRA_V3_DECLARE_CCHI_TYPE1_PORT_CONCEPTS(1)
        COHESTRA_V3_DECLARE_CCHI_TYPE1_PORT_CONCEPTS(2)
        COHESTRA_V3_DECLARE_CCHI_TYPE1_PORT_CONCEPTS(3)
        COHESTRA_V3_DECLARE_CCHI_TYPE1_PORT_CONCEPTS(4)
        COHESTRA_V3_DECLARE_CCHI_TYPE1_PORT_CONCEPTS(5)
        COHESTRA_V3_DECLARE_CCHI_TYPE1_PORT_CONCEPTS(6)
        COHESTRA_V3_DECLARE_CCHI_TYPE1_PORT_CONCEPTS(7)

#undef COHESTRA_V3_DECLARE_CCHI_TYPE1_PORT_CONCEPTS
#undef COHESTRA_V3_CCHI_TYPE1_UW_PERSIST_REQUIREMENTS
#undef COHESTRA_V3_CCHI_TYPE1_UW_PREDICT_REQUIREMENTS
#undef COHESTRA_V3_CCHI_TYPE1_PORT_REQUIREMENTS

        template<class T>
        class ResizeableFIFO {
        protected:
            size_t  size    = 0;
            size_t  count   = 0;
            T*      storage = nullptr;

            size_t  wptr    = 0;
            size_t  rptr    = 0;

        public:
            size_t      Size() const noexcept;
            size_t      Count() const noexcept;
            bool        IsEmpty() const noexcept;
            bool        IsFull() const noexcept;

            void        Clear() noexcept;
            bool        TryResize(size_t newSize) noexcept;

            T*          Peek() noexcept;
            const T*    Peek() const noexcept;

            T*          Pop() noexcept;

            bool        Push(const T& item) noexcept;
        };
    }


    template<class VT>
    class V3CCHIInterface : public CCHIInterface {
    public:
        static constexpr bool HasType1Port0() noexcept;
        static constexpr bool HasType1Port0UWPredict() noexcept;
        static constexpr bool HasType1Port0UWPersist() noexcept;
        static constexpr bool HasType1Port1() noexcept;
        static constexpr bool HasType1Port1UWPredict() noexcept;
        static constexpr bool HasType1Port1UWPersist() noexcept;
        static constexpr bool HasType1Port2() noexcept;
        static constexpr bool HasType1Port2UWPredict() noexcept;
        static constexpr bool HasType1Port2UWPersist() noexcept;
        static constexpr bool HasType1Port3() noexcept;
        static constexpr bool HasType1Port3UWPredict() noexcept;
        static constexpr bool HasType1Port3UWPersist() noexcept;
        static constexpr bool HasType1Port4() noexcept;
        static constexpr bool HasType1Port4UWPredict() noexcept;
        static constexpr bool HasType1Port4UWPersist() noexcept;
        static constexpr bool HasType1Port5() noexcept;
        static constexpr bool HasType1Port5UWPredict() noexcept;
        static constexpr bool HasType1Port5UWPersist() noexcept;
        static constexpr bool HasType1Port6() noexcept;
        static constexpr bool HasType1Port6UWPredict() noexcept;
        static constexpr bool HasType1Port6UWPersist() noexcept;
        static constexpr bool HasType1Port7() noexcept;
        static constexpr bool HasType1Port7UWPredict() noexcept;
        static constexpr bool HasType1Port7UWPersist() noexcept;

        static constexpr bool HasType1Port(size_t index) noexcept;
        static constexpr bool HasType1PortUWPredict(size_t index) noexcept;
        static constexpr bool HasType1PortUWPersist(size_t index) noexcept;

    private:
        VT* verilated;

        std::set<size_t> type1Indices;

        std::unordered_map<size_t, details::ResizeableFIFO<CCHI::Flits::EVT<CommonFlitConfigurationType1>>> type1EVTBuffers;
        std::unordered_map<size_t, details::ResizeableFIFO<CCHI::Flits::REQ<CommonFlitConfigurationType1>>> type1REQBuffers;
    //  std::unordered_map<size_t, details::ResizeableFIFO<CCHI::Flits::SNP<CommonFlitConfigurationType1>>> type1SNPBuffers;
    //  std::unordered_map<size_t, details::ResizeableFIFO<CCHI::Flits::DnRSP<CommonFlitConfigurationType1>>> type1DnRSPBuffers;
    //  std::unordered_map<size_t, details::ResizeableFIFO<CCHI::Flits::DnDAT<CommonFlitConfigurationType1>>> type1DnDATBuffers;
        std::unordered_map<size_t, details::ResizeableFIFO<CCHI::Flits::UpRSP<CommonFlitConfigurationType1>>> type1UpRSPBuffers;
        std::unordered_map<size_t, details::ResizeableFIFO<CCHI::Flits::UpDAT<CommonFlitConfigurationType1>>> type1UpDATBuffers;

        // ! NOTICE: When there is no buffer (or 0 size) enabled for a TX (upstream -> downstream) port,
        // !         the 'ready' signal of that port was not allowed to be dependent on the 'valid' signal
        // !         and any of its payload bundle field at the same cycle.

    public:
        V3CCHIInterface(VT* module) noexcept;
        virtual ~V3CCHIInterface() noexcept = default;

    public:
        virtual void    TickPreHandshake(uint64_t time) noexcept override;
        virtual void    TickPostHandshake(uint64_t time) noexcept override;

    public:
        virtual size_t  GetType1Count() const noexcept override;

        virtual std::set<size_t>
                        GetType1Indices() const noexcept override;

        virtual size_t  GetType1MaxIndex() const noexcept override;

        virtual bool    HasType1SNP(size_t index) const noexcept override;
        virtual std::optional<CCHI::Flits::SNP<CommonFlitConfigurationType1>>
                        PeekType1SNP(size_t index) const noexcept override;
        virtual std::optional<CCHI::Flits::SNP<CommonFlitConfigurationType1>>
                        PopType1SNP(size_t index) noexcept override;

        virtual bool    HasType1DnRSP(size_t index) const noexcept override;
        virtual std::optional<CCHI::Flits::DnRSP<CommonFlitConfigurationType1>>
                        PeekType1DnRSP(size_t index) const noexcept override;
        virtual std::optional<CCHI::Flits::DnRSP<CommonFlitConfigurationType1>>
                        PopType1DnRSP(size_t index) noexcept override;

        virtual bool    HasType1DnDAT(size_t index) const noexcept override;
        virtual std::optional<CCHI::Flits::DnDAT<CommonFlitConfigurationType1>>
                        PeekType1DnDAT(size_t index) const noexcept override;
        virtual std::optional<CCHI::Flits::DnDAT<CommonFlitConfigurationType1>>
                        PopType1DnDAT(size_t index) noexcept override;

        virtual bool    PushType1EVT(size_t index, const CCHI::Flits::EVT<CommonFlitConfigurationType1>& flit) noexcept override;

        virtual bool    PushType1REQ(size_t index, const CCHI::Flits::REQ<CommonFlitConfigurationType1>& flit) noexcept override;

        virtual bool    PushType1UpRSP(size_t index, const CCHI::Flits::UpRSP<CommonFlitConfigurationType1>& flit) noexcept override;

        virtual bool    PushType1UpDAT(size_t index, const CCHI::Flits::UpDAT<CommonFlitConfigurationType1>& flit) noexcept override;

    public:
        bool            SetType1EVTBufferSize(size_t index, size_t bufferSize = 0) noexcept;
        bool            SetType1REQBufferSize(size_t index, size_t bufferSize = 0) noexcept;
        bool            SetType1SNPBufferSize(size_t index, size_t bufferSize = 0) noexcept;
        bool            SetType1DnRSPBufferSize(size_t index, size_t bufferSize = 0) noexcept;
        bool            SetType1DnDATBufferSize(size_t index, size_t bufferSize = 0) noexcept;
        bool            SetType1UpRSPBufferSize(size_t index, size_t bufferSize = 0) noexcept;
        bool            SetType1UpDATBufferSize(size_t index, size_t bufferSize = 0) noexcept;

        bool            SetAllType1EVTBufferSize(size_t bufferSize = 0) noexcept override;
        bool            SetAllType1REQBufferSize(size_t bufferSize = 0) noexcept override;
        bool            SetAllType1SNPBufferSize(size_t bufferSize = 0) noexcept;
        bool            SetAllType1DnRSPBufferSize(size_t bufferSize = 0) noexcept;
        bool            SetAllType1DnDATBufferSize(size_t bufferSize = 0) noexcept;
        bool            SetAllType1UpRSPBufferSize(size_t bufferSize = 0) noexcept override;
        bool            SetAllType1UpDATBufferSize(size_t bufferSize = 0) noexcept override;
    };
}


// Implementation of: class details::ResizeableFIFO
namespace Cohestra::details {

    template<class T>
    size_t ResizeableFIFO<T>::Size() const noexcept
    {
        return size;
    }

    template<class T>
    size_t ResizeableFIFO<T>::Count() const noexcept
    {
        return count;
    }

    template<class T>
    bool ResizeableFIFO<T>::IsEmpty() const noexcept
    {
        return count == 0;
    }

    template<class T>
    bool ResizeableFIFO<T>::IsFull() const noexcept
    {
        return count == size;
    }

    template<class T>
    void ResizeableFIFO<T>::Clear() noexcept
    {
        count = 0;
        wptr = 0;
        rptr = 0;
    }

    template<class T>
    bool ResizeableFIFO<T>::TryResize(size_t newSize) noexcept
    {
        if (newSize == size)
            return true;

        if (newSize < count)
            return false;

        if (newSize == 0)
        {
            delete[] storage;

            storage = nullptr;
            size = 0;
            count = 0;
            wptr = 0;
            rptr = 0;

            return true;
        }

        T* newStorage = new T[newSize];
        for (size_t i = 0; i < count; ++i)
            newStorage[i] = storage[(rptr + i) % size];

        delete[] storage;
        storage = newStorage;
        size = newSize;
        wptr = count;
        rptr = 0;

        return true;
    }

    template<class T>
    T* ResizeableFIFO<T>::Peek() noexcept
    {
        if (count == 0)
            return nullptr;

        return &storage[rptr];
    }

    template<class T>
    const T* ResizeableFIFO<T>::Peek() const noexcept
    {
        if (count == 0)
            return nullptr;

        return &storage[rptr];
    }

    template<class T>
    T* ResizeableFIFO<T>::Pop() noexcept
    {
        if (count == 0)
            return nullptr;

        T* item = &storage[rptr];
        rptr = (rptr + 1) % size;
        --count;

        return item;
    }

    template<class T>
    bool ResizeableFIFO<T>::Push(const T& item) noexcept
    {
        if (count == size)
            return false;

        storage[wptr] = item;
        wptr = (wptr + 1) % size;
        ++count;

        return true;
    }
}


// Implementation of: class V3CCHIInterface
namespace Cohestra {

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port0() noexcept
    {
        return details::V3CCHIModuleT1P0<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port0UWPredict() noexcept
    {
        return details::V3CCHIModuleT1P0UWPredict<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port0UWPersist() noexcept
    {
        return details::V3CCHIModuleT1P0UWPersist<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port1() noexcept
    {
        return details::V3CCHIModuleT1P1<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port1UWPredict() noexcept
    {
        return details::V3CCHIModuleT1P1UWPredict<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port1UWPersist() noexcept
    {
        return details::V3CCHIModuleT1P1UWPersist<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port2() noexcept
    {
        return details::V3CCHIModuleT1P2<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port2UWPredict() noexcept
    {
        return details::V3CCHIModuleT1P2UWPredict<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port2UWPersist() noexcept
    {
        return details::V3CCHIModuleT1P2UWPersist<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port3() noexcept
    {
        return details::V3CCHIModuleT1P3<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port3UWPredict() noexcept
    {
        return details::V3CCHIModuleT1P3UWPredict<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port3UWPersist() noexcept
    {
        return details::V3CCHIModuleT1P3UWPersist<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port4() noexcept
    {
        return details::V3CCHIModuleT1P4<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port4UWPredict() noexcept
    {
        return details::V3CCHIModuleT1P4UWPredict<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port4UWPersist() noexcept
    {
        return details::V3CCHIModuleT1P4UWPersist<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port5() noexcept
    {
        return details::V3CCHIModuleT1P5<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port5UWPredict() noexcept
    {
        return details::V3CCHIModuleT1P5UWPredict<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port5UWPersist() noexcept
    {
        return details::V3CCHIModuleT1P5UWPersist<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port6() noexcept
    {
        return details::V3CCHIModuleT1P6<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port6UWPredict() noexcept
    {
        return details::V3CCHIModuleT1P6UWPredict<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port6UWPersist() noexcept
    {
        return details::V3CCHIModuleT1P6UWPersist<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port7() noexcept
    {
        return details::V3CCHIModuleT1P7<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port7UWPredict() noexcept
    {
        return details::V3CCHIModuleT1P7UWPredict<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port7UWPersist() noexcept
    {
        return details::V3CCHIModuleT1P7UWPersist<VT>;
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1Port(size_t index) noexcept
    {
        switch (index)
        {
            case 0: return HasType1Port0();
            case 1: return HasType1Port1();
            case 2: return HasType1Port2();
            case 3: return HasType1Port3();
            case 4: return HasType1Port4();
            case 5: return HasType1Port5();
            case 6: return HasType1Port6();
            case 7: return HasType1Port7();
            default:
                return false;
        }
    }

    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1PortUWPredict(size_t index) noexcept
    {
        switch (index)
        {
            case 0: return HasType1Port0UWPredict();
            case 1: return HasType1Port1UWPredict();
            case 2: return HasType1Port2UWPredict();
            case 3: return HasType1Port3UWPredict();
            case 4: return HasType1Port4UWPredict();
            case 5: return HasType1Port5UWPredict();
            case 6: return HasType1Port6UWPredict();
            case 7: return HasType1Port7UWPredict();
            default:
                return false;
        }
    }
    
    template<class VT>
    inline constexpr bool V3CCHIInterface<VT>::HasType1PortUWPersist(size_t index) noexcept
    {
        switch (index)
        {
            case 0: return HasType1Port0UWPersist();
            case 1: return HasType1Port1UWPersist();
            case 2: return HasType1Port2UWPersist();
            case 3: return HasType1Port3UWPersist();
            case 4: return HasType1Port4UWPersist();
            case 5: return HasType1Port5UWPersist();
            case 6: return HasType1Port6UWPersist();
            case 7: return HasType1Port7UWPersist();
            default:
                return false;
        }
    }

    template<class VT>
    inline V3CCHIInterface<VT>::V3CCHIInterface(VT* verilated) noexcept
        : verilated(verilated)
    {
#define _INIT_TYPE1(index) \
        if constexpr (HasType1Port##index()) \
        { \
            type1Indices.insert(index); \
            \
        /* FIFO the DUT-facing channels with default depth 4: routes every \
           upstream flit through the buffer so that upstream model retirement \
           happens at enqueue time, decoupled from the DUT's ready timing. \
           The empty-buffer direct-drive fast path retires on a stale ready \
           sample, which re-presents the flit when the DUT's ready depends \
           combinationally on valid. */ \
            type1EVTBuffers[index].TryResize(4); \
            type1REQBuffers[index].TryResize(4); \
        /*  type1SNPBuffers[index];*/ \
        /*  type1DnRSPBuffers[index];*/ \
        /*  type1DnDATBuffers[index];*/ \
            type1UpRSPBuffers[index].TryResize(4); \
            type1UpDATBuffers[index].TryResize(4); \
        }

        _INIT_TYPE1(0)
        _INIT_TYPE1(1)
        _INIT_TYPE1(2)
        _INIT_TYPE1(3)
        _INIT_TYPE1(4)
        _INIT_TYPE1(5)
        _INIT_TYPE1(6)
        _INIT_TYPE1(7)

        // TODO: support more ports

#undef _INIT_TYPE1

        // TODO: support more device types in future
    }

    template<class VT>
    inline void V3CCHIInterface<VT>::TickPreHandshake(uint64_t time) noexcept
    {
        // Drive phase (cycle start): clear the harness-driven ready pins and
        // (re-)drive the head flit of every buffered harness->DUT channel.
        // NO pops here: retirement happens in TickPostHandshake on the
        // mid-cycle settled valid/ready (== the DUT's upcoming posedge sample).
#define _TICK_TYPE1(port) \
        if constexpr (HasType1Port##port()) \
        { \
            details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
            \
            t1p##port->cchi_t1p##port##_txsnp_ready = 0; \
            t1p##port->cchi_t1p##port##_txrsp_ready = 0; \
            t1p##port->cchi_t1p##port##_txdat_ready = 0; \
            \
            if (auto it = type1EVTBuffers.find(port); it != type1EVTBuffers.end()) \
            { \
                t1p##port->cchi_t1p##port##_rxevt_valid = 0; \
                \
                if (!it->second.IsEmpty()) \
                { \
                    const auto& flit = *it->second.Peek(); \
                    \
                    t1p##port->cchi_t1p##port##_rxevt_valid         = 1; \
                    t1p##port->cchi_t1p##port##_rxevt_bits_TxnID    = flit.TxnID; \
                    t1p##port->cchi_t1p##port##_rxevt_bits_SrcID    = flit.SrcID; \
                    t1p##port->cchi_t1p##port##_rxevt_bits_TgtID    = flit.TgtID; \
                    t1p##port->cchi_t1p##port##_rxevt_bits_Opcode   = flit.Opcode; \
                    t1p##port->cchi_t1p##port##_rxevt_bits_Addr     = flit.Addr; \
                    t1p##port->cchi_t1p##port##_rxevt_bits_NS       = flit.NS; \
                    t1p##port->cchi_t1p##port##_rxevt_bits_MemAttr  = flit.MemAttr; \
                    \
                    if constexpr (HasType1Port##port##UWPersist()) \
                    { \
                        t1p##port->cchi_t1p##port##_rxevt_bits_WayValid = flit.WayValid; \
                        t1p##port->cchi_t1p##port##_rxevt_bits_Way      = flit.Way; \
                    } \
                    \
                    t1p##port->cchi_t1p##port##_rxevt_bits_TraceTag = flit.TraceTag; \
                } \
            } \
            \
            if (auto it = type1REQBuffers.find(port); it != type1REQBuffers.end()) \
            { \
                t1p##port->cchi_t1p##port##_rxreq_valid = 0; \
                \
                if (!it->second.IsEmpty()) \
                { \
                    const auto& flit = *it->second.Peek(); \
                    \
                    t1p##port->cchi_t1p##port##_rxreq_valid            = 1; \
                    t1p##port->cchi_t1p##port##_rxreq_bits_TxnID       = flit.TxnID; \
                    t1p##port->cchi_t1p##port##_rxreq_bits_SrcID       = flit.SrcID; \
                    t1p##port->cchi_t1p##port##_rxreq_bits_TgtID       = flit.TgtID; \
                    t1p##port->cchi_t1p##port##_rxreq_bits_Opcode      = flit.Opcode; \
                    t1p##port->cchi_t1p##port##_rxreq_bits_Size        = flit.Size; \
                    t1p##port->cchi_t1p##port##_rxreq_bits_Addr        = flit.Addr; \
                    if constexpr (CCHI::Flits::REQ<CommonFlitConfigurationType1>::hasTagAlias) \
                        t1p##port->cchi_t1p##port##_rxreq_bits_TagAlias = flit.TagAlias; \
                    else \
                        t1p##port->cchi_t1p##port##_rxreq_bits_TagAlias = 0; \
                    t1p##port->cchi_t1p##port##_rxreq_bits_NS          = flit.NS; \
                    t1p##port->cchi_t1p##port##_rxreq_bits_Order       = flit.Order; \
                    t1p##port->cchi_t1p##port##_rxreq_bits_MemAttr     = flit.MemAttr; \
                    t1p##port->cchi_t1p##port##_rxreq_bits_Excl        = flit.Excl; \
                    t1p##port->cchi_t1p##port##_rxreq_bits_ExpCompData = flit.ExpCompData; \
                    \
                    if constexpr (HasType1Port##port##UWPredict() || HasType1Port##port##UWPersist()) \
                    { \
                        t1p##port->cchi_t1p##port##_rxreq_bits_WayValid = flit.WayValid; \
                        t1p##port->cchi_t1p##port##_rxreq_bits_Way      = flit.Way; \
                    } \
                    else \
                    { \
                        t1p##port->cchi_t1p##port##_rxreq_bits_WayValid = 0; \
                        t1p##port->cchi_t1p##port##_rxreq_bits_Way      = 0; \
                    } \
                    \
                    t1p##port->cchi_t1p##port##_rxreq_bits_TraceTag = flit.TraceTag; \
                } \
            } \
            \
            if (auto it = type1UpRSPBuffers.find(port); it != type1UpRSPBuffers.end()) \
            { \
                t1p##port->cchi_t1p##port##_rxrsp_valid = 0; \
                \
                if (!it->second.IsEmpty()) \
                { \
                    const auto& flit = *it->second.Peek(); \
                    \
                    t1p##port->cchi_t1p##port##_rxrsp_valid         = 1; \
                    t1p##port->cchi_t1p##port##_rxrsp_bits_TxnID    = flit.TxnID; \
                    t1p##port->cchi_t1p##port##_rxrsp_bits_SrcID    = flit.SrcID; \
                    t1p##port->cchi_t1p##port##_rxrsp_bits_TgtID    = flit.TgtID; \
                    t1p##port->cchi_t1p##port##_rxrsp_bits_Opcode   = flit.Opcode; \
                    t1p##port->cchi_t1p##port##_rxrsp_bits_RespErr  = flit.RespErr; \
                    t1p##port->cchi_t1p##port##_rxrsp_bits_Resp     = flit.Resp; \
                    t1p##port->cchi_t1p##port##_rxrsp_bits_TraceTag = flit.TraceTag; \
                } \
            } \
            \
            if (auto it = type1UpDATBuffers.find(port); it != type1UpDATBuffers.end()) \
            { \
                t1p##port->cchi_t1p##port##_rxdat_valid = 0; \
                \
                if (!it->second.IsEmpty()) \
                { \
                    const auto& flit = *it->second.Peek(); \
                    \
                    t1p##port->cchi_t1p##port##_rxdat_valid         = 1; \
                    t1p##port->cchi_t1p##port##_rxdat_bits_TxnID    = flit.TxnID; \
                    t1p##port->cchi_t1p##port##_rxdat_bits_SrcID    = flit.SrcID; \
                    t1p##port->cchi_t1p##port##_rxdat_bits_TgtID    = flit.TgtID; \
                    t1p##port->cchi_t1p##port##_rxdat_bits_Opcode   = flit.Opcode; \
                    t1p##port->cchi_t1p##port##_rxdat_bits_RespErr  = flit.RespErr; \
                    t1p##port->cchi_t1p##port##_rxdat_bits_Resp     = flit.Resp; \
                    t1p##port->cchi_t1p##port##_rxdat_bits_DataID   = flit.DataID; \
                    std::memcpy(&t1p##port->cchi_t1p##port##_rxdat_bits_Data[0], flit.Data, sizeof(flit.Data)); \
                    t1p##port->cchi_t1p##port##_rxdat_bits_BE       = flit.BE; \
                    t1p##port->cchi_t1p##port##_rxdat_bits_TraceTag = flit.TraceTag; \
                } \
            } \
        }

        _TICK_TYPE1(0)
        _TICK_TYPE1(1)
        _TICK_TYPE1(2)
        _TICK_TYPE1(3)
        _TICK_TYPE1(4)
        _TICK_TYPE1(5)
        _TICK_TYPE1(6)
        _TICK_TYPE1(7)

        // TODO: support more ports

#undef _TICK_TYPE1
    }

    template<class VT>
    inline void V3CCHIInterface<VT>::TickPostHandshake(uint64_t time) noexcept
    {
        // Retire phase (mid-cycle): called after the clock=0 eval and before
        // the posedge eval. The settled valid/ready pairs at this point are
        // exactly what the DUT registers at the upcoming posedge, so the FIFO
        // pop here agrees 1:1 with the DUT's capture. Popping on the live pins
        // at cycle start instead reads the post-posedge state, which diverges
        // whenever 'ready' rises exactly at a posedge (e.g. an entry freed by
        // a dealloc at that edge combinationally opening an allocation grant):
        // the flit would be popped without the DUT ever capturing it.
        // No pin writes here: the popped flit stays on the pins through the
        // capturing posedge; the next head is driven at the next
        // TickPreHandshake.
#define _TICKPOST_TYPE1(port) \
        if constexpr (HasType1Port##port()) \
        { \
            details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
            \
            if (auto it = type1EVTBuffers.find(port); it != type1EVTBuffers.end()) \
            { \
                if (t1p##port->cchi_t1p##port##_rxevt_valid && t1p##port->cchi_t1p##port##_rxevt_ready) \
                { \
                    if (it->second.IsEmpty()) \
                    { \
                        /* TODO: FIFO underflow exception event */ \
                    } \
                    \
                    it->second.Pop(); \
                } \
            } \
            \
            if (auto it = type1REQBuffers.find(port); it != type1REQBuffers.end()) \
            { \
                if (t1p##port->cchi_t1p##port##_rxreq_valid && t1p##port->cchi_t1p##port##_rxreq_ready) \
                { \
                    if (it->second.IsEmpty()) \
                    { \
                        /* TODO: FIFO underflow exception event */ \
                    } \
                    \
                    it->second.Pop(); \
                } \
            } \
            \
            if (auto it = type1UpRSPBuffers.find(port); it != type1UpRSPBuffers.end()) \
            { \
                if (t1p##port->cchi_t1p##port##_rxrsp_valid && t1p##port->cchi_t1p##port##_rxrsp_ready) \
                { \
                    if (it->second.IsEmpty()) \
                    { \
                        /* TODO: FIFO underflow exception event */ \
                    } \
                    \
                    it->second.Pop(); \
                } \
            } \
            \
            if (auto it = type1UpDATBuffers.find(port); it != type1UpDATBuffers.end()) \
            { \
                if (t1p##port->cchi_t1p##port##_rxdat_valid && t1p##port->cchi_t1p##port##_rxdat_ready) \
                { \
                    if (it->second.IsEmpty()) \
                    { \
                        /* TODO: FIFO underflow exception event */ \
                    } \
                    \
                    it->second.Pop(); \
                } \
            } \
        }

        _TICKPOST_TYPE1(0)
        _TICKPOST_TYPE1(1)
        _TICKPOST_TYPE1(2)
        _TICKPOST_TYPE1(3)
        _TICKPOST_TYPE1(4)
        _TICKPOST_TYPE1(5)
        _TICKPOST_TYPE1(6)
        _TICKPOST_TYPE1(7)

        // TODO: support more ports

#undef _TICKPOST_TYPE1
    }

    template<class VT>
    inline size_t V3CCHIInterface<VT>::GetType1Count() const noexcept
    {
        return type1Indices.size();
    }

    template<class VT>
    inline std::set<size_t> V3CCHIInterface<VT>::GetType1Indices() const noexcept
    {
        return type1Indices;
    }

    template<class VT>
    inline size_t V3CCHIInterface<VT>::GetType1MaxIndex() const noexcept
    {
        if (type1Indices.empty())
            return 0;

        return *type1Indices.rbegin();
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::HasType1SNP(size_t index) const noexcept
    {
#define _HAS_TYPE1_SNP(port) \
        if (index == port) \
        { \
            if constexpr (HasType1Port##port()) \
            { \
                details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
                return t1p##port->cchi_t1p##port##_txsnp_valid != 0; \
            } \
            else \
                return false; \
        }

        _HAS_TYPE1_SNP(0)
        _HAS_TYPE1_SNP(1)
        _HAS_TYPE1_SNP(2)
        _HAS_TYPE1_SNP(3)
        _HAS_TYPE1_SNP(4)
        _HAS_TYPE1_SNP(5)
        _HAS_TYPE1_SNP(6)
        _HAS_TYPE1_SNP(7)

        // TODO: support more ports

#undef _HAS_TYPE1_SNP

        return false;
    }

    template<class VT>
    inline std::optional<CCHI::Flits::SNP<CommonFlitConfigurationType1>> V3CCHIInterface<VT>::PeekType1SNP(size_t index) const noexcept
    {
#define _PEEK_TYPE1_SNP(port) \
        if (index == port) \
        { \
            if constexpr (HasType1Port##port()) \
            { \
                details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
                \
                if (t1p##port->cchi_t1p##port##_txsnp_valid != 0) \
                { \
                    CCHI::Flits::SNP<CommonFlitConfigurationType1> flit; \
                    flit.TxnID      = t1p##port->cchi_t1p##port##_txsnp_bits_TxnID; \
                    flit.SrcID      = t1p##port->cchi_t1p##port##_txsnp_bits_SrcID; \
                    flit.TgtID      = t1p##port->cchi_t1p##port##_txsnp_bits_TgtID; \
                    flit.Opcode     = t1p##port->cchi_t1p##port##_txsnp_bits_Opcode; \
                    flit.Addr       = t1p##port->cchi_t1p##port##_txsnp_bits_Addr; \
                    flit.NS         = t1p##port->cchi_t1p##port##_txsnp_bits_NS; \
                    flit.TraceTag   = t1p##port->cchi_t1p##port##_txsnp_bits_TraceTag; \
                    \
                    return { flit }; \
                } \
                else \
                    return std::nullopt; \
            } \
        }

        _PEEK_TYPE1_SNP(0)
        _PEEK_TYPE1_SNP(1)
        _PEEK_TYPE1_SNP(2)
        _PEEK_TYPE1_SNP(3)
        _PEEK_TYPE1_SNP(4)
        _PEEK_TYPE1_SNP(5)
        _PEEK_TYPE1_SNP(6)
        _PEEK_TYPE1_SNP(7)

        // TODO: support more ports

#undef _PEEK_TYPE1_SNP

        return std::nullopt;
    }

    template<class VT>
    inline std::optional<CCHI::Flits::SNP<CommonFlitConfigurationType1>> V3CCHIInterface<VT>::PopType1SNP(size_t index) noexcept
    {
#define _POP_TYPE1_SNP(port) \
        if (index == port) \
        { \
            if constexpr (HasType1Port##port()) \
            { \
                details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
                \
                if (t1p##port->cchi_t1p##port##_txsnp_valid != 0) \
                { \
                    CCHI::Flits::SNP<CommonFlitConfigurationType1> flit; \
                    flit.TxnID      = t1p##port->cchi_t1p##port##_txsnp_bits_TxnID; \
                    flit.SrcID      = t1p##port->cchi_t1p##port##_txsnp_bits_SrcID; \
                    flit.TgtID      = t1p##port->cchi_t1p##port##_txsnp_bits_TgtID; \
                    flit.Opcode     = t1p##port->cchi_t1p##port##_txsnp_bits_Opcode; \
                    flit.Addr       = t1p##port->cchi_t1p##port##_txsnp_bits_Addr; \
                    flit.NS         = t1p##port->cchi_t1p##port##_txsnp_bits_NS; \
                    flit.TraceTag   = t1p##port->cchi_t1p##port##_txsnp_bits_TraceTag; \
                    \
                    t1p##port->cchi_t1p##port##_txsnp_ready = 1; /* cleared on TickPreHandshake */ \
                    \
                    return { flit }; \
                } \
                else \
                    return std::nullopt; \
            } \
        }

        _POP_TYPE1_SNP(0)
        _POP_TYPE1_SNP(1)
        _POP_TYPE1_SNP(2)
        _POP_TYPE1_SNP(3)
        _POP_TYPE1_SNP(4)
        _POP_TYPE1_SNP(5)
        _POP_TYPE1_SNP(6)
        _POP_TYPE1_SNP(7)

        // TODO: support more ports

#undef _POP_TYPE1_SNP

        return std::nullopt;
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::HasType1DnRSP(size_t index) const noexcept
    {
#define _HAS_TYPE1_DNRSP(port) \
        if (index == port) \
        { \
            if constexpr (HasType1Port##port()) \
            { \
                details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
                return t1p##port->cchi_t1p##port##_txrsp_valid != 0; \
            } \
            else \
                return false; \
        }

        _HAS_TYPE1_DNRSP(0)
        _HAS_TYPE1_DNRSP(1)
        _HAS_TYPE1_DNRSP(2)
        _HAS_TYPE1_DNRSP(3)
        _HAS_TYPE1_DNRSP(4)
        _HAS_TYPE1_DNRSP(5)
        _HAS_TYPE1_DNRSP(6)
        _HAS_TYPE1_DNRSP(7)

        // TODO: support more ports

#undef _HAS_TYPE1_DNRSP

        return false;
    }

    template<class VT>
    inline std::optional<CCHI::Flits::DnRSP<CommonFlitConfigurationType1>> V3CCHIInterface<VT>::PeekType1DnRSP(size_t index) const noexcept
    {
#define _PEEK_TYPE1_DNRSP(port) \
        if (index == port) \
        { \
            if constexpr (HasType1Port##port()) \
            { \
                details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
                \
                if (t1p##port->cchi_t1p##port##_txrsp_valid != 0) \
                { \
                    CCHI::Flits::DnRSP<CommonFlitConfigurationType1> flit; \
                    flit.TxnID      = t1p##port->cchi_t1p##port##_txrsp_bits_TxnID; \
                    flit.SrcID      = t1p##port->cchi_t1p##port##_txrsp_bits_SrcID; \
                    flit.TgtID      = t1p##port->cchi_t1p##port##_txrsp_bits_TgtID; \
                    flit.DBID       = t1p##port->cchi_t1p##port##_txrsp_bits_DBID; \
                    flit.Opcode     = t1p##port->cchi_t1p##port##_txrsp_bits_Opcode; \
                    flit.RespErr    = t1p##port->cchi_t1p##port##_txrsp_bits_RespErr; \
                    flit.Resp       = t1p##port->cchi_t1p##port##_txrsp_bits_Resp; \
                    flit.CBusy      = t1p##port->cchi_t1p##port##_txrsp_bits_CBusy; \
                    \
                    if constexpr (HasType1Port##port##UWPersist()) \
                    { \
                        flit.WayValid = t1p##port->cchi_t1p##port##_txrsp_bits_WayValid; \
                        flit.Way      = t1p##port->cchi_t1p##port##_txrsp_bits_Way; \
                    } \
                    else \
                    { \
                        flit.WayValid = 0; \
                        flit.Way      = 0; \
                    } \
                    \
                    flit.TraceTag   = t1p##port->cchi_t1p##port##_txrsp_bits_TraceTag; \
                    \
                    return { flit }; \
                } \
                else \
                    return std::nullopt; \
            } \
        }

        _PEEK_TYPE1_DNRSP(0)
        _PEEK_TYPE1_DNRSP(1)
        _PEEK_TYPE1_DNRSP(2)
        _PEEK_TYPE1_DNRSP(3)
        _PEEK_TYPE1_DNRSP(4)
        _PEEK_TYPE1_DNRSP(5)
        _PEEK_TYPE1_DNRSP(6)
        _PEEK_TYPE1_DNRSP(7)

        // TODO: support more ports

#undef _PEEK_TYPE1_DNRSP

        return std::nullopt;
    }

    template<class VT>
    inline std::optional<CCHI::Flits::DnRSP<CommonFlitConfigurationType1>> V3CCHIInterface<VT>::PopType1DnRSP(size_t index) noexcept
    {
#define _POP_TYPE1_DNRSP(port) \
        if (index == port) \
        { \
            if constexpr (HasType1Port##port()) \
            { \
                details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
                \
                if (t1p##port->cchi_t1p##port##_txrsp_valid != 0) \
                { \
                    CCHI::Flits::DnRSP<CommonFlitConfigurationType1> flit; \
                    flit.TxnID      = t1p##port->cchi_t1p##port##_txrsp_bits_TxnID; \
                    flit.SrcID      = t1p##port->cchi_t1p##port##_txrsp_bits_SrcID; \
                    flit.TgtID      = t1p##port->cchi_t1p##port##_txrsp_bits_TgtID; \
                    flit.DBID       = t1p##port->cchi_t1p##port##_txrsp_bits_DBID; \
                    flit.Opcode     = t1p##port->cchi_t1p##port##_txrsp_bits_Opcode; \
                    flit.RespErr    = t1p##port->cchi_t1p##port##_txrsp_bits_RespErr; \
                    flit.Resp       = t1p##port->cchi_t1p##port##_txrsp_bits_Resp; \
                    flit.CBusy      = t1p##port->cchi_t1p##port##_txrsp_bits_CBusy; \
                    \
                    if constexpr (HasType1Port##port##UWPersist()) \
                    { \
                        flit.WayValid = t1p##port->cchi_t1p##port##_txrsp_bits_WayValid; \
                        flit.Way      = t1p##port->cchi_t1p##port##_txrsp_bits_Way; \
                    } \
                    else \
                    { \
                        flit.WayValid = 0; \
                        flit.Way      = 0; \
                    } \
                    \
                    flit.TraceTag   = t1p##port->cchi_t1p##port##_txrsp_bits_TraceTag; \
                    \
                    t1p##port->cchi_t1p##port##_txrsp_ready = 1; /* cleared on TickPreHandshake */ \
                    \
                    return { flit }; \
                } \
                else \
                    return std::nullopt; \
            } \
        }

        _POP_TYPE1_DNRSP(0)
        _POP_TYPE1_DNRSP(1)
        _POP_TYPE1_DNRSP(2)
        _POP_TYPE1_DNRSP(3)
        _POP_TYPE1_DNRSP(4)
        _POP_TYPE1_DNRSP(5)
        _POP_TYPE1_DNRSP(6)
        _POP_TYPE1_DNRSP(7)

        // TODO: support more ports

#undef _POP_TYPE1_DNRSP

        return std::nullopt;
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::HasType1DnDAT(size_t index) const noexcept
    {
#define _HAS_TYPE1_DNDAT(port) \
        if (index == port) \
        { \
            if constexpr (HasType1Port##port()) \
            { \
                details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
                return t1p##port->cchi_t1p##port##_txdat_valid != 0; \
            } \
            else \
                return false; \
        }

        _HAS_TYPE1_DNDAT(0)
        _HAS_TYPE1_DNDAT(1)
        _HAS_TYPE1_DNDAT(2)
        _HAS_TYPE1_DNDAT(3)
        _HAS_TYPE1_DNDAT(4)
        _HAS_TYPE1_DNDAT(5)
        _HAS_TYPE1_DNDAT(6)
        _HAS_TYPE1_DNDAT(7)

        // TODO: support more ports

#undef _HAS_TYPE1_DNDAT

        return false;
    }

    template<class VT>
    inline std::optional<CCHI::Flits::DnDAT<CommonFlitConfigurationType1>> V3CCHIInterface<VT>::PeekType1DnDAT(size_t index) const noexcept
    {
#define _PEEK_TYPE1_DNDAT(port) \
        if (index == port) \
        { \
            if constexpr (HasType1Port##port()) \
            { \
                details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
                \
                if (t1p##port->cchi_t1p##port##_txdat_valid != 0) \
                { \
                    CCHI::Flits::DnDAT<CommonFlitConfigurationType1> flit; \
                    flit.TxnID      = t1p##port->cchi_t1p##port##_txdat_bits_TxnID; \
                    flit.SrcID      = t1p##port->cchi_t1p##port##_txdat_bits_SrcID; \
                    flit.TgtID      = t1p##port->cchi_t1p##port##_txdat_bits_TgtID; \
                    flit.DBID       = t1p##port->cchi_t1p##port##_txdat_bits_DBID; \
                    flit.Opcode     = t1p##port->cchi_t1p##port##_txdat_bits_Opcode; \
                    flit.RespErr    = t1p##port->cchi_t1p##port##_txdat_bits_RespErr; \
                    flit.Resp       = t1p##port->cchi_t1p##port##_txdat_bits_Resp; \
                    flit.DataSource = t1p##port->cchi_t1p##port##_txdat_bits_DataSource; \
                    flit.CBusy      = t1p##port->cchi_t1p##port##_txdat_bits_CBusy; \
                    \
                    if constexpr (HasType1Port##port##UWPersist()) \
                        flit.WayValid = t1p##port->cchi_t1p##port##_txdat_bits_WayValid; \
                    else \
                        flit.WayValid = 0; \
                    \
                    if constexpr (HasType1Port##port##UWPredict()) \
                        flit.Way = t1p##port->cchi_t1p##port##_txdat_bits_Way; \
                    else \
                        flit.Way = 0; \
                    \
                    flit.DataID     = t1p##port->cchi_t1p##port##_txdat_bits_DataID; \
                    std::memcpy(flit.Data, &t1p##port->cchi_t1p##port##_txdat_bits_Data[0], sizeof(flit.Data)); \
                    flit.TraceTag   = t1p##port->cchi_t1p##port##_txdat_bits_TraceTag; \
                    \
                    return { flit }; \
                } \
                else \
                    return std::nullopt; \
            } \
        }

        _PEEK_TYPE1_DNDAT(0)
        _PEEK_TYPE1_DNDAT(1)
        _PEEK_TYPE1_DNDAT(2)
        _PEEK_TYPE1_DNDAT(3)
        _PEEK_TYPE1_DNDAT(4)
        _PEEK_TYPE1_DNDAT(5)
        _PEEK_TYPE1_DNDAT(6)
        _PEEK_TYPE1_DNDAT(7)

        // TODO: support more ports

#undef _PEEK_TYPE1_DNDAT

        return std::nullopt;
    }

    template<class VT>
    inline std::optional<CCHI::Flits::DnDAT<CommonFlitConfigurationType1>> V3CCHIInterface<VT>::PopType1DnDAT(size_t index) noexcept
    {
#define _POP_TYPE1_DNDAT(port) \
        if (index == port) \
        { \
            if constexpr (HasType1Port##port()) \
            { \
                details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
                \
                if (t1p##port->cchi_t1p##port##_txdat_valid != 0) \
                { \
                    CCHI::Flits::DnDAT<CommonFlitConfigurationType1> flit; \
                    flit.TxnID      = t1p##port->cchi_t1p##port##_txdat_bits_TxnID; \
                    flit.SrcID      = t1p##port->cchi_t1p##port##_txdat_bits_SrcID; \
                    flit.TgtID      = t1p##port->cchi_t1p##port##_txdat_bits_TgtID; \
                    flit.DBID       = t1p##port->cchi_t1p##port##_txdat_bits_DBID; \
                    flit.Opcode     = t1p##port->cchi_t1p##port##_txdat_bits_Opcode; \
                    flit.RespErr    = t1p##port->cchi_t1p##port##_txdat_bits_RespErr; \
                    flit.Resp       = t1p##port->cchi_t1p##port##_txdat_bits_Resp; \
                    flit.DataSource = t1p##port->cchi_t1p##port##_txdat_bits_DataSource; \
                    flit.CBusy      = t1p##port->cchi_t1p##port##_txdat_bits_CBusy; \
                    \
                    if constexpr (HasType1Port##port##UWPersist()) \
                        flit.WayValid = t1p##port->cchi_t1p##port##_txdat_bits_WayValid; \
                    else \
                        flit.WayValid = 0; \
                    \
                    if constexpr (HasType1Port##port##UWPredict()) \
                        flit.Way = t1p##port->cchi_t1p##port##_txdat_bits_Way; \
                    else \
                        flit.Way = 0; \
                    \
                    flit.DataID     = t1p##port->cchi_t1p##port##_txdat_bits_DataID; \
                    std::memcpy(flit.Data, &t1p##port->cchi_t1p##port##_txdat_bits_Data[0], sizeof(flit.Data)); \
                    flit.TraceTag   = t1p##port->cchi_t1p##port##_txdat_bits_TraceTag; \
                    \
                    t1p##port->cchi_t1p##port##_txdat_ready = 1; /* cleared on TickPreHandshake */ \
                    \
                    return { flit }; \
                } \
                else \
                    return std::nullopt; \
            } \
        }

        _POP_TYPE1_DNDAT(0)
        _POP_TYPE1_DNDAT(1)
        _POP_TYPE1_DNDAT(2)
        _POP_TYPE1_DNDAT(3)
        _POP_TYPE1_DNDAT(4)
        _POP_TYPE1_DNDAT(5)
        _POP_TYPE1_DNDAT(6)
        _POP_TYPE1_DNDAT(7)

        // TODO: support more ports

#undef _POP_TYPE1_DNDAT

        return std::nullopt;
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::PushType1EVT(size_t index, const CCHI::Flits::EVT<CommonFlitConfigurationType1>& flit) noexcept
    {
        if (auto iter = type1EVTBuffers.find(index); iter != type1EVTBuffers.end())
            if (iter->second.Size() != 0)
                return iter->second.Push(flit);

#define _PUSH_TYPE1_EVT(port) \
        if (index == port) \
        { \
            if constexpr (HasType1Port##port()) \
            { \
                details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
                \
                t1p##port->cchi_t1p##port##_rxevt_valid         = 1; /* cleared on TickPreHandshake */ \
                t1p##port->cchi_t1p##port##_rxevt_bits_TxnID    = flit.TxnID; \
                t1p##port->cchi_t1p##port##_rxevt_bits_SrcID    = flit.SrcID; \
                t1p##port->cchi_t1p##port##_rxevt_bits_TgtID    = flit.TgtID; \
                t1p##port->cchi_t1p##port##_rxevt_bits_Opcode   = flit.Opcode; \
                t1p##port->cchi_t1p##port##_rxevt_bits_Addr     = flit.Addr; \
                t1p##port->cchi_t1p##port##_rxevt_bits_NS       = flit.NS; \
                t1p##port->cchi_t1p##port##_rxevt_bits_MemAttr  = flit.MemAttr; \
                \
                if constexpr (HasType1Port##port##UWPersist()) \
                { \
                    t1p##port->cchi_t1p##port##_rxevt_bits_WayValid = flit.WayValid; \
                    t1p##port->cchi_t1p##port##_rxevt_bits_Way      = flit.Way; \
                } \
                else \
                { \
                    t1p##port->cchi_t1p##port##_rxevt_bits_WayValid = 0; \
                    t1p##port->cchi_t1p##port##_rxevt_bits_Way      = 0; \
                } \
                \
                t1p##port->cchi_t1p##port##_rxevt_bits_TraceTag = flit.TraceTag; \
                \
                return t1p##port->cchi_t1p##port##_rxevt_ready; \
            } \
        }

        _PUSH_TYPE1_EVT(0)
        _PUSH_TYPE1_EVT(1)
        _PUSH_TYPE1_EVT(2)
        _PUSH_TYPE1_EVT(3)
        _PUSH_TYPE1_EVT(4)
        _PUSH_TYPE1_EVT(5)
        _PUSH_TYPE1_EVT(6)
        _PUSH_TYPE1_EVT(7)

        // TODO: support more ports

#undef _PUSH_TYPE1_EVT

        return false;
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::PushType1REQ(size_t index, const CCHI::Flits::REQ<CommonFlitConfigurationType1>& flit) noexcept
    {
        if (auto iter = type1REQBuffers.find(index); iter != type1REQBuffers.end())
            if (iter->second.Size() != 0)
                return iter->second.Push(flit);

    #define _PUSH_TYPE1_REQ(port) \
        if (index == port) \
        { \
            if constexpr (HasType1Port##port()) \
            { \
                details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
                \
                t1p##port->cchi_t1p##port##_rxreq_valid            = 1; /* cleared on TickPreHandshake */ \
                t1p##port->cchi_t1p##port##_rxreq_bits_TxnID       = flit.TxnID; \
                t1p##port->cchi_t1p##port##_rxreq_bits_SrcID       = flit.SrcID; \
                t1p##port->cchi_t1p##port##_rxreq_bits_TgtID       = flit.TgtID; \
                t1p##port->cchi_t1p##port##_rxreq_bits_Opcode      = flit.Opcode; \
                t1p##port->cchi_t1p##port##_rxreq_bits_Size        = flit.Size; \
                t1p##port->cchi_t1p##port##_rxreq_bits_Addr        = flit.Addr; \
                if constexpr (CCHI::Flits::REQ<CommonFlitConfigurationType1>::hasTagAlias) \
                    t1p##port->cchi_t1p##port##_rxreq_bits_TagAlias = flit.TagAlias; \
                else \
                    t1p##port->cchi_t1p##port##_rxreq_bits_TagAlias = 0; \
                t1p##port->cchi_t1p##port##_rxreq_bits_NS          = flit.NS; \
                t1p##port->cchi_t1p##port##_rxreq_bits_Order       = flit.Order; \
                t1p##port->cchi_t1p##port##_rxreq_bits_MemAttr     = flit.MemAttr; \
                t1p##port->cchi_t1p##port##_rxreq_bits_Excl        = flit.Excl; \
                t1p##port->cchi_t1p##port##_rxreq_bits_ExpCompData = flit.ExpCompData; \
                \
                if constexpr (HasType1Port##port##UWPredict() || HasType1Port##port##UWPersist()) \
                { \
                    t1p##port->cchi_t1p##port##_rxreq_bits_WayValid = flit.WayValid; \
                    t1p##port->cchi_t1p##port##_rxreq_bits_Way      = flit.Way; \
                } \
                else \
                { \
                    t1p##port->cchi_t1p##port##_rxreq_bits_WayValid = 0; \
                    t1p##port->cchi_t1p##port##_rxreq_bits_Way      = 0; \
                } \
                \
                t1p##port->cchi_t1p##port##_rxreq_bits_TraceTag = flit.TraceTag; \
                \
                return t1p##port->cchi_t1p##port##_rxreq_ready; \
            } \
        }

        _PUSH_TYPE1_REQ(0)
        _PUSH_TYPE1_REQ(1)
        _PUSH_TYPE1_REQ(2)
        _PUSH_TYPE1_REQ(3)
        _PUSH_TYPE1_REQ(4)
        _PUSH_TYPE1_REQ(5)
        _PUSH_TYPE1_REQ(6)
        _PUSH_TYPE1_REQ(7)

        // TODO: support more ports

    #undef _PUSH_TYPE1_REQ

        return false;
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::PushType1UpRSP(size_t index, const CCHI::Flits::UpRSP<CommonFlitConfigurationType1>& flit) noexcept
    {
        if (auto iter = type1UpRSPBuffers.find(index); iter != type1UpRSPBuffers.end())
            if (iter->second.Size() != 0)
                return iter->second.Push(flit);

    #define _PUSH_TYPE1_UPRSP(port) \
        if (index == port) \
        { \
            if constexpr (HasType1Port##port()) \
            { \
                details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
                \
                t1p##port->cchi_t1p##port##_rxrsp_valid         = 1; /* cleared on TickPreHandshake */ \
                t1p##port->cchi_t1p##port##_rxrsp_bits_TxnID    = flit.TxnID; \
                t1p##port->cchi_t1p##port##_rxrsp_bits_SrcID    = flit.SrcID; \
                t1p##port->cchi_t1p##port##_rxrsp_bits_TgtID    = flit.TgtID; \
                t1p##port->cchi_t1p##port##_rxrsp_bits_Opcode   = flit.Opcode; \
                t1p##port->cchi_t1p##port##_rxrsp_bits_RespErr  = flit.RespErr; \
                t1p##port->cchi_t1p##port##_rxrsp_bits_Resp     = flit.Resp; \
                t1p##port->cchi_t1p##port##_rxrsp_bits_TraceTag = flit.TraceTag; \
                \
                return t1p##port->cchi_t1p##port##_rxrsp_ready; \
            } \
        }

        _PUSH_TYPE1_UPRSP(0)
        _PUSH_TYPE1_UPRSP(1)
        _PUSH_TYPE1_UPRSP(2)
        _PUSH_TYPE1_UPRSP(3)
        _PUSH_TYPE1_UPRSP(4)
        _PUSH_TYPE1_UPRSP(5)
        _PUSH_TYPE1_UPRSP(6)
        _PUSH_TYPE1_UPRSP(7)

        // TODO: support more ports

    #undef _PUSH_TYPE1_UPRSP

        return false;
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::PushType1UpDAT(size_t index, const CCHI::Flits::UpDAT<CommonFlitConfigurationType1>& flit) noexcept
    {
        if (auto iter = type1UpDATBuffers.find(index); iter != type1UpDATBuffers.end())
            if (iter->second.Size() != 0)
                return iter->second.Push(flit);

    #define _PUSH_TYPE1_UPDAT(port) \
        if (index == port) \
        { \
            if constexpr (HasType1Port##port()) \
            { \
                details::V3CCHIModuleT1P##port auto* t1p##port = verilated; \
                \
                t1p##port->cchi_t1p##port##_rxdat_valid         = 1; /* cleared on TickPreHandshake */ \
                t1p##port->cchi_t1p##port##_rxdat_bits_TxnID    = flit.TxnID; \
                t1p##port->cchi_t1p##port##_rxdat_bits_SrcID    = flit.SrcID; \
                t1p##port->cchi_t1p##port##_rxdat_bits_TgtID    = flit.TgtID; \
                t1p##port->cchi_t1p##port##_rxdat_bits_Opcode   = flit.Opcode; \
                t1p##port->cchi_t1p##port##_rxdat_bits_RespErr  = flit.RespErr; \
                t1p##port->cchi_t1p##port##_rxdat_bits_Resp     = flit.Resp; \
                t1p##port->cchi_t1p##port##_rxdat_bits_DataID   = flit.DataID; \
                std::memcpy(&t1p##port->cchi_t1p##port##_rxdat_bits_Data[0], flit.Data, sizeof(flit.Data)); \
                t1p##port->cchi_t1p##port##_rxdat_bits_BE       = flit.BE; \
                t1p##port->cchi_t1p##port##_rxdat_bits_TraceTag = flit.TraceTag; \
                \
                return t1p##port->cchi_t1p##port##_rxdat_ready; \
            } \
        }

        _PUSH_TYPE1_UPDAT(0)
        _PUSH_TYPE1_UPDAT(1)
        _PUSH_TYPE1_UPDAT(2)
        _PUSH_TYPE1_UPDAT(3)
        _PUSH_TYPE1_UPDAT(4)
        _PUSH_TYPE1_UPDAT(5)
        _PUSH_TYPE1_UPDAT(6)
        _PUSH_TYPE1_UPDAT(7)

        // TODO: support more ports

    #undef _PUSH_TYPE1_UPDAT

        return false;
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::SetType1EVTBufferSize(size_t index, size_t bufferSize) noexcept
    {
        if (auto iter = type1EVTBuffers.find(index); iter != type1EVTBuffers.end())
            return iter->second.TryResize(bufferSize);

        return false;
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::SetType1REQBufferSize(size_t index, size_t bufferSize) noexcept
    {
        if (auto iter = type1REQBuffers.find(index); iter != type1REQBuffers.end())
            return iter->second.TryResize(bufferSize);

        return false;
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::SetType1SNPBufferSize(size_t index, size_t bufferSize) noexcept
    {
        // *NOTICE: Not supporting buffer for all RX channels currently
        return false;

        /*
        if (auto iter = type1SNPBuffers.find(index); iter != type1SNPBuffers.end())
            return iter->second.TryResize(bufferSize);

        return false;
        */
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::SetType1DnRSPBufferSize(size_t index, size_t bufferSize) noexcept
    {
        // *NOTICE: Not supporting buffer for all RX channels currently
        return false;

        /*
        if (auto iter = type1DnRSPBuffers.find(index); iter != type1DnRSPBuffers.end())
            return iter->second.TryResize(bufferSize);

        return false;
        */
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::SetType1DnDATBufferSize(size_t index, size_t bufferSize) noexcept
    {
        // *NOTICE: Not supporting buffer for all RX channels currently
        return false;

        /*
        if (auto iter = type1DnDATBuffers.find(index); iter != type1DnDATBuffers.end())
            return iter->second.TryResize(bufferSize);

        return false;
        */
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::SetType1UpRSPBufferSize(size_t index, size_t bufferSize) noexcept
    {
        if (auto iter = type1UpRSPBuffers.find(index); iter != type1UpRSPBuffers.end())
            return iter->second.TryResize(bufferSize);

        return false;
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::SetType1UpDATBufferSize(size_t index, size_t bufferSize) noexcept
    {
        if (auto iter = type1UpDATBuffers.find(index); iter != type1UpDATBuffers.end())
            return iter->second.TryResize(bufferSize);

        return false;
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::SetAllType1EVTBufferSize(size_t bufferSize) noexcept
    {
        bool result = true;

        for (auto index : type1Indices)
            result &= SetType1EVTBufferSize(index, bufferSize);

        return result;
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::SetAllType1REQBufferSize(size_t bufferSize) noexcept
    {
        bool result = true;

        for (auto index : type1Indices)
            result &= SetType1REQBufferSize(index, bufferSize);

        return result;
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::SetAllType1SNPBufferSize(size_t bufferSize) noexcept
    {
        // *NOTICE: Not supporting buffer for all RX channels currently
        return false;

        /*
        bool result = true;

        for (auto index : type1Indices)
            result &= SetType1SNPBufferSize(index, bufferSize);

        return result;
        */
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::SetAllType1DnRSPBufferSize(size_t bufferSize) noexcept
    {
        // *NOTICE: Not supporting buffer for all RX channels currently
        return false;

        /*
        bool result = true;

        for (auto index : type1Indices)
            result &= SetType1DnRSPBufferSize(index, bufferSize);

        return result;
        */
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::SetAllType1DnDATBufferSize(size_t bufferSize) noexcept
    {
        // *NOTICE: Not supporting buffer for all RX channels currently
        return false;

        /*
        bool result = true;

        for (auto index : type1Indices)
            result &= SetType1DnDATBufferSize(index, bufferSize);

        return result;
        */
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::SetAllType1UpRSPBufferSize(size_t bufferSize) noexcept
    {
        bool result = true;

        for (auto index : type1Indices)
            result &= SetType1UpRSPBufferSize(index, bufferSize);

        return result;
    }

    template<class VT>
    inline bool V3CCHIInterface<VT>::SetAllType1UpDATBufferSize(size_t bufferSize) noexcept
    {
        bool result = true;

        for (auto index : type1Indices)
            result &= SetType1UpDATBufferSize(index, bufferSize);

        return result;
    }
}
