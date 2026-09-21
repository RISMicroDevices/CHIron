#pragma once

#include <array>
#include <memory>
#include <vector>
#include <type_traits>

#include "cohestra_common.hpp"
#include "cohestra_config.hpp"
#include "cohestra_interface.hpp"
#include "cohestra_stimulator.hpp"

#include "spdlog/spdlog.h"

#include "../icn/taurus/cchi_taurus_component.hpp"

#include "../../common/eventbus.hpp"


namespace Cohestra {

    enum class InstanceState {
        NOT_INITIALIZED = 0,
        ALIVE,
        FINISHED,
        FAILED
    };

    // TODO: put default configuration here ?

    class Instance {
    public:
        static const ConfigurationEntryBack<std::vector<uint32_t>>  CONFIG_T1_UPSTREAM_NODE_ID;
        static const ConfigurationEntryBack<uint32_t>               CONFIG_T1_UPSTREAM_INFLIGHT_EVT;
        static const ConfigurationEntryBack<uint32_t>               CONFIG_T1_UPSTREAM_INFLIGHT_SNP;
        static const ConfigurationEntryBack<uint32_t>               CONFIG_T1_UPSTREAM_INFLIGHT_REQ;
        static const ConfigurationEntryBack<uint32_t>               CONFIG_T1_UPSTREAM_INFLIGHT;

        static const ConfigurationEntryBack<uint32_t>               CONFIG_T1_UPSTREAM_BUFFER_EVT;
        static const ConfigurationEntryBack<uint32_t>               CONFIG_T1_UPSTREAM_BUFFER_REQ;
        static const ConfigurationEntryBack<uint32_t>               CONFIG_T1_UPSTREAM_BUFFER_RSP;
        static const ConfigurationEntryBack<uint32_t>               CONFIG_T1_UPSTREAM_BUFFER_DAT;

        static const ConfigurationEntryBack<std::string>            CONFIG_STIMULATOR_0;
        static const ConfigurationEntryBack<bool>                   CONFIG_STIMULATOR_0_ENABLE;
        static const ConfigurationEntryBack<std::vector<uint32_t>>  CONFIG_STIMULATOR_0_T1_UPSTREAMS;
        static const ConfigurationEntryBack<std::string>            CONFIG_STIMULATOR_1;
        static const ConfigurationEntryBack<bool>                   CONFIG_STIMULATOR_1_ENABLE;
        static const ConfigurationEntryBack<std::vector<uint32_t>>  CONFIG_STIMULATOR_1_T1_UPSTREAMS;
        static const ConfigurationEntryBack<std::string>            CONFIG_STIMULATOR_2;
        static const ConfigurationEntryBack<bool>                   CONFIG_STIMULATOR_2_ENABLE;
        static const ConfigurationEntryBack<std::vector<uint32_t>>  CONFIG_STIMULATOR_2_T1_UPSTREAMS;
        static const ConfigurationEntryBack<std::string>            CONFIG_STIMULATOR_3;
        static const ConfigurationEntryBack<bool>                   CONFIG_STIMULATOR_3_ENABLE;
        static const ConfigurationEntryBack<std::vector<uint32_t>>  CONFIG_STIMULATOR_3_T1_UPSTREAMS;
        static const ConfigurationEntryBack<std::string>            CONFIG_STIMULATOR_4;
        static const ConfigurationEntryBack<bool>                   CONFIG_STIMULATOR_4_ENABLE;
        static const ConfigurationEntryBack<std::vector<uint32_t>>  CONFIG_STIMULATOR_4_T1_UPSTREAMS;
        static const ConfigurationEntryBack<std::string>            CONFIG_STIMULATOR_5;
        static const ConfigurationEntryBack<bool>                   CONFIG_STIMULATOR_5_ENABLE;
        static const ConfigurationEntryBack<std::vector<uint32_t>>  CONFIG_STIMULATOR_5_T1_UPSTREAMS;
        static const ConfigurationEntryBack<std::string>            CONFIG_STIMULATOR_6;
        static const ConfigurationEntryBack<bool>                   CONFIG_STIMULATOR_6_ENABLE;
        static const ConfigurationEntryBack<std::vector<uint32_t>>  CONFIG_STIMULATOR_6_T1_UPSTREAMS;
        static const ConfigurationEntryBack<std::string>            CONFIG_STIMULATOR_7;
        static const ConfigurationEntryBack<bool>                   CONFIG_STIMULATOR_7_ENABLE;
        static const ConfigurationEntryBack<std::vector<uint32_t>>  CONFIG_STIMULATOR_7_T1_UPSTREAMS;

    public:
        static const std::array<ConfigurationEntryBack<std::string>, 8>             VECTOR_CONFIG_STIMULATOR;
        static const std::array<ConfigurationEntryBack<bool>, 8>                    VECTOR_CONFIG_STIMULATOR_ENABLE;
        static const std::array<ConfigurationEntryBack<std::vector<uint32_t>>, 8>   VECTOR_CONFIG_STIMULATOR_T1_UPSTREAMS;

    public:
        class EventHub;

        std::shared_ptr<EventHub>
                        events          = std::make_shared<EventHub>();

    protected:
        std::shared_ptr<spdlog::logger>
                        logger          = spdlog::default_logger();

    public:
        Configuration   config;

    protected:
        InstanceState   state           = InstanceState::NOT_INITIALIZED;

        std::shared_ptr<CCHIInterface>  
                        downstreams     = nullptr;

        std::vector<CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>*>
                        type1Upstreams;

        std::vector<Stimulator*> 
                        stimulators;

        // TODO: cache scoreboards and event hooks

    protected:
        std::vector<std::string>
                        finishMessage;

        std::vector<std::string>
                        failMessage;

    public:
        Instance() noexcept = default;
        ~Instance() noexcept;

    public:
        void                    Initialize(std::shared_ptr<CCHIInterface> downstreams,
                                           std::shared_ptr<StimulatorRegistry> stimulators) noexcept;

    public:
        void                    LoadConfiguration(const Configuration& config) noexcept;

    private:
        template<class T>
        bool                    LoadPreInitConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry) noexcept;

    public:
        bool                    LoadPreInitConfiguration(const StringLoaderBase& loader) noexcept;
        bool                    LoadPostInitConfiguration(const StringLoaderBase& loader) noexcept;

    public:
        bool                    Tick(uint64_t time) noexcept;

        // Mid-cycle retire phase for the downstream interface: forwards to
        // CCHIInterface::TickPostHandshake. Call after the clock=0 eval and
        // before the posedge eval.
        void                    TickPostHandshake(uint64_t time) noexcept;

    public:
        std::shared_ptr<spdlog::logger>
                                GetLogger() const noexcept;
        void                    SetLogger(std::shared_ptr<spdlog::logger> logger) noexcept;

        Configuration&          GetConfiguration() noexcept;
        const Configuration&    GetConfiguration() const noexcept;

        InstanceState           GetState() const noexcept;
        bool                    IsAlive() const noexcept;
        bool                    IsFinished() const noexcept;
        bool                    IsFailed() const noexcept;

        void                    SetAlive() noexcept;
        void                    SetFinished() noexcept;
        void                    SetFinished(const std::vector<std::string>& message) noexcept;
        void                    SetFailed() noexcept;
        void                    SetFailed(const std::vector<std::string>& message) noexcept;

        const std::vector<std::string>& 
                                GetFinishMessage() const noexcept;

        const std::vector<std::string>& 
                                GetFailMessage() const noexcept;

    public:
        std::shared_ptr<CCHIInterface>
                                GetDownstreams() noexcept;
        std::shared_ptr<const CCHIInterface>
                                GetDownstreams() const noexcept;

        size_t                  GetType1UpstreamCount() const noexcept;
        CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>*
                                GetType1Upstream(size_t index) noexcept;
        const CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>*
                                GetType1Upstream(size_t index) const noexcept;
        std::optional<size_t>   GetType1UpstreamIndex(const CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>* upstream) const noexcept;

        size_t                  GetStimulatorCount() const noexcept;
        Stimulator*             GetStimulator(size_t index) noexcept;
        const Stimulator*       GetStimulator(size_t index) const noexcept;

    public:
        template<class T>
        std::optional<T>        operator[](ConfigurationEntry<T>) const noexcept;
    };


    class InstanceEventBase {
    protected:
        Instance& instance;

    public:
        InstanceEventBase(Instance& instance) noexcept;

    public:
        Instance&               GetInstance() noexcept;
        const Instance&         GetInstance() const noexcept;
    };

    template<class T>
    class InstanceConfigurationEntryEventBase : public InstanceEventBase {
    protected:
        ConfigurationEntry<T>   entry;

    public:
        InstanceConfigurationEntryEventBase(Instance& instance, ConfigurationEntry<T> entry) noexcept;

    public:
        ConfigurationEntry<T>   GetEntry() const noexcept;
    };

    template<class T>
    class InstanceConfigurationEntryValueEventBase : public InstanceConfigurationEntryEventBase<T> {
    protected:
        T                       value;

    public:
        InstanceConfigurationEntryValueEventBase(Instance& instance, ConfigurationEntry<T> entry, const T& value) noexcept;

    public:
        const T&                GetValue() const noexcept;
    };

    class InstanceUpstreamEventBase : public InstanceEventBase {
    protected:
        size_t                  index;
        CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>&
                                upstream;

    public:
        InstanceUpstreamEventBase(Instance& instance, size_t index, CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>& upstream) noexcept;

    public:
        size_t                  GetIndex() const noexcept;
        CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>&
                                GetUpstream() noexcept;
        const CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>&
                                GetUpstream() const noexcept;
    };

    class InstanceStimulatorEventBase : public InstanceEventBase {
    protected:
        size_t                  index;
        Stimulator&             stimulator;
    
    public:
        InstanceStimulatorEventBase(Instance& instance, size_t index, Stimulator& stimulator) noexcept;

    public:
        size_t                  GetIndex() const noexcept;
        Stimulator&             GetStimulator() noexcept;
        const Stimulator&       GetStimulator() const noexcept;
    };


    template<class T>
    class InstancePreInitConfigurationEvent : public InstanceConfigurationEntryValueEventBase<T>
                                            , public Gravity::Event<InstancePreInitConfigurationEvent<T>> {
    public:
        InstancePreInitConfigurationEvent(Instance& instance, ConfigurationEntry<T> entry, const T& value) noexcept;
    };

    extern template class InstancePreInitConfigurationEvent<std::string>;
    extern template class InstancePreInitConfigurationEvent<bool>;
    extern template class InstancePreInitConfigurationEvent<uint32_t>;
    extern template class InstancePreInitConfigurationEvent<uint64_t>;

    extern template class InstancePreInitConfigurationEvent<std::vector<uint32_t>>;
    extern template class InstancePreInitConfigurationEvent<std::vector<uint64_t>>;

    class InstanceUpstreamInitializedEvent : public InstanceUpstreamEventBase 
                                           , public Gravity::Event<InstanceUpstreamInitializedEvent> {
    public:
        InstanceUpstreamInitializedEvent(Instance& instance, size_t index, CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>& upstream) noexcept;
    };

    class InstanceUpstreamAbsentEvent : public InstanceEventBase {
    protected:
        size_t                  index;

    public:
        InstanceUpstreamAbsentEvent(Instance& instance, size_t index) noexcept;

    public:
        size_t                  GetIndex() const noexcept;
    };

    class InstanceStimulatorInstantiatedEvent : public InstanceStimulatorEventBase
                                              , public Gravity::Event<InstanceStimulatorInstantiatedEvent> {
    public:
        InstanceStimulatorInstantiatedEvent(Instance& instance, size_t index, Stimulator& stimulator) noexcept;
    };

    class InstanceStimulatorInitializedEvent : public InstanceStimulatorEventBase
                                             , public Gravity::Event<InstanceStimulatorInitializedEvent> {
    public:
        InstanceStimulatorInitializedEvent(Instance& instance, size_t index, Stimulator& stimulator) noexcept;
    };


    class Instance::EventHub {
    public:
        Gravity::EventBus<InstancePreInitConfigurationEvent<std::string>>  
                                                                OnPreInitConfigString;
        Gravity::EventBus<InstancePreInitConfigurationEvent<bool>>         
                                                                OnPreInitConfigBool;
        Gravity::EventBus<InstancePreInitConfigurationEvent<uint32_t>>     
                                                                OnPreInitConfigUInt32;
        Gravity::EventBus<InstancePreInitConfigurationEvent<uint64_t>>     
                                                                OnPreInitConfigUInt64;
        Gravity::EventBus<InstancePreInitConfigurationEvent<std::vector<uint32_t>>> 
                                                                OnPreInitConfigVectorUInt32;
        Gravity::EventBus<InstancePreInitConfigurationEvent<std::vector<uint64_t>>> 
                                                                OnPreInitConfigVectorUInt64;

        Gravity::EventBus<InstanceUpstreamInitializedEvent>     OnUpstreamInitialized;
        Gravity::EventBus<InstanceUpstreamAbsentEvent>          OnUpstreamAbsent;
        Gravity::EventBus<InstanceStimulatorInstantiatedEvent>  OnStimulatorInstantiated;
        Gravity::EventBus<InstanceStimulatorInitializedEvent>   OnStimulatorInitialized;

    public:
        EventHub() noexcept = default;

    public:
        template<class T>
        void OnPreInitConfig(Instance& instance, ConfigurationEntry<T> entry, const T& value) noexcept;
    };
}


// Inline implementation of: class Instance
namespace Cohestra {

    template<class T>
    inline std::optional<T> Instance::operator[](ConfigurationEntry<T> entry) const noexcept
    {
        return config.Get(entry);
    }

    template<class T>
    inline void Instance::EventHub::OnPreInitConfig(Instance& instance, ConfigurationEntry<T> entry, const T& value) noexcept
    {
        if constexpr (std::is_same_v<T, std::string>)
            OnPreInitConfigString(instance, entry, value);
        else if constexpr (std::is_same_v<T, bool>)
            OnPreInitConfigBool(instance, entry, value);
        else if constexpr (std::is_same_v<T, uint32_t>)
            OnPreInitConfigUInt32(instance, entry, value);
        else if constexpr (std::is_same_v<T, uint64_t>)
            OnPreInitConfigUInt64(instance, entry, value);
        else if constexpr (std::is_same_v<T, std::vector<uint32_t>>)
            OnPreInitConfigVectorUInt32(instance, entry, value);
        else if constexpr (std::is_same_v<T, std::vector<uint64_t>>)
            OnPreInitConfigVectorUInt64(instance, entry, value);
    }
}
