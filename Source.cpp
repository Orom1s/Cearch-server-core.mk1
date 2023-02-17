#include <algorithm>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <map>

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
    int relevance;
};



class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document) {
        for (auto& word : SplitIntoWordsNoStop(document)) {
            word_to_documents_[word].insert({ document_id });
        }
    }

    vector<Document> FindTopDocuments(const string& raw_query) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                return lhs.relevance > rhs.relevance;
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

private:

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    map<string, set<int>> word_to_documents_;
    int document_count_ = 0;
    set<string> stop_words_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
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
        // Word shouldn't be empty
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

    vector<Document> FindAllDocuments(const Query& query) const {
        vector<Document> matched_documents;
        map <int, int> document_to_relevance;
        for (const auto& plus : query.plus_words) {
            for (int i = 0; i < document_count_; i++) {
                if (word_to_documents_.at(plus).count(i)) {
                    document_to_relevance[i]++;
                }
            }
        }
        for (const auto& minus : query.minus_words) {
            for (int i = 0; i < document_count_; i++) {
                if (word_to_documents_.at(minus).count(i) != 0) {
                    document_to_relevance.erase(i);
                }
            }
        }
        for (const int& [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({ document_id, relevance });
        }
        return matched_documents;
    }

    //static int DocWithMinus(const map<string, set<int>>& content, const Query& query) {
    //    if (query.minus_words.empty()) {
    //        return 0;
    //    }
    //    set<string> matched_minus;
    //    for (const string& word : content) {
    //        if (query.minus_words.count(word) != 0) {
    //            return static_cast<int>(matched_minus.size());
    //        }
    //    }
    //}

    //static int MatchDocument(const map<string, set<int>>& content, const Query& query) {
    //    if (query.plus_words.empty()) {
    //        return 0;
    //    }
    //    set<string> matched_words;
    //    for (const string& word : content) {
    //        if (query.minus_words.count(word) != 0) {
    //            return 0;
    //        }
    //        if (matched_words.count(word) != 0) {
    //            continue;
    //        }
    //        if (query.plus_words.count(word) != 0) {
    //            matched_words.insert(word);
    //        }
    //    }
    //    return static_cast<int>(matched_words.size());
    //}
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());

    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        search_server.AddDocument(document_id, ReadLine());
    }

    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();

    const string query = ReadLine();
    for (const auto& [id, relevance] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << id << ", "
            << "relevance = "s << relevance << " }"s << endl;
    }
}