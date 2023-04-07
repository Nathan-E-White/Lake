#include <iostream>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <string>
#include <concepts>
#include <type_traits>
#include <filesystem>
#include <map>
#include <variant>
#include <memory>
#include <functional>
#include <cmath>


template<typename Key, typename Value,
        typename InsertPolicy = std::ostream &(*)(std::ostream &, const Value &),
        typename ExtractPolicy = std::istream &(*)(std::istream &, Value &)>
class DataLake {

private:
    /// The path to the file
    std::filesystem::path path;

    /// The map
    std::unordered_map<Key, Value> map;

    /// The insert policy
    InsertPolicy insertPolicy;

    /// The extract policy
    ExtractPolicy extractPolicy;

    /// The lake index
    std::map<Key, std::vector<std::streamoff>> m_index;

    /// The last used file
    std::filesystem::path m_filename;

    /// The directory where the files are stored
    std::filesystem::path m_directory;

public:
    explicit DataLake(const std::filesystem::path &path) : path(path) {
        std::ifstream file(path);
        if (file.is_open()) {
            Value value;
            while (extractPolicy(file, value)) {
                map.insert({value.getKey(), value});
            }
        }
    }

public:
    void insert(const Key &key, const Value &value) {
        std::ofstream out(m_filename, std::ios::app | std::ios_base::binary);
        if (out.is_open()) {
            insertPolicy(out, value);
            m_index(key, value);
        }
    }

    std::vector<Value> operator[](const Key &key) const {
        std::vector<Value> values;
        auto it = m_index.find(key);
        if (it != m_index.end()) {
            for (auto offset: it->second) {
                std::ifstream in(m_filename, std::ios::binary);
                if (in.is_open()) {
                    in.seekg(offset);
                    Value value;
                    extractPolicy(in, value);
                    values.push_back(value);
                }
            }
        }
        return values;
    }

    void remove(const Key &key) {
        auto it = m_index.find(key);
        if (it != m_index.end()) {
            m_index.erase(it);
        }
    }

    void clear_index() {
        m_index.clear();
    }

    void index_directory(const std::filesystem::path &d) {
        m_directory = d;
        for (const auto &entry: std::filesystem::directory_iterator(d)) {
            if (entry.is_regular_file()) {
                m_filename = entry.path();
                std::ifstream in(m_filename, std::ios::binary);
                if (in.is_open()) {
                    Value value;
                    while (extractPolicy(in, value)) {
                        m_index(value.getKey(), in.tellg());
                    }
                }
            }
        }
    }


private:
    std::streamoff getOffset(const Key &key) {
        auto it = m_index.find(key);
        if (it != m_index.end()) {
            return it->second.back();
        }
        return -1;
    }

private:
    std::streamoff get_fsize(const std::filesystem::path &p) {
        std::ifstream in(p, std::ios::binary | std::ios::ate);
        return in.tellg();
    }

};


template<
        typename State,
        typename Label
>
class KripkeFrame {


public:
    using StateType = State;
    using LabelType = Label;

private:
    std::vector<StateType> m_states;
    std::vector<LabelType> m_labels;
    std::vector<std::vector<std::size_t>> m_transitions;

public:

    constexpr KripkeFrame() noexcept = default;
    constexpr KripkeFrame(const KripkeFrame &) noexcept = default;
    constexpr KripkeFrame(KripkeFrame &&) noexcept = default;
    constexpr virtual ~KripkeFrame() noexcept = default;

    constexpr KripkeFrame &operator=(const KripkeFrame &) noexcept = default;
    constexpr KripkeFrame &operator=(KripkeFrame &&) noexcept = default;

    void add_state(const StateType &state, const LabelType &label) {
        m_states.push_back(state);
    }

    void add_state(StateType &&state, LabelType &&label) {
        m_states.push_back(std::move(state));
    }

    void add_transition(std::size_t from, std::size_t to) {
        m_transitions[from].push_back(to);
    }

    [[nodiscard]] std::size_t num_states() const {
        return m_states.size();
    }

    constexpr auto get_state(std::size_t idx) const -> const StateType & {
        return m_states[idx];
    }

    constexpr auto get_state(std::size_t idx) -> StateType & {
        return m_states[idx];
    }

    constexpr auto get_label(std::size_t idx) const -> const LabelType & {
        return m_labels[idx];
    }

    constexpr auto get_label(std::size_t idx) -> LabelType & {
        return m_labels[idx];
    }

    constexpr auto begin() const noexcept -> decltype(m_states.begin()) {
        return m_states.begin();
    }

    constexpr auto begin() noexcept -> decltype(m_states.begin()) {
        return m_states.begin();
    }

    constexpr auto end() const noexcept -> decltype(m_states.end()) {
        return m_states.end();
    }

    constexpr auto end() noexcept -> decltype(m_states.end()) {
        return m_states.end();
    }

    constexpr auto cbegin() const noexcept -> decltype(m_states.cbegin()) {
        return m_states.cbegin();
    }

};

/* Template class for "Expression" */
template<typename T>
class Expression {
public:
    [[nodiscard]] virtual T evaluate() const = 0;

    virtual ~Expression() = default;
};

// Constant Expression
template<typename T>
class Constant : public Expression<T> {
private:
    T m_value;
public:
    explicit Constant(T value) : m_value(value) {}

    T evaluate() const override {
        return m_value;
    }
};

// Mutable Expression
template<typename T>
class Mutable : public Expression<T> {
private:
    T m_value;
public:
    explicit Mutable(T value) : m_value(value) {}

    T evaluate() const override {
        return m_value;
    }

    void set(T value) {
        m_value = value;
    }
};

// Binary Expression
template<typename T, typename Op>
class Binary : public Expression<T> {
private:
    std::unique_ptr<Expression<T>> m_left;
    std::unique_ptr<Expression<T>> m_right;
public:
    Binary(std::unique_ptr<Expression<T>> left, std::unique_ptr<Expression<T>> right) :
            m_left(std::move(left)), m_right(std::move(right)) {}

    T evaluate() const override {
        return Op::apply(m_left->evaluate(), m_right->evaluate());
    }
};

// Unary Expression
template<typename T, typename Op>
class Unary : public Expression<T> {
private:
    std::unique_ptr<Expression<T>> m_expr;
public:
    explicit Unary(std::unique_ptr<Expression<T>> expr) : m_expr(std::move(expr)) {}

    T evaluate() const override {
        return Op::apply(m_expr->evaluate());
    }
};

// NAry Expression
template<typename T, typename Op>
class NAry : public Expression<T> {
private:
    std::vector<std::unique_ptr<Expression<T>>> m_exprs;
public:
    explicit NAry(std::vector<std::unique_ptr<Expression<T>>> exprs) : m_exprs(std::move(exprs)) {}

    T evaluate() const override {
        return Op::apply(m_exprs);
    }
};

// Binary Operators
template<typename T>
struct Add {
    static T apply(T left, T right) {
        return left + right;
    }
};

template<typename T>
struct Subtract {
    static T apply(T left, T right) {
        return left - right;
    }
};

template<typename T>
struct Multiply {
    static T apply(T left, T right) {
        return left * right;
    }
};

template<typename T>
struct Divide {
    static T apply(T left, T right) {
        return left / right;
    }
};

template<typename T, std::size_t N>
struct Modulo {
    static T apply(T left, T right) {
        return left % right;
    }
};

template<typename T, std::size_t N>
struct Power {
    static T apply(T left, T right) {
        return std::pow(left, right);
    }
};





int main() {


    auto a = std::make_unique<Constant<int>>(5);
    auto b = std::make_unique<Constant<int>>(10);
    auto c = std::make_unique<Constant<int>>(15);

    auto d = std::make_unique<Binary<int, Add<int>>>(std::move(a), std::move(b));
    auto e = std::make_unique<Binary<int, Add<int>>>(std::move(d), std::move(c));

    std::cout << e->evaluate() << std::endl;


    return 0x0;
}