#include "search_engine.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;
namespace lqs {

static std::string lower_ascii(std::string s) {
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static std::string collapse_spaces(const std::string& s) {
    return std::regex_replace(s, std::regex(R"(\s+)"), " ");
}

std::string normalize_text(const std::string& input) {
    std::string s = lower_ascii(input);

    // Numeric grouping: 1 200 -> 1200 and 1,200 -> 1200.
    // Repeated because a long number may contain multiple grouping separators.
    for (int i = 0; i < 4; ++i) {
        s = std::regex_replace(s, std::regex(R"((\d)[ ,](\d{3})(?!\d))"), "$1$2");
    }

    // The requested normalization examples are intentionally supported:
    // 1,013.105 Pa -> 1013105Pa
    s = std::regex_replace(s, std::regex(R"((\d+)[.,](\d{3})\s*(pa)\b)"), "$1$2Pa");
    s = std::regex_replace(s, std::regex(R"((\d+(?:[.,]\d+)?)\s*(°c|ºc|℃|celsius)\b)"), "$1C");
    s = std::regex_replace(s, std::regex(R"((\d+(?:[.,]\d+)?)\s*(lít|lit|litre|litres|l)\b)"), "$1L");
    s = std::regex_replace(s, std::regex(R"((\d+(?:[.,]\d+)?)\s*(kpa)\b)"), "$1kPa");
    s = std::regex_replace(s, std::regex(R"((\d+(?:[.,]\d+)?)\s*(mpa)\b)"), "$1MPa");
    s = std::regex_replace(s, std::regex(R"((\d+(?:[.,]\d+)?)\s*(atm)\b)"), "$1atm");
    s = std::regex_replace(s, std::regex(R"((\d+(?:[.,]\d+)?)\s*(mol)\b)"), "$1mol");
    s = std::regex_replace(s, std::regex(R"((\d+(?:[.,]\d+)?)\s*(kg)\b)"), "$1kg");
    s = std::regex_replace(s, std::regex(R"((\d+(?:[.,]\d+)?)\s*(g)\b)"), "$1g");
    s = std::regex_replace(s, std::regex(R"((\d+(?:[.,]\d+)?)\s*(m|cm|mm|km)\b)"), "$1$2");

    // Strip punctuation while preserving UTF-8 bytes and alphanumeric content.
    std::string out;
    out.reserve(s.size());
    for (unsigned char c : s) {
        if (std::isspace(c) || std::isalnum(c) || c >= 128) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back(' ');
        }
    }
    return collapse_spaces(out);
}

std::vector<std::string> tokenize(const std::string& input) {
    const std::string s = normalize_text(input);
    std::vector<std::string> tokens;
    std::string cur;

    // UTF-8 is retained byte-wise; ASCII separators split terms.
    for (unsigned char c : s) {
        if (std::isspace(c) || c < 128 && !std::isalnum(c)) {
            if (!cur.empty()) {
                tokens.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(static_cast<char>(c));
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

static std::string pct_encode(const std::string& s) {
    std::ostringstream o;
    o << std::uppercase << std::hex;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            o << static_cast<char>(c);
        else
            o << '%' << std::setw(2) << std::setfill('0') << (int)c;
    }
    return o.str();
}

std::string build_text_fragment(const std::string& text, const std::string& query) {
    const auto t = tokenize(text);
    const auto q = tokenize(query);
    if (t.empty() || q.empty()) return "";

    const size_t n = std::min<size_t>(6, q.size());
    size_t start = std::string::npos;

    for (size_t i = 0; i + n <= t.size(); ++i) {
        bool match = true;
        for (size_t j = 0; j < n; ++j) {
            if (t[i + j] != q[j]) { match = false; break; }
        }
        if (match) { start = i; break; }
    }
    if (start == std::string::npos) {
        for (size_t i = 0; i < t.size(); ++i) {
            if (t[i] == q[0]) { start = i; break; }
        }
    }
    if (start == std::string::npos) return "";

    std::ostringstream o;
    o << "#:~:text=" << pct_encode(t[start]);
    if (n > 1 && start + n - 1 < t.size())
        o << "%20" << pct_encode(t[start + n - 1]);
    return o.str();
}

struct Stats {
    std::unordered_map<std::string, int> tf;
    size_t length = 0;
};

std::vector<SearchResult> bm25_search(const std::vector<Document>& docs,
                                      const std::string& query,
                                      int top_k) {
    constexpr double k1 = 1.5;
    constexpr double b = 0.75;

    const auto q = tokenize(query);
    if (q.empty() || docs.empty()) return {};

    std::vector<Stats> stats(docs.size());
    std::unordered_map<std::string, size_t> df;
    double total_len = 0.0;

    for (size_t i = 0; i < docs.size(); ++i) {
        const auto tokens = tokenize(docs[i].text);
        stats[i].length = tokens.size();
        total_len += tokens.size();

        std::unordered_set<std::string> seen;
        for (const auto& term : tokens) {
            ++stats[i].tf[term];
            if (seen.insert(term).second) ++df[term];
        }
    }

    const double avgdl = total_len / static_cast<double>(docs.size());
    std::unordered_set<std::string> unique_q(q.begin(), q.end());
    std::vector<SearchResult> results;

    for (size_t i = 0; i < docs.size(); ++i) {
        double score = 0.0;

        for (const auto& term : unique_q) {
            auto tf_it = stats[i].tf.find(term);
            if (tf_it == stats[i].tf.end()) continue;

            const double tf = tf_it->second;
            const double dfi = static_cast<double>(df[term]);
            const double N = static_cast<double>(docs.size());
            const double idf = std::log(1.0 + (N - dfi + 0.5) / (dfi + 0.5));
            const double norm = tf + k1 * (1.0 - b + b * stats[i].length / std::max(1.0, avgdl));
            score += idf * (tf * (k1 + 1.0)) / norm;
        }

        if (score > 0.0)
            results.push_back({docs[i].id, score});
    }

    std::sort(results.begin(), results.end(),
              [](const SearchResult& a, const SearchResult& b) {
                  return a.score > b.score;
              });
    if ((int)results.size() > top_k) results.resize(top_k);
    return results;
}

} // namespace lqs

PYBIND11_MODULE(search_core, m) {
    py::class_<lqs::Document>(m, "Document")
        .def(py::init<>())
        .def_readwrite("id", &lqs::Document::id)
        .def_readwrite("site_name", &lqs::Document::site_name)
        .def_readwrite("title", &lqs::Document::title)
        .def_readwrite("url", &lqs::Document::url)
        .def_readwrite("text", &lqs::Document::text);

    py::class_<lqs::SearchResult>(m, "SearchResult")
        .def_readonly("id", &lqs::SearchResult::id)
        .def_readonly("score", &lqs::SearchResult::score);

    m.def("normalize_text", &lqs::normalize_text);
    m.def("tokenize", &lqs::tokenize);
    m.def("build_text_fragment", &lqs::build_text_fragment);
    m.def("bm25_search", &lqs::bm25_search, py::arg("docs"), py::arg("query"), py::arg("top_k") = 5);
}
