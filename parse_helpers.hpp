#ifndef PARSE_HELPERS_HPP
#define PARSE_HELPERS_HPP

#include <string_view>

constexpr bool is_upper(char ch) {
    return ch >= 'A' && ch <= 'Z';
}

constexpr bool is_lower(char ch) {
    return ch >= 'a' && ch <= 'z';
}

constexpr bool is_digit(char ch) {
    return ch >= '0' && ch <= '9';
}

constexpr char to_upper(char ch) {
    return is_lower(ch) ? ch - ('a' - 'A') : ch;
}

constexpr char to_lower(char ch) {
    return is_upper(ch) ? ch + ('a' - 'A') : ch;
}

inline void trim_front(std::string_view &sv) {
    size_t i = 0;
    for (; i < sv.size() && sv[i] == ' '; ++i);
    sv = sv.substr(i);
}

inline void skip_word(std::string_view &sv) {
    size_t i = 0;
    for (; i < sv.size() && sv[i] != ' '; ++i);
    sv = sv.substr(i);
}

inline void next_word(std::string_view &sv) {
    size_t i = 0;
    for (; i < sv.size() && sv[i] == ' '; ++i);
    for (; i < sv.size() && sv[i] != ' '; ++i);
    for (; i < sv.size() && sv[i] == ' '; ++i);
    sv = sv.substr(i);
}

//checks if strings are equal, ignoring case
inline bool istr_equal(std::string_view a, std::string_view b) {
    if (a.size() != b.size())
        return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (to_lower(a[i]) != to_lower(b[i]))
            return false;
    return true;
}

#endif 
