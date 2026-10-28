#include "chi_lunatic_directory.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace CHI::Lunatic {

Directory::Directory(size_t sets, size_t ways)
    : sets(sets), ways(ways)
{
    if (!sets || !ways)
        throw std::invalid_argument("Lunatic directory geometry must be non-zero");
    if (sets > std::numeric_limits<size_t>::max() / ways)
        throw std::invalid_argument("Lunatic directory geometry is too large");

    lines.resize(sets * ways);
}

Directory::Directory(const Options& options)
    : Directory(options.slcSets, options.slcWays)
{
}

size_t Directory::Index(size_t set, size_t way) const noexcept
{
    return set * ways + way;
}

size_t Directory::SetIndex(const LineKey& key) const noexcept
{
    return size_t((key.address / LINE_SIZE) % sets);
}

HomeLine* Directory::Find(const LineKey& key) noexcept
{
    const size_t set = SetIndex(key);
    for (size_t way = 0; way < ways; ++way) {
        HomeLine& line = At(set, way);
        if (line.valid && line.key == key)
            return &line;
    }
    return nullptr;
}

const HomeLine* Directory::Find(const LineKey& key) const noexcept
{
    const size_t set = SetIndex(key);
    for (size_t way = 0; way < ways; ++way) {
        const HomeLine& line = At(set, way);
        if (line.valid && line.key == key)
            return &line;
    }
    return nullptr;
}

HomeLine& Directory::At(size_t set, size_t way) noexcept
{
    return lines[Index(set, way)];
}

const HomeLine& Directory::At(size_t set, size_t way) const noexcept
{
    return lines[Index(set, way)];
}

std::optional<size_t> Directory::FindWay(const LineKey& key) const noexcept
{
    const size_t set = SetIndex(key);
    for (size_t way = 0; way < ways; ++way) {
        const HomeLine& line = At(set, way);
        if (line.valid && line.key == key)
            return way;
    }
    return std::nullopt;
}

std::vector<size_t> Directory::VictimCandidates(const LineKey&) const
{
    std::vector<size_t> candidates;
    candidates.reserve(ways);
    for (size_t way = 0; way < ways; ++way)
        candidates.push_back(way);
    return candidates;
}

size_t Directory::ChooseVictim(const LineKey& key) const
{
    const size_t set = SetIndex(key);
    size_t victim = 0;
    for (size_t way = 0; way < ways; ++way) {
        if (!At(set, way).valid)
            return way;
    }
    for (size_t way = 1; way < ways; ++way) {
        if (At(set, way).age < At(set, victim).age)
            victim = way;
    }
    return victim;
}

HomeLine& Directory::Install(const LineKey& key, size_t way, uint64_t age)
{
    if (way >= ways)
        throw std::out_of_range("Lunatic directory way is out of range");

    HomeLine& line = At(SetIndex(key), way);
    line = HomeLine{};
    line.key = key;
    line.age = age;
    line.valid = true;
    return line;
}

void Directory::Invalidate(size_t set, size_t way) noexcept
{
    At(set, way) = HomeLine{};
}

void Directory::Clear() noexcept
{
    for (HomeLine& line : lines)
        line = HomeLine{};
}

size_t Directory::ValidCount() const noexcept
{
    return size_t(std::count_if(lines.begin(), lines.end(), [](const HomeLine& line) {
        return line.valid;
    }));
}

}
