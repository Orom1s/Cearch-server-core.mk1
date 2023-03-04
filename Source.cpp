#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>
#include <cmath>


using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result = 0;
    cin >> result;
    ReadLine();
    return result;
}


vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }
    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    void AddDocument(int document_id, const string& document, const DocumentStatus status, const vector<int>& ratings) {
        ++document_count;
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (auto& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{
                                        ComputeAverageRating(ratings),
                                        status
                                                    });
    }
    //template <typename Func_Pred>
    bool FuncPred(int& document_id, DocumentStatus& status, int& rating) {
       
    }



    template<typename Key_mapper>
    vector<Document> FindTopDocuments(const string& raw_query,const  Key_mapper status) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, status);
        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                const double EPSILON = 1e-6;
                return lhs.relevance > rhs.relevance || (abs(lhs.relevance - rhs.relevance) < EPSILON && lhs.rating > rhs.rating);
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        return FindTopDocuments(raw_query, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; });
    }

    vector<Document> FindTopDocuments(const string& raw_query, const DocumentStatus status) const {
        return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus statused, int rating) { return statused == status; });
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
        Query query = ParseQuery(raw_query);
        vector<string> match_words;
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
        return {match_words, documents_.at(document_id).status};
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    map<string, map<int, double>> word_to_document_freqs_;
    set<string> stop_words_;
    int document_count = 0;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        int sum = 0;
        for (const int& x : ratings) {
            sum += x;
        }
        int size_rating = static_cast<int>(ratings.size());
        sum = sum / size_rating;
        return static_cast<int> (sum);
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(document_count * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template<typename Key_mapper>
    vector<Document> FindAllDocuments(const Query& query,const Key_mapper& status ) const {
        vector<Document> matched_documents;
        map <int, double> document_to_relevance;
        for (const auto& plus : query.plus_words) {
            if (word_to_document_freqs_.count(plus) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(plus);
            for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(plus)) {
                const auto& document_data = documents_.at(document_id);
                if (status(document_id, document_data.status, document_data.rating)) {
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
};



//SearchServer CreateSearchServer() {
//    SearchServer search_server;
//    search_server.SetStopWords(ReadLine());
//    const int document_count = ReadLineWithNumber();
//    for (int document_id = 0; document_id < document_count; ++document_id) {
//        const string document = ReadLine();
//
//        int status_raw;
//        cin >> status_raw;
//        const DocumentStatus status = static_cast<DocumentStatus>(status_raw);
//        int ratings_size;
//        cin >> ratings_size;
//
//        vector<int> ratings(ratings_size, 0);
//        for (int& rating : ratings) {
//            cin >> rating;
//        }
//        search_server.AddDocument(document_id, document, status, ratings);
//        ReadLine();
//    }
//    return search_server;
//}
//
//
//int main() {
//    const SearchServer search_server = CreateSearchServer();
//    const string query = ReadLine();
//    const DocumentStatus status = static_cast<DocumentStatus>(ReadLineWithNumber());
//    for (const Document& document : search_server.FindTopDocuments(query, status)) {
//        PrintDocument(document);
//    }
//    return 0;
//}
// 
//void PrintMatchDocumentResult(int document_id, const vector<string>& words, DocumentStatus status) {
//    cout << "{ "s
//        << "document_id = "s << document_id << ", "s
//        << "status = "s << static_cast<int>(status) << ", "s
//        << "words ="s;
//    for (const string& word : words) {
//        cout << ' ' << word;
//    }
//    cout << "}"s << endl;
//}
//int main() {
//    SearchServer search_server;
//    search_server.SetStopWords("и в на"s);
//    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
//    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
//    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
//    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
//    const int document_count = search_server.GetDocumentCount();
//    for (int document_id = 0; document_id < document_count; ++document_id) {
//        const auto [words, status] = search_server.MatchDocument("пушистый кот"s, document_id);
//        PrintMatchDocumentResult(document_id, words, status);
//    }
//}

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating
        << " }"s << endl;
}
int main() {
    SearchServer search_server;
    search_server.SetStopWords("и в на"s);
    search_server.AddDocument(0, "белый кот и модный ошейник"s, DocumentStatus::ACTUAL, { 8, -3 });
    search_server.AddDocument(1, "пушистый кот пушистый хвост"s, DocumentStatus::ACTUAL, { 7, 2, 7 });
    search_server.AddDocument(2, "ухоженный пёс выразительные глаза"s, DocumentStatus::ACTUAL, { 5, -12, 2, 1 });
    search_server.AddDocument(3, "ухоженный скворец евгений"s, DocumentStatus::BANNED, { 9 });
    cout << "ACTUAL by default:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s)) {
        PrintDocument(document);
    }
    cout << "ACTUAL:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return status == DocumentStatus::ACTUAL; })) {
        PrintDocument(document);
    }
    cout << "Even ids:"s << endl;
    for (const Document& document : search_server.FindTopDocuments("пушистый ухоженный кот"s, [](int document_id, DocumentStatus status, int rating) { return document_id % 2 == 0; })) {
        PrintDocument(document);
    }
    return 0;
}