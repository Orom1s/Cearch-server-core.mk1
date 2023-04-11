#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <numeric>
#include <cmath>

#include "document.h"
#include "string_processing.h"

using namespace std::string_literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

bool IsValidWord(const std::string& word);

class StopWords {
public:
 
    StopWords() = default;
    
    explicit StopWords(const std::string& text) : StopWords(SplitIntoWords(text)) {}
 
    template <typename Container>
    StopWords(const Container& container);

 
    bool IsStopWord(const std::string& word) const;
    
    
private:
    std::set<std::string> stop_words = {""s};
};
 
class SearchServer {
public:
    
    SearchServer() = default;
 
    template <typename StringCollection>
    explicit SearchServer(const StringCollection& stop_words = ""s) :stop_words_(stop_words){}

    int GetDocumentCount() const;
    
    int GetDocumentId (int index) const;

    void AddDocument(int document_id, const std::string& document, const DocumentStatus status, const std::vector<int>& ratings);
  
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string& raw_query,
                                        DocumentPredicate document_predicate) const;
 
    std::vector<Document> FindTopDocuments(const std::string& raw_query, DocumentStatus status) const;
 
    std::vector<Document> FindTopDocuments(const std::string& raw_query) const;

    std::tuple<std::vector<std::string>, DocumentStatus> MatchDocument(const std::string& raw_query, int document_id ) const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    struct QueryWord {
        std::string data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        std::set<std::string> plus_words;
        std::set<std::string> minus_words;
    };
    
    std::vector<int> count_documents_;
    std::map<std::string, std::map<int, double>> word_to_document_freqs_;
    StopWords stop_words_;
    int document_count = 0;
    std::map<int, DocumentData> documents_;

    

    static int ComputeAverageRating(const std::vector<int>& ratings);

    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;

    QueryWord  ParseQueryWord(std::string text) const;

    Query ParseQuery(const std::string& text) const;

    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template<typename Key_mapper>
    std::vector<Document> FindAllDocuments(const Query& query,const Key_mapper& status ) const;
};

template <typename Container>
StopWords::StopWords(const Container& container) {
        for (const auto& element : container) {
            if (!element.empty())  {
                if (!IsValidWord(element)){
                throw std::invalid_argument("Стоп-слово содержит спецсимволы"s);
            } 
            stop_words.insert(element);
            }
        }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query,
                                        DocumentPredicate document_predicate) const {
        Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);
        sort(matched_documents.begin(), matched_documents.end(),
             [](const Document& lhs, const Document& rhs) {
                 if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
                     return lhs.rating > rhs.rating;
                 } else {
                     return lhs.relevance > rhs.relevance;
                 }
             });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

template<typename Key_mapper>
std::vector<Document> SearchServer::FindAllDocuments(const Query& query,const Key_mapper& status ) const {
        std::vector<Document> matched_documents;
        std::map <int, double> document_to_relevance;
        for (const auto& plus : query.plus_words) {
            if (word_to_document_freqs_.count(plus) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(plus);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(plus)) {
                const auto& document_data = documents_.at(document_id);
                if (static_cast<bool>(status(document_id, document_data.status, document_data.rating))) {
                    document_to_relevance[document_id] += term_freq * inverse_document_freq;
                }
            }
        }
        for (const auto& minus : query.minus_words) {
            if (word_to_document_freqs_.count(minus) == 0) {
                continue;
            }
            for (const auto& [document_id, _] : word_to_document_freqs_.at(minus)) {
                document_to_relevance.erase(document_id);
            }
        }
        for (const auto& [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({ document_id, 
                                          relevance, 
                                          documents_.at(document_id).rating});
        }
        return matched_documents;
    }

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings);

void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);

void MatchDocuments(const SearchServer& search_server, const std::string& query);
