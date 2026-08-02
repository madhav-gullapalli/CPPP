/*
 * typesCppp.cpp
 *
 * Provides runtime helper definitions and type support for generated C++ output.
 * This file is part of the CP++ transpiler and is documented here for
 * maintainability and onboarding.
 */

#include "typesCppp.h"

#include "listsCppp.h"

#include <cctype>
#include <map>
#include <set>
#include <sstream>

namespace {
std::set<std::string>& runtimeRequirementSet() {
    static std::set<std::string> helpers;
    return helpers;
}

std::set<std::string>& structMethodRequirementSet() {
    static std::set<std::string> methods;
    return methods;
}

std::string& runtimeRequirementOwner() {
    static std::string owner;
    return owner;
}

std::map<std::string, std::set<std::string>>& runtimeRequirementOwners() {
    static std::map<std::string, std::set<std::string>> owners;
    return owners;
}

std::map<std::string, std::set<std::string>>& containerMemberRequirementOwners() {
    static std::map<std::string, std::set<std::string>> owners;
    return owners;
}

std::string containerTypeName(const Type& type) {
    switch (type.primitive) {
        case PrimitiveType::Pair: return "CPPPPair";
        case PrimitiveType::List: return "CPPPList";
        case PrimitiveType::Stack: return "CPPPStack";
        case PrimitiveType::Queue: return "CPPPQueue";
        case PrimitiveType::Deque: return "CPPPDeque";
        case PrimitiveType::Set: return "CPPPSet";
        case PrimitiveType::Map: return "CPPPMap";
        default: return "";
    }
}

std::string containerMemberForLine(const std::string& line) {
    const std::vector<std::string> names = {"first", "second", "begin", "end", "cbegin", "cend", "rbegin", "rend", "empty", "size", "reserve", "resize", "clear", "push", "push_front", "push_back", "pop_back", "emplace_back", "insert", "erase", "at", "front", "back", "top", "pop_value", "pop_front_value", "pop_back_value", "to_list", "find", "lower_bound", "upper_bound"};
    if (line.find("operator[]") != std::string::npos) {
        return line.find(") const") != std::string::npos ? "index_const" : "index_mut";
    }
    if (line.find("CPPPPair()") != std::string::npos || line.find("CPPPList()") != std::string::npos ||
        line.find("CPPPStack()") != std::string::npos || line.find("CPPPQueue()") != std::string::npos ||
        line.find("CPPPDeque()") != std::string::npos || line.find("CPPPSet()") != std::string::npos ||
        line.find("CPPPMap()") != std::string::npos) return "ctor_default";
    if (line.find("CPPPPair(const A&") != std::string::npos) return "ctor_values";
    if (line.find("CPPPPair(const pair") != std::string::npos) return "ctor_std";
    if (line.find("CPPPList(initializer_list") != std::string::npos) return "ctor_init";
    if (line.find("CPPPList(size_type count") != std::string::npos) return "ctor_size";
    if (line.find("CPPPList(const vector<U>") != std::string::npos) return "ctor_convert";
    if (line.find("CPPPList(const vector") != std::string::npos || line.find("CPPPList(vector") != std::string::npos) return "ctor_vector";
    if (line.find("CPPPList(It first") != std::string::npos) return "ctor_iterator";
    if (line.find("CPPPSet(initializer_list") != std::string::npos || line.find("CPPPMap(initializer_list") != std::string::npos) return "ctor_init";
    if (line.find("CPPPSet(const set<U>") != std::string::npos) return "ctor_convert";
    if (line.find("CPPPSet(const set") != std::string::npos || line.find("CPPPSet(set") != std::string::npos ||
        line.find("CPPPMap(const map") != std::string::npos || line.find("CPPPMap(map") != std::string::npos) return "ctor_std";
    if (line.find("CPPPSet(It first") != std::string::npos || line.find("CPPPMap(It first") != std::string::npos) return "ctor_iterator";
    if (line.find("friend bool operator==") != std::string::npos) return "compare_eq";
    if (line.find("friend bool operator!=") != std::string::npos) return "compare_ne";
    if (line.find("friend bool operator<=") != std::string::npos) return "compare_le";
    if (line.find("friend bool operator>=") != std::string::npos) return "compare_ge";
    if (line.find("friend bool operator<") != std::string::npos) return "compare_lt";
    if (line.find("friend bool operator>") != std::string::npos) return "compare_gt";
    if (line.find("template <typename It> iterator insert") != std::string::npos ||
        line.find("template <typename It> void insert") != std::string::npos) return "insert_range";
    if (line.find("iterator insert") != std::string::npos || line.find("pair<iterator,bool> insert") != std::string::npos) return "insert_one";
    if (line.find("iterator erase(const_iterator first") != std::string::npos) return "erase_range";
    if (line.find("iterator erase(const_iterator pos") != std::string::npos) return "erase_one";
    if (line.find("size_type erase") != std::string::npos) return "erase_key";
    if (line.find("operator vector") != std::string::npos || line.find("operator set") != std::string::npos || line.find("operator map") != std::string::npos ||
        line.find("operator const vector") != std::string::npos || line.find("operator const set") != std::string::npos || line.find("operator const map") != std::string::npos ||
        line.find("operator storage_type") != std::string::npos || line.find("operator const storage_type") != std::string::npos) return "convert";
    for (const std::string& name : names) {
        if (line.find(" " + name + "(") != std::string::npos) {
            const bool overloadedByConst = name == "first" || name == "second" || name == "begin" || name == "end" ||
                name == "rbegin" || name == "rend" || name == "at" || name == "front" || name == "back" ||
                name == "find" || name == "lower_bound" || name == "upper_bound";
            if (overloadedByConst) return name + (line.find(") const") != std::string::npos ? "_const" : "_mut");
            return name;
        }
    }
    return "";
}

void collectContainerMemberUses(const std::string& line, std::set<std::string>& members) {
    const std::vector<std::string> names = {"first", "second", "begin", "end", "cbegin", "cend", "rbegin", "rend", "empty", "size", "reserve", "resize", "clear", "push", "push_front", "push_back", "pop_back", "emplace_back", "insert", "erase", "at", "front", "back", "top", "pop_value", "pop_front_value", "pop_back_value", "to_list", "find", "lower_bound", "upper_bound"};
    for (const std::string& name : names) {
        if (line.find("." + name + "(") != std::string::npos) members.insert(name);
    }
    if (line.find('[') != std::string::npos) members.insert("index");
    if (line.find("CPPPList<") != std::string::npos && line.find('{') != std::string::npos) members.insert("ctor_init");
    if (line.find("CPPPList<") != std::string::npos && line.find("begin()") != std::string::npos && line.find("end()") != std::string::npos) members.insert("ctor_iterator");
    if (line.find(" : ") != std::string::npos) { members.insert("begin"); members.insert("end"); }
}

bool declaresDefaultConstructedContainer(const std::string& line, const std::string& typeName) {
    size_t searchFrom = 0;
    while (true) {
        const size_t typeStart = line.find(typeName + "<", searchFrom);
        if (typeStart == std::string::npos) return false;
        size_t cursor = typeStart + typeName.size();
        int depth = 0;
        for (; cursor < line.size(); ++cursor) {
            if (line[cursor] == '<') ++depth;
            else if (line[cursor] == '>' && --depth == 0) {
                ++cursor;
                break;
            }
        }
        while (cursor < line.size() && std::isspace(static_cast<unsigned char>(line[cursor]))) ++cursor;
        if (cursor < line.size() && (std::isalpha(static_cast<unsigned char>(line[cursor])) || line[cursor] == '_')) {
            while (cursor < line.size() &&
                (std::isalnum(static_cast<unsigned char>(line[cursor])) || line[cursor] == '_')) ++cursor;
            while (cursor < line.size() && std::isspace(static_cast<unsigned char>(line[cursor]))) ++cursor;
            if (cursor < line.size() && line[cursor] == ';') return true;
        }
        searchFrom = typeStart + typeName.size() + 1;
    }
}

std::vector<std::string> smartPointerSupport() {
    return {
        "template <typename T>",
        "class cppp_smart_pointer {",
        "    struct Block {",
        "        size_t refcount = 1;",
        "        T data;",
        "        template <typename... Args> explicit Block(Args&&... args) : data(std::forward<Args>(args)...) {}",
        "    };",
        "    Block* block = nullptr;",
        "    void addref() { if (block) ++block->refcount; }",
        "    void delref() { if (block && --block->refcount == 0) delete block; }",
        "public:",
        "    cppp_smart_pointer() = default;",
        "    cppp_smart_pointer(nullptr_t) {}",
        "    cppp_smart_pointer(const cppp_smart_pointer& other) : block(other.block) { addref(); }",
        "    cppp_smart_pointer(cppp_smart_pointer&& other) noexcept : block(other.block) { other.block = nullptr; }",
        "    ~cppp_smart_pointer() { delref(); }",
        "    cppp_smart_pointer& operator=(const cppp_smart_pointer& other) { if (this != &other) { Block* replacement = other.block; if (replacement) ++replacement->refcount; delref(); block = replacement; } return *this; }",
        "    cppp_smart_pointer& operator=(cppp_smart_pointer&& other) noexcept { if (this != &other) { Block* replacement = other.block; other.block = nullptr; delref(); block = replacement; } return *this; }",
        "    cppp_smart_pointer& operator=(nullptr_t) { delref(); block = nullptr; return *this; }",
        "    template <typename... Args> static cppp_smart_pointer make(Args&&... args) { cppp_smart_pointer result; result.block = new Block(std::forward<Args>(args)...); return result; }",
        "    T* operator->() { return &block->data; }",
        "    const T* operator->() const { return &block->data; }",
        "    T& operator*() { return block->data; }",
        "    const T& operator*() const { return block->data; }",
        "    explicit operator bool() const { return block != nullptr; }",
        "    bool operator==(nullptr_t) const { return block == nullptr; }",
        "    bool operator!=(nullptr_t) const { return block != nullptr; }",
        "    friend bool operator==(nullptr_t, const cppp_smart_pointer& value) { return value == nullptr; }",
        "    friend bool operator!=(nullptr_t, const cppp_smart_pointer& value) { return value != nullptr; }",
        "};",
        ""
    };
}

std::vector<std::string> containerSupport(
    const std::set<std::string>& requiredTypes,
    const std::set<std::string>& requiredMembers
) {
    const std::vector<std::string> all = {
        "template <typename A, typename B>",
        "class CPPPPair {",
        "    A firstValue;",
        "    B secondValue;",
        "public:",
        "    CPPPPair() = default;",
        "    CPPPPair(const A& firstValue, const B& secondValue) : firstValue(firstValue), secondValue(secondValue) {}",
        "    template <typename X, typename Y> CPPPPair(const pair<X,Y>& value) : firstValue(value.first), secondValue(value.second) {}",
        "    A& first() { return firstValue; }",
        "    const A& first() const { return firstValue; }",
        "    B& second() { return secondValue; }",
        "    const B& second() const { return secondValue; }",
        "    friend bool operator==(const CPPPPair& a, const CPPPPair& b) { return a.firstValue == b.firstValue && a.secondValue == b.secondValue; }",
        "    friend bool operator!=(const CPPPPair& a, const CPPPPair& b) { return pair<A,B>(a.firstValue, a.secondValue) != pair<A,B>(b.firstValue, b.secondValue); }",
        "    friend bool operator<(const CPPPPair& a, const CPPPPair& b) { return pair<A,B>(a.firstValue, a.secondValue) < pair<A,B>(b.firstValue, b.secondValue); }",
        "    friend bool operator>(const CPPPPair& a, const CPPPPair& b) { return pair<A,B>(a.firstValue, a.secondValue) > pair<A,B>(b.firstValue, b.secondValue); }",
        "    friend bool operator<=(const CPPPPair& a, const CPPPPair& b) { return pair<A,B>(a.firstValue, a.secondValue) <= pair<A,B>(b.firstValue, b.secondValue); }",
        "    friend bool operator>=(const CPPPPair& a, const CPPPPair& b) { return pair<A,B>(a.firstValue, a.secondValue) >= pair<A,B>(b.firstValue, b.secondValue); }",
        "};",
        "",
        "template <typename T>",
        "class CPPPList {",
        "    cppp_smart_pointer<vector<T>> values;",
        "public:",
        "    using value_type = T; using size_type = typename vector<T>::size_type; using difference_type = typename vector<T>::difference_type;",
        "    using reference = typename vector<T>::reference; using const_reference = typename vector<T>::const_reference;",
        "    using iterator = typename vector<T>::iterator; using const_iterator = typename vector<T>::const_iterator;",
        "    using reverse_iterator = typename vector<T>::reverse_iterator; using const_reverse_iterator = typename vector<T>::const_reverse_iterator;",
        "    CPPPList() : values(cppp_smart_pointer<vector<T>>::make()) {}",
        "    CPPPList(initializer_list<T> init) : values(cppp_smart_pointer<vector<T>>::make(init)) {}",
        "    template <typename... Sizes> CPPPList(size_type count, Sizes... sizes) : values(cppp_smart_pointer<vector<T>>::make()) { values->reserve(count); for (size_type i = 0; i < count; ++i) values->emplace_back(sizes...); }",
        "    CPPPList(const vector<T>& init) : values(cppp_smart_pointer<vector<T>>::make(init)) {}",
        "    CPPPList(vector<T>&& init) : values(cppp_smart_pointer<vector<T>>::make(std::move(init))) {}",
        "    template <typename U> CPPPList(const vector<U>& init) : values(cppp_smart_pointer<vector<T>>::make()) { values->reserve(init.size()); for (const auto& item : init) values->emplace_back(item); }",
        "    template <typename It, typename = enable_if_t<!is_integral<It>::value>> CPPPList(It first, It last) : values(cppp_smart_pointer<vector<T>>::make(first, last)) {}",
        "    iterator begin() { return values->begin(); }",
        "    const_iterator begin() const { return values->begin(); }",
        "    iterator end() { return values->end(); }",
        "    const_iterator end() const { return values->end(); }",
        "    const_iterator cbegin() const { return values->cbegin(); }",
        "    const_iterator cend() const { return values->cend(); }",
        "    reverse_iterator rbegin() { return values->rbegin(); }",
        "    const_reverse_iterator rbegin() const { return values->rbegin(); }",
        "    reverse_iterator rend() { return values->rend(); }",
        "    const_reverse_iterator rend() const { return values->rend(); }",
        "    bool empty() const { return values->empty(); }",
        "    size_type size() const { return values->size(); }",
        "    void reserve(size_type n) { values->reserve(n); }",
        "    void resize(size_type n) { values->resize(n); }",
        "    void clear() { values->clear(); }",
        "    void push_back(T value) { values->push_back(std::move(value)); }",
        "    void pop_back() { values->pop_back(); }",
        "    template <typename... Args> void emplace_back(Args&&... args) { values->emplace_back(std::forward<Args>(args)...); }",
        "    iterator insert(const_iterator pos, const T& value) { return values->insert(pos, value); }",
        "    template <typename It> iterator insert(const_iterator pos, It first, It last) { return values->insert(pos, first, last); }",
        "    iterator erase(const_iterator pos) { return values->erase(pos); }",
        "    iterator erase(const_iterator first, const_iterator last) { return values->erase(first, last); }",
        "    reference operator[](size_type i) { return (*values)[i]; }",
        "    const_reference operator[](size_type i) const { return (*values)[i]; }",
        "    reference at(size_type i) { return values->at(i); }",
        "    const_reference at(size_type i) const { return values->at(i); }",
        "    reference front() { return values->front(); }",
        "    const_reference front() const { return values->front(); }",
        "    reference back() { return values->back(); }",
        "    const_reference back() const { return values->back(); }",
        "    operator vector<T>&() { return *values; }",
        "    operator const vector<T>&() const { return *values; }",
        "    friend bool operator==(const CPPPList& a, const CPPPList& b) { return *a.values == *b.values; }",
        "    friend bool operator!=(const CPPPList& a, const CPPPList& b) { return *a.values != *b.values; }",
        "    friend bool operator<(const CPPPList& a, const CPPPList& b) { return *a.values < *b.values; }",
        "    friend bool operator>(const CPPPList& a, const CPPPList& b) { return *a.values > *b.values; }",
        "    friend bool operator<=(const CPPPList& a, const CPPPList& b) { return *a.values <= *b.values; }",
        "    friend bool operator>=(const CPPPList& a, const CPPPList& b) { return *a.values >= *b.values; }",
        "};",
        "",
        "template <typename T>",
        "class CPPPStack {",
        "    cppp_smart_pointer<stack<T>> values;",
        "public:",
        "    CPPPStack() : values(cppp_smart_pointer<stack<T>>::make()) {}",
        "    void push(const T& value) { values->push(value); }",
        "    bool empty() const { return values->empty(); }",
        "    size_t size() const { return values->size(); }",
        "    const T& top(int line, int column) const { if (values->empty()) throw runtime_error(to_string(line) + \":\" + to_string(column) + \":cannot take top of empty Stack\"); return values->top(); }",
        "    T pop_value(int line, int column) { if (values->empty()) throw runtime_error(to_string(line) + \":\" + to_string(column) + \":cannot pop empty Stack\"); T result = values->top(); values->pop(); return result; }",
        "    CPPPList<T> to_list() const { stack<T> copy = *values; vector<T> reversed; reversed.reserve(copy.size()); while (!copy.empty()) { reversed.push_back(copy.top()); copy.pop(); } reverse(reversed.begin(), reversed.end()); return CPPPList<T>(std::move(reversed)); }",
        "};",
        "",
        "template <typename T>",
        "class CPPPQueue {",
        "    cppp_smart_pointer<queue<T>> values;",
        "public:",
        "    CPPPQueue() : values(cppp_smart_pointer<queue<T>>::make()) {}",
        "    void push(const T& value) { values->push(value); }",
        "    bool empty() const { return values->empty(); }",
        "    size_t size() const { return values->size(); }",
        "    const T& top(int line, int column) const { if (values->empty()) throw runtime_error(to_string(line) + \":\" + to_string(column) + \":cannot take top of empty Queue\"); return values->front(); }",
        "    T pop_value(int line, int column) { if (values->empty()) throw runtime_error(to_string(line) + \":\" + to_string(column) + \":cannot pop empty Queue\"); T result = values->front(); values->pop(); return result; }",
        "    CPPPList<T> to_list() const { queue<T> copy = *values; CPPPList<T> result; result.reserve(copy.size()); while (!copy.empty()) { result.push_back(copy.front()); copy.pop(); } return result; }",
        "};",
        "",
        "template <typename T>",
        "class CPPPDeque {",
        "    cppp_smart_pointer<deque<T>> values;",
        "public:",
        "    CPPPDeque() : values(cppp_smart_pointer<deque<T>>::make()) {}",
        "    void push_front(const T& value) { values->push_front(value); }",
        "    void push_back(const T& value) { values->push_back(value); }",
        "    bool empty() const { return values->empty(); }",
        "    size_t size() const { return values->size(); }",
        "    const T& front(int line, int column) const { if (values->empty()) throw runtime_error(to_string(line) + \":\" + to_string(column) + \":cannot take front of empty Deque\"); return values->front(); }",
        "    const T& back(int line, int column) const { if (values->empty()) throw runtime_error(to_string(line) + \":\" + to_string(column) + \":cannot take back of empty Deque\"); return values->back(); }",
        "    T pop_front_value(int line, int column) { if (values->empty()) throw runtime_error(to_string(line) + \":\" + to_string(column) + \":cannot popFront() empty Deque\"); T result = values->front(); values->pop_front(); return result; }",
        "    T pop_back_value(int line, int column) { if (values->empty()) throw runtime_error(to_string(line) + \":\" + to_string(column) + \":cannot popBack() empty Deque\"); T result = values->back(); values->pop_back(); return result; }",
        "    CPPPList<T> to_list() const { return CPPPList<T>(values->begin(), values->end()); }",
        "};",
        "",
        "template <typename T>",
        "class CPPPSet {",
        "    struct Compare { function<bool(const T&, const T&)> value; Compare() : value([](const T& left, const T& right) { return left < right; }) {} template <typename F> Compare(F compare) : value(compare) {} bool operator()(const T& left, const T& right) const { return value(left, right); } };",
        "    using storage_type = set<T, Compare>;",
        "    cppp_smart_pointer<storage_type> values;",
        "public:",
        "    using value_type = T; using size_type = typename storage_type::size_type; using iterator = typename storage_type::iterator; using const_iterator = typename storage_type::const_iterator; using reverse_iterator = typename storage_type::reverse_iterator; using const_reverse_iterator = typename storage_type::const_reverse_iterator;",
        "    CPPPSet() : values(cppp_smart_pointer<storage_type>::make()) {}",
        "    template <typename F> CPPPSet(F compare) : values(cppp_smart_pointer<storage_type>::make(Compare(compare))) {}",
        "    CPPPSet(initializer_list<T> init) : values(cppp_smart_pointer<storage_type>::make(init)) {}",
        "    CPPPSet(const set<T>& init) : values(cppp_smart_pointer<storage_type>::make(init.begin(), init.end())) {}",
        "    CPPPSet(set<T>&& init) : values(cppp_smart_pointer<storage_type>::make(init.begin(), init.end())) {}",
        "    template <typename U> CPPPSet(const set<U>& init) : values(cppp_smart_pointer<storage_type>::make()) { for (const auto& item : init) values->emplace(item); }",
        "    template <typename It> CPPPSet(It first, It last) : values(cppp_smart_pointer<storage_type>::make(first, last)) {}",
        "    iterator begin() { return values->begin(); }",
        "    const_iterator begin() const { return values->begin(); }",
        "    iterator end() { return values->end(); }",
        "    const_iterator end() const { return values->end(); }",
        "    reverse_iterator rbegin() { return values->rbegin(); }",
        "    const_reverse_iterator rbegin() const { return values->rbegin(); }",
        "    reverse_iterator rend() { return values->rend(); }",
        "    const_reverse_iterator rend() const { return values->rend(); }",
        "    bool empty() const { return values->empty(); }",
        "    size_type size() const { return values->size(); }",
        "    void clear() { values->clear(); }",
        "    pair<iterator,bool> insert(const T& value) { return values->insert(value); }",
        "    template <typename It> void insert(It first, It last) { values->insert(first, last); }",
        "    iterator erase(const_iterator pos) { return values->erase(pos); }",
        "    size_type erase(const T& key) { return values->erase(key); }",
        "    iterator find(const T& key) { return values->find(key); }",
        "    const_iterator find(const T& key) const { return values->find(key); }",
        "    iterator lower_bound(const T& key) { return values->lower_bound(key); }",
        "    const_iterator lower_bound(const T& key) const { return values->lower_bound(key); }",
        "    iterator upper_bound(const T& key) { return values->upper_bound(key); }",
        "    const_iterator upper_bound(const T& key) const { return values->upper_bound(key); }",
        "    operator storage_type&() { return *values; }",
        "    operator const storage_type&() const { return *values; }",
        "    friend bool operator==(const CPPPSet& a, const CPPPSet& b) { return *a.values == *b.values; }",
        "    friend bool operator!=(const CPPPSet& a, const CPPPSet& b) { return *a.values != *b.values; }",
        "    friend bool operator<(const CPPPSet& a, const CPPPSet& b) { return *a.values < *b.values; }",
        "    friend bool operator>(const CPPPSet& a, const CPPPSet& b) { return *a.values > *b.values; }",
        "    friend bool operator<=(const CPPPSet& a, const CPPPSet& b) { return *a.values <= *b.values; }",
        "    friend bool operator>=(const CPPPSet& a, const CPPPSet& b) { return *a.values >= *b.values; }",
        "};",
        "",
        "template <typename K, typename V>",
        "class CPPPMap {",
        "    struct Compare { function<bool(const K&, const K&)> value; Compare() : value([](const K& left, const K& right) { return left < right; }) {} template <typename F> Compare(F compare) : value(compare) {} bool operator()(const K& left, const K& right) const { return value(left, right); } };",
        "    using storage_type = map<K,V,Compare>;",
        "    cppp_smart_pointer<storage_type> values;",
        "public:",
        "    using value_type = pair<const K,V>; using size_type = typename storage_type::size_type; using iterator = typename storage_type::iterator; using const_iterator = typename storage_type::const_iterator; using reverse_iterator = typename storage_type::reverse_iterator; using const_reverse_iterator = typename storage_type::const_reverse_iterator;",
        "    CPPPMap() : values(cppp_smart_pointer<storage_type>::make()) {}",
        "    template <typename F> CPPPMap(F compare) : values(cppp_smart_pointer<storage_type>::make(Compare(compare))) {}",
        "    CPPPMap(initializer_list<value_type> init) : values(cppp_smart_pointer<storage_type>::make(init)) {}",
        "    CPPPMap(const map<K,V>& init) : values(cppp_smart_pointer<storage_type>::make(init.begin(), init.end())) {}",
        "    CPPPMap(map<K,V>&& init) : values(cppp_smart_pointer<storage_type>::make(init.begin(), init.end())) {}",
        "    template <typename It> CPPPMap(It first, It last) : values(cppp_smart_pointer<storage_type>::make(first, last)) {}",
        "    iterator begin() { return values->begin(); }",
        "    const_iterator begin() const { return values->begin(); }",
        "    iterator end() { return values->end(); }",
        "    const_iterator end() const { return values->end(); }",
        "    reverse_iterator rbegin() { return values->rbegin(); }",
        "    const_reverse_iterator rbegin() const { return values->rbegin(); }",
        "    reverse_iterator rend() { return values->rend(); }",
        "    const_reverse_iterator rend() const { return values->rend(); }",
        "    bool empty() const { return values->empty(); }",
        "    size_type size() const { return values->size(); }",
        "    void clear() { values->clear(); }",
        "    V& operator[](const K& key) { return (*values)[key]; }",
        "    V& at(const K& key) { return values->at(key); }",
        "    const V& at(const K& key) const { return values->at(key); }",
        "    pair<iterator,bool> insert(const value_type& value) { return values->insert(value); }",
        "    template <typename It> void insert(It first, It last) { values->insert(first, last); }",
        "    iterator erase(const_iterator pos) { return values->erase(pos); }",
        "    size_type erase(const K& key) { return values->erase(key); }",
        "    iterator find(const K& key) { return values->find(key); }",
        "    const_iterator find(const K& key) const { return values->find(key); }",
        "    iterator lower_bound(const K& key) { return values->lower_bound(key); }",
        "    const_iterator lower_bound(const K& key) const { return values->lower_bound(key); }",
        "    iterator upper_bound(const K& key) { return values->upper_bound(key); }",
        "    const_iterator upper_bound(const K& key) const { return values->upper_bound(key); }",
        "    operator storage_type&() { return *values; }",
        "    operator const storage_type&() const { return *values; }",
        "    friend bool operator==(const CPPPMap& a, const CPPPMap& b) { return *a.values == *b.values; }",
        "    friend bool operator!=(const CPPPMap& a, const CPPPMap& b) { return *a.values != *b.values; }",
        "    friend bool operator<(const CPPPMap& a, const CPPPMap& b) { return *a.values < *b.values; }",
        "    friend bool operator>(const CPPPMap& a, const CPPPMap& b) { return *a.values > *b.values; }",
        "    friend bool operator<=(const CPPPMap& a, const CPPPMap& b) { return *a.values <= *b.values; }",
        "    friend bool operator>=(const CPPPMap& a, const CPPPMap& b) { return *a.values >= *b.values; }",
        "};",
        ""
    };

    std::vector<std::string> selected;
    std::vector<std::string> group;
    std::string groupType;
    for (const std::string& line : all) {
        if (group.empty() && line.empty()) continue;
        const std::string member = containerMemberForLine(line);
        const size_t variantSeparator = member.find('_');
        const std::string genericMember = variantSeparator == std::string::npos
            ? member
            : member.substr(0, variantSeparator);
        if (member.empty() || requiredMembers.count("all") != 0 ||
            requiredMembers.count(member) != 0 ||
            requiredMembers.count(genericMember) != 0 ||
            requiredMembers.count(groupType + "." + member) != 0 ||
            requiredMembers.count(groupType + "." + genericMember) != 0) {
            group.push_back(line);
        }
        if (line == "class CPPPPair {") groupType = "CPPPPair";
        if (line == "class CPPPList {") groupType = "CPPPList";
        if (line == "class CPPPStack {") groupType = "CPPPStack";
        if (line == "class CPPPQueue {") groupType = "CPPPQueue";
        if (line == "class CPPPDeque {") groupType = "CPPPDeque";
        if (line == "class CPPPSet {") groupType = "CPPPSet";
        if (line == "class CPPPMap {") groupType = "CPPPMap";
        if (line == "};") {
            if (requiredTypes.count(groupType) != 0) {
                selected.insert(selected.end(), group.begin(), group.end());
                selected.push_back("");
            }
            group.clear();
            groupType.clear();
        }
    }
    return selected;
}
}

// runtimeHelpers provides runtime support for generated code.
std::vector<RuntimeHelper> runtimeHelpers() {
    std::vector<RuntimeHelper> helpers = {
        {"CPPPContainerCompare", {}, {}, {}},
        {
            "CPPPFunctionType",
            {
                "template <typename Signature> class CPPPFunction;",
                "template <typename R, typename... Args>",
                "class CPPPFunction<R(Args...)> {",
                "    using Pointer = R (*)(Args...);",
                "    Pointer direct = nullptr;",
                "    function<R(Args...)>* closure = nullptr;",
                "public:",
                "    CPPPFunction() = default;",
                "    CPPPFunction(Pointer value) : direct(value) {}",
                "    template <typename F, enable_if_t<!is_same<decay_t<F>, CPPPFunction>::value, int> = 0>",
                "    CPPPFunction(F&& value) : closure(new function<R(Args...)>(std::forward<F>(value))) {}",
                "    R operator()(Args... args) const { return direct ? direct(std::forward<Args>(args)...) : (*closure)(std::forward<Args>(args)...); }",
                "    friend bool operator==(const CPPPFunction& left, const CPPPFunction& right) {",
                "        if (left.direct || right.direct) return left.direct == right.direct && left.closure == right.closure;",
                "        return left.closure == right.closure;",
                "    }",
                "    friend bool operator!=(const CPPPFunction& left, const CPPPFunction& right) { return !(left == right); }",
                "    friend ostream& operator<<(ostream& output, const CPPPFunction&) { return output << \"<function>\"; }",
                "};",
                ""
            },
            {},
            {"CPPPFunction<"}
        },
        {
            "CPPPCharType",
            {
                "struct CPPPChar {",
                "    char value = '\\0';",
                "    CPPPChar() = default;",
                "    CPPPChar(char initialValue) : value(initialValue) {}",
                "    operator char() const { return value; }",
                "};",
                ""
            },
            {},
            {"CPPPChar"}
        },
        {
            "CPPPCharOutput",
            {
                "ostream& operator<<(ostream& output, const CPPPChar& value) {",
                "    if (value.value == '\\0') {",
                "        return output << 0;",
                "    }",
                "",
                "    return output << value.value;",
                "}",
                ""
            },
            {"CPPPCharType"},
            {"operator<<(ostream& output, const CPPPChar"}
        },
        {
            "CPPPCharInput",
            {
                "istream& operator>>(istream& input, CPPPChar& value) {",
                "    char ch;",
                "    input >> ch;",
                "    value = CPPPChar(ch);",
                "    return input;",
                "}",
                ""
            },
            {"CPPPCharType"},
            {"operator>>(istream& input, CPPPChar"}
        },
        {
            "CPPPCharIncrement",
            {
                "CPPPChar& operator++(CPPPChar& value) { ++value.value; return value; }",
                ""
            },
            {"CPPPCharType"},
            {"operator++(CPPPChar"}
        },
        {
            "CPPPCharDecrement",
            {
                "CPPPChar& operator--(CPPPChar& value) { --value.value; return value; }",
                ""
            },
            {"CPPPCharType"},
            {"operator--(CPPPChar"}
        },
        {
            "CPPPRangeType",
            {
                "struct CPPPRange {",
                "    struct Iterator {",
                "        long long current = 0;",
                "        long long stop = 0;",
                "        long long step = 1;",
                "",
                "        long long operator*() const { return current; }",
                "        Iterator& operator++() { current += step; return *this; }",
                "        bool operator!=(const Iterator&) const {",
                "            return step > 0 ? current < stop : current > stop;",
                "        }",
                "    };",
                "",
                "    long long start = 0;",
                "    long long stop = 0;",
                "    long long step = 1;",
                "",
                "    CPPPRange() = default;",
                "    CPPPRange(long long startValue, long long stopValue, long long stepValue) :",
                "        start(startValue),",
                "        stop(stopValue),",
                "        step(stepValue) {}",
                "",
                "    Iterator begin() const { return {start, stop, step}; }",
                "    Iterator end() const { return {stop, stop, step}; }",
                "    bool empty() const { return step > 0 ? start >= stop : start <= stop; }",
                "    bool contains(long long value) const {",
                "        if (empty()) {",
                "            return false;",
                "        }",
                "        if (step > 0) {",
                "            if (value < start || value >= stop) {",
                "                return false;",
                "            }",
                "        } else if (value > start || value <= stop) {",
                "            return false;",
                "        }",
                "        const long long distance = value >= start ? value - start : start - value;",
                "        const long long stride = step >= 0 ? step : -step;",
                "        return stride != 0 && distance % stride == 0;",
                "    }",
                "};",
                ""
            },
            {},
            {"CPPPRange"}
        },
        {
            "CPPPRangeMakeStop",
            {
                "CPPPRange CPPPMakeRange(long long stop) {",
                "    return stop >= 0 ? CPPPRange(0, stop, 1) : CPPPRange(0, stop, -1);",
                "}",
                ""
            },
            {"CPPPRangeType"},
            {"CPPPMakeRange("}
        },
        {
            "CPPPRangeMakeBounds",
            {
                "CPPPRange CPPPMakeRange(long long start, long long stop) {",
                "    return start <= stop ? CPPPRange(start, stop, 1) : CPPPRange(start, stop, -1);",
                "}",
                ""
            },
            {"CPPPRangeType"},
            {"CPPPMakeRange("}
        },
        {
            "CPPPRangeMakeStep",
            {
                "CPPPRange CPPPMakeRange(long long start, long long stop, long long step, int line, int column) {",
                "    if (step == 0) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":range step cannot be zero\");",
                "    }",
                "    const long long stride = step >= 0 ? step : -step;",
                "    return start <= stop ? CPPPRange(start, stop, stride) : CPPPRange(start, stop, -stride);",
                "}",
                ""
            },
            {"CPPPRangeType"},
            {"CPPPMakeRange("}
        },
        {
            "CPPPRangeToList",
            {
                "CPPPList<long long> CPPPRangeToList(const CPPPRange& range) {",
                "    CPPPList<long long> values;",
                "    for (long long value : range) {",
                "        values.push_back(value);",
                "    }",
                "    return values;",
                "}",
                ""
            },
            {"CPPPRangeType"},
            {"CPPPRangeToList("}
        },
        {
            "CPPPRangeToSet",
            {
                "CPPPSet<long long> CPPPRangeToSet(const CPPPRange& range) {",
                "    CPPPSet<long long> values;",
                "    for (long long value : range) {",
                "        values.insert(value);",
                "    }",
                "    return values;",
                "}",
                ""
            },
            {"CPPPRangeType"},
            {"CPPPRangeToSet("}
        },
        {
            "CPPPToBoolBool",
            {
                "bool CPPPToBoolBool(bool value) { return value; }",
                ""
            },
            {},
            {"CPPPToBoolBool("}
        },
        {
            "CPPPToBoolInt",
            {
                "bool CPPPToBoolInt(long long value) { return value != 0; }",
                ""
            },
            {},
            {"CPPPToBoolInt("}
        },
        {
            "CPPPToBoolFloat",
            {
                "bool CPPPToBoolFloat(long double value) { return value != 0.0L && !isnan(value); }",
                ""
            },
            {},
            {"CPPPToBoolFloat("}
        },
        {
            "CPPPToBoolChar",
            {
                "bool CPPPToBoolChar(const CPPPChar& value) { return value.value != '\\0'; }",
                ""
            },
            {"CPPPCharType"},
            {"CPPPToBoolChar("}
        },
        {
            "CPPPToBoolFallback",
            {
                "bool CPPPToBool(bool value) { return value; }",
                "bool CPPPToBool(int value) { return value != 0; }",
                "bool CPPPToBool(long long value) { return value != 0; }",
                "bool CPPPToBool(long double value) { return value != 0.0L && !isnan(value); }",
                "bool CPPPToBool(const CPPPChar& value) { return value.value != '\\0'; }",
                ""
            },
            {"CPPPCharType"},
            {"CPPPToBool("}
        },
        {
            "CPPPInputBool",
            {
                "bool CPPPInputBool() { bool value; cin >> value; return value; }",
                ""
            },
            {},
            {"CPPPInputBool("}
        },
        {
            "CPPPInputChar",
            {
                "CPPPChar CPPPInputChar() { CPPPChar value; cin >> value; return value; }",
                ""
            },
            {"CPPPCharInput"},
            {"CPPPInputChar("}
        },
        {
            "CPPPInputInt",
            {
                "long long CPPPInputInt() { long long value; cin >> value; return value; }",
                ""
            },
            {},
            {"CPPPInputInt("}
        },
        {
            "CPPPInputFloat",
            {
                "long double CPPPInputFloat() { long double value; cin >> value; return value; }",
                ""
            },
            {},
            {"CPPPInputFloat("}
        },
        {
            "CPPPInputStringWord",
            {
                "CPPPList<CPPPChar> CPPPInputString() {",
                "    string value;",
                "    cin >> value;",
                "    CPPPList<CPPPChar> result;",
                "    result.reserve(value.size());",
                "    for (char ch : value) {",
                "        result.push_back(CPPPChar(ch));",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {"CPPPCharType"},
            {"CPPPInputString()"}
        },
        {
            "CPPPInputStringCount",
            {
                "CPPPList<CPPPChar> CPPPInputString(long long count) {",
                "    string value;",
                "    value.reserve(static_cast<size_t>(max(0LL, count)));",
                "    cin >> ws;",
                "    for (long long i = 0; i < count; ++i) {",
                "        char ch = '\\0';",
                "        if (!cin.get(ch)) {",
                "            break;",
                "        }",
                "        value.push_back(ch);",
                "    }",
                "    CPPPList<CPPPChar> result;",
                "    result.reserve(value.size());",
                "    for (char ch : value) {",
                "        result.push_back(CPPPChar(ch));",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {"CPPPCharType"},
            {"CPPPInputString(long long"}
        },
        {
            "CPPPInputList",
            {
                "template <typename Reader>",
                "auto CPPPInputList(long long count, Reader reader) {",
                "    using Value = decltype(reader());",
                "    vector<Value> values;",
                "    for (long long i = 0; i < count; ++i) {",
                "        values.push_back(reader());",
                "    }",
                "    return values;",
                "}",
                ""
            },
            {},
            {"CPPPInputList("}
        },
        {
            "CPPPInputListLine",
            {
                "template <typename T>",
                "CPPPList<T> CPPPInputListLine() {",
                "    string line;",
                "    getline(cin >> ws, line);",
                "    istringstream stream(line);",
                "    CPPPList<T> values;",
                "    T value;",
                "    while (stream >> value) {",
                "        values.push_back(value);",
                "    }",
                "    return values;",
                "}",
                ""
            },
            {},
            {"CPPPInputListLine<"}
        },
        {
            "CPPPStringLiteral",
            {
                "CPPPList<CPPPChar> CPPPStringLiteral(const string& value) {",
                "    CPPPList<CPPPChar> result;",
                "    result.reserve(value.size());",
                "    for (char ch : value) {",
                "        result.push_back(CPPPChar(ch));",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {"CPPPCharType"},
            {"CPPPStringLiteral("}
        },
        {
            "CPPPStringFromStd",
            {
                "CPPPList<CPPPChar> CPPPStringFromStd(const string& value) {",
                "    CPPPList<CPPPChar> result;",
                "    result.reserve(value.size());",
                "    for (char ch : value) {",
                "        result.push_back(CPPPChar(ch));",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {"CPPPCharType"},
            {"CPPPStringFromStd("}
        },
        {
            "CPPPStdStringFromChars",
            {
                "string CPPPStdStringFromChars(const CPPPList<CPPPChar>& value) {",
                "    string result;",
                "    result.reserve(value.size());",
                "    for (const CPPPChar& ch : value) {",
                "        result.push_back(ch.value);",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {"CPPPCharType"},
            {"CPPPStdStringFromChars("}
        },
        {
            "CPPPToStringBool",
            {
                "CPPPList<CPPPChar> CPPPToStringBool(bool value) {",
                "    return CPPPStringFromStd(value ? \"1\" : \"0\");",
                "}",
                ""
            },
            {"CPPPStringFromStd"},
            {"CPPPToStringBool("}
        },
        {
            "CPPPToStringChar",
            {
                "CPPPList<CPPPChar> CPPPToStringChar(const CPPPChar& value) {",
                "    return {value};",
                "}",
                ""
            },
            {"CPPPCharType"},
            {"CPPPToStringChar("}
        },
        {
            "CPPPToStringInt",
            {
                "CPPPList<CPPPChar> CPPPToStringInt(long long value) {",
                "    return CPPPStringFromStd(to_string(value));",
                "}",
                ""
            },
            {"CPPPStringFromStd"},
            {"CPPPToStringInt("}
        },
        {
            "CPPPToStringFloat",
            {
                "CPPPList<CPPPChar> CPPPToStringFloat(long double value) {",
                "    ostringstream stream;",
                "    stream << value;",
                "    return CPPPStringFromStd(stream.str());",
                "}",
                ""
            },
            {"CPPPStringFromStd"},
            {"CPPPToStringFloat("}
        },
        {
            "CPPPStringToBool",
            {
                "bool CPPPStringToBool(const CPPPList<CPPPChar>& value, int line, int column) {",
                "    const string text = CPPPStdStringFromChars(value);",
                "    if (text == \"1\" || text == \"true\") {",
                "        return true;",
                "    }",
                "    if (text == \"0\" || text == \"false\") {",
                "        return false;",
                "    }",
                "    throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid bool string\");",
                "}",
                ""
            },
            {"CPPPStdStringFromChars"},
            {"CPPPStringToBool("}
        },
        {
            "CPPPStringToChar",
            {
                "CPPPChar CPPPStringToChar(const CPPPList<CPPPChar>& value, int line, int column) {",
                "    if (value.size() != 1) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid char string\");",
                "    }",
                "    return value[0];",
                "}",
                ""
            },
            {"CPPPCharType"},
            {"CPPPStringToChar("}
        },
        {
            "CPPPStringToInt",
            {
                "long long CPPPStringToInt(const CPPPList<CPPPChar>& value, int line, int column) {",
                "    const string text = CPPPStdStringFromChars(value);",
                "    if (text.empty()) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid int string\");",
                "    }",
                "    size_t index = 0;",
                "    if (text[index] == '+' || text[index] == '-') {",
                "        ++index;",
                "    }",
                "    if (index >= text.size()) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid int string\");",
                "    }",
                "    for (size_t i = index; i < text.size(); ++i) {",
                "        if (!isdigit(static_cast<unsigned char>(text[i]))) {",
                "            throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid int string\");",
                "        }",
                "    }",
                "    try {",
                "        return stoll(text);",
                "    } catch (...) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid int string\");",
                "    }",
                "}",
                ""
            },
            {"CPPPStdStringFromChars"},
            {"CPPPStringToInt("}
        },
        {
            "CPPPStringToFloat",
            {
                "long double CPPPStringToFloat(const CPPPList<CPPPChar>& value, int line, int column) {",
                "    const string text = CPPPStdStringFromChars(value);",
                "    if (text.empty()) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid float string\");",
                "    }",
                "    size_t index = 0;",
                "    if (text[index] == '+' || text[index] == '-') {",
                "        ++index;",
                "    }",
                "    const size_t wholeStart = index;",
                "    while (index < text.size() && isdigit(static_cast<unsigned char>(text[index]))) {",
                "        ++index;",
                "    }",
                "    if (wholeStart == index) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid float string\");",
                "    }",
                "    if (index < text.size() && text[index] == '.') {",
                "        ++index;",
                "        const size_t fractionalStart = index;",
                "        while (index < text.size() && isdigit(static_cast<unsigned char>(text[index]))) {",
                "            ++index;",
                "        }",
                "        if (fractionalStart == index) {",
                "            throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid float string\");",
                "        }",
                "    }",
                "    if (index != text.size()) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid float string\");",
                "    }",
                "    try {",
                "        return stold(text);",
                "    } catch (...) {",
                "        throw runtime_error(to_string(line) + \":\" + to_string(column) + \":invalid float string\");",
                "    }",
                "}",
                ""
            },
            {"CPPPStdStringFromChars"},
            {"CPPPStringToFloat("}
        },
        {
            "CPPPListToSet",
            {
                "template <typename Out, typename In, typename Converter>",
                "CPPPSet<Out> CPPPListToSet(const CPPPList<In>& values, Converter convert) {",
                "    CPPPSet<Out> result;",
                "    for (const In& value : values) {",
                "        result.insert(convert(value));",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {},
            {"CPPPListToSet<"}
        },
        {
            "CPPPSetToList",
            {
                "template <typename Out, typename In, typename Converter>",
                "CPPPList<Out> CPPPSetToList(const CPPPSet<In>& values, Converter convert) {",
                "    CPPPList<Out> result;",
                "    result.reserve(values.size());",
                "    for (const In& value : values) {",
                "        result.push_back(convert(value));",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {},
            {"CPPPSetToList<"}
        },
        {
            "CPPPListToMap",
            {
                "template <typename KOut, typename VOut, typename KIn, typename VIn, typename KeyConverter, typename ValueConverter>",
                "CPPPMap<KOut, VOut> CPPPListToMap(const CPPPList<CPPPPair<KIn, VIn>>& values, KeyConverter keyConvert, ValueConverter valueConvert) {",
                "    CPPPMap<KOut, VOut> result;",
                "    for (const CPPPPair<KIn, VIn>& entry : values) {",
                "        result[keyConvert(entry.first())] = valueConvert(entry.second());",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {},
            {"CPPPListToMap<"}
        },
        {
            "CPPPMapToList",
            {
                "template <typename KOut, typename VOut, typename KIn, typename VIn, typename KeyConverter, typename ValueConverter>",
                "CPPPList<CPPPPair<KOut, VOut>> CPPPMapToList(const CPPPMap<KIn, VIn>& values, KeyConverter keyConvert, ValueConverter valueConvert) {",
                "    CPPPList<CPPPPair<KOut, VOut>> result;",
                "    result.reserve(values.size());",
                "    for (const pair<const KIn, VIn>& entry : values) {",
                "        result.push_back(CPPPPair<KOut, VOut>(keyConvert(entry.first), valueConvert(entry.second)));",
                "    }",
                "    return result;",
                "}",
                ""
            },
            {},
            {"CPPPMapToList<"}
        },
        {
            "CPPPListToStack",
            {
                "template <typename Out, typename In, typename Converter>",
                "CPPPStack<Out> CPPPListToStack(const CPPPList<In>& values, Converter convert) { CPPPStack<Out> result; for (const auto& value : values) result.push(convert(value)); return result; }",
                ""
            },
            {},
            {"CPPPListToStack<"}
        },
        {
            "CPPPListToQueue",
            {
                "template <typename Out, typename In, typename Converter>",
                "CPPPQueue<Out> CPPPListToQueue(const CPPPList<In>& values, Converter convert) { CPPPQueue<Out> result; for (const auto& value : values) result.push(convert(value)); return result; }",
                ""
            },
            {},
            {"CPPPListToQueue<"}
        },
        {
            "CPPPListToDeque",
            {
                "template <typename Out, typename In, typename Converter>",
                "CPPPDeque<Out> CPPPListToDeque(const CPPPList<In>& values, Converter convert) { CPPPDeque<Out> result; for (const auto& value : values) result.push_back(convert(value)); return result; }",
                ""
            },
            {},
            {"CPPPListToDeque<"}
        },
        {
            "CPPPStackToList",
            {
                "template <typename Out, typename In, typename Converter>",
                "CPPPList<Out> CPPPStackToList(const CPPPStack<In>& values, Converter convert) { CPPPList<Out> result; CPPPList<In> source = values.to_list(); result.reserve(source.size()); for (const auto& value : source) result.push_back(convert(value)); return result; }",
                ""
            },
            {},
            {"CPPPStackToList<"}
        },
        {
            "CPPPQueueToList",
            {
                "template <typename Out, typename In, typename Converter>",
                "CPPPList<Out> CPPPQueueToList(const CPPPQueue<In>& values, Converter convert) { CPPPList<Out> result; CPPPList<In> source = values.to_list(); result.reserve(source.size()); for (const auto& value : source) result.push_back(convert(value)); return result; }",
                ""
            },
            {},
            {"CPPPQueueToList<"}
        },
        {
            "CPPPDequeToList",
            {
                "template <typename Out, typename In, typename Converter>",
                "CPPPList<Out> CPPPDequeToList(const CPPPDeque<In>& values, Converter convert) { CPPPList<Out> result; CPPPList<In> source = values.to_list(); result.reserve(source.size()); for (const auto& value : source) result.push_back(convert(value)); return result; }",
                ""
            },
            {},
            {"CPPPDequeToList<"}
        },
        {
            "CPPPPrintValueString",
            {
                "void CPPPPrintValue(ostream& output, const CPPPList<CPPPChar>& value) {",
                "    for (const CPPPChar& ch : value) {",
                "        output << ch.value;",
                "    }",
                "}",
                ""
            },
            {"CPPPCharType"},
            {"CPPPPrintValueString("}
        },
        {"CPPPPrintValueBase", {"template <typename A, typename B> class CPPPPair; template <typename T> class CPPPList; template <typename T> class CPPPSet; template <typename K, typename V> class CPPPMap;", "template <typename T> void CPPPPrintValue(ostream& output, const T& value);", "template <typename A, typename B> void CPPPPrintValue(ostream& output, const CPPPPair<A, B>& value);", "template <typename T> void CPPPPrintValue(ostream& output, const CPPPList<T>& values);", "template <typename T> void CPPPPrintValue(ostream& output, const CPPPSet<T>& values);", "template <typename K, typename V> void CPPPPrintValue(ostream& output, const CPPPMap<K, V>& values);", "template <typename T> void CPPPPrintValue(ostream& output, const cppp_smart_pointer<T>& value) { if (!value) { output << \"NULL\"; return; } CPPPPrintValue(output, *value); }", ""}, {}, {}},
        {"CPPPPrintValuePair", {"template <typename A, typename B> void CPPPPrintValue(ostream& output, const CPPPPair<A, B>& value) { output << '('; CPPPPrintValue(output, value.first()); output << ','; CPPPPrintValue(output, value.second()); output << ')'; }", ""}, {"CPPPPrintValueBase"}, {}},
        {"CPPPPrintValueList", {"template <typename T> void CPPPPrintValue(ostream& output, const CPPPList<T>& values) { output << '['; for (size_t i = 0; i < values.size(); ++i) { if (i > 0) output << \", \"; CPPPPrintValue(output, values[i]); } output << ']'; }", ""}, {"CPPPPrintValueBase"}, {}},
        {"CPPPPrintValueStack", {"template <typename T> void CPPPPrintValue(ostream& output, const CPPPStack<T>& values) { CPPPPrintValue(output, values.to_list()); }", ""}, {"CPPPPrintValueBase", "CPPPPrintValueList"}, {}},
        {"CPPPPrintValueQueue", {"template <typename T> void CPPPPrintValue(ostream& output, const CPPPQueue<T>& values) { CPPPPrintValue(output, values.to_list()); }", ""}, {"CPPPPrintValueBase", "CPPPPrintValueList"}, {}},
        {"CPPPPrintValueDeque", {"template <typename T> void CPPPPrintValue(ostream& output, const CPPPDeque<T>& values) { CPPPPrintValue(output, values.to_list()); }", ""}, {"CPPPPrintValueBase", "CPPPPrintValueList"}, {}},
        {"CPPPPrintValueSet", {"template <typename T> void CPPPPrintValue(ostream& output, const CPPPSet<T>& values) { output << '{'; bool first = true; for (const auto& value : values) { if (!first) output << \", \"; first = false; CPPPPrintValue(output, value); } output << '}'; }", ""}, {"CPPPPrintValueBase"}, {}},
        {"CPPPPrintValueMap", {"template <typename K, typename V> void CPPPPrintValue(ostream& output, const CPPPMap<K, V>& values) { output << '{'; bool first = true; for (const auto& entry : values) { if (!first) output << \", \"; first = false; CPPPPrintValue(output, entry.first); output << ':'; CPPPPrintValue(output, entry.second); } output << '}'; }", ""}, {"CPPPPrintValueBase"}, {}},
        {"CPPPPrintValue", {"template <typename T> void CPPPPrintValue(ostream& output, const T& value) { output << value; }", ""}, {"CPPPPrintValueBase"}, {"CPPPPrintValue("}},
        {
            "CPPPStructClone",
            {
                "template <typename T>",
                "cppp_smart_pointer<T> CPPPStructClone(const cppp_smart_pointer<T>& value) {",
                "    return value ? cppp_smart_pointer<T>::make(*value) : nullptr;",
                "}",
                ""
            },
            {},
            {"CPPPStructClone("}
        },
        {"CPPPCopyBase", {"template <typename T> T CPPPCopy(const T& value);", "template <typename T> struct CPPPDeepCopier { static T run(const T& value) { return value; } };", ""}, {}, {}},
        {"CPPPCopyPair", {"template <typename A, typename B> struct CPPPDeepCopier<CPPPPair<A,B>> { static CPPPPair<A,B> run(const CPPPPair<A,B>& value) { return {CPPPCopy(value.first()), CPPPCopy(value.second())}; } };", ""}, {"CPPPCopyBase"}, {}},
        {"CPPPCopyList", {"template <typename T> struct CPPPDeepCopier<CPPPList<T>> { static CPPPList<T> run(const CPPPList<T>& value) { CPPPList<T> result; result.reserve(value.size()); for (const auto& item : value) result.push_back(CPPPCopy(item)); return result; } };", ""}, {"CPPPCopyBase"}, {}},
        {"CPPPCopyStack", {"template <typename T> struct CPPPDeepCopier<CPPPStack<T>> { static CPPPStack<T> run(const CPPPStack<T>& value) { CPPPStack<T> result; CPPPList<T> items = value.to_list(); for (const auto& item : items) result.push(CPPPCopy(item)); return result; } };", ""}, {"CPPPCopyBase"}, {}},
        {"CPPPCopyQueue", {"template <typename T> struct CPPPDeepCopier<CPPPQueue<T>> { static CPPPQueue<T> run(const CPPPQueue<T>& value) { CPPPQueue<T> result; CPPPList<T> items = value.to_list(); for (const auto& item : items) result.push(CPPPCopy(item)); return result; } };", ""}, {"CPPPCopyBase"}, {}},
        {"CPPPCopyDeque", {"template <typename T> struct CPPPDeepCopier<CPPPDeque<T>> { static CPPPDeque<T> run(const CPPPDeque<T>& value) { CPPPDeque<T> result; CPPPList<T> items = value.to_list(); for (const auto& item : items) result.push_back(CPPPCopy(item)); return result; } };", ""}, {"CPPPCopyBase"}, {}},
        {"CPPPCopySet", {"template <typename T> struct CPPPDeepCopier<CPPPSet<T>> { static CPPPSet<T> run(const CPPPSet<T>& value) { CPPPSet<T> result; for (const auto& item : value) result.insert(CPPPCopy(item)); return result; } };", ""}, {"CPPPCopyBase"}, {}},
        {"CPPPCopyMap", {"template <typename K, typename V> struct CPPPDeepCopier<CPPPMap<K,V>> { static CPPPMap<K,V> run(const CPPPMap<K,V>& value) { CPPPMap<K,V> result; for (const auto& item : value) result[CPPPCopy(item.first)] = CPPPCopy(item.second); return result; } };", ""}, {"CPPPCopyBase"}, {}},
        {"CPPPCopyStruct", {"template <typename T> struct CPPPDeepCopier<cppp_smart_pointer<T>> { static cppp_smart_pointer<T> run(const cppp_smart_pointer<T>& value) { return value ? cppp_smart_pointer<T>::make(*value) : nullptr; } };", ""}, {"CPPPCopyBase"}, {}},
        {"CPPPCopy", {"template <typename T> T CPPPCopy(const T& value) { return CPPPDeepCopier<T>::run(value); }", ""}, {"CPPPCopyBase"}, {"CPPPCopy("}},
        {
            "CPPPPrintDelimited",
            {
                "void CPPPPrintDelimiter(ostream& output, const CPPPList<CPPPChar>& value) {",
                "    CPPPPrintValue(output, value);",
                "}",
                "",
                "template <typename T>",
                "void CPPPPrintDelimiter(ostream& output, const T& value) {",
                "    CPPPPrintValue(output, value);",
                "}",
                "",
                "template <typename T, typename Delimiter>",
                "void CPPPPrintDelimited(ostream& output, const CPPPList<T>& values, const Delimiter& delimiter) {",
                "    for (size_t i = 0; i < values.size(); ++i) {",
                "        if (i > 0) {",
                "            CPPPPrintDelimiter(output, delimiter);",
                "        }",
                "        CPPPPrintValue(output, values[i]);",
                "    }",
                "}",
                "",
                "template <typename T, typename Delimiter>",
                "void CPPPPrintDelimited(ostream& output, const CPPPSet<T>& values, const Delimiter& delimiter) {",
                "    bool first = true;",
                "    for (const auto& value : values) {",
                "        if (!first) {",
                "            CPPPPrintDelimiter(output, delimiter);",
                "        }",
                "        first = false;",
                "        CPPPPrintValue(output, value);",
                "    }",
                "}",
                ""
            },
            {"CPPPCharType", "CPPPPrintValue"},
            {"CPPPPrintDelimited("}
        }
    };

    const std::vector<RuntimeHelper> listHelpers = listRuntimeHelpers();
    helpers.insert(helpers.end(), listHelpers.begin(), listHelpers.end());
    return helpers;
}

// typeSupportPreamble implements the typeSupportPreamble behavior for the typesCppp.cpp module.
std::vector<std::string> typeSupportPreamble() {
    std::vector<std::string> preamble = smartPointerSupport();
    const std::vector<std::string> containers = containerSupport({"CPPPPair", "CPPPList", "CPPPStack", "CPPPQueue", "CPPPDeque", "CPPPSet", "CPPPMap"}, {"all"});
    preamble.insert(preamble.end(), containers.begin(), containers.end());
    for (const RuntimeHelper& helper : runtimeHelpers()) {
        preamble.insert(preamble.end(), helper.code.begin(), helper.code.end());
    }
    return preamble;
}

void clearRequiredRuntimeHelpers() {
    runtimeRequirementSet().clear();
    structMethodRequirementSet().clear();
    runtimeRequirementOwners().clear();
    containerMemberRequirementOwners().clear();
    runtimeRequirementOwner().clear();
}

void requireContainerMember(const Type& type, const std::string& memberName) {
    const std::string typeName = containerTypeName(type);
    if (typeName.empty()) return;
    containerMemberRequirementOwners()[typeName + "." + memberName].insert(runtimeRequirementOwner());
    if (isStackType(type) && memberName == "to_list") {
        requireContainerMember(Type(PrimitiveType::List, type.subtypes), "ctor_vector");
    } else if (isQueueType(type) && memberName == "to_list") {
        const Type listType(PrimitiveType::List, type.subtypes);
        requireContainerMember(listType, "ctor_default");
        requireContainerMember(listType, "reserve");
        requireContainerMember(listType, "push_back");
    } else if (isDequeType(type) && memberName == "to_list") {
        requireContainerMember(Type(PrimitiveType::List, type.subtypes), "ctor_iterator");
    }
    if ((isSetType(type) || isMapType(type)) && !type.subtypes.empty() &&
        (isCollectionType(type.subtypes[0]) || isPairType(type.subtypes[0]))) {
        requireContainerMember(type.subtypes[0], "compare_lt");
    }
}

std::set<std::string> requiredContainerMembersForOwners(const std::set<std::string>& ownerKeys) {
    std::set<std::string> members;
    for (const auto& requirement : containerMemberRequirementOwners()) {
        if (requirement.second.count("") != 0) {
            members.insert(requirement.first);
            continue;
        }
        for (const std::string& owner : requirement.second) {
            if (ownerKeys.count(owner) != 0) {
                members.insert(requirement.first);
                break;
            }
        }
    }
    return members;
}

void requireRuntimeHelper(const std::string& helperName) {
    runtimeRequirementSet().insert(helperName);
    runtimeRequirementOwners()[helperName].insert(runtimeRequirementOwner());
}

const std::set<std::string>& requiredRuntimeHelpers() {
    return runtimeRequirementSet();
}

void requireCopyHelpersForType(const Type& type) {
    requireRuntimeHelper("CPPPCopy");
    if (isPairType(type)) {
        requireRuntimeHelper("CPPPCopyPair");
    } else if (isListType(type)) {
        requireRuntimeHelper("CPPPCopyList");
    } else if (isStackType(type)) {
        requireRuntimeHelper("CPPPCopyStack");
    } else if (isQueueType(type)) {
        requireRuntimeHelper("CPPPCopyQueue");
    } else if (isDequeType(type)) {
        requireRuntimeHelper("CPPPCopyDeque");
    } else if (isSetType(type)) {
        requireRuntimeHelper("CPPPCopySet");
    } else if (isMapType(type)) {
        requireRuntimeHelper("CPPPCopyMap");
    } else if (isStructType(type)) {
        requireRuntimeHelper("CPPPCopyStruct");
    }
    for (const Type& subtype : type.subtypes) {
        requireCopyHelpersForType(subtype);
    }
}

void requirePrintHelpersForType(const Type& type) {
    requireRuntimeHelper("CPPPPrintValue");
    if (isPairType(type)) {
        requireRuntimeHelper("CPPPPrintValuePair");
    } else if (isListType(type)) {
        requireRuntimeHelper("CPPPPrintValueList");
    } else if (isStackType(type)) {
        requireRuntimeHelper("CPPPPrintValueStack");
    } else if (isQueueType(type)) {
        requireRuntimeHelper("CPPPPrintValueQueue");
    } else if (isDequeType(type)) {
        requireRuntimeHelper("CPPPPrintValueDeque");
    } else if (isSetType(type)) {
        requireRuntimeHelper("CPPPPrintValueSet");
    } else if (isMapType(type)) {
        requireRuntimeHelper("CPPPPrintValueMap");
    }
    for (const Type& subtype : type.subtypes) {
        requirePrintHelpersForType(subtype);
    }
}

void setRuntimeRequirementOwner(const std::string& ownerKey) {
    runtimeRequirementOwner() = ownerKey;
}

std::set<std::string> requiredRuntimeHelpersForOwners(const std::set<std::string>& ownerKeys) {
    std::set<std::string> helpers;
    for (const auto& requirement : runtimeRequirementOwners()) {
        if (requirement.second.count("") != 0) {
            helpers.insert(requirement.first);
            continue;
        }
        for (const std::string& owner : requirement.second) {
            if (ownerKeys.count(owner) != 0) {
                helpers.insert(requirement.first);
                break;
            }
        }
    }
    return helpers;
}

void requireStructMethod(const std::string& structName, const std::string& methodName) {
    structMethodRequirementSet().insert(structName + "." + methodName);
}

const std::set<std::string>& requiredStructMethods() {
    return structMethodRequirementSet();
}

// typeSupportPreambleForSubmit implements the typeSupportPreambleForSubmit behavior for the typesCppp.cpp module.
std::vector<std::string> typeSupportPreambleForSubmit(
    const std::set<std::string>& requiredHelpers,
    const std::set<std::string>& requiredContainerTypes,
    const std::set<std::string>& requiredContainerMembers
) {
    const std::vector<RuntimeHelper> helpers = runtimeHelpers();
    std::map<std::string, RuntimeHelper> helpersByName;
    for (const RuntimeHelper& helper : helpers) {
        helpersByName[helper.name] = helper;
    }

    std::set<std::string> resolvedHelpers = requiredHelpers;
// worklist implements the worklist behavior for the typesCppp.cpp module.
    std::vector<std::string> worklist(requiredHelpers.begin(), requiredHelpers.end());

    for (size_t i = 0; i < worklist.size(); ++i) {
        const RuntimeHelper& helper = helpersByName.at(worklist[i]);
        for (const std::string& dep : helper.deps) {
            if (resolvedHelpers.insert(dep).second) {
                worklist.push_back(dep);
            }
        }
    }

    std::set<std::string> containerTypes = requiredContainerTypes;
    std::set<std::string> containerMembers = requiredContainerMembers;
    if (resolvedHelpers.count("CPPPInputList") != 0 || resolvedHelpers.count("CPPPInputListLine") != 0) {
        containerMembers.insert("CPPPList.ctor_default");
        containerMembers.insert("CPPPList.ctor_vector");
        containerMembers.insert("CPPPList.ctor_convert");
    }
    for (const RuntimeHelper& helper : helpers) {
        if (resolvedHelpers.count(helper.name) == 0) continue;
        if (helper.name == "CPPPPrintValueBase") continue;
        std::set<std::string> helperTypes;
        std::set<std::string> helperMembers;
        for (const std::string& line : helper.code) {
            collectContainerMemberUses(line, helperMembers);
            if (line.find("CPPPPair<") != std::string::npos) helperTypes.insert("CPPPPair");
            if (line.find("CPPPList<") != std::string::npos) helperTypes.insert("CPPPList");
            if (line.find("CPPPStack<") != std::string::npos) helperTypes.insert("CPPPStack");
            if (line.find("CPPPQueue<") != std::string::npos) helperTypes.insert("CPPPQueue");
            if (line.find("CPPPDeque<") != std::string::npos) helperTypes.insert("CPPPDeque");
            if (line.find("CPPPSet<") != std::string::npos) helperTypes.insert("CPPPSet");
            if (line.find("CPPPMap<") != std::string::npos) helperTypes.insert("CPPPMap");
        }
        containerTypes.insert(helperTypes.begin(), helperTypes.end());
        for (const std::string& type : helperTypes) {
            for (const std::string& line : helper.code) {
                if (declaresDefaultConstructedContainer(line, type)) {
                    containerMembers.insert(type + ".ctor_default");
                    break;
                }
            }
            for (const std::string& member : helperMembers) {
                containerMembers.insert(type + "." + member);
            }
        }
    }
    std::vector<std::string> preamble = smartPointerSupport();
    const std::vector<std::string> containers = containerSupport(containerTypes, containerMembers);
    preamble.insert(preamble.end(), containers.begin(), containers.end());
    for (const RuntimeHelper& helper : helpers) {
        if (resolvedHelpers.count(helper.name) == 0) {
            continue;
        }

        preamble.insert(preamble.end(), helper.code.begin(), helper.code.end());
    }

    return preamble;
}
