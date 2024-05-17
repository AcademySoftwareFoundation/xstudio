// SPDX-License-Identifier: Apache-2.0
#pragma once

#ifdef _WIN32
#include <winsock2.h>
#endif
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iterator>
#include <locale>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#ifdef __linux__
#include <unistd.h>
#endif
#include <vector>
#include <cstdarg>

namespace xstudio {
namespace utility {
    // trim from start (in place)
    static inline void ltrim_inplace(std::string &s) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
                    return !std::isspace(ch);
                }));
    }

    // trim from end (in place)
    static inline void rtrim_inplace(std::string &s) {
        s.erase(
            std::find_if(s.rbegin(), s.rend(), [](int ch) { return !std::isspace(ch); }).base(),
            s.end());
    }

    // trim from both ends (in place)
    static inline void trim_inplace(std::string &s) {
        ltrim_inplace(s);
        rtrim_inplace(s);
    }

    // trim from both ends (in place)
    static inline std::string ltrim(const std::string &s) {
        std::string res(s);
        ltrim_inplace(res);
        return res;
    }

    static inline std::string
    replace_once(std::string str, const std::string &from, const std::string &to) {
        size_t start_pos = str.find(from);
        if (start_pos != std::string::npos)
            str.replace(start_pos, from.length(), to);
        return str;
    }

    static inline std::string
    replace_all(std::string str, const std::string &from, const std::string &to) {
        size_t start_pos = 0;
        while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
            str.replace(start_pos, from.length(), to);
            start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
        }
        return str;
    }

    // trim from both ends (in place)
    static inline std::string rtrim(const std::string &s) {
        std::string res(s);
        rtrim_inplace(res);
        return res;
    }

    // trim from both ends (in place)
    static inline std::string trim(const std::string &s) {
        std::string res(s);
        trim_inplace(res);
        return res;
    }


    template <typename Out> void split(const std::string &s, char delim, Out result) {
        std::stringstream ss(s);
        std::string item;
        while (std::getline(ss, item, delim)) {
            *(result++) = item;
        }
    }

    inline std::vector<std::string>
    resplit(const std::string &s, const std::regex &sep_regex = std::regex{"\\s+"}) {
        std::sregex_token_iterator iter(s.begin(), s.end(), sep_regex, -1);
        std::sregex_token_iterator end;
        return {iter, end};
    }

    inline std::vector<std::string> split(const std::string &s, char delim) {
        std::vector<std::string> elems;
        split(s, delim, std::back_inserter(elems));
        return elems;
    }

    // not optimal..
    inline bool starts_with(const std::string &haystack, const std::string &needle) {
        if (haystack.size() < needle.size())
            return false;

        return haystack.find(needle) == 0;
    }
    // not optimal..
    inline bool ends_with(const std::string &haystack, const std::string &needle) {
        if (haystack.size() < needle.size())
            return false;

        return haystack.rfind(needle) == haystack.size() - needle.size();
    }

    inline std::string escape_quote(const std::string &str) {
        static std::regex quote_re("'");
        return std::regex_replace(str, quote_re, "''");
    }

    template <typename TInputIter>
    std::string make_hex_string(
        TInputIter first,
        TInputIter last,
        bool use_uppercase = true,
        bool insert_spaces = false) {
        std::ostringstream ss;
        ss << std::hex << std::setfill('0');
        if (use_uppercase)
            ss << std::uppercase;
        while (first != last) {
            ss << std::setw(2) << static_cast<int>(*first++);
            if (insert_spaces && first != last)
                ss << " ";
        }
        return ss.str();
    }

    template <typename T>
    inline std::string
    join_as_string(const std::vector<T> &items, const std::string &separator) {
        std::string result;
        if (!items.empty()) {
            result = to_string(items[0]);
            if (items.size() > 1) {
                for (size_t i = 1; i < items.size(); i++)
                    result += separator + to_string(items[i]);
            }
        }

        return result;
    }

    inline std::string
    join_as_string(const std::vector<std::string> &items, const std::string &separator) {
        std::string result;
        if (!items.empty()) {
            result = items[0];
            if (items.size() > 1) {
                for (size_t i = 1; i < items.size(); i++)
                    result += separator + items[i];
            }
        }

        return result;
    }

    inline std::string to_lower(const std::string &str) {
        static std::locale loc;
        std::string result;
        result.reserve(str.size());

        for (auto elem : str)
            result += std::tolower(elem, loc);

        return result;
    }

    inline std::wstring to_upper(const std::wstring& str) {
        static std::locale loc;
        std::wstring result;
        result.reserve(str.size());

        for (auto elem : str)
            result += std::toupper(elem, loc);

        return result;
    }

    inline std::string to_upper(const std::string &str) {
        static std::locale loc;
        std::string result;
        result.reserve(str.size());

        for (auto elem : str)
            result += std::toupper(elem, loc);

        return result;
    }
    
   //TODO: Ahead to refactor
   inline std::string to_upper_path(const std::filesystem::path &path) {
        static std::locale loc;
        std::string result;
        result.reserve(path.string().size());

        for (auto elem : path.string())
            result += std::toupper(elem, loc);

        return result;
    }

    inline std::optional<std::string> get_env(const std::string &key) {
        const char *val = std::getenv(key.c_str());
        if (val)
            return std::string(val);
        return {};
    }

    inline std::string get_hostname() {
        std::array<char, 4096> buffer;
        if (not gethostname(buffer.data(), (int)buffer.size())) {
            return std::string(buffer.data());
        }
        return std::string();
    }

    inline ptrdiff_t count_occurrence(const std::string &haystack, const char needle) {
        return std::count(haystack.begin(), haystack.end(), needle);
    }


    static inline void rtrim_char_inplace(std::string &s, int trim_char) {
        s.erase(
            std::find_if(
                s.rbegin(), s.rend(), [trim_char](int ch) { return !(ch == trim_char); })
                .base(),
            s.end());
    }

    static inline void ltrim_char_inplace(std::string &s, int trim_char) {
        s.erase(s.begin(), std::find_if(s.begin(), s.end(), [trim_char](int ch) {
                    return !(ch == trim_char);
                }));
    }

    static inline void rtrim_slash_inplace(std::string &s) { rtrim_char_inplace(s, '/'); }

    // trim from both ends (in place)
    static inline std::string ltrim_char(const std::string &s, int trim_char) {
        std::string res(s);
        ltrim_char_inplace(res, trim_char);
        return res;
    }


    inline std::string clean_path(const std::string &path) {
        std::string p = trim(path);
        rtrim_slash_inplace(p);
        return p + "/";
    }

    inline void replace_string_in_place(
        std::string &subject, const std::string &search, const std::string &replace) {
        size_t pos = 0;
        while ((pos = subject.find(search, pos)) != std::string::npos) {
            subject.replace(pos, search.length(), replace);
            pos += replace.length();
        }
    }

    inline std::string str_sprintf(const char *format, ...) {
        va_list va;
        va_start(va, format);
        std::vector<char> buf(strlen(format) + 4096);
        snprintf(buf.data(), buf.size(), format, va);
        va_end(va);
        return std::string(buf.data());
    }

    inline std::string snake_case(const std::string &src) {
        // assume no change..
        // ignores a1 != a_1
        if (src == to_lower(src))
            return src;

        std::regex words_regex("[A-Z][a-z]*_?|[0-9]+_?");
        std::string result;
        auto words_begin = std::sregex_iterator(src.begin(), src.end(), words_regex);
        auto words_end   = std::sregex_iterator();

        // std::cout << "Found "
        //           << std::distance(words_begin, words_end)
        //           << " words:\n";

        for (std::sregex_iterator i = words_begin; i != words_end; ++i) {
            std::smatch match     = *i;
            std::string match_str = match.str();
            // std::cout << match_str << '\n';

            if (not match_str.empty() and i != words_begin and
                match_str[match_str.size() - 1] != '_' and match_str[0] != '_')
                result += "_";
            result += to_lower(match_str);
        }
        return result;
    }

    template <typename V>
    std::vector<std::vector<typename V::value_type>> split_vector(const V &m, size_t count) {
        std::vector<std::vector<typename V::value_type>> result;
        count = std::min(count, m.size());

        auto chunk_count = m.size() / count + ((m.size() % count ? 1 : 0));

        result.reserve(chunk_count);
        for (size_t chunk = 0; chunk < chunk_count; chunk++) {

            result.emplace_back(std::vector<typename V::value_type>(
                m.begin() + (chunk * count),
                m.begin() + (chunk * count) +
                    ((chunk + 1) * count <= m.size() ? count : m.size() % count)));
        }

        // for (auto it = m.begin(); it != m.end(); ++it) {

        //     result.push_back(it->second);
        // }
        return result;
    }


} // namespace utility
} // namespace xstudio
