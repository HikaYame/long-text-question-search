#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace lqs {

struct Document {
    std::string id;
    std::string site_name;
    std::string title;
    std::string url;
    std::string text;
};

struct SearchResult {
    std::string id;
    double score;
};

std::string normalize_text(const std::string& input);
std::vector<std::string> tokenize(const std::string& input);
std::string build_text_fragment(const std::string& text, const std::string& query);
std::vector<SearchResult> bm25_search(const std::vector<Document>& docs,
                                      const std::string& query,
                                      int top_k = 5);

} // namespace lqs
