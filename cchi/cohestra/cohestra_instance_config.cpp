#include "cohestra_instance.hpp"


// Implementation of: class Instance
namespace Cohestra {

    const ConfigurationEntryBack<std::vector<uint32_t>> Instance::CONFIG_T1_UPSTREAM_NODE_ID   
        ("T1_UPSTREAM_NODE_ID", "upstream.type1.nodeid", "root", {0, 1, 2, 3, 4, 5, 6, 7}, DeserializeUInt32Vector, SerializeUInt32Vector);

    const ConfigurationEntryBack<uint32_t> Instance::CONFIG_T1_UPSTREAM_INFLIGHT_EVT
        ("T1_UPSTREAM_INFLIGHT_EVT", "upstream.type1.inflight.evt", "root", 16, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> Instance::CONFIG_T1_UPSTREAM_INFLIGHT_SNP
        ("T1_UPSTREAM_INFLIGHT_SNP", "upstream.type1.inflight.snp", "root", 16, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> Instance::CONFIG_T1_UPSTREAM_INFLIGHT_REQ
        ("T1_UPSTREAM_INFLIGHT_REQ", "upstream.type1.inflight.req", "root", 16, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> Instance::CONFIG_T1_UPSTREAM_INFLIGHT
        ("T1_UPSTREAM_INFLIGHT", "upstream.type1.inflight", "root", 32, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> Instance::CONFIG_T1_UPSTREAM_BUFFER_EVT
        ("T1_UPSTREAM_BUFFER_EVT", "upstream.type1.buffer.evt", "root", 4, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> Instance::CONFIG_T1_UPSTREAM_BUFFER_REQ
        ("T1_UPSTREAM_BUFFER_REQ", "upstream.type1.buffer.req", "root", 4, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> Instance::CONFIG_T1_UPSTREAM_BUFFER_RSP
        ("T1_UPSTREAM_BUFFER_RSP", "upstream.type1.buffer.rsp", "root", 4, DeserializeUInt32, SerializeUInt32);

    const ConfigurationEntryBack<uint32_t> Instance::CONFIG_T1_UPSTREAM_BUFFER_DAT
        ("T1_UPSTREAM_BUFFER_DAT", "upstream.type1.buffer.dat", "root", 4, DeserializeUInt32, SerializeUInt32);


    const ConfigurationEntryBack<std::string> Instance::CONFIG_STIMULATOR_0
        ("STIMULATOR_0", "stimulator.0", "root", "", DeserializeString, SerializeString);

    const ConfigurationEntryBack<bool> Instance::CONFIG_STIMULATOR_0_ENABLE
        ("STIMULATOR_0_ENABLE", "stimulator.0.enable", "root", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<std::vector<uint32_t>> Instance::CONFIG_STIMULATOR_0_T1_UPSTREAMS
        ("STIMULATOR_0_T1_UPSTREAMS", "stimulator.0.upstreams.type1", "root", {}, DeserializeUInt32Vector, SerializeUInt32Vector);

    const ConfigurationEntryBack<std::string> Instance::CONFIG_STIMULATOR_1
        ("STIMULATOR_1", "stimulator.1", "root", "", DeserializeString, SerializeString);

    const ConfigurationEntryBack<bool> Instance::CONFIG_STIMULATOR_1_ENABLE
        ("STIMULATOR_1_ENABLE", "stimulator.1.enable", "root", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<std::vector<uint32_t>> Instance::CONFIG_STIMULATOR_1_T1_UPSTREAMS
        ("STIMULATOR_1_T1_UPSTREAMS", "stimulator.1.upstreams.type1", "root", {}, DeserializeUInt32Vector, SerializeUInt32Vector);

    const ConfigurationEntryBack<std::string> Instance::CONFIG_STIMULATOR_2
        ("STIMULATOR_2", "stimulator.2", "root", "", DeserializeString, SerializeString);

    const ConfigurationEntryBack<bool> Instance::CONFIG_STIMULATOR_2_ENABLE
        ("STIMULATOR_2_ENABLE", "stimulator.2.enable", "root", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<std::vector<uint32_t>> Instance::CONFIG_STIMULATOR_2_T1_UPSTREAMS
        ("STIMULATOR_2_T1_UPSTREAMS", "stimulator.2.upstreams.type1", "root", {}, DeserializeUInt32Vector, SerializeUInt32Vector);

    const ConfigurationEntryBack<std::string> Instance::CONFIG_STIMULATOR_3
        ("STIMULATOR_3", "stimulator.3", "root", "", DeserializeString, SerializeString);

    const ConfigurationEntryBack<bool> Instance::CONFIG_STIMULATOR_3_ENABLE
        ("STIMULATOR_3_ENABLE", "stimulator.3.enable", "root", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<std::vector<uint32_t>> Instance::CONFIG_STIMULATOR_3_T1_UPSTREAMS
        ("STIMULATOR_3_T1_UPSTREAMS", "stimulator.3.upstreams.type1", "root", {}, DeserializeUInt32Vector, SerializeUInt32Vector);

    const ConfigurationEntryBack<std::string> Instance::CONFIG_STIMULATOR_4
        ("STIMULATOR_4", "stimulator.4", "root", "", DeserializeString, SerializeString);

    const ConfigurationEntryBack<bool> Instance::CONFIG_STIMULATOR_4_ENABLE
        ("STIMULATOR_4_ENABLE", "stimulator.4.enable", "root", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<std::vector<uint32_t>> Instance::CONFIG_STIMULATOR_4_T1_UPSTREAMS
        ("STIMULATOR_4_T1_UPSTREAMS", "stimulator.4.upstreams.type1", "root", {}, DeserializeUInt32Vector, SerializeUInt32Vector);

    const ConfigurationEntryBack<std::string> Instance::CONFIG_STIMULATOR_5
        ("STIMULATOR_5", "stimulator.5", "root", "", DeserializeString, SerializeString);

    const ConfigurationEntryBack<bool> Instance::CONFIG_STIMULATOR_5_ENABLE
        ("STIMULATOR_5_ENABLE", "stimulator.5.enable", "root", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<std::vector<uint32_t>> Instance::CONFIG_STIMULATOR_5_T1_UPSTREAMS
        ("STIMULATOR_5_T1_UPSTREAMS", "stimulator.5.upstreams.type1", "root", {}, DeserializeUInt32Vector, SerializeUInt32Vector);

    const ConfigurationEntryBack<std::string> Instance::CONFIG_STIMULATOR_6
        ("STIMULATOR_6", "stimulator.6", "root", "", DeserializeString, SerializeString);

    const ConfigurationEntryBack<bool> Instance::CONFIG_STIMULATOR_6_ENABLE
        ("STIMULATOR_6_ENABLE", "stimulator.6.enable", "root", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<std::vector<uint32_t>> Instance::CONFIG_STIMULATOR_6_T1_UPSTREAMS
        ("STIMULATOR_6_T1_UPSTREAMS", "stimulator.6.upstreams.type1", "root", {}, DeserializeUInt32Vector, SerializeUInt32Vector);

    const ConfigurationEntryBack<std::string> Instance::CONFIG_STIMULATOR_7
        ("STIMULATOR_7", "stimulator.7", "root", "", DeserializeString, SerializeString);

    const ConfigurationEntryBack<bool> Instance::CONFIG_STIMULATOR_7_ENABLE
        ("STIMULATOR_7_ENABLE", "stimulator.7.enable", "root", false, DeserializeBool, SerializeBool);

    const ConfigurationEntryBack<std::vector<uint32_t>> Instance::CONFIG_STIMULATOR_7_T1_UPSTREAMS
        ("STIMULATOR_7_T1_UPSTREAMS", "stimulator.7.upstreams.type1", "root", {}, DeserializeUInt32Vector, SerializeUInt32Vector);


    const std::array<ConfigurationEntryBack<std::string>, 8> Instance::VECTOR_CONFIG_STIMULATOR
        = { CONFIG_STIMULATOR_0, CONFIG_STIMULATOR_1, CONFIG_STIMULATOR_2, CONFIG_STIMULATOR_3,
            CONFIG_STIMULATOR_4, CONFIG_STIMULATOR_5, CONFIG_STIMULATOR_6, CONFIG_STIMULATOR_7 };

    const std::array<ConfigurationEntryBack<bool>, 8> Instance::VECTOR_CONFIG_STIMULATOR_ENABLE
        = { CONFIG_STIMULATOR_0_ENABLE, CONFIG_STIMULATOR_1_ENABLE, CONFIG_STIMULATOR_2_ENABLE, CONFIG_STIMULATOR_3_ENABLE,
            CONFIG_STIMULATOR_4_ENABLE, CONFIG_STIMULATOR_5_ENABLE, CONFIG_STIMULATOR_6_ENABLE, CONFIG_STIMULATOR_7_ENABLE };

    const std::array<ConfigurationEntryBack<std::vector<uint32_t>>, 8> Instance::VECTOR_CONFIG_STIMULATOR_T1_UPSTREAMS
        = { CONFIG_STIMULATOR_0_T1_UPSTREAMS, CONFIG_STIMULATOR_1_T1_UPSTREAMS, CONFIG_STIMULATOR_2_T1_UPSTREAMS, CONFIG_STIMULATOR_3_T1_UPSTREAMS,
            CONFIG_STIMULATOR_4_T1_UPSTREAMS, CONFIG_STIMULATOR_5_T1_UPSTREAMS, CONFIG_STIMULATOR_6_T1_UPSTREAMS, CONFIG_STIMULATOR_7_T1_UPSTREAMS };
}
