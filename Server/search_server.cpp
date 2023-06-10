#include "search_server.h"

bool StopWords::IsStopWord(const std::string_view& word) const {
    return stop_words.count(word) > 0;
}

int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}

const std::set<int>::const_iterator SearchServer::begin() const noexcept {
    return count_documents_.begin();
}

const std::set<int>::const_iterator SearchServer::end() const noexcept {
    return count_documents_.end();
}

void SearchServer::AddDocument(int document_id, const std::string_view& document, const DocumentStatus status, const std::vector<int>& ratings) {
    if (document_id < 0 || documents_.count(document_id) > 0) {
        throw std::invalid_argument("Попытка добавить документ с некорректным id");
    }
    if (!IsValidWord(document)) {
        throw std::invalid_argument("Документ содержит спецсимволы");
    }
    const std::string document_string{ document };
    documents_.emplace(document_id, DocumentData{ SearchServer::ComputeAverageRating(ratings), status, document_string });

    auto words = SplitIntoWordsNoStop(documents_.at(document_id).text_);
    const double inv_word_count = 1.0 / words.size();
    for (auto& word : words) {
        word_to_document_freqs_[word][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][word] += inv_word_count;
    }

    count_documents_.emplace(document_id);
}



std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query,
        [&status](int document_id, DocumentStatus document_status, int rating)
        { return document_status == status; });
}

std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::string_view& raw_query, int document_id) const {
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy&, const std::string_view& raw_query, int document_id) const {
    std::vector<std::string_view> match_words;
    Query query = ParseQuery(raw_query);
    for (const auto& minus : query.minus_words) {
        if ((!word_to_document_freqs_.count(minus) == 0 && word_to_document_freqs_.at(minus).count(document_id))) {
            match_words.clear();
            return { {} , documents_.at(document_id).status };
        }
    }
    for (const auto& plus : query.plus_words) {
        if (!word_to_document_freqs_.count(plus) == 0 && word_to_document_freqs_.at(plus).count(document_id)) {
            match_words.push_back(plus);
        }
    }
    return { match_words, documents_.at(document_id).status };

}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy&, const std::string_view& raw_query, int document_id) const {
    std::vector<std::string_view> match_words;
    Query query = ParseQueryParallel(raw_query);
    if (any_of(std::execution::par_unseq, query.minus_words.begin(), query.minus_words.end(), [&](auto& minus) {return (word_to_document_freqs_.count(minus) > 0 &&
        word_to_document_freqs_.at(minus).count(document_id) > 0); })) {
        return { match_words, documents_.at(document_id).status };
    }
    match_words.resize(query.plus_words.size());
    const auto& it = std::copy_if(std::execution::par_unseq, query.plus_words.begin(), query.plus_words.end(), match_words.begin(),
        [&](auto& plus)
        {return (word_to_document_freqs_.count(plus) > 0 && word_to_document_freqs_.at(plus).count(document_id) > 0); });
    match_words.erase(it, match_words.end());

    std::sort(std::execution::par_unseq, match_words.begin(), match_words.end());
    const auto& itr = std::unique(std::execution::par_unseq, match_words.begin(), match_words.end());
    match_words.erase(itr, match_words.end());
    return { match_words, documents_.at(document_id).status };

}

int SearchServer::ComputeAverageRating(const std::vector<int>& ratings) {
    int sum = accumulate(ratings.begin(), ratings.end(), 0);
    int size_rating = static_cast<int>(ratings.size());
    if (size_rating != 0) {
        sum = sum / size_rating;
        return static_cast<int> (sum);
    } return static_cast<int> (sum);
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static const std::map<std::string_view, double> order;
    if (document_to_word_freqs_.count(document_id) == 0) {
        return order;
    }
    else {
        return document_to_word_freqs_.at(document_id);
    }
}

void SearchServer::RemoveDocument(int document_id) {
    return RemoveDocument(std::execution::seq, document_id);
}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id) {
    documents_.erase(document_id);
    count_documents_.erase(document_id);
    for (auto [word, __] : document_to_word_freqs_.at(document_id)) {
        word_to_document_freqs_.at(word).erase(document_id);
    }
    document_to_word_freqs_.erase(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id) {

    std::vector<const std::string_view*> result(document_to_word_freqs_.at(document_id).size());
    std::transform(std::execution::par, document_to_word_freqs_.at(document_id).begin(), document_to_word_freqs_.at(document_id).end(), result.begin(), [](const auto& word) {return &word.first; });
    std::for_each(std::execution::par, result.begin(), result.end(), [this, document_id](const auto& word) {word_to_document_freqs_.at(*word).erase(document_id); });
    document_to_word_freqs_.erase(document_id);
    documents_.erase(document_id);
    count_documents_.erase(document_id);
}

std::vector<std::string_view> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
    std::vector<std::string_view> words;
    for (const std::string_view& word : SplitIntoWordsView(text)) {
        if (!stop_words_.IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

SearchServer::QueryWord  SearchServer::ParseQueryWord(std::string_view text) const {
    QueryWord queryWord;
    bool is_minus = false;
    // Word shouldn't be empty
    if (text == "-"s) {
        throw std::invalid_argument("Запрос содержит некорректные слова");
    }
    if (text[0] == '-' && text[1] == '-') {
        throw std::invalid_argument("Запрос содержит некорректные слова");
    }
    if (text[static_cast<int>(text.size()) - 1] == '-') {
        throw std::invalid_argument("Запрос содержит некорректные слова");
    }
    if (!IsValidWord(text))
    {
        throw std::invalid_argument("Запрос содержит спецсимволы");
    }
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    return queryWord = { text, is_minus, stop_words_.IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text) const {
    Query query;
    for (std::string_view word : SplitIntoWordsView(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            }
            else {
                query.plus_words.push_back(query_word.data);
            }
        }
    }
    std::sort(query.plus_words.begin(), query.plus_words.end());
    const auto& itm = std::unique(query.plus_words.begin(), query.plus_words.end());
    query.plus_words.resize(std::distance(query.plus_words.begin(), itm));

    std::sort(query.minus_words.begin(), query.minus_words.end());
    const auto& itp = std::unique(query.minus_words.begin(), query.minus_words.end());
    query.minus_words.resize(std::distance(query.minus_words.begin(), itp));

    return query;
}

SearchServer::Query SearchServer::ParseQueryParallel(std::string_view text) const {
    Query query;
    for (std::string_view word : SplitIntoWordsView(text)) {
        const QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                query.minus_words.push_back(query_word.data);
            }
            else {
                query.plus_words.push_back(query_word.data);
            }
        }

    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const std::string_view& word) const {
    return log(count_documents_.size() * 1.0 / word_to_document_freqs_.at(word).size());
}