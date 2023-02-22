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

//vector<int> SplitIntoNumbers(const string& text) {
//    vector<int> numbers;
//    string num;
//    for (const char c : text) {
//        if (c == ' ') {
//            if (!num.empty()) {
//                int number = stoi(num);
//                numbers.push_back(number);
//                num.clear();
//            }
//        }
//        else {
//            num += c;
//        }
//    }
//    if (!num.empty()) {
//        int number = stoi(num);
//        numbers.push_back(number);
//    }
//    return numbers;
//}

struct Document {
    int id;
    double relevance;
    int rating;
};



class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, const vector<int>& ratings) {
        ++document_count;
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (auto& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        document_ratings_[document_id] = ComputeAverageRating(ratings);
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

    map<string, map<int, double>> word_to_document_freqs_;
    set<string> stop_words_;
    int document_count = 0;
    map<int, int>  document_ratings_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        int sum = 0;
        for (const int& x : ratings) {
            sum += x;
        }
        int size_rating = ratings.size();
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

    vector<Document> FindAllDocuments(const Query& query) const {
        vector<Document> matched_documents;
        map <int, double> document_to_relevance;
        for (const auto& plus : query.plus_words) {
            if (word_to_document_freqs_.count(plus) == 0) {
                continue;
            }
            const double inverse_document_freq = ComputeWordInverseDocumentFreq(plus);
            for (const auto [document_id, term_freq] : word_to_document_freqs_.at(plus)) {
                document_to_relevance[document_id] += term_freq* inverse_document_freq;
            }
        }
        for (const auto& minus : query.minus_words) {
            if (word_to_document_freqs_.count(minus) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(minus)) {
                document_to_relevance.erase(document_id);
            }
        }
        for (const auto& [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back({ document_id, relevance, document_ratings_.at(document_id)});
        }
        return matched_documents;
    }
};

SearchServer CreateSearchServer() {
    SearchServer search_server;
    search_server.SetStopWords(ReadLine());
    const int document_count = ReadLineWithNumber();
    for (int document_id = 0; document_id < document_count; ++document_id) {
        const string document = ReadLine();
        int ratings_size;
        cin >> ratings_size;
        vector<int> ratings(ratings_size, 0);
        for (int& rating : ratings) {
            cin >> rating;
        }

        search_server.AddDocument(document_id, document, ratings);
        ReadLine();
    }
    return search_server;
}

int main() {
    const SearchServer search_server = CreateSearchServer();
    const string query = ReadLine();
    for (const auto& [id, relevance, rating] : search_server.FindTopDocuments(query)) {
        cout << "{ document_id = "s << id << ", "
            << "relevance = "s << relevance << ", " << "rating = "s << rating << " }"s << endl;
    }
}