#include "cohestra_instance.hpp"


// Implementation of: class Instance
namespace Cohestra {

    Instance::~Instance() noexcept
    {
        for (auto* stimulator : stimulators)
        {
            if (stimulator)
                delete stimulator;
        }
        stimulators.clear();

        for (auto* upstream : type1Upstreams)
        {
            if (upstream)
                delete upstream;
        }
        type1Upstreams.clear();
    }

    void Instance::Initialize(std::shared_ptr<CCHIInterface>        downstreams,
                              std::shared_ptr<StimulatorRegistry>   stimulators) noexcept
    {
        if (state != InstanceState::NOT_INITIALIZED)
            return;

        if (!downstreams)
            return;

        this->downstreams = std::move(downstreams);

        auto indices = this->downstreams->GetType1Indices();
        auto type1MaxIndex = this->downstreams->GetType1MaxIndex();

        try {
            // TX-channel buffer depths (upstream.type1.buffer.*). Applied before
            // any Tick, so the FIFOs are still empty and always resize cleanly.
            uint32_t bufferEVT = config.GetOrDefault(&CONFIG_T1_UPSTREAM_BUFFER_EVT);
            uint32_t bufferREQ = config.GetOrDefault(&CONFIG_T1_UPSTREAM_BUFFER_REQ);
            uint32_t bufferRSP = config.GetOrDefault(&CONFIG_T1_UPSTREAM_BUFFER_RSP);
            uint32_t bufferDAT = config.GetOrDefault(&CONFIG_T1_UPSTREAM_BUFFER_DAT);

            if (!this->downstreams->SetAllType1EVTBufferSize(bufferEVT) ||
                !this->downstreams->SetAllType1REQBufferSize(bufferREQ) ||
                !this->downstreams->SetAllType1UpRSPBufferSize(bufferRSP) ||
                !this->downstreams->SetAllType1UpDATBufferSize(bufferDAT))
            {
                logger->error("Failed to apply Type 1 buffer depths (EVT={}, REQ={}, RSP={}, DAT={}). Stopping initialization.",
                    bufferEVT, bufferREQ, bufferRSP, bufferDAT);
                state = InstanceState::FAILED;
                return;
            }

            logger->info("Type 1 buffer depths: EVT={}, REQ={}, RSP={}, DAT={}.",
                bufferEVT, bufferREQ, bufferRSP, bufferDAT);

            // Upstream Node initialization
            type1Upstreams.resize(type1MaxIndex + 1, nullptr);

            for (size_t index = 0; index < type1Upstreams.size(); ++index)
            {
                if (index >= config.GetOrDefault(&CONFIG_T1_UPSTREAM_NODE_ID).size())
                {
                    logger->error("Type 1 Upstream index {} exceeds the configured T1_UPSTREAM_NODE_ID size. Stopping initialization.", index);
                    state = InstanceState::FAILED;
                    return;
                }

                if (indices.find(index) != indices.end())
                {
                    type1Upstreams[index] = new CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>(
                        config.GetOrDefault(&CONFIG_T1_UPSTREAM_NODE_ID).at(index),
                        nullptr,
                        config.GetOrDefault(&CONFIG_T1_UPSTREAM_INFLIGHT_EVT),
                        config.GetOrDefault(&CONFIG_T1_UPSTREAM_INFLIGHT_SNP),
                        config.GetOrDefault(&CONFIG_T1_UPSTREAM_INFLIGHT_REQ),
                        config.GetOrDefault(&CONFIG_T1_UPSTREAM_INFLIGHT),
                        4,      // prefetchQueueLimit (header default)
                        4,      // cmoQueueLimit (header default)
                        false,  // enableSilentEviction
                        false   // enableStrictInitialState
                    );

                    if (events)
                        events->OnUpstreamInitialized(*this, index, *type1Upstreams[index]);
                }
                else
                {
                    if (events)
                        events->OnUpstreamAbsent(*this, index);
                }
            }

            // Stimulators initialization
            this->stimulators.resize(8, nullptr);

            for (size_t i = 0; i < this->stimulators.size(); ++i)
            {
                std::string stimName = config.GetOrDefault(&VECTOR_CONFIG_STIMULATOR.at(i));
                bool stimEnable = config.GetOrDefault(&VECTOR_CONFIG_STIMULATOR_ENABLE.at(i));
                auto stimUpstreams = config.GetOrDefault(&VECTOR_CONFIG_STIMULATOR_T1_UPSTREAMS.at(i));

                if (stimEnable && !stimName.empty())
                {
                    if (!stimulators)
                    {
                        logger->error("Stimulator registry is not provided, but stimulator {} is enabled. Stopping initialization.", stimName);
                        state = InstanceState::FAILED;
                        return;
                    }

                    auto stimFactory = stimulators->Get(stimName);
                    if (!stimFactory)
                    {
                        logger->error("Stimulator {} is not found in the registry. Stopping initialization.", stimName);
                        state = InstanceState::FAILED;
                        return;
                    }

                    this->stimulators[i] = stimFactory->Create(*this, i, stimUpstreams).release();

                    if (events)
                        events->OnStimulatorInstantiated(*this, i, *this->stimulators[i]);
                }
            }

        } catch (const std::exception& e) {
            logger->error("Exception during Instance initialization: {}", e.what());
            state = InstanceState::FAILED;
            return;
        } catch (...) {
            logger->error("Unknown exception during Instance initialization.");
            state = InstanceState::FAILED;
            return;
        }

        state = InstanceState::ALIVE;
    }

    void Instance::LoadConfiguration(const Configuration& config) noexcept
    {
        this->config = config;
    }

    template<class T>
    bool Instance::LoadPreInitConfigurationEntry(const StringLoaderBase& loader, ConfigurationEntry<T> entry) noexcept
    {
        std::optional<std::string> rawValue = loader.GetRawString(entry);

        if (!rawValue)
        {
            config.Set(entry, entry->defaultValue);
            
            if (events)
                events->OnPreInitConfig(*this, entry, entry->defaultValue);

            return true;
        }
        else
        {
            std::optional<T> deserializedValue = entry->deserializer(*rawValue);

            if (!deserializedValue)
            {
                logger->error("Failed to parse {} ({}) from configuration value: {}", 
                    entry->key, entry->name, *rawValue);

                return false;
            }

            config.Set(entry, *deserializedValue);
            
            if (events)
                events->OnPreInitConfig(*this, entry, *deserializedValue);

            return true;
        }

        return true;
    }

    bool Instance::LoadPreInitConfiguration(const StringLoaderBase& loader) noexcept
    {
        bool good = LoadPreInitConfigurationEntry(loader, &CONFIG_T1_UPSTREAM_NODE_ID) &&
                    LoadPreInitConfigurationEntry(loader, &CONFIG_T1_UPSTREAM_INFLIGHT_EVT) &&
                    LoadPreInitConfigurationEntry(loader, &CONFIG_T1_UPSTREAM_INFLIGHT_SNP) &&
                    LoadPreInitConfigurationEntry(loader, &CONFIG_T1_UPSTREAM_INFLIGHT_REQ) &&
                    LoadPreInitConfigurationEntry(loader, &CONFIG_T1_UPSTREAM_INFLIGHT) &&
                    LoadPreInitConfigurationEntry(loader, &CONFIG_T1_UPSTREAM_BUFFER_EVT) &&
                    LoadPreInitConfigurationEntry(loader, &CONFIG_T1_UPSTREAM_BUFFER_REQ) &&
                    LoadPreInitConfigurationEntry(loader, &CONFIG_T1_UPSTREAM_BUFFER_RSP) &&
                    LoadPreInitConfigurationEntry(loader, &CONFIG_T1_UPSTREAM_BUFFER_DAT);

        for (size_t i = 0; i < VECTOR_CONFIG_STIMULATOR.size() && good; ++i)
        {
            good &= LoadPreInitConfigurationEntry(loader, &VECTOR_CONFIG_STIMULATOR.at(i)) &&
                    LoadPreInitConfigurationEntry(loader, &VECTOR_CONFIG_STIMULATOR_ENABLE.at(i)) &&
                    LoadPreInitConfigurationEntry(loader, &VECTOR_CONFIG_STIMULATOR_T1_UPSTREAMS.at(i));
        }

        return good;
    }

    bool Instance::LoadPostInitConfiguration(const StringLoaderBase& loader) noexcept
    {
        for (size_t i = 0; i < stimulators.size(); i++)
        {
            if (!stimulators[i])
                continue;

            if (!stimulators[i]->Initialize(loader))
                return false;

            if (events)
                events->OnStimulatorInitialized(*this, i, *stimulators[i]);
        }

        return true;
    }

    bool Instance::Tick(uint64_t time) noexcept
    {
        if (state != InstanceState::ALIVE)
            return false;

        for (auto* stimulator : stimulators)
        {
            if (stimulator)
                stimulator->Tick(time);
        }

        for (auto* upstream : type1Upstreams)
        {
            if (upstream)
                upstream->Tick(time);
        }

        if (downstreams)
            downstreams->TickPreHandshake(time);

        for (size_t index = 0; index < type1Upstreams.size(); ++index)
        {
            auto* upstream = type1Upstreams[index];

            if (!upstream)
                continue;

            if (auto flit = upstream->PeekTXEVT())
            {
                if (downstreams->PushType1EVT(index, *flit))
                    upstream->PopTXEVT();
            }

            if (auto flit = upstream->PeekTXRSP())
            {
                if (downstreams->PushType1UpRSP(index, *flit))
                    upstream->PopTXRSP();
            }

            if (auto flit = upstream->PeekTXDAT())
            {
                if (downstreams->PushType1UpDAT(index, *flit))
                    upstream->PopTXDAT();
            }

            if (auto flit = upstream->PeekTXREQ())
            {
                if (downstreams->PushType1REQ(index, *flit))
                    upstream->PopTXREQ();
            }

            if (auto flit = downstreams->PeekType1SNP(index))
            {
                if (upstream->PushRXSNP(*flit))
                    downstreams->PopType1SNP(index);
            }

            if (auto flit = downstreams->PeekType1DnRSP(index))
            {
                if (upstream->PushRXRSP(*flit))
                    downstreams->PopType1DnRSP(index);
            }

            if (auto flit = downstreams->PeekType1DnDAT(index))
            {
                if (upstream->PushRXDAT(*flit))
                    downstreams->PopType1DnDAT(index);
            }
        }

        return true;
    }

    void Instance::TickPostHandshake(uint64_t time) noexcept
    {
        if (downstreams)
            downstreams->TickPostHandshake(time);
    }

    std::shared_ptr<spdlog::logger> Instance::GetLogger() const noexcept
    {
        return logger;
    }

    void Instance::SetLogger(std::shared_ptr<spdlog::logger> logger) noexcept
    {
        this->logger = std::move(logger);
    }

    Configuration& Instance::GetConfiguration() noexcept
    {
        return config;
    }

    const Configuration& Instance::GetConfiguration() const noexcept
    {
        return config;
    }

    InstanceState Instance::GetState() const noexcept
    {
        return state;
    }

    bool Instance::IsAlive() const noexcept
    {
        return state == InstanceState::ALIVE;
    }

    bool Instance::IsFinished() const noexcept
    {
        return state == InstanceState::FINISHED;
    }

    bool Instance::IsFailed() const noexcept
    {
        return state == InstanceState::FAILED;
    }

    void Instance::SetAlive() noexcept
    {
        state = InstanceState::ALIVE;
    }

    void Instance::SetFinished() noexcept
    {
        state = InstanceState::FINISHED;
    }

    void Instance::SetFinished(const std::vector<std::string>& message) noexcept
    {
        finishMessage = message;
        state = InstanceState::FINISHED;
    }

    void Instance::SetFailed() noexcept
    {
        state = InstanceState::FAILED;
    }

    void Instance::SetFailed(const std::vector<std::string>& message) noexcept
    {
        failMessage = message;
        state = InstanceState::FAILED;
    }

    const std::vector<std::string>& Instance::GetFinishMessage() const noexcept
    {
        return finishMessage;
    }

    const std::vector<std::string>& Instance::GetFailMessage() const noexcept
    {
        return failMessage;
    }

    std::shared_ptr<CCHIInterface> Instance::GetDownstreams() noexcept
    {
        return downstreams;
    }

    std::shared_ptr<const CCHIInterface> Instance::GetDownstreams() const noexcept
    {
        return downstreams;
    }

    size_t Instance::GetType1UpstreamCount() const noexcept
    {
        return type1Upstreams.size();
    }

    CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>*
    Instance::GetType1Upstream(size_t index) noexcept
    {
        if (index >= type1Upstreams.size())
            return nullptr;

        return type1Upstreams[index];
    }

    const CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>*
    Instance::GetType1Upstream(size_t index) const noexcept
    {
        if (index >= type1Upstreams.size())
            return nullptr;

        return type1Upstreams[index];
    }

    std::optional<size_t> Instance::GetType1UpstreamIndex(const CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>* upstream) const noexcept
    {
        for (size_t index = 0; index < type1Upstreams.size(); ++index)
        {
            if (type1Upstreams[index] == upstream)
                return { index };
        }

        return std::nullopt;
    }

    size_t Instance::GetStimulatorCount() const noexcept
    {
        return stimulators.size();
    }

    Stimulator* Instance::GetStimulator(size_t index) noexcept
    {
        if (index >= stimulators.size())
            return nullptr;

        return stimulators[index];
    }

    const Stimulator* Instance::GetStimulator(size_t index) const noexcept
    {
        if (index >= stimulators.size())
            return nullptr;

        return stimulators[index];
    }
}


// Implementation of: class InstanceEventBase
namespace Cohestra {

    InstanceEventBase::InstanceEventBase(Instance& instance) noexcept
        : instance(instance)
    { }

    Instance& InstanceEventBase::GetInstance() noexcept
    {
        return instance;
    }

    const Instance& InstanceEventBase::GetInstance() const noexcept
    {
        return instance;
    }
}

// Implementation of: class InstanceConfigurationEntryEventBase
namespace Cohestra {
    
    template<class T>
    InstanceConfigurationEntryEventBase<T>::InstanceConfigurationEntryEventBase(Instance& instance, ConfigurationEntry<T> entry) noexcept
        : InstanceEventBase (instance)
        , entry             (entry)
    { }

    template<class T>
    ConfigurationEntry<T> InstanceConfigurationEntryEventBase<T>::GetEntry() const noexcept
    {
        return entry;
    }
}

// Implementation of: class InstanceConfigurationEntryValueEventBase
namespace Cohestra {

    template<class T>
    InstanceConfigurationEntryValueEventBase<T>::InstanceConfigurationEntryValueEventBase(Instance& instance, ConfigurationEntry<T> entry, const T& value) noexcept
        : InstanceConfigurationEntryEventBase<T> (instance, entry)
        , value                                  (value)
    { }

    template<class T>
    const T& InstanceConfigurationEntryValueEventBase<T>::GetValue() const noexcept
    {
        return value;
    }
}

// Implementation of: class InstancePreInitConfigurationEvent
namespace Cohestra {

    template<class T>
    InstancePreInitConfigurationEvent<T>::InstancePreInitConfigurationEvent(Instance& instance, ConfigurationEntry<T> entry, const T& value) noexcept
        : InstanceConfigurationEntryValueEventBase<T> (instance, entry, value)
    { }

    template class InstancePreInitConfigurationEvent<std::string>;
    template class InstancePreInitConfigurationEvent<bool>;
    template class InstancePreInitConfigurationEvent<uint32_t>;
    template class InstancePreInitConfigurationEvent<uint64_t>;

    template class InstancePreInitConfigurationEvent<std::vector<uint32_t>>;
    template class InstancePreInitConfigurationEvent<std::vector<uint64_t>>;
}

// Implementation of: class InstanceUpstreamEventBase
namespace Cohestra {

    InstanceUpstreamEventBase::InstanceUpstreamEventBase(Instance&                                                  instance, 
                                                         size_t                                                     index, 
                                                         CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>&  upstream) noexcept
        : InstanceEventBase (instance)
        , index             (index)
        , upstream          (upstream)
    { }

    size_t InstanceUpstreamEventBase::GetIndex() const noexcept
    {
        return index;
    }

    CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>& InstanceUpstreamEventBase::GetUpstream() noexcept
    {
        return upstream;
    }

    const CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>& InstanceUpstreamEventBase::GetUpstream() const noexcept
    {
        return upstream;
    }
}

// Implementation of: class InstanceStimulatorEventBase
namespace Cohestra {

    InstanceStimulatorEventBase::InstanceStimulatorEventBase(Instance& instance, size_t index, Stimulator& stimulator) noexcept
        : InstanceEventBase (instance)
        , index             (index)
        , stimulator        (stimulator)
    { }

    size_t InstanceStimulatorEventBase::GetIndex() const noexcept
    {
        return index;
    }

    Stimulator& InstanceStimulatorEventBase::GetStimulator() noexcept
    {
        return stimulator;
    }

    const Stimulator& InstanceStimulatorEventBase::GetStimulator() const noexcept
    {
        return stimulator;
    }
}


// Implementation of: class InstanceUpstreamInitializedEvent
namespace Cohestra {

    InstanceUpstreamInitializedEvent::InstanceUpstreamInitializedEvent(Instance&                                                  instance, 
                                                                       size_t                                                     index, 
                                                                       CCHI::Taurus::UpstreamNode<CommonFlitConfigurationType1>&  upstream) noexcept
        : InstanceUpstreamEventBase (instance, index, upstream)
    { }
}

// Implementation of: class InstanceUpstreamAbsentEvent
namespace Cohestra {

    InstanceUpstreamAbsentEvent::InstanceUpstreamAbsentEvent(Instance& instance, size_t index) noexcept
        : InstanceEventBase (instance)
        , index             (index)
    { }

    size_t InstanceUpstreamAbsentEvent::GetIndex() const noexcept
    {
        return index;
    }
}

// Implementation of: class InstanceStimulatorInstantiatedEvent
namespace Cohestra {

    InstanceStimulatorInstantiatedEvent::InstanceStimulatorInstantiatedEvent(Instance& instance, size_t index, Stimulator& stimulator) noexcept
        : InstanceStimulatorEventBase (instance, index, stimulator)
    { }
}

// Implementation of: class InstanceStimulatorInitializedEvent
namespace Cohestra {

    InstanceStimulatorInitializedEvent::InstanceStimulatorInitializedEvent(Instance& instance, size_t index, Stimulator& stimulator) noexcept
        : InstanceStimulatorEventBase (instance, index, stimulator)
    { }
}
