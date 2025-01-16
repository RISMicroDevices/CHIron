#pragma once
//
// Gravity EventBus
//
// --------------------------------------
// *tweaked for C++17, detached from BullsEye::Gravity upstream*
//
// --------------------------------------
// Forked from BullsEye EventBus:
//
//  * Better and more fine-grained EventBus component type management freedom
//
//  * Removed T <- Event<T> derival constraint to achieve 
//
//      a. Event<T> for it's EventBus is no longer singleton in type, for example:
//              - A extends Event<A>
//              - B extends Event<A>
//              - C extends A
//          In this case, A, B and C will both be fired on EventBus of Event<A>
//
//      b. Hybrid EventBus is now achievable (as above)
//

#ifndef __BULLSEYE_SIMS_GRAVITY__EVENTBUS
#define __BULLSEYE_SIMS_GRAVITY__EVENTBUS

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <algorithm>
#include <optional>
#include <functional>


namespace Gravity {

    // --> essentials

    // Pre-declarations
    template<class _TEvent, 
             class _TEventListener, 
             class _TEventBusId,
             class _TEventBus, 
             class _TEventBusGroup>  
    class Event;
    
    template<class _TEvent>
    class EventListener;

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    class EventBus;

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus>
    class EventBusGroup;


    typedef unsigned int                EventBusId;


    // Event Instance Interface
    template<class _TEvent, 
             class _TEventListener  = EventListener<_TEvent>,
             class _TEventBusId     = EventBusId,
             class _TEventBus       = EventBus<_TEvent, _TEventListener, _TEventBusId>, 
             class _TEventBusGroup  = EventBusGroup<_TEvent, _TEventListener, _TEventBusId, _TEventBus>>
    class Event {
    public:
        static _TEventBusGroup&         GetEventBusGroup() noexcept;
        static std::optional<std::reference_wrapper<_TEventBus>>
                                        GetEventBus(_TEventBusId busId = 0) noexcept;
        static bool                     HasEventBus(_TEventBusId busId = 0) noexcept;
        static _TEventBus&              RequireEventBus(_TEventBusId busId = 0) noexcept;

        static void                     Register(std::shared_ptr<_TEventListener> listener, _TEventBusId busId = 0) noexcept;

        static int                      Unregister(const std::string& name, _TEventBusId busId = 0) noexcept;
        static bool                     UnregisterOnce(const std::string& name, _TEventBusId busId = 0) noexcept;
        static void                     UnregisterAll(_TEventBusId busId = 0) noexcept;

    public:
        _TEvent&                        Fire(_TEventBusId busId = 0);
        _TEvent&                        Fire(_TEventBus& eventbus);
    };

    template<template<typename> typename _TListener, class _TEvent>
    inline void RegisterListener(std::shared_ptr<_TListener<_TEvent>> listener, EventBusId busId = 0) noexcept
    { Event<_TEvent>::Register(listener, busId); }


    // Cancellable Event
    class CancellableEvent {
    private:
        bool    cancelled;

    public:
        CancellableEvent() noexcept;

        void                        SetCancelled(bool cancalled = true) noexcept;
        bool                        IsCancelled() const noexcept;
    };

    // Execptionable Event
    template<class _TException>
    class ExceptionableEvent {
    private:
        bool        exceptioned;
        _TException exception;

    public:
        ExceptionableEvent() noexcept;

        bool                        HasException() const noexcept;
        const _TException&          GetException() const noexcept;

        void                        SetException(_TException exception, bool exceptioned = true) noexcept;
    };


    // Event Handler
    template<class _TEvent>
    class EventListener {
    private:
        const std::string   name;
        const int           priority;

    public:
        EventListener(const std::string& name, int priority) noexcept;
        virtual ~EventListener() noexcept;

        const std::string&          GetName() const noexcept;
        int                         GetPriority() const noexcept;

        virtual void                OnEvent(_TEvent& event) = 0;
    };


    // Event Bus
    template<class _TEvent,
             class _TEventListener  = EventListener<_TEvent>,
             class _TEventBusId     = EventBusId>
    class EventBus {
    private:
        const _TEventBusId                              id;
        std::vector<std::shared_ptr<_TEventListener>>   list;

    public:
        using iterator          = typename std::vector<std::shared_ptr<_TEventListener>>::iterator;
        using const_iterator    = typename std::vector<std::shared_ptr<_TEventListener>>::const_iterator;

    private:
        typename std::vector<std::shared_ptr<_TEventListener>>::const_iterator _NextPos(int priority) noexcept;

    public:
        EventBus(const _TEventBusId& id) noexcept;
        ~EventBus() noexcept;

        const _TEventBusId&         GetId() const noexcept;

        void                        Register(std::shared_ptr<_TEventListener> listener) noexcept;

        int                         Unregister(const std::string& name) noexcept;
        bool                        UnregisterOnce(const std::string& name) noexcept;
        void                        UnregisterAll() noexcept;

        iterator                    begin() noexcept;
        const_iterator              begin() const noexcept;
        iterator                    end() noexcept;
        const_iterator              end() const noexcept;

        _TEvent&                    Fire(_TEvent& event);
        void                        Fire(_TEvent&& event);

        _TEvent&                    operator()(_TEvent& event);
        void                        operator()(_TEvent&& event);
    };


    // Event Bus Group
    template<class _TEvent,
             class _TEventListener  = EventListener<_TEvent>,
             class _TEventBusId     = EventBusId,
             class _TEventBus       = EventBus<_TEvent, _TEventListener, _TEventBusId>>
    class EventBusGroup {
    protected:
        std::vector<_TEventBus>     buses;

    public:
        EventBusGroup() noexcept;
        ~EventBusGroup() noexcept;

        EventBusGroup(const EventBusGroup<_TEvent>&) = delete;
        EventBusGroup(EventBusGroup<_TEvent>&&) = delete;

        std::optional<std::reference_wrapper<_TEventBus>>
                                    Get(_TEventBusId busId = 0) noexcept;

        std::optional<std::reference_wrapper<const _TEventBus>>
                                    Get(_TEventBusId busId = 0) const noexcept;

        _TEventBus&                 Require(_TEventBusId busId) noexcept;

        bool                        Has(_TEventBusId busId) const noexcept;
        size_t                      GetCount() const noexcept;
    };


    // Event Bus Dispatchment
    class EventBusDispatchment {
    public:
        static EventBusDispatchment&    Global() noexcept;

    private:
        std::atomic_uint    busIdIncrement;

    public:
        EventBusDispatchment() noexcept;
        ~EventBusDispatchment() noexcept;

        unsigned int                    NextEventBusId() noexcept;
    };



    // --> utilities

    // Event Handler Functional Stub
    template<class _TEvent>
    class EventListenerFunctionalStub : public EventListener<_TEvent> {
    private:
        std::function<void(_TEvent&)>   func;

    public:
        EventListenerFunctionalStub(const std::string& name, int priority, std::function<void(_TEvent&)> func) noexcept;
        virtual ~EventListenerFunctionalStub() noexcept;

        virtual void                OnEvent(_TEvent& event) override;
    };

    template<class _TEvent>
    using FunctionalEventListener   = EventListenerFunctionalStub<_TEvent>;

    template<class _TEvent>
    inline std::shared_ptr<FunctionalEventListener<_TEvent>>
        MakeListener(const std::string& name, int priority, std::function<void(_TEvent&)>&& func) noexcept
    { return std::make_shared<FunctionalEventListener<_TEvent>>(name, priority, func); }

    template<class _TEvent>
    inline std::shared_ptr<FunctionalEventListener<_TEvent>>
        MakeListener(const std::string& name, int priority, void(*func)(_TEvent&)) noexcept
    { return std::make_shared<FunctionalEventListener<_TEvent>>(name, priority, func); }
    
    template<class _TEvent, class _Tx>
    inline std::shared_ptr<FunctionalEventListener<_TEvent>>
        MakeListener(const std::string& name, int priority, void(_Tx::* mfunc)(_TEvent&), _Tx* mthis) noexcept
    { return std::make_shared<FunctionalEventListener<_TEvent>>(name, priority, std::bind(mfunc, mthis, std::placeholders::_1)); }

    template<class _TEvent, class _Tx>
    inline std::shared_ptr<FunctionalEventListener<_TEvent>>
        MakeListener(const std::string& name, int priority, void(_Tx::* mfunc)(_TEvent&) const, const _Tx* mthis) noexcept
    { return std::make_shared<FunctionalEventListener<_TEvent>>(name, priority, std::bind(mfunc, mthis, std::placeholders::_1)); }


    // Event Handler Reference
    template<class _TEvent>
    class EventListenerReference : public EventListener<_TEvent> {
    private:
        EventListener<_TEvent>& ref;

    public:
        EventListenerReference(EventListener<_TEvent>& ref) noexcept;
        virtual ~EventListenerReference() noexcept;

        EventListener<_TEvent>&         GetReference() noexcept;
        const EventListener<_TEvent>&   GetReference() const noexcept;

        virtual void                    OnEvent(_TEvent& event) override;
    };

    template<class _TEvent>
    using RefEventListener          = EventListenerReference<_TEvent>;


    //
    template<class _TEvent>
    inline void UnregisterListener(const std::string& name, EventBusId busId = 0) noexcept
    { Event<_TEvent>::Unregister(name, busId); }

    template<template<typename> typename _TListener, class _TEvent>
    inline void UnregisterListener(const std::string& name, [[maybe_unused]] const _TListener<_TEvent>& listener, EventBusId busId = 0) noexcept
    { Event<_TEvent>::Unregister(name, busId); }

    template<template<typename> typename _TListener, class _TEvent>
    inline void UnregisterListener(const std::string& name, [[maybe_unused]] const std::shared_ptr<_TListener<_TEvent>>& listener, EventBusId busId = 0) noexcept
    { Event<_TEvent>::Unregister(name, busId); }

    template<class _TEvent>
    inline void UnregisterListener(const std::string& name, [[maybe_unused]] std::function<void(_TEvent&)>&& func, EventBusId busId = 0) noexcept
    { Event<_TEvent>::Unregister(name, busId); }

    template<class _TEvent>
    inline void UnregisterListener(const std::string& name, [[maybe_unused]] void(*func)(_TEvent&), EventBusId busId = 0) noexcept
    { Event<_TEvent>::Unregister(name, busId); }

    template<class _TEvent, class _Tx>
    inline void UnregisterListener(const std::string& name, [[maybe_unused]] void(_Tx::* mfunc)(_TEvent&), EventBusId busId = 0) noexcept
    { Event<_TEvent>::Unregister(name, busId); }

    template<class _TEvent, class _Tx>
    inline void UnregisterListener(const std::string& name, [[maybe_unused]] void(_Tx::* mfunc)(_TEvent&) const, EventBusId busId = 0) noexcept
    { Event<_TEvent>::Unregister(name, busId); }
}



// Implementation of: template<class _TEvent> class Event
namespace Gravity {

    //
    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus,
             class _TEventBusGroup>
    inline _TEventBusGroup& 
    Event<_TEvent, _TEventListener, _TEventBusId, _TEventBus, _TEventBusGroup>::GetEventBusGroup() noexcept
    {
        static _TEventBusGroup global_eventBusGroup;

        return global_eventBusGroup;
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus,
             class _TEventBusGroup>
    inline std::optional<std::reference_wrapper<_TEventBus>> 
    Event<_TEvent, _TEventListener, _TEventBusId, _TEventBus, _TEventBusGroup>::GetEventBus(_TEventBusId busId) noexcept
    {
        return GetEventBusGroup().Get(busId);
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus,
             class _TEventBusGroup>
    inline bool 
    Event<_TEvent, _TEventListener, _TEventBusId, _TEventBus, _TEventBusGroup>::HasEventBus(_TEventBusId busId) noexcept
    {
        return GetEventBusGroup().Has(busId);
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus,
             class _TEventBusGroup>
    inline _TEventBus& 
    Event<_TEvent, _TEventListener, _TEventBusId, _TEventBus, _TEventBusGroup>::RequireEventBus(_TEventBusId busId) noexcept
    {
        return GetEventBusGroup().Require(busId);
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus,
             class _TEventBusGroup>
    inline void 
    Event<_TEvent, _TEventListener, _TEventBusId, _TEventBus, _TEventBusGroup>::Register(std::shared_ptr<_TEventListener> listener, _TEventBusId busId) noexcept
    {
        RequireEventBus(busId).Register(listener);
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus,
             class _TEventBusGroup>
    inline int 
    Event<_TEvent, _TEventListener, _TEventBusId, _TEventBus, _TEventBusGroup>::Unregister(const std::string& name, _TEventBusId busId) noexcept
    {
        if (auto bus_ref = GetEventBus(busId))
            return bus_ref->get().Unregister(name);
        
        return 0;
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus,
             class _TEventBusGroup>
    inline bool 
    Event<_TEvent, _TEventListener, _TEventBusId, _TEventBus, _TEventBusGroup>::UnregisterOnce(const std::string& name, _TEventBusId busId) noexcept
    {
        if (auto bus_ref = GetEventBus(busId))
            return bus_ref->get().UnregisterOnce(name);
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus,
             class _TEventBusGroup>
    inline void 
    Event<_TEvent, _TEventListener, _TEventBusId, _TEventBus, _TEventBusGroup>::UnregisterAll(_TEventBusId busId) noexcept
    {
        if (auto bus_ref = GetEventBus(busId))
            bus_ref->get().UnregisterAll();
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus,
             class _TEventBusGroup>
    inline _TEvent& 
    Event<_TEvent, _TEventListener, _TEventBusId, _TEventBus, _TEventBusGroup>::Fire(_TEventBusId busId)
    {
        if (auto bus_ref = GetEventBus(busId))
            bus_ref->get().Fire(static_cast<_TEvent&>(*this));

        return static_cast<_TEvent&>(*this);
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus,
             class _TEventBusGroup>
    inline _TEvent& 
    Event<_TEvent, _TEventListener, _TEventBusId, _TEventBus, _TEventBusGroup>::Fire(_TEventBus& eventbus)
    {
        eventbus.Fire(static_cast<_TEvent&>(*this));

        return static_cast<_TEvent&>(*this);
    }
}


// Implementation of: class CancellableEvent
namespace Gravity {
    //
    // bool    cancelled;
    //

    inline CancellableEvent::CancellableEvent() noexcept
        : cancelled (false)
    { }

    inline void CancellableEvent::SetCancelled(bool cancelled) noexcept
    {
        this->cancelled = cancelled;
    }

    inline bool CancellableEvent::IsCancelled() const noexcept
    {
        return this->cancelled;
    }
}


// Implementation of: template<class _TException> class ExceptionableEvent
namespace Gravity {
    //
    // bool        exceptioned;
    // _TException exception;
    //

    template<class _TException>
    inline ExceptionableEvent<_TException>::ExceptionableEvent() noexcept
        : exceptioned   (false)
        , exception     ()
    { }

    template<class _TException>
    inline bool ExceptionableEvent<_TException>::HasException() const noexcept
    {
        return this->exceptioned;
    }

    template<class _TException>
    inline void ExceptionableEvent<_TException>::SetException(_TException exception, bool exceptioned) noexcept
    {
        this->exception     = exception;
        this->exceptioned   = exceptioned;
    }

    template<class _TException>
    inline const _TException& ExceptionableEvent<_TException>::GetException() const noexcept
    {
        return this->exception;
    }
}



// Implementation of: template<class _TEvent> class EventListener
namespace Gravity {
    //
    // const std::string   name;
    // const int           priority;
    //

    template<class _TEvent>
    inline EventListener<_TEvent>::EventListener(const std::string& name, int priority) noexcept
        : name      (name)
        , priority  (priority)
    { }

    template<class _TEvent>
    inline EventListener<_TEvent>::~EventListener() noexcept
    { }

    template<class _TEvent>
    inline const std::string& EventListener<_TEvent>::GetName() const noexcept
    {
        return name;
    }

    template<class _TEvent>
    inline int EventListener<_TEvent>::GetPriority() const noexcept
    {
        return priority;
    }
}


// Implementation of: template<class _TEvent> class EventBus
namespace Gravity {
    //
    // std::vector<std::shared_ptr<EventListener<_TEvent>>> list;
    //

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline typename std::vector<std::shared_ptr<_TEventListener>>::const_iterator 
    EventBus<_TEvent, _TEventListener, _TEventBusId>::_NextPos(int priority) noexcept
    {
        auto iter = list.begin();
        for (; iter != list.end(); iter++)
            if (iter->get()->GetPriority() > priority)
                break;

        return iter;
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline EventBus<_TEvent, _TEventListener, _TEventBusId>::EventBus(const _TEventBusId& id) noexcept
        : id    (id)
        , list  ()
    { }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline EventBus<_TEvent, _TEventListener, _TEventBusId>::~EventBus() noexcept
    { }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline const _TEventBusId&
    EventBus<_TEvent, _TEventListener, _TEventBusId>::GetId() const noexcept
    {
        return id;
    }
    
    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline void 
    EventBus<_TEvent, _TEventListener, _TEventBusId>::Register(std::shared_ptr<_TEventListener> listener) noexcept
    {
        list.insert(_NextPos(listener->GetPriority()), listener);
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline int 
    EventBus<_TEvent, _TEventListener, _TEventBusId>::Unregister(const std::string& name) noexcept
    {
        auto epos = std::remove_if(list.begin(), list.end(), 
            [name](std::shared_ptr<EventListener<_TEvent>> obj) -> bool {
                return obj->GetName() == name;
            });

        int count = std::distance(epos, list.end());

        list.erase(epos, list.end());

        return count;
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline bool 
    EventBus<_TEvent, _TEventListener, _TEventBusId>::UnregisterOnce(const std::string& name) noexcept
    {
        bool found;

        auto epos = list.begin();
        for (; epos != list.end(); epos++)
            if ((*epos)->GetName() == name)
            {
                found = true;
                break;
            }
        
        if (found)
            list.erase(epos);

        return found;
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline void 
    EventBus<_TEvent, _TEventListener, _TEventBusId>::UnregisterAll() noexcept
    {
        list.clear();
        list.shrink_to_fit();
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline typename EventBus<_TEvent, _TEventListener, _TEventBusId>::iterator 
    EventBus<_TEvent, _TEventListener, _TEventBusId>::begin() noexcept
    {
        return list.begin();
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline typename EventBus<_TEvent, _TEventListener, _TEventBusId>::const_iterator 
    EventBus<_TEvent, _TEventListener, _TEventBusId>::begin() const noexcept
    {
        return list.begin();
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline typename EventBus<_TEvent, _TEventListener, _TEventBusId>::iterator 
    EventBus<_TEvent, _TEventListener, _TEventBusId>::end() noexcept
    {
        return list.end();
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline typename EventBus<_TEvent, _TEventListener, _TEventBusId>::const_iterator 
    EventBus<_TEvent, _TEventListener, _TEventBusId>::end() const noexcept
    {
        return list.end();
    }
    
    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline _TEvent& 
    EventBus<_TEvent, _TEventListener, _TEventBusId>::Fire(_TEvent& event)
    {
        for (auto& listener : list)
            listener->OnEvent(event);

        return event;
    }
    
    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline void
    EventBus<_TEvent, _TEventListener, _TEventBusId>::Fire(_TEvent&& event)
    {
        for (auto& listener : list)
            listener->OnEvent(event);
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline _TEvent&
    EventBus<_TEvent, _TEventListener, _TEventBusId>::operator()(_TEvent& event)
    {
        return Fire(event);
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId>
    inline void
    EventBus<_TEvent, _TEventListener, _TEventBusId>::operator()(_TEvent&& event)
    {
        Fire(event);
    }
}


// Implementation of: template<class _TEvent> class EventBusGroup
namespace Gravity {
    /*
    std::vector<EventBus<_TEvent>>  buses;
    */

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus>
    inline EventBusGroup<_TEvent, _TEventListener, _TEventBusId, _TEventBus>::EventBusGroup() noexcept
        : buses ()
    { 
        buses.emplace_back(_TEventBusId(0)); // default eventbus
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus>
    inline EventBusGroup<_TEvent, _TEventListener, _TEventBusId, _TEventBus>::~EventBusGroup() noexcept
    { }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus>
    inline std::optional<std::reference_wrapper<_TEventBus>> 
    EventBusGroup<_TEvent, _TEventListener, _TEventBusId, _TEventBus>::Get(_TEventBusId busId) noexcept
    {
        if (Has(busId))
            return { std::ref(buses[busId]) };
        
        return std::nullopt;
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus>
    inline std::optional<std::reference_wrapper<const _TEventBus>> 
    EventBusGroup<_TEvent, _TEventListener, _TEventBusId, _TEventBus>::Get(_TEventBusId busId) const noexcept
    {
        if (Has(busId))
            return { std::cref(buses[busId]) };

        return std::nullopt;
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus>
    inline _TEventBus& 
    EventBusGroup<_TEvent, _TEventListener, _TEventBusId, _TEventBus>::Require(_TEventBusId busId) noexcept
    {
        while (!Has(busId))
            buses.emplace_back(_TEventBusId(buses.size()));

        return buses[busId];
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus>
    inline bool 
    EventBusGroup<_TEvent, _TEventListener, _TEventBusId, _TEventBus>::Has(_TEventBusId busId) const noexcept
    {
        return busId < buses.size();
    }

    template<class _TEvent,
             class _TEventListener,
             class _TEventBusId,
             class _TEventBus>
    inline size_t 
    EventBusGroup<_TEvent, _TEventListener, _TEventBusId, _TEventBus>::GetCount() const noexcept
    {
        return buses.size();
    }
}


// Implementation of: class EventBusDispatchment
namespace Gravity {
    //
    // std::atomic_uint    busIdIncrement;
    //

    inline EventBusDispatchment& EventBusDispatchment::Global() noexcept
    {
        static EventBusDispatchment global_eventBusDispatchment;

        return global_eventBusDispatchment;
    }

    inline EventBusDispatchment::EventBusDispatchment() noexcept
        : busIdIncrement    (0)
    { }

    inline EventBusDispatchment::~EventBusDispatchment() noexcept
    { }

    inline unsigned int EventBusDispatchment::NextEventBusId() noexcept
    {
        return busIdIncrement.fetch_add(1) + 1;
    }
}


// Implementation of: template<class _TEvent> class EventListenerFunctionalStub
namespace Gravity {
    //
    // std::function<void(_TEvent&)>   func;
    //

    template<class _TEvent>
    inline EventListenerFunctionalStub<_TEvent>::EventListenerFunctionalStub(
            const std::string&              name, 
            int                             priority, 
            std::function<void(_TEvent&)>   func) noexcept
        : EventListener<_TEvent>(name, priority)
        , func                  (func)
    { }

    template<class _TEvent>
    inline EventListenerFunctionalStub<_TEvent>::~EventListenerFunctionalStub() noexcept
    { }

    template<class _TEvent>
    inline void EventListenerFunctionalStub<_TEvent>::OnEvent(_TEvent& event)
    {
        return func(event);
    }
}


// Implementation of: template<class _TEvent> class EventListenerReference
namespace Gravity {
    //
    // EventListener<_TEvent>& ref;
    //

    template<class _TEvent>
    inline EventListenerReference<_TEvent>::EventListenerReference(EventListener<_TEvent>& ref) noexcept
        : ref   (ref)
    { }

    template<class _TEvent>
    inline EventListenerReference<_TEvent>::~EventListenerReference() noexcept
    { }

    template<class _TEvent>
    inline EventListener<_TEvent>& EventListenerReference<_TEvent>::GetReference() noexcept
    {
        return ref;
    }

    template<class _TEvent>
    inline const EventListener<_TEvent>& EventListenerReference<_TEvent>::GetReference() const noexcept
    {
        return ref;
    }

    template<class _TEvent>
    inline void EventListenerReference<_TEvent>::OnEvent(_TEvent& event)
    {
        ref.OnEvent(event);
    }
}


#endif // __BULLSEYE_SIMS_GRAVITY__EVENTBUS
