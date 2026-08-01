#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>


using namespace std;

template <typename T>
class cppp_smart_pointer {
    struct Block {
        size_t refcount = 1;
        T data;
        template <typename... Args> explicit Block(Args&&... args) : data(std::forward<Args>(args)...) {}
    };
    Block* block = nullptr;
    void addref() { if (block) ++block->refcount; }
    void delref() { if (block && --block->refcount == 0) delete block; }
public:
    cppp_smart_pointer() = default;
    cppp_smart_pointer(nullptr_t) {}
    cppp_smart_pointer(const cppp_smart_pointer& other) : block(other.block) { addref(); }
    cppp_smart_pointer(cppp_smart_pointer&& other) noexcept : block(other.block) { other.block = nullptr; }
    ~cppp_smart_pointer() { delref(); }
    cppp_smart_pointer& operator=(const cppp_smart_pointer& other) { if (this != &other) { Block* replacement = other.block; if (replacement) ++replacement->refcount; delref(); block = replacement; } return *this; }
    cppp_smart_pointer& operator=(cppp_smart_pointer&& other) noexcept { if (this != &other) { Block* replacement = other.block; other.block = nullptr; delref(); block = replacement; } return *this; }
    cppp_smart_pointer& operator=(nullptr_t) { delref(); block = nullptr; return *this; }
    template <typename... Args> static cppp_smart_pointer make(Args&&... args) { cppp_smart_pointer result; result.block = new Block(std::forward<Args>(args)...); return result; }
    T* operator->() { return &block->data; }
    const T* operator->() const { return &block->data; }
    T& operator*() { return block->data; }
    const T& operator*() const { return block->data; }
    explicit operator bool() const { return block != nullptr; }
    bool operator==(nullptr_t) const { return block == nullptr; }
    bool operator!=(nullptr_t) const { return block != nullptr; }
    friend bool operator==(nullptr_t, const cppp_smart_pointer& value) { return value == nullptr; }
    friend bool operator!=(nullptr_t, const cppp_smart_pointer& value) { return value != nullptr; }
};

template <typename A, typename B>
class CPPPPair {
    A firstValue;
    B secondValue;
public:
    CPPPPair() = default;
    CPPPPair(const A& firstValue, const B& secondValue) : firstValue(firstValue), secondValue(secondValue) {}
    template <typename X, typename Y> CPPPPair(const pair<X,Y>& value) : firstValue(value.first), secondValue(value.second) {}
    A& first() { return firstValue; } const A& first() const { return firstValue; }
    B& second() { return secondValue; } const B& second() const { return secondValue; }
    friend bool operator==(const CPPPPair& a, const CPPPPair& b) { return a.firstValue == b.firstValue && a.secondValue == b.secondValue; }
    friend bool operator!=(const CPPPPair& a, const CPPPPair& b) { return !(a == b); }
    friend bool operator<(const CPPPPair& a, const CPPPPair& b) { return pair<A,B>(a.firstValue, a.secondValue) < pair<A,B>(b.firstValue, b.secondValue); }
    friend bool operator>(const CPPPPair& a, const CPPPPair& b) { return b < a; }
    friend bool operator<=(const CPPPPair& a, const CPPPPair& b) { return !(b < a); }
    friend bool operator>=(const CPPPPair& a, const CPPPPair& b) { return !(a < b); }
};

template <typename T>
class CPPPList {
    cppp_smart_pointer<vector<T>> values;
public:
    using value_type = T; using size_type = typename vector<T>::size_type; using difference_type = typename vector<T>::difference_type;
    using reference = typename vector<T>::reference; using const_reference = typename vector<T>::const_reference;
    using iterator = typename vector<T>::iterator; using const_iterator = typename vector<T>::const_iterator;
    using reverse_iterator = typename vector<T>::reverse_iterator; using const_reverse_iterator = typename vector<T>::const_reverse_iterator;
    CPPPList() : values(cppp_smart_pointer<vector<T>>::make()) {}
    CPPPList(initializer_list<T> init) : values(cppp_smart_pointer<vector<T>>::make(init)) {}
    CPPPList(const vector<T>& init) : values(cppp_smart_pointer<vector<T>>::make(init)) {}
    CPPPList(vector<T>&& init) : values(cppp_smart_pointer<vector<T>>::make(std::move(init))) {}
    template <typename U> CPPPList(const vector<U>& init) : values(cppp_smart_pointer<vector<T>>::make()) { values->reserve(init.size()); for (const auto& item : init) values->emplace_back(item); }
    template <typename It> CPPPList(It first, It last) : values(cppp_smart_pointer<vector<T>>::make(first, last)) {}
    iterator begin() { return values->begin(); }
    const_iterator begin() const { return values->begin(); }
    iterator end() { return values->end(); }
    const_iterator end() const { return values->end(); }
    const_iterator cbegin() const { return values->cbegin(); }
    const_iterator cend() const { return values->cend(); }
    reverse_iterator rbegin() { return values->rbegin(); }
    const_reverse_iterator rbegin() const { return values->rbegin(); }
    reverse_iterator rend() { return values->rend(); }
    const_reverse_iterator rend() const { return values->rend(); }
    bool empty() const { return values->empty(); }
    size_type size() const { return values->size(); }
    void reserve(size_type n) { values->reserve(n); }
    void resize(size_type n) { values->resize(n); }
    void clear() { values->clear(); }
    void push_back(const T& value) { values->push_back(value); }
    void push_back(T&& value) { values->push_back(std::move(value)); }
    void pop_back() { values->pop_back(); }
    template <typename... Args> void emplace_back(Args&&... args) { values->emplace_back(std::forward<Args>(args)...); }
    iterator insert(const_iterator pos, const T& value) { return values->insert(pos, value); }
    template <typename It> iterator insert(const_iterator pos, It first, It last) { return values->insert(pos, first, last); }
    iterator erase(const_iterator pos) { return values->erase(pos); }
    iterator erase(const_iterator first, const_iterator last) { return values->erase(first, last); }
    reference operator[](size_type i) { return (*values)[i]; }
    const_reference operator[](size_type i) const { return (*values)[i]; }
    reference at(size_type i) { return values->at(i); }
    const_reference at(size_type i) const { return values->at(i); }
    reference front() { return values->front(); }
    const_reference front() const { return values->front(); }
    reference back() { return values->back(); }
    const_reference back() const { return values->back(); }
    operator vector<T>&() { return *values; }
    operator const vector<T>&() const { return *values; }
    friend bool operator==(const CPPPList& a, const CPPPList& b) { return *a.values == *b.values; }
    friend bool operator!=(const CPPPList& a, const CPPPList& b) { return !(a == b); }
    friend bool operator<(const CPPPList& a, const CPPPList& b) { return *a.values < *b.values; }
    friend bool operator>(const CPPPList& a, const CPPPList& b) { return b < a; }
    friend bool operator<=(const CPPPList& a, const CPPPList& b) { return !(b < a); }
    friend bool operator>=(const CPPPList& a, const CPPPList& b) { return !(a < b); }
};

template <typename T>
class CPPPStack {
    cppp_smart_pointer<stack<T>> values;
public:
    CPPPStack() : values(cppp_smart_pointer<stack<T>>::make()) {}
    void push(const T& value) { values->push(value); }
    bool empty() const { return values->empty(); }
    size_t size() const { return values->size(); }
    const T& top(int line, int column) const { if (values->empty()) throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot take top of empty Stack"); return values->top(); }
    T pop_value(int line, int column) { if (values->empty()) throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot pop empty Stack"); T result = values->top(); values->pop(); return result; }
    CPPPList<T> to_list() const { stack<T> copy = *values; vector<T> reversed; reversed.reserve(copy.size()); while (!copy.empty()) { reversed.push_back(copy.top()); copy.pop(); } reverse(reversed.begin(), reversed.end()); return CPPPList<T>(std::move(reversed)); }
};

template <typename T>
class CPPPQueue {
    cppp_smart_pointer<queue<T>> values;
public:
    CPPPQueue() : values(cppp_smart_pointer<queue<T>>::make()) {}
    void push(const T& value) { values->push(value); }
    bool empty() const { return values->empty(); }
    size_t size() const { return values->size(); }
    const T& top(int line, int column) const { if (values->empty()) throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot take top of empty Queue"); return values->front(); }
    T pop_value(int line, int column) { if (values->empty()) throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot pop empty Queue"); T result = values->front(); values->pop(); return result; }
    CPPPList<T> to_list() const { queue<T> copy = *values; CPPPList<T> result; result.reserve(copy.size()); while (!copy.empty()) { result.push_back(copy.front()); copy.pop(); } return result; }
};

template <typename T>
class CPPPDeque {
    cppp_smart_pointer<deque<T>> values;
public:
    CPPPDeque() : values(cppp_smart_pointer<deque<T>>::make()) {}
    void push_front(const T& value) { values->push_front(value); }
    void push_back(const T& value) { values->push_back(value); }
    bool empty() const { return values->empty(); }
    size_t size() const { return values->size(); }
    const T& front(int line, int column) const { if (values->empty()) throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot take front of empty Deque"); return values->front(); }
    const T& back(int line, int column) const { if (values->empty()) throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot take back of empty Deque"); return values->back(); }
    T pop_front_value(int line, int column) { if (values->empty()) throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot popFront() empty Deque"); T result = values->front(); values->pop_front(); return result; }
    T pop_back_value(int line, int column) { if (values->empty()) throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot popBack() empty Deque"); T result = values->back(); values->pop_back(); return result; }
    CPPPList<T> to_list() const { return CPPPList<T>(values->begin(), values->end()); }
};

template <typename T>
class CPPPSet {
    struct Compare { bool operator()(const T& left, const T& right) const { return left < right; } };
    using storage_type = set<T, Compare>;
    cppp_smart_pointer<storage_type> values;
public:
    using value_type = T; using size_type = typename storage_type::size_type; using iterator = typename storage_type::iterator; using const_iterator = typename storage_type::const_iterator; using reverse_iterator = typename storage_type::reverse_iterator; using const_reverse_iterator = typename storage_type::const_reverse_iterator;
    CPPPSet() : values(cppp_smart_pointer<storage_type>::make()) {} CPPPSet(initializer_list<T> init) : values(cppp_smart_pointer<storage_type>::make(init)) {}
    CPPPSet(const set<T>& init) : values(cppp_smart_pointer<storage_type>::make(init.begin(), init.end())) {} CPPPSet(set<T>&& init) : values(cppp_smart_pointer<storage_type>::make(init.begin(), init.end())) {}
    template <typename U> CPPPSet(const set<U>& init) : values(cppp_smart_pointer<storage_type>::make()) { for (const auto& item : init) values->emplace(item); }
    template <typename It> CPPPSet(It first, It last) : values(cppp_smart_pointer<storage_type>::make(first, last)) {}
    iterator begin() { return values->begin(); } const_iterator begin() const { return values->begin(); } iterator end() { return values->end(); } const_iterator end() const { return values->end(); }
    reverse_iterator rbegin() { return values->rbegin(); } const_reverse_iterator rbegin() const { return values->rbegin(); } reverse_iterator rend() { return values->rend(); } const_reverse_iterator rend() const { return values->rend(); }
    bool empty() const { return values->empty(); } size_type size() const { return values->size(); } void clear() { values->clear(); }
    pair<iterator,bool> insert(const T& value) { return values->insert(value); } template <typename It> void insert(It first, It last) { values->insert(first, last); }
    iterator erase(const_iterator pos) { return values->erase(pos); } size_type erase(const T& key) { return values->erase(key); }
    iterator find(const T& key) { return values->find(key); } const_iterator find(const T& key) const { return values->find(key); }
    iterator lower_bound(const T& key) { return values->lower_bound(key); } const_iterator lower_bound(const T& key) const { return values->lower_bound(key); }
    iterator upper_bound(const T& key) { return values->upper_bound(key); } const_iterator upper_bound(const T& key) const { return values->upper_bound(key); }
    operator storage_type&() { return *values; } operator const storage_type&() const { return *values; }
    friend bool operator==(const CPPPSet& a, const CPPPSet& b) { return *a.values == *b.values; } friend bool operator!=(const CPPPSet& a, const CPPPSet& b) { return !(a == b); }
    friend bool operator<(const CPPPSet& a, const CPPPSet& b) { return *a.values < *b.values; } friend bool operator>(const CPPPSet& a, const CPPPSet& b) { return b < a; } friend bool operator<=(const CPPPSet& a, const CPPPSet& b) { return !(b < a); } friend bool operator>=(const CPPPSet& a, const CPPPSet& b) { return !(a < b); }
};

template <typename K, typename V>
class CPPPMap {
    struct Compare { bool operator()(const K& left, const K& right) const { return left < right; } };
    using storage_type = map<K,V,Compare>;
    cppp_smart_pointer<storage_type> values;
public:
    using value_type = pair<const K,V>; using size_type = typename storage_type::size_type; using iterator = typename storage_type::iterator; using const_iterator = typename storage_type::const_iterator; using reverse_iterator = typename storage_type::reverse_iterator; using const_reverse_iterator = typename storage_type::const_reverse_iterator;
    CPPPMap() : values(cppp_smart_pointer<storage_type>::make()) {} CPPPMap(initializer_list<value_type> init) : values(cppp_smart_pointer<storage_type>::make(init)) {}
    CPPPMap(const map<K,V>& init) : values(cppp_smart_pointer<storage_type>::make(init.begin(), init.end())) {} CPPPMap(map<K,V>&& init) : values(cppp_smart_pointer<storage_type>::make(init.begin(), init.end())) {}
    template <typename It> CPPPMap(It first, It last) : values(cppp_smart_pointer<storage_type>::make(first, last)) {}
    iterator begin() { return values->begin(); } const_iterator begin() const { return values->begin(); } iterator end() { return values->end(); } const_iterator end() const { return values->end(); }
    reverse_iterator rbegin() { return values->rbegin(); } const_reverse_iterator rbegin() const { return values->rbegin(); } reverse_iterator rend() { return values->rend(); } const_reverse_iterator rend() const { return values->rend(); }
    bool empty() const { return values->empty(); } size_type size() const { return values->size(); } void clear() { values->clear(); }
    V& operator[](const K& key) { return (*values)[key]; } V& at(const K& key) { return values->at(key); } const V& at(const K& key) const { return values->at(key); }
    pair<iterator,bool> insert(const value_type& value) { return values->insert(value); } template <typename It> void insert(It first, It last) { values->insert(first, last); }
    iterator erase(const_iterator pos) { return values->erase(pos); } size_type erase(const K& key) { return values->erase(key); }
    iterator find(const K& key) { return values->find(key); } const_iterator find(const K& key) const { return values->find(key); }
    iterator lower_bound(const K& key) { return values->lower_bound(key); } const_iterator lower_bound(const K& key) const { return values->lower_bound(key); }
    iterator upper_bound(const K& key) { return values->upper_bound(key); } const_iterator upper_bound(const K& key) const { return values->upper_bound(key); }
    operator storage_type&() { return *values; } operator const storage_type&() const { return *values; }
    friend bool operator==(const CPPPMap& a, const CPPPMap& b) { return *a.values == *b.values; } friend bool operator!=(const CPPPMap& a, const CPPPMap& b) { return !(a == b); }
    friend bool operator<(const CPPPMap& a, const CPPPMap& b) { return *a.values < *b.values; } friend bool operator>(const CPPPMap& a, const CPPPMap& b) { return b < a; } friend bool operator<=(const CPPPMap& a, const CPPPMap& b) { return !(b < a); } friend bool operator>=(const CPPPMap& a, const CPPPMap& b) { return !(a < b); }
};

struct CPPPChar {
    char value = '\0';
    CPPPChar() = default;
    CPPPChar(char initialValue) : value(initialValue) {}
    operator char() const { return value; }
};

ostream& operator<<(ostream& output, const CPPPChar& value) {
    if (value.value == '\0') {
        return output << 0;
    }

    return output << value.value;
}

istream& operator>>(istream& input, CPPPChar& value) {
    char ch;
    input >> ch;
    value = CPPPChar(ch);
    return input;
}

CPPPChar& operator++(CPPPChar& value) { ++value.value; return value; }

CPPPChar& operator--(CPPPChar& value) { --value.value; return value; }

struct CPPPRange {
    struct Iterator {
        long long current = 0;
        long long stop = 0;
        long long step = 1;

        long long operator*() const { return current; }
        Iterator& operator++() { current += step; return *this; }
        bool operator!=(const Iterator&) const {
            return step > 0 ? current < stop : current > stop;
        }
    };

    long long start = 0;
    long long stop = 0;
    long long step = 1;

    CPPPRange() = default;
    CPPPRange(long long startValue, long long stopValue, long long stepValue) :
        start(startValue),
        stop(stopValue),
        step(stepValue) {}

    Iterator begin() const { return {start, stop, step}; }
    Iterator end() const { return {stop, stop, step}; }
    bool empty() const { return step > 0 ? start >= stop : start <= stop; }
    bool contains(long long value) const {
        if (empty()) {
            return false;
        }
        if (step > 0) {
            if (value < start || value >= stop) {
                return false;
            }
        } else if (value > start || value <= stop) {
            return false;
        }
        const long long distance = value >= start ? value - start : start - value;
        const long long stride = step >= 0 ? step : -step;
        return stride != 0 && distance % stride == 0;
    }
};

CPPPRange CPPPMakeRange(long long stop) {
    return stop >= 0 ? CPPPRange(0, stop, 1) : CPPPRange(0, stop, -1);
}

CPPPRange CPPPMakeRange(long long start, long long stop) {
    return start <= stop ? CPPPRange(start, stop, 1) : CPPPRange(start, stop, -1);
}

CPPPRange CPPPMakeRange(long long start, long long stop, long long step, int line, int column) {
    if (step == 0) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":range step cannot be zero");
    }
    const long long stride = step >= 0 ? step : -step;
    return start <= stop ? CPPPRange(start, stop, stride) : CPPPRange(start, stop, -stride);
}

CPPPList<long long> CPPPRangeToList(const CPPPRange& range) {
    CPPPList<long long> values;
    for (long long value : range) {
        values.push_back(value);
    }
    return values;
}

CPPPSet<long long> CPPPRangeToSet(const CPPPRange& range) {
    CPPPSet<long long> values;
    for (long long value : range) {
        values.insert(value);
    }
    return values;
}

bool CPPPToBoolBool(bool value) { return value; }

bool CPPPToBoolInt(long long value) { return value != 0; }

bool CPPPToBoolFloat(long double value) { return value != 0.0L && !isnan(value); }

bool CPPPToBoolChar(const CPPPChar& value) { return value.value != '\0'; }

bool CPPPToBool(bool value) { return value; }
bool CPPPToBool(int value) { return value != 0; }
bool CPPPToBool(long long value) { return value != 0; }
bool CPPPToBool(long double value) { return value != 0.0L && !isnan(value); }
bool CPPPToBool(const CPPPChar& value) { return value.value != '\0'; }

bool CPPPInputBool() { bool value; cin >> value; return value; }

CPPPChar CPPPInputChar() { CPPPChar value; cin >> value; return value; }

long long CPPPInputInt() { long long value; cin >> value; return value; }

long double CPPPInputFloat() { long double value; cin >> value; return value; }

CPPPList<CPPPChar> CPPPInputString() {
    string value;
    cin >> value;
    CPPPList<CPPPChar> result;
    result.reserve(value.size());
    for (char ch : value) {
        result.push_back(CPPPChar(ch));
    }
    return result;
}

CPPPList<CPPPChar> CPPPInputString(long long count) {
    string value;
    value.reserve(static_cast<size_t>(max(0LL, count)));
    cin >> ws;
    for (long long i = 0; i < count; ++i) {
        char ch = '\0';
        if (!cin.get(ch)) {
            break;
        }
        value.push_back(ch);
    }
    CPPPList<CPPPChar> result;
    result.reserve(value.size());
    for (char ch : value) {
        result.push_back(CPPPChar(ch));
    }
    return result;
}

template <typename Reader>
auto CPPPInputList(long long count, Reader reader) {
    using Value = decltype(reader());
    vector<Value> values;
    for (long long i = 0; i < count; ++i) {
        values.push_back(reader());
    }
    return values;
}

template <typename T>
CPPPList<T> CPPPInputListLine() {
    string line;
    getline(cin >> ws, line);
    istringstream stream(line);
    CPPPList<T> values;
    T value;
    while (stream >> value) {
        values.push_back(value);
    }
    return values;
}

CPPPList<CPPPChar> CPPPStringLiteral(const string& value) {
    CPPPList<CPPPChar> result;
    result.reserve(value.size());
    for (char ch : value) {
        result.push_back(CPPPChar(ch));
    }
    return result;
}

CPPPList<CPPPChar> CPPPStringFromStd(const string& value) {
    CPPPList<CPPPChar> result;
    result.reserve(value.size());
    for (char ch : value) {
        result.push_back(CPPPChar(ch));
    }
    return result;
}

string CPPPStdStringFromChars(const CPPPList<CPPPChar>& value) {
    string result;
    result.reserve(value.size());
    for (const CPPPChar& ch : value) {
        result.push_back(ch.value);
    }
    return result;
}

CPPPList<CPPPChar> CPPPToStringBool(bool value) {
    return CPPPStringFromStd(value ? "1" : "0");
}

CPPPList<CPPPChar> CPPPToStringChar(const CPPPChar& value) {
    return {value};
}

CPPPList<CPPPChar> CPPPToStringInt(long long value) {
    return CPPPStringFromStd(to_string(value));
}

CPPPList<CPPPChar> CPPPToStringFloat(long double value) {
    ostringstream stream;
    stream << value;
    return CPPPStringFromStd(stream.str());
}

bool CPPPStringToBool(const CPPPList<CPPPChar>& value, int line, int column) {
    const string text = CPPPStdStringFromChars(value);
    if (text == "1" || text == "true") {
        return true;
    }
    if (text == "0" || text == "false") {
        return false;
    }
    throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid bool string");
}

CPPPChar CPPPStringToChar(const CPPPList<CPPPChar>& value, int line, int column) {
    if (value.size() != 1) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid char string");
    }
    return value[0];
}

long long CPPPStringToInt(const CPPPList<CPPPChar>& value, int line, int column) {
    const string text = CPPPStdStringFromChars(value);
    if (text.empty()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid int string");
    }
    size_t index = 0;
    if (text[index] == '+' || text[index] == '-') {
        ++index;
    }
    if (index >= text.size()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid int string");
    }
    for (size_t i = index; i < text.size(); ++i) {
        if (!isdigit(static_cast<unsigned char>(text[i]))) {
            throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid int string");
        }
    }
    try {
        return stoll(text);
    } catch (...) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid int string");
    }
}

long double CPPPStringToFloat(const CPPPList<CPPPChar>& value, int line, int column) {
    const string text = CPPPStdStringFromChars(value);
    if (text.empty()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid float string");
    }
    size_t index = 0;
    if (text[index] == '+' || text[index] == '-') {
        ++index;
    }
    const size_t wholeStart = index;
    while (index < text.size() && isdigit(static_cast<unsigned char>(text[index]))) {
        ++index;
    }
    if (wholeStart == index) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid float string");
    }
    if (index < text.size() && text[index] == '.') {
        ++index;
        const size_t fractionalStart = index;
        while (index < text.size() && isdigit(static_cast<unsigned char>(text[index]))) {
            ++index;
        }
        if (fractionalStart == index) {
            throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid float string");
        }
    }
    if (index != text.size()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid float string");
    }
    try {
        return stold(text);
    } catch (...) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid float string");
    }
}

template <typename Out, typename In, typename Converter>
CPPPSet<Out> CPPPListToSet(const CPPPList<In>& values, Converter convert) {
    CPPPSet<Out> result;
    for (const In& value : values) {
        result.insert(convert(value));
    }
    return result;
}

template <typename Out, typename In, typename Converter>
CPPPList<Out> CPPPSetToList(const CPPPSet<In>& values, Converter convert) {
    CPPPList<Out> result;
    result.reserve(values.size());
    for (const In& value : values) {
        result.push_back(convert(value));
    }
    return result;
}

template <typename KOut, typename VOut, typename KIn, typename VIn, typename KeyConverter, typename ValueConverter>
CPPPMap<KOut, VOut> CPPPListToMap(const CPPPList<CPPPPair<KIn, VIn>>& values, KeyConverter keyConvert, ValueConverter valueConvert) {
    CPPPMap<KOut, VOut> result;
    for (const CPPPPair<KIn, VIn>& entry : values) {
        result[keyConvert(entry.first())] = valueConvert(entry.second());
    }
    return result;
}

template <typename KOut, typename VOut, typename KIn, typename VIn, typename KeyConverter, typename ValueConverter>
CPPPList<CPPPPair<KOut, VOut>> CPPPMapToList(const CPPPMap<KIn, VIn>& values, KeyConverter keyConvert, ValueConverter valueConvert) {
    CPPPList<CPPPPair<KOut, VOut>> result;
    result.reserve(values.size());
    for (const pair<const KIn, VIn>& entry : values) {
        result.push_back(CPPPPair<KOut, VOut>(keyConvert(entry.first), valueConvert(entry.second)));
    }
    return result;
}

template <typename Out, typename In, typename Converter>
CPPPStack<Out> CPPPListToStack(const CPPPList<In>& values, Converter convert) { CPPPStack<Out> result; for (const auto& value : values) result.push(convert(value)); return result; }

template <typename Out, typename In, typename Converter>
CPPPQueue<Out> CPPPListToQueue(const CPPPList<In>& values, Converter convert) { CPPPQueue<Out> result; for (const auto& value : values) result.push(convert(value)); return result; }

template <typename Out, typename In, typename Converter>
CPPPDeque<Out> CPPPListToDeque(const CPPPList<In>& values, Converter convert) { CPPPDeque<Out> result; for (const auto& value : values) result.push_back(convert(value)); return result; }

template <typename Out, typename In, typename Converter>
CPPPList<Out> CPPPStackToList(const CPPPStack<In>& values, Converter convert) { CPPPList<Out> result; CPPPList<In> source = values.to_list(); result.reserve(source.size()); for (const auto& value : source) result.push_back(convert(value)); return result; }

template <typename Out, typename In, typename Converter>
CPPPList<Out> CPPPQueueToList(const CPPPQueue<In>& values, Converter convert) { CPPPList<Out> result; CPPPList<In> source = values.to_list(); result.reserve(source.size()); for (const auto& value : source) result.push_back(convert(value)); return result; }

template <typename Out, typename In, typename Converter>
CPPPList<Out> CPPPDequeToList(const CPPPDeque<In>& values, Converter convert) { CPPPList<Out> result; CPPPList<In> source = values.to_list(); result.reserve(source.size()); for (const auto& value : source) result.push_back(convert(value)); return result; }

void CPPPPrintValue(ostream& output, const CPPPList<CPPPChar>& value) {
    for (const CPPPChar& ch : value) {
        output << ch.value;
    }
}

template <typename A, typename B> class CPPPPair; template <typename T> class CPPPList; template <typename T> class CPPPSet; template <typename K, typename V> class CPPPMap;
template <typename T> void CPPPPrintValue(ostream& output, const T& value);
template <typename A, typename B> void CPPPPrintValue(ostream& output, const CPPPPair<A, B>& value);
template <typename T> void CPPPPrintValue(ostream& output, const CPPPList<T>& values);
template <typename T> void CPPPPrintValue(ostream& output, const CPPPSet<T>& values);
template <typename K, typename V> void CPPPPrintValue(ostream& output, const CPPPMap<K, V>& values);
template <typename T> void CPPPPrintValue(ostream& output, const cppp_smart_pointer<T>& value) { if (!value) { output << "NULL"; return; } CPPPPrintValue(output, *value); }

template <typename A, typename B> void CPPPPrintValue(ostream& output, const CPPPPair<A, B>& value) { output << '('; CPPPPrintValue(output, value.first()); output << ','; CPPPPrintValue(output, value.second()); output << ')'; }

template <typename T> void CPPPPrintValue(ostream& output, const CPPPList<T>& values) { output << '['; for (size_t i = 0; i < values.size(); ++i) { if (i > 0) output << ", "; CPPPPrintValue(output, values[i]); } output << ']'; }

template <typename T> void CPPPPrintValue(ostream& output, const CPPPStack<T>& values) { CPPPPrintValue(output, values.to_list()); }

template <typename T> void CPPPPrintValue(ostream& output, const CPPPQueue<T>& values) { CPPPPrintValue(output, values.to_list()); }

template <typename T> void CPPPPrintValue(ostream& output, const CPPPDeque<T>& values) { CPPPPrintValue(output, values.to_list()); }

template <typename T> void CPPPPrintValue(ostream& output, const CPPPSet<T>& values) { output << '{'; bool first = true; for (const auto& value : values) { if (!first) output << ", "; first = false; CPPPPrintValue(output, value); } output << '}'; }

template <typename K, typename V> void CPPPPrintValue(ostream& output, const CPPPMap<K, V>& values) { output << '{'; bool first = true; for (const auto& entry : values) { if (!first) output << ", "; first = false; CPPPPrintValue(output, entry.first); output << ':'; CPPPPrintValue(output, entry.second); } output << '}'; }

template <typename T> void CPPPPrintValue(ostream& output, const T& value) { output << value; }

template <typename T>
cppp_smart_pointer<T> CPPPStructClone(const cppp_smart_pointer<T>& value) {
    return value ? cppp_smart_pointer<T>::make(*value) : nullptr;
}

template <typename T> T CPPPCopy(const T& value);
template <typename T> struct CPPPDeepCopier { static T run(const T& value) { return value; } };

template <typename A, typename B> struct CPPPDeepCopier<CPPPPair<A,B>> { static CPPPPair<A,B> run(const CPPPPair<A,B>& value) { return {CPPPCopy(value.first()), CPPPCopy(value.second())}; } };

template <typename T> struct CPPPDeepCopier<CPPPList<T>> { static CPPPList<T> run(const CPPPList<T>& value) { CPPPList<T> result; result.reserve(value.size()); for (const auto& item : value) result.push_back(CPPPCopy(item)); return result; } };

template <typename T> struct CPPPDeepCopier<CPPPSet<T>> { static CPPPSet<T> run(const CPPPSet<T>& value) { CPPPSet<T> result; for (const auto& item : value) result.insert(CPPPCopy(item)); return result; } };

template <typename K, typename V> struct CPPPDeepCopier<CPPPMap<K,V>> { static CPPPMap<K,V> run(const CPPPMap<K,V>& value) { CPPPMap<K,V> result; for (const auto& item : value) result[CPPPCopy(item.first)] = CPPPCopy(item.second); return result; } };

template <typename T> struct CPPPDeepCopier<cppp_smart_pointer<T>> { static cppp_smart_pointer<T> run(const cppp_smart_pointer<T>& value) { return value ? cppp_smart_pointer<T>::make(*value) : nullptr; } };

template <typename T> T CPPPCopy(const T& value) { return CPPPDeepCopier<T>::run(value); }

void CPPPPrintDelimiter(ostream& output, const CPPPList<CPPPChar>& value) {
    CPPPPrintValue(output, value);
}

template <typename T>
void CPPPPrintDelimiter(ostream& output, const T& value) {
    CPPPPrintValue(output, value);
}

template <typename T, typename Delimiter>
void CPPPPrintDelimited(ostream& output, const CPPPList<T>& values, const Delimiter& delimiter) {
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            CPPPPrintDelimiter(output, delimiter);
        }
        CPPPPrintValue(output, values[i]);
    }
}

template <typename T, typename Delimiter>
void CPPPPrintDelimited(ostream& output, const CPPPSet<T>& values, const Delimiter& delimiter) {
    bool first = true;
    for (const auto& value : values) {
        if (!first) {
            CPPPPrintDelimiter(output, delimiter);
        }
        first = false;
        CPPPPrintValue(output, value);
    }
}

template <typename T, typename U>
void CPPPListInsert(CPPPList<T>& list, const U& value, long long index, int line, int column) {
    if (index < 0 || index > static_cast<long long>(list.size())) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid list index");
    }
    list.insert(list.begin() + static_cast<typename CPPPList<T>::difference_type>(index), value);
}

template <typename T>
T CPPPListPop(CPPPList<T>& list, int line, int column) {
    if (list.empty()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot remove from empty list");
    }
    T value = list.back();
    list.pop_back();
    return value;
}

template <typename T>
T CPPPListRemoveAt(CPPPList<T>& list, long long index, int line, int column) {
    if (index < 0 || index >= static_cast<long long>(list.size())) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid list index");
    }
    auto iterator = list.begin() + static_cast<typename CPPPList<T>::difference_type>(index);
    T value = *iterator;
    list.erase(iterator);
    return value;
}

template <typename T, typename U>
void CPPPListSet(CPPPList<T>& list, long long index, const U& value, int line, int column) {
    if (index < 0 || index >= static_cast<long long>(list.size())) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid list index");
    }
    list[static_cast<typename CPPPList<T>::difference_type>(index)] = value;
}

template <typename T>
long long CPPPNormalizeListIndex(const CPPPList<T>& list, long long index, int line, int column) {
    if (index < 0) {
        index += static_cast<long long>(list.size());
    }
    if (index < 0 || index >= static_cast<long long>(list.size())) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":invalid list index");
    }
    return index;
}

template <typename T>
typename CPPPList<T>::const_reference CPPPListAt(const CPPPList<T>& list, long long index, int line, int column) {
    return list[static_cast<typename CPPPList<T>::difference_type>(CPPPNormalizeListIndex(list, index, line, column))];
}

template <typename T>
decltype(auto) CPPPListRef(CPPPList<T>& list, long long index, int line, int column) {
    return list[static_cast<typename CPPPList<T>::difference_type>(CPPPNormalizeListIndex(list, index, line, column))];
}

template <typename T>
CPPPList<T> CPPPListSlice(const CPPPList<T>& list, long long start, long long end) {
    const long long size = static_cast<long long>(list.size());
    if (start < 0) {
        start += size;
    }
    if (end < 0) {
        end += size;
    }
    start = max(0LL, min(start, size));
    end = max(0LL, min(end, size));
    if (start >= end) {
        return {};
    }
    return CPPPList<T>(
        list.begin() + static_cast<typename CPPPList<T>::difference_type>(start),
        list.begin() + static_cast<typename CPPPList<T>::difference_type>(end)
    );
}

template <typename T>
bool CPPPListContainsSublist(const CPPPList<T>& haystack, const CPPPList<T>& needle) {
    if (needle.empty()) {
        return true;
    }
    if (needle.size() > haystack.size()) {
        return false;
    }
    vector<size_t> prefix(needle.size(), 0);
    for (size_t i = 1, matched = 0; i < needle.size(); ++i) {
        while (matched > 0 && needle[i] != needle[matched]) {
            matched = prefix[matched - 1];
        }
        if (needle[i] == needle[matched]) {
            ++matched;
        }
        prefix[i] = matched;
    }
    for (size_t i = 0, matched = 0; i < haystack.size(); ++i) {
        while (matched > 0 && haystack[i] != needle[matched]) {
            matched = prefix[matched - 1];
        }
        if (haystack[i] == needle[matched]) {
            ++matched;
            if (matched == needle.size()) {
                return true;
            }
        }
    }
    return false;
}

template <typename T, typename U>
CPPPList<long long> CPPPListFindValue(const CPPPList<T>& haystack, const U& needle) {
    CPPPList<long long> matches;
    for (size_t i = 0; i < haystack.size(); ++i) {
        if (haystack[i] == needle) {
            matches.push_back(static_cast<long long>(i));
        }
    }
    return matches;
}

template <typename T>
CPPPList<long long> CPPPListFindSublist(const CPPPList<T>& haystack, const CPPPList<T>& needle) {
    CPPPList<long long> matches;
    if (needle.empty()) {
        for (size_t i = 0; i <= haystack.size(); ++i) {
            matches.push_back(static_cast<long long>(i));
        }
        return matches;
    }
    if (needle.size() > haystack.size()) {
        return matches;
    }
    vector<size_t> prefix(needle.size(), 0);
    for (size_t i = 1, matched = 0; i < needle.size(); ++i) {
        while (matched > 0 && needle[i] != needle[matched]) {
            matched = prefix[matched - 1];
        }
        if (needle[i] == needle[matched]) {
            ++matched;
        }
        prefix[i] = matched;
    }
    for (size_t i = 0, matched = 0; i < haystack.size(); ++i) {
        while (matched > 0 && haystack[i] != needle[matched]) {
            matched = prefix[matched - 1];
        }
        if (haystack[i] == needle[matched]) {
            ++matched;
            if (matched == needle.size()) {
                matches.push_back(static_cast<long long>(i + 1 - matched));
                matched = prefix[matched - 1];
            }
        }
    }
    return matches;
}

template <typename T, typename U>
CPPPList<CPPPList<T>> CPPPListSplitValue(const CPPPList<T>& haystack, const U& needle) {
    CPPPList<CPPPList<T>> parts;
    CPPPList<T> current;
    bool matched = false;
    for (const T& value : haystack) {
        if (value == needle) {
            matched = true;
            if (!current.empty()) {
                parts.emplace_back(current.begin(), current.end());
                current.clear();
            }
        } else {
            current.push_back(value);
        }
    }
    if (!current.empty() || !matched) {
        parts.emplace_back(current.begin(), current.end());
    }
    return parts;
}

template <typename T>
CPPPList<CPPPList<T>> CPPPListSplitSublist(const CPPPList<T>& haystack, const CPPPList<T>& needle) {
    if (needle.empty()) {
        return {haystack};
    }
    CPPPList<CPPPList<T>> parts;
    auto start = haystack.begin();
    bool matched = false;
    while (true) {
        auto found = search(start, haystack.end(), needle.begin(), needle.end());
        if (found == haystack.end()) {
            break;
        }
        matched = true;
        if (start != found) {
            parts.emplace_back(start, found);
        }
        start = found + static_cast<typename CPPPList<T>::difference_type>(needle.size());
    }
    if (start != haystack.end() || !matched) {
        parts.emplace_back(start, haystack.end());
    }
    return parts;
}

template <typename T>
typename CPPPList<T>::const_reference CPPPListMin(const CPPPList<T>& list, int line, int column) {
    if (list.empty()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot take min of empty list");
    }
    return *min_element(list.begin(), list.end());
}

template <typename T>
typename CPPPList<T>::const_reference CPPPListMax(const CPPPList<T>& list, int line, int column) {
    if (list.empty()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot take max of empty list");
    }
    return *max_element(list.begin(), list.end());
}

template <typename T, typename U>
T CPPPSetRemove(CPPPSet<T>& values, const U& key, int line, int column) {
    T lookupKey = key;
    auto iterator = values.find(lookupKey);
    if (iterator == values.end()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":key not found in set");
    }
    T value = *iterator;
    values.erase(iterator);
    return value;
}

template <typename T>
const T& CPPPSetMin(const CPPPSet<T>& values, int line, int column) {
    if (values.empty()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot take min of empty set");
    }
    return *values.begin();
}

template <typename T>
const T& CPPPSetMax(const CPPPSet<T>& values, int line, int column) {
    if (values.empty()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot take max of empty set");
    }
    return *values.rbegin();
}

template <typename T, typename U>
const T& CPPPSetPrev(const CPPPSet<T>& values, const U& key, int line, int column) {
    T lookupKey = key;
    auto iterator = values.lower_bound(lookupKey);
    if (iterator == values.begin()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":no previous value in set");
    }
    --iterator;
    return *iterator;
}

template <typename T, typename U>
const T& CPPPSetNext(const CPPPSet<T>& values, const U& key, int line, int column) {
    T lookupKey = key;
    auto iterator = values.upper_bound(lookupKey);
    if (iterator == values.end()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":no next value in set");
    }
    return *iterator;
}

template <typename T, typename U>
bool CPPPSetHasPrev(const CPPPSet<T>& values, const U& key, int line, int column) {
    T lookupKey = key;
    auto iterator = values.find(lookupKey);
    if (iterator == values.end()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot call hasPrev() with a value that is not in set");
    }
    return iterator != values.begin();
}

template <typename T, typename U>
bool CPPPSetHasNext(const CPPPSet<T>& values, const U& key, int line, int column) {
    T lookupKey = key;
    auto iterator = values.find(lookupKey);
    if (iterator == values.end()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot call hasNext() with a value that is not in set");
    }
    return ++iterator != values.end();
}

template <typename K, typename V, typename U>
V CPPPMapRemove(CPPPMap<K, V>& values, const U& key, int line, int column) {
    K lookupKey = key;
    auto iterator = values.find(lookupKey);
    if (iterator == values.end()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":key not found in map");
    }
    V value = iterator->second;
    values.erase(iterator);
    return value;
}

template <typename K, typename V>
const K& CPPPMapMin(const CPPPMap<K, V>& values, int line, int column) {
    if (values.empty()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot take min of empty map");
    }
    return values.begin()->first;
}

template <typename K, typename V>
const K& CPPPMapMax(const CPPPMap<K, V>& values, int line, int column) {
    if (values.empty()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot take max of empty map");
    }
    return values.rbegin()->first;
}

template <typename K, typename V, typename U>
const V& CPPPMapAt(const CPPPMap<K, V>& values, const U& key, int line, int column) {
    K lookupKey = key;
    auto iterator = values.find(lookupKey);
    if (iterator == values.end()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":key not found in map");
    }
    return iterator->second;
}

template <typename K, typename V, typename U>
const K& CPPPMapPrev(const CPPPMap<K, V>& values, const U& key, int line, int column) {
    K lookupKey = key;
    auto iterator = values.lower_bound(lookupKey);
    if (iterator == values.begin()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":no previous key in map");
    }
    --iterator;
    return iterator->first;
}

template <typename K, typename V, typename U>
const K& CPPPMapNext(const CPPPMap<K, V>& values, const U& key, int line, int column) {
    K lookupKey = key;
    auto iterator = values.upper_bound(lookupKey);
    if (iterator == values.end()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":no next key in map");
    }
    return iterator->first;
}

template <typename K, typename V, typename U>
bool CPPPMapHasPrev(const CPPPMap<K, V>& values, const U& key, int line, int column) {
    K lookupKey = key;
    auto iterator = values.find(lookupKey);
    if (iterator == values.end()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot call hasPrev() with a key that is not in map");
    }
    return iterator != values.begin();
}

template <typename K, typename V, typename U>
bool CPPPMapHasNext(const CPPPMap<K, V>& values, const U& key, int line, int column) {
    K lookupKey = key;
    auto iterator = values.find(lookupKey);
    if (iterator == values.end()) {
        throw runtime_error(to_string(line) + ":" + to_string(column) + ":cannot call hasNext() with a key that is not in map");
    }
    return ++iterator != values.end();
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    long long a = 3;
    long double b = 2.5L;
    CPPPChar c = CPPPChar('x');
    bool ok = true;
    cout << a; cout << '\n';
    cout << b; cout << '\n';
    cout << c; cout << '\n';
    cout << ok; cout << '\n';
    return 0;
}
