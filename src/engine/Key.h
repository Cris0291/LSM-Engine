#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace lsm {

using KeyBytes = std::vector<std::byte>;

template <typename K>
concept Key =
    std::same_as<K, std::string> || std::same_as<K, std::string_view> ||
    std::same_as<K, std::uint64_t> || std::same_as<K, std::int64_t> ||
    std::same_as<K, std::uint32_t> || std::same_as<K, std::int32_t>;

} // namespace lsm
