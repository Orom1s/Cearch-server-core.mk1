#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <numeric>
#include <cmath>
#include <set>
#include <execution>
#include <string_view>
#include <type_traits>

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"
using namespace std::string_literals;

constexpr int MAX_RESULT_DOCUMENT_COUNT = 5;
constexpr double EPSILON = 1e-6;
constexpr size_t BUCKETS = 16;

class StopWords {
public:

    StopWords() = default;

    explicit StopWords(const std::string& text) : StopWords(SplitIntoWords(text)) {}

    explicit StopWords(const std::string_view& text) : StopWords(SplitIntoWordsView(text)) {}

    template <typename Container>
    StopWords(const Container& container);


    bool IsStopWord(const std::string_view& word) const;


private:
    std::set<std::string, std::less<>> stop_words = { ""s };
};

class SearchServer {
public:

    SearchServer() = default;

    template <typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words = ""s) :stop_words_(stop_words) {}

    int GetDocumentCount() const;

    const std::set<int>::const_iterator begin() const noexcept;

    const std::set<int>::const_iterator end() const noexcept;

    void AddDocument(int document_id, const std::string_view& document, const DocumentStatus status, const std::vector<int>& ratings);

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query,
        DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    template <typename DocumentPredicate, typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query,
        DocumentPredicate document_predicate) const;
    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const;

    using match_tuple = std::tuple<std::vector<std::string_view>, DocumentStatus>;

    match_tuple MatchDocument(const std::string_view& raw_query, int document_id) const;

    match_tuple MatchDocument(const std::execution::sequenced_policy&, const std::string_view& raw_query, int document_id) const;

    match_tuple MatchDocument(const std::execution::parallel_policy&, const std::string_view& raw_query, int document_id) const;

    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    void RemoveDocument(int document_id);

    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);

    void RemoveDocument(const std::execution::parallel_policy&, int document_id);

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::string text_;
    };

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    std::set<int> count_documents_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    StopWords stop_words_;
    std::map<int, DocumentData> documents_;



    static int ComputeAverageRating(const std::vector<int>& ratings);

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    QueryWord  ParseQueryWord(std::string_view text) const;

    Query ParseQuery(std::string_view text, const bool& is_match_par = false) const;

    double ComputeWordInverseDocumentFreq(const std::string_view& word) const;

    template<typename Key_mapper>
    std::vector<Document> FindAllDocuments(const Query& query, const Key_mapper& status) const;

    template<typename Key_mapper>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, const Key_mapper& status) const;

    template<typename Key_mapper>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, Key_mapper status) const;
};

template <typename Container>
StopWords::StopWords(const Container& container) {
    for (auto element : container) {
        if (!element.empty()) {
            if (!IsValidWord(element)) {
                throw std::invalid_argument("Стоп-слово содержит спецсимволы"s);
            }
            stop_words.emplace(element);
        }
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query,
    DocumentPredicate document_predicate) const {
    return SearchServer::FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query,
    DocumentPredicate document_predicate) const {
    const Query query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy, query, document_predicate);
    std::sort(policy, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
                return lhs.rating > rhs.rating;
            }
            else {
                return lhs.relevance > rhs.relevance;
            }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query,
        [status](int document_id, DocumentStatus document_status, int rating)
        { return document_status == status; });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template<typename Key_mapper>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query, const Key_mapper& status) const {
    return FindAllDocuments(std::execution::seq, query, status);
}

template<typename Key_mapper>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy& policy, const Query& query, const Key_mapper& status) const {
    std::vector<Document> matched_documents;
    std::map <int, double> document_to_relevance;
    for (auto plus : query.plus_words) {
        if (word_to_document_freqs_.count(plus) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(plus);
        for (auto [document_id, term_freq] : word_to_document_freqs_.at(plus)) {
            const auto document_data = documents_.at(document_id);
            if (status(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }
    for (const auto minus : query.minus_words) {
        if (word_to_document_freqs_.count(minus) == 0) {
            continue;
        }
        for (const auto& [document_id, _] : word_to_document_freqs_.at(minus)) {
            document_to_relevance.erase(document_id);
        }
    }
    for (auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back({ document_id,
                                      relevance,
                                      documents_.at(document_id).rating });
    }
    return matched_documents;
}

template<typename Key_mapper>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const Query& query, Key_mapper status) const {
    ConcurrentMap<int, double> document_to_relevance(BUCKETS);
    std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [this, &document_to_relevance](std::string_view minus) {
        if (word_to_document_freqs_.count(minus)) {
            for (const auto [document_id, _] : word_to_document_freqs_.at(minus)) {
                document_to_relevance.Erase(document_id);
            }
        }
        });
    std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), [this, &status, &document_to_relevance](std::string_view plus) {
        if (word_to_document_freqs_.count(plus)) {
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(plus);
            for (auto [document_id, term_freq] : word_to_document_freqs_.at(plus)) {
                const auto document_data = documents_.at(document_id);
                if (status(document_id, document_data.status, document_data.rating)) {
                    document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }
            }
        };
        });

    std::map<int, double> document_to_relevance_reduced = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents;
    matched_documents.reserve(document_to_relevance_reduced.size());
    for (auto [document_id, relevance] : document_to_relevance_reduced) {
        matched_documents.push_back({ document_id,
                                      relevance,
                                      documents_.at(document_id).rating });
    }
    return matched_documents;
}

