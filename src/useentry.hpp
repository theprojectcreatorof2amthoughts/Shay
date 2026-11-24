#ifndef SHAY_USEENTRY_HPP
#define SHAY_USEENTRY_HPP

#include <string>
#include <unordered_set>
#include <unordered_map>

namespace shay {
inline std::unordered_set<std::string>& mutable_useentry_registry() {
    static std::unordered_set<std::string> entries = {
        "uiinit", "reginit", "advsynt", "exprinit", "varinit",
        "typeinit", "constinit", "scopeinit", "funcinit", "classinit",
        "objinit", "structinit", "enuminit", "geninit", "operinit",
        "logicinit", "condinit", "loopinit", "flowinit", "errinit",
        "asyncinit", "strinit", "numinit", "boolinit", "arrayinit",
        "meminit", "ptrinit", "ioinit", "mathinit", "eventinit",
        "netinit", "threadinit"
    };
    return entries;
}

inline bool is_lowercase_useentry_name(const std::string& name) {
    if (name.empty()) return false;
    for (const unsigned char c : name)
        if (c < 'a' || c > 'z') return false;
    return true;
}

struct EntrySyntaxDefinition { std::string entry; std::string behavior; };
inline std::unordered_map<std::string, EntrySyntaxDefinition>& mutable_entry_syntax_registry() {
    static std::unordered_map<std::string, EntrySyntaxDefinition> definitions;
    return definitions;
}
inline void register_entry_syntax(const std::string& entry, const std::string& syntax, const std::string& behavior) {
    if (is_lowercase_useentry_name(entry) && is_lowercase_useentry_name(syntax) && behavior == "function")
        mutable_entry_syntax_registry()[syntax] = {entry, behavior};
}
inline std::string entry_syntax_behavior(const std::string& syntax, const std::unordered_set<std::string>& active_entries) {
    const auto found = mutable_entry_syntax_registry().find(syntax);
    if (found == mutable_entry_syntax_registry().end() || !active_entries.count(found->second.entry)) return {};
    return found->second.behavior;
}

inline void register_useentry_name(const std::string& name) {
    if (is_lowercase_useentry_name(name))
        mutable_useentry_registry().insert(name);
}

inline bool is_valid_useentry_name(const std::string& name) {
    return is_lowercase_useentry_name(name) &&
           mutable_useentry_registry().count(name) != 0;
}
}

#endif
