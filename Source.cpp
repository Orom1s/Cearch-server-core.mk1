#include <algorithm> 
#include <iostream> 
#include <set> 
#include <string> 
#include <utility> 
#include <vector> 
#include <map> 
#include <cmath> 
#include <cstdlib> 
#include <iomanip> 
#include <numeric> 



using namespace std;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;
const int FIRST = 0;
const int SECOND = 1;






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




    template<typename Key_mapper>
    vector<Document> FindTopDocuments(const string& raw_query, const  Key_mapper status) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, status);
        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {

                return lhs.relevance > rhs.relevance || (abs(lhs.relevance - rhs.relevance) < EPSILON && lhs.rating > rhs.rating);
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, const DocumentStatus status = DocumentStatus::ACTUAL) const {
        return FindTopDocuments(raw_query, [status]([[maybe_unused]] int document_id, DocumentStatus statused, [[maybe_unused]] int rating) { return statused == status; });
    }

    tuple<vector<string>&, DocumentStatus> MatchDocument(const string& raw_query, int document_id) const {
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
        return { match_words, documents_.at(document_id).status };
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
        return static_cast<int>(ratings.empty() ? 0 : accumulate(cbegin(ratings), cend(ratings), 0) / ratings.size());

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
    vector<Document> FindAllDocuments(const Query& query, const Key_mapper& status) const {
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
                                          documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};

#define TEST
#ifdef TEST
using namespace std;
template < typename  T1>
ostream& operator<<(ostream& os, const vector<T1>& element) {
    bool is_last = true;
    os << "[";
    for (const auto& value : element) {
        if (!is_last) {
            os << ", ";
        }
        os << value;
        is_last = false;
    }
    os << "]";
    return os;
}

ostream& operator<<(ostream& os, const DocumentStatus status) {
    switch (status) {
    case DocumentStatus::ACTUAL:
        os << "ACTUAL"s;
        break;
    case DocumentStatus::BANNED:
        os << "BANNED"s;
        break;
    case DocumentStatus::IRRELEVANT:
        os << "IRRELEVANT"s;
        break;
    case DocumentStatus::REMOVED:
        os << "REMOVED"s;
        break;
    }
    return os;
}

/* ѕодставьте вашу реализацию класса SearchServer сюда */

/*
   ѕодставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST

*/
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
    const string& func, unsigned line, const string& hint) {
    if (t == u) {
        cout << boolalpha;
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cout << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
    const string& hint) {
    if (!value) {
        cout << file << "("s << line << "): "s << func << ": "s;
        cout << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cout << " Hint: "s << hint;
        }
        cout << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl((expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename T>
void RunTestImpl(const T&, const string& t_str) {
    cerr << t_str << " OK"s << endl;
}

// -------- Ќачало модульных тестов поисковой системы ----------

// “ест провер€ет, что поискова€ система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs.at(FIRST);
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
            "Stop words must be excluded from documents"s);
    }
}

void TestAddDocumentAndSearchThis() {
    const int doc_id = 233;
    const string content = "It's a sunny day in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("sunny"s);
        ASSERT_EQUAL_HINT(found_docs.size(), 1, " It mustn`t be empty"s);
        const Document& doc0 = found_docs.at(FIRST);
        ASSERT_EQUAL(doc0.id, doc_id);
    }
}


void TestExcludeMinusWordsFromQuery() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    // сначала убедимс€, что слова не €вл€ющиес€ минус-словами, наход€т нужный документ
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1);
        const Document& doc0 = found_docs.at(FIRST);
        ASSERT_EQUAL(doc0.id, doc_id);
    }
    // затем убедимс€, что поиск по минус-слову возвращает пустоту 
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("-in the"s).empty());
    }
}

// “ест на проверку сопоставлени€ содержимого документа и поискового запроса

void TestMatchDocument() {

    const int doc_id = 1;
    const string content = "cat in the city"s;
    const vector<int> ratings = { 1, 2, 3 };

    SearchServer server;
    server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);

    // убедимс€,что слова из поискового запроса вернулись
    {
        const vector<string> search = { "in"s, "the"s, "cat"s };
        const auto [wordes, status] = server.MatchDocument("in the cat"s, doc_id);
        const vector<string> words = wordes;  //Ќе получилось разобратьс€ как сделать более информативно      
        ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(words, search);
    }

    // убедимс€, что присутствие минус-слова возвращает пустой вектор
    {
        const auto [wordes, status] = server.MatchDocument("in -the cat"s, doc_id);
        const vector<string> words = wordes;
        ASSERT_EQUAL(words.empty(), true);
    }

    {
        const vector<string> search = { "cat"s };
        server.SetStopWords("in the"s);
        const auto [wordes, status] = server.MatchDocument("in -the cat"s, doc_id);
        const vector<string> words = wordes;
        ASSERT_EQUAL(words.size(), 3);
        ASSERT_EQUAL(status, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(words, search);
    }

}

void TestRatingDocument() {
    const int doc_id = 233;
    const string content = "It's a sunny day in the city"s;
    const vector<int> ratings = { 1, 2, 3 };
    const string query = "sunny"s;
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments(query);
        int arithmetic_mean = 0;
        if (!ratings.empty()) {
            arithmetic_mean = (accumulate(ratings.begin(), ratings.end(), 0) / static_cast<int>(ratings.size()));
        }

        ASSERT_EQUAL(arithmetic_mean, found_docs.at(FIRST).rating);

    }
}

void TestFilterByPredicate() {
    const int doc_id1 = 1;
    const int doc_id2 = 2;
    const int doc_id3 = 3;
    const string content_1 = "cat in the city"s;
    const vector<int> ratings_1 = { -1, 2, 2 }; // rating 1 relev = 0.304098831081123
    const string content_2 = "dog of a hidden village"s;
    const vector<int> ratings_2 = { 1, 2, 3 }; // rating 2 relev = 0.0810930216216328
    const string content_3 = "silent assasin village cat in the village of darkest realms"s;
    const vector<int> ratings_3 = { 2, 3, 4 }; // rating 3 relev = 0.202732554054082

    SearchServer server;
    server.AddDocument(doc_id1, content_1, DocumentStatus::ACTUAL, ratings_1);
    server.AddDocument(doc_id2, content_2, DocumentStatus::ACTUAL, ratings_2);
    server.AddDocument(doc_id3, content_3, DocumentStatus::BANNED, ratings_3);

    // веренем документы с не четным »ƒ
    {
        const auto found_docs = server.FindTopDocuments("cat in the loan village"s,
            [](int document_id, DocumentStatus, int)
            { return document_id % 2 != 0; });

        ASSERT_EQUAL(found_docs.at(FIRST).id, doc_id1);
        ASSERT_EQUAL(found_docs.at(SECOND).id, doc_id2);
    }

    // вернем документы с рейтингом
    {
        const auto found_docs = server.FindTopDocuments("cat in the loan village"s,
            [](int, DocumentStatus, int rating)
            { return rating == 3; });

        ASSERT_EQUAL(found_docs.at(FIRST).id, doc_id3);
    }

}

void TestSearchByStatusDocuments() {
    const int doc_id1 = 1;
    const int doc_id2 = 2;
    const int doc_id3 = 3;
    const int doc_id4 = 4;
    const string content_1 = "cat in the city"s;
    const vector<int> ratings_1 = { -1, 2, 2 };
    const string content_2 = "dog of a hidden village"s;
    const vector<int> ratings_2 = { 1, 2, 3 };
    const string content_3 = "silent assasin village cat in the village of darkest realms"s;
    const vector<int> ratings_3 = { 2, 3, 4 };
    const string content_4 = "cat of the loan village"s;
    const vector<int> ratings_4 = { 6, 4, 2 };

    SearchServer server;
    server.AddDocument(doc_id1, content_1, DocumentStatus::ACTUAL, ratings_1);
    server.AddDocument(doc_id2, content_2, DocumentStatus::IRRELEVANT, ratings_2);
    server.AddDocument(doc_id3, content_3, DocumentStatus::BANNED, ratings_3);
    server.AddDocument(doc_id4, content_4, DocumentStatus::REMOVED, ratings_4);

    // вернем документы со статусом јктуальный
    {
        const auto found_docs = server.FindTopDocuments("cat of village"s, DocumentStatus::ACTUAL);
        ASSERT_EQUAL(found_docs.at(FIRST).id, doc_id1);
    }

    // вернем документы со статусом Ќерелевантынй
    {
        const auto found_docs = server.FindTopDocuments("cat of village"s, DocumentStatus::IRRELEVANT);
        ASSERT_EQUAL(found_docs.at(FIRST).id, doc_id2);
    }

    // вернем документы со статусом «аблокированный
    {
        const auto found_docs = server.FindTopDocuments("cat of village"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs.at(FIRST).id, doc_id3);
    }

    // вернем документы со статусом ”даленный
    {
        const auto found_docs = server.FindTopDocuments("cat of village"s, DocumentStatus::REMOVED);
        ASSERT_EQUAL(found_docs.at(FIRST).id, doc_id4);
    }
    {
        const auto found_docs = server.FindTopDocuments("cat of village"s, DocumentStatus::BANNED);
        ASSERT_EQUAL(found_docs.at(FIRST).id, doc_id4);
    }

}

void TestSortRelevance() {
    const int doc_id = 0;
    const int doc_id1 = 1;
    const int doc_id2 = 2;
    const int doc_id3 = 3;
    const string content = "white cat and collar"s;
    const string content1 = "a groomed dog"s;
    const string content2 = "skvoretz evgeny"s;
    const string content3 = "cat fluffy tail"s;
    const vector<int> ratings = { 8, -3 };
    const vector<int> ratings1 = { 7, 2, 7 };
    const vector<int> ratings2 = { 5, -12, 2, 1 };
    const vector<int> ratings3 = { 9 };
    const string query = "fluffy  groomed cat"s;
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        server.AddDocument(doc_id1, content3, DocumentStatus::ACTUAL, ratings1);
        server.AddDocument(doc_id2, content1, DocumentStatus::ACTUAL, ratings2);
        server.AddDocument(doc_id3, content2, DocumentStatus::BANNED, ratings3);
        const auto  relevance = server.FindTopDocuments(query);
       // const bool sort_relevance = is_sorted(relevance.begin(), relevance.end(),
         //   [](const Document& l, const Document& r) {
           //     return l.relevance > r.relevance; });
        ASSERT_HINT(!(is_sorted(relevance.begin(), relevance.end(),
            [](const Document& lhs, const Document& rhs) {
                return lhs.relevance > rhs.relevance || (abs(lhs.relevance - rhs.relevance) < EPSILON && lhs.rating > rhs.rating); })), ("Not sorted correctly by relevance"s));
    }

}
#define RUN_TEST(func)  RunTestImpl((func), #func)
// ‘ункци€ TestSearchServer €вл€етс€ точкой входа дл€ запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    RUN_TEST(TestExcludeMinusWordsFromQuery);
    RUN_TEST(TestMatchDocument);
    RUN_TEST(TestRatingDocument);
    RUN_TEST(TestFilterByPredicate);
    RUN_TEST(TestSearchByStatusDocuments);
    RUN_TEST(TestSortRelevance);
    RUN_TEST(TestAddDocumentAndSearchThis);
}

#endif TEST
// --------- ќкончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // ≈сли вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}