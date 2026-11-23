#ifndef SHAY_USEENTRY_HPP
#define SHAY_USEENTRY_HPP

#include <string>
#include <unordered_set>

namespace shay {
inline const std::unordered_set<std::string>& useentry_registry() {
    static const std::unordered_set<std::string> entries = {
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

inline bool is_valid_useentry_name(const std::string& name) {
    if (name.empty()) return false;
    for (const unsigned char c : name) {
        if (c < 'a' || c > 'z') return false;
    }
    return useentry_registry().count(name) != 0;
}
}

#endif
