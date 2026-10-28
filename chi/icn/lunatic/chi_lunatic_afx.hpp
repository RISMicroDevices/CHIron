#pragma once

#include "../../../chi_eb/spec/chi_eb_protocol_flits.hpp"

namespace CHI::Lunatic {

class EventHub;
class Core;
class Directory;
class SnoopFilter;
class CachePolicy;
class Tracker;
class HazardGraph;
class SparseMemory;

struct Options;
struct Request;
struct Response;
struct Data;
struct Snoop;

template<CHI::Eb::FlitConfigurationConcept config>
class HomeNode;

template<CHI::Eb::FlitConfigurationConcept config>
class Interconnect;

template<CHI::Eb::FlitConfigurationConcept config>
class Bridge;

}
