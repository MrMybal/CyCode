#include "cycode/SemanticTokens.h"
#include <algorithm>
#include <stdexcept>

namespace cycode {
using Json = nlohmann::json;
namespace {
uint32_t integer(const Json &value) {
    if (!value.is_number_integer() ||
        (value.is_number_unsigned() ? value.get<uint64_t>() > 2147483647ULL
                                    : value.get<int64_t>() < 0 || value.get<int64_t>() > 2147483647LL))
        throw std::runtime_error("Invalid semantic token integer");
    return value.get<uint32_t>();
}
std::vector<uint32_t> readWords(const Json &value) {
    if (!value.is_array() || value.size() > SemanticTokens::maxWords)
        throw std::runtime_error("Invalid semantic token array size");
    std::vector<uint32_t> result;
    result.reserve(value.size());
    for (const auto &word : value)
        result.push_back(integer(word));
    return result;
}
} // namespace
Json SemanticTokens::Data::result() const {
    return {{"data", words}};
}
SemanticTokens::Snapshot SemanticTokens::decode(const Json &response, const Snapshot &base) {
    auto result = std::make_shared<Data>();
    if (response.is_null())
        return result;
    if (!response.is_object() || response.contains("error") ||
        response.contains("data") == response.contains("edits"))
        throw std::runtime_error("Invalid semantic token response");
    if (response.contains("resultId")) {
        const auto &id = response.at("resultId");
        if (!id.is_string() || id.get_ref<const std::string &>().size() > 4096)
            throw std::runtime_error("Invalid semantic token result ID");
        result->resultId = id.get<std::string>();
    }
    if (response.contains("data"))
        result->words = readWords(response.at("data"));
    else {
        if (!base || !base->resultId || base->words.size() > maxWords || base->words.size() % 5)
            throw std::runtime_error("Semantic token delta has no reviewed base");
        const auto &edits = response.at("edits");
        if (!edits.is_array() || edits.size() > maxEdits)
            throw std::runtime_error("Invalid semantic token edit count");
        struct Edit {
            size_t start, count;
            std::vector<uint32_t> data;
        };
        std::vector<Edit> ordered;
        size_t inserted = 0, removed = 0;
        for (const auto &edit : edits) {
            const auto start = integer(edit.at("start")), count = integer(edit.at("deleteCount"));
            if (start > base->words.size() || count > base->words.size() - start)
                throw std::runtime_error("Semantic token edit exceeds its base");
            auto data = edit.contains("data") ? readWords(edit.at("data")) : std::vector<uint32_t>{};
            inserted += data.size();
            removed += count;
            if (inserted > maxWords)
                throw std::runtime_error("Semantic token delta insertion limit exceeded");
            ordered.push_back({start, count, std::move(data)});
        }
        std::stable_sort(ordered.begin(), ordered.end(),
                         [](const auto &a, const auto &b) { return a.start < b.start; });
        size_t cursor = 0;
        for (const auto &edit : ordered) {
            if (edit.start < cursor)
                throw std::runtime_error("Overlapping semantic token edits");
            cursor = edit.start + edit.count;
        }
        const auto size = base->words.size() - removed + inserted;
        if (size > maxWords)
            throw std::runtime_error("Semantic token delta result limit exceeded");
        result->words.reserve(size);
        cursor = 0;
        for (const auto &edit : ordered) {
            result->words.insert(result->words.end(), base->words.begin() + cursor,
                                 base->words.begin() + edit.start);
            result->words.insert(result->words.end(), edit.data.begin(), edit.data.end());
            cursor = edit.start + edit.count;
        }
        result->words.insert(result->words.end(), base->words.begin() + cursor, base->words.end());
    }
    if (result->words.size() % 5)
        throw std::runtime_error("Semantic token result has incomplete tokens");
    return result;
}
SemanticTokens::Snapshot SemanticTokens::find(const std::string &uri) {
    const auto found = entries_.find(uri);
    if (found == entries_.end())
        return {};
    found->second.used = ++clock_;
    return found->second.data;
}
void SemanticTokens::remember(const std::string &uri, Snapshot data) {
    erase(uri);
    if (!data || !data->resultId || data->resultId->size() > 4096 || data->words.size() > maxWords ||
        data->words.size() % 5)
        return;
    while (!entries_.empty() &&
           (entries_.size() >= maxEntries || words_ + data->words.size() > maxCachedWords)) {
        const auto oldest =
            std::min_element(entries_.begin(), entries_.end(),
                             [](const auto &a, const auto &b) { return a.second.used < b.second.used; });
        erase(oldest->first);
    }
    const auto size = data->words.size();
    entries_.emplace(uri, Entry{std::move(data), ++clock_});
    words_ += size;
}
void SemanticTokens::erase(const std::string &uri) {
    const auto found = entries_.find(uri);
    if (found != entries_.end()) {
        words_ -= found->second.data->words.size();
        entries_.erase(found);
    }
}
void SemanticTokens::clear() {
    entries_.clear();
    words_ = 0;
    clock_ = 0;
}
} // namespace cycode
