
#include "search_server.h"

    bool IsValidWord(const std::string& word) {
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
            });
    }   

    bool StopWords::IsStopWord(const std::string& word) const {
        return stop_words.count(word) > 0;
    }

    int SearchServer::GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }
    
    int SearchServer::GetDocumentId (int index) const {
        return count_documents_.at(index);
    }

    void SearchServer::AddDocument(int document_id, const std::string& document, const DocumentStatus status, const std::vector<int>& ratings) {
        ++document_count;
        if(document_id < 0 || documents_.count(document_id) > 0){
            throw std::invalid_argument("Попытка добавить документ с некорректным id");
        }
        if(!IsValidWord(document)) {
            throw std::invalid_argument("Документ содержит спецсимволы");
        }
        const std::vector<std::string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (auto& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{
                                        ComputeAverageRating(ratings),
                                        status});
        
        count_documents_.push_back(document_id);
    }
  
    
 
    std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query, DocumentStatus status ) const {
        return FindTopDocuments(raw_query, 
                                [status](int document_id, DocumentStatus document_status, int rating) 
                                { return document_status == status;}); 
    }
 
    std::vector<Document> SearchServer::FindTopDocuments(const std::string& raw_query) const {
        return FindTopDocuments(raw_query,  DocumentStatus::ACTUAL);
    }

    std::tuple<std::vector<std::string>, DocumentStatus> SearchServer::MatchDocument(const std::string& raw_query, int document_id ) const {
        std::vector<std::string> match_words;        
        Query query = ParseQuery(raw_query);
        for (const auto& plus : query.plus_words) {
            if (!word_to_document_freqs_.count(plus) == 0 && word_to_document_freqs_.at(plus).count(document_id)) {
                match_words.push_back(plus);
            } 
        }
        for (const auto& minus : query.minus_words) {
            if ((!word_to_document_freqs_.count(minus) == 0 && word_to_document_freqs_.at(minus).count(document_id))) {
                match_words.clear();
                break;
            }
        }  
        auto stasid = documents_.at(document_id).status;
        return {match_words, stasid};
        
    }

    int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
        int sum = accumulate(ratings.begin(), ratings.end(), 0); 
        int size_rating = static_cast<int>(ratings.size());  
        if (size_rating != 0){ 
            sum = sum / size_rating; 
            return static_cast<int> (sum);  
        } return static_cast<int> (sum); 
    }

    std::vector<std::string> SearchServer::SplitIntoWordsNoStop(const std::string& text) const {
        std::vector<std::string> words;
        for (const std::string& word : SplitIntoWords(text)) {
            if (!stop_words_.IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    SearchServer::QueryWord  SearchServer::ParseQueryWord(std::string text  ) const {
        QueryWord queryWord;
        bool is_minus = false;
        // Word shouldn't be empty
        if (text == "-"s ) {
            throw std::invalid_argument("Запрос содержит некорректные слова");
        }
        if (text[0] == '-'  && text[1] == '-') {
            throw std::invalid_argument("Запрос содержит некорректные слова");
        }
        if (text[static_cast<int>(text.size()) - 1] == '-') {
            throw std::invalid_argument("Запрос содержит некорректные слова");
        }
        if (!IsValidWord(text))
        {
            throw std::invalid_argument("Запрос содержит спецсимволы");
        }
        if (text[0] == '-' ) {
            is_minus = true;
            text = text.substr(1);
        }
        return queryWord = {text, is_minus, stop_words_.IsStopWord(text)};
    }

    SearchServer::Query SearchServer::ParseQuery(const std::string& text) const {
        Query query;
        for (const std::string& word : SplitIntoWords(text)) {
            QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                } else {
                    query.plus_words.insert(query_word.data);
                }
            } 
            
        }
        
        return query;
    }

    double SearchServer::ComputeWordInverseDocumentFreq(const std::string& word) const {
        return log(document_count * 1.0 / word_to_document_freqs_.at(word).size());
    }

    void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status, const std::vector<int>& ratings) {
        try {
            search_server.AddDocument(document_id, document, status, ratings);
        }
        catch (const std::exception& e) {
            std::cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << std::endl;
        }
    }

    void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query) {
        std::cout << "Результаты поиска по запросу: "s << raw_query << std::endl;
        try {
            for (const Document& document : search_server.FindTopDocuments(raw_query)) {
                PrintDocument(document);
            }
        }
        catch (const std::exception& e) {
            std::cout << "Ошибка поиска: "s << e.what() << std::endl;
        }
    }

    void MatchDocuments(const SearchServer& search_server, const std::string& query) {
        try {
            std::cout << "Матчинг документов по запросу: "s << query << std::endl;
            const int document_count = search_server.GetDocumentCount();
            for (int index = 0; index < document_count; ++index) {
                const int document_id = search_server.GetDocumentId(index);
                const auto [words, status] = search_server.MatchDocument(query, document_id);
                PrintMatchDocumentResult(document_id, words, status);
            }
        }
        catch (const std::exception& e) {
            std::cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << std::endl;
        }
    }