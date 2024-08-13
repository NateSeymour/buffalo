#ifndef BUFFALO_H
#define BUFFALO_H

#include <algorithm>
#include <vector>
#include <deque>
#include <optional>
#include <expected>
#include <span>
#include <ranges>
#include <string_view>
#include <functional>
#include <cctype>
#include <variant>
#include <stack>
#include <ctre.hpp>

// TODO: append_range instead of insert_range

namespace buffalo
{
    using id_t = std::size_t;

#pragma region Concepts
    template<typename T>
    concept IGrammar = requires(T)
    {
        typename T::ValueType;
        typename T::TransductorType;
        typename T::SemanticReasonerType;
        { T::SymbolId() } -> std::same_as<id_t>;
    };
#pragma endregion

#pragma region Forwards Decls
    template<IGrammar G>
    class Parser;

    template<IGrammar G>
    class NonTerminal;

    template<IGrammar G>
    class Terminal;
#pragma endregion

#pragma region std::visit Hackery
    /**
     * Helper for std::visit provided by Andreas Fertig.
     * Apple-Clang 15 isn't C++23 compliant enough for the prettier solution, so C++17 style it is.
     * https://andreasfertig.blog/2023/07/visiting-a-stdvariant-safely/
     * @tparam Ts
     */
    template<class...>
    constexpr bool always_false_v = false;

    template<class... Ts>
    struct overload : Ts...
    {
        using Ts::operator()...;

        // Prevent implicit type conversions
        template<typename T>
        constexpr void operator()(T) const
        {
            static_assert(always_false_v<T>, "Unsupported type");
        }
    };

    template<class... Ts>
    overload(Ts...) -> overload<Ts...>;
#pragma endregion

#pragma region Error Types
    class Error
    {
        char const *error_;

    public:
        Error(char const *error) : error_(error) {}
    };

    class ParseError : public Error {};

    class LexError : public Error
    {
    public:
        LexError(char const *error) : Error(error) {}
    };
#pragma endregion

    struct Token
    {
        id_t type;
        int begin = -1;
        int end = -1;
        std::string_view raw;
    };

    template<ctll::fixed_string name, typename T>
    class Grammar
    {
    public:
        // Forward decls
        class Terminal;

    protected:
        static inline id_t symbol_count = 0;

    public:
        using ValueType = T;
        using TransductorType = std::function<T(std::vector<T> const&)>;
        using SemanticReasonerType = std::function<T(Token const&)>;

        static id_t SymbolId()
        {
            return symbol_count++;
        }
    };

    template<IGrammar G>
    class Terminal
    {
    protected:
        G::SemanticReasonerType semantic_reasoner_;

    public:
        const id_t id = G::SymbolId();

        [[nodiscard]] virtual constexpr std::optional<Token> Match(std::string_view input) const = 0;

        explicit Terminal(G::SemanticReasonerType semantic_reasoner) : semantic_reasoner_(semantic_reasoner) {}
    };

    template<IGrammar G, ctll::fixed_string pattern, typename SemanticType>
    class DefineTerminal : public Terminal<G>
    {
    public:
        constexpr std::optional<Token> Match(std::string_view input) const override
        {
            auto match = ctre::starts_with<pattern>(input);
            if(match)
            {
                auto raw = match.to_view();

                return Token {
                        .type = this->id,
                        .begin = 0,
                        .end = (int)raw.size(),
                        .raw = raw,
                };
            }

            return std::nullopt;
        }

        SemanticType operator()(G::ValueType const &value)
        {
            return std::get<SemanticType>(value);
        }

        DefineTerminal(G::SemanticReasonerType semantic_reasoner) : Terminal<G>(semantic_reasoner) {}
    };

    template<IGrammar G>
    class Tokenizer
    {
    protected:
        const std::initializer_list<Terminal<G> const *> terminals_;

    public:
        class TokenStream
        {
            Tokenizer<G> const &tokenizer_;
            std::string_view input_;
            std::deque<Token> buffer_;
            std::size_t index_ = 0;

        public:
            std::expected<Token, LexError> ReadNext()
            {
                // Clear whitespace
                while(std::isspace(this->input_[0]))
                {
                    this->input_ = this->input_.substr(1);
                    this->index_++;
                }

                // Do terminal matching
                for(auto terminal : this->tokenizer_.terminals_)
                {
                    if(auto match = terminal->Match(this->input_))
                    {
                        this->input_ = this->input_.substr(match->end);

                        match->begin += this->index_;
                        match->end += this->index_;
                        int match_length = match->end - match->begin;

                        return *match;
                    }
                }

                return std::unexpected("End of token stream!");
            }

            std::expected<Token, LexError> Consume()
            {
                if(!this->buffer_.empty())
                {
                    Token token = this->buffer_[0];
                    this->buffer_.pop_front();

                    return token;
                }

                return this->ReadNext();
            }

            std::expected<Token, LexError> Peek(std::size_t lookahead = 0)
            {
                // Fill buffer to desired location
                while(this->buffer_.size() <= lookahead)
                {
                    this->buffer_.push_back(*this->ReadNext());
                }

                return this->buffer_[lookahead];
            }

            TokenStream(Tokenizer<G> const &tokenizer, std::string_view input) : tokenizer_(tokenizer), input_(input) {}
        };

        TokenStream Stream(std::string_view input) const
        {
            return TokenStream(*this, input);
        }

        Tokenizer(std::initializer_list<Terminal<G> const *> terminals) : terminals_(terminals) {}
    };

    template<IGrammar G>
    class ProductionRule
    {
    public:
        using SymbolType = std::variant<Terminal<G>*, NonTerminal<G>*>;

    protected:
        std::optional<typename G::TransductorType> tranductor_ = std::nullopt;

        std::vector<SymbolType> parse_sequence_;

    public:
        template<typename F>
        ProductionRule<G> &operator<=>(F transductor)
        {
            this->tranductor_ = transductor;
            return *this;
        }

        ProductionRule<G> &operator+(Terminal<G> &rhs)
        {
            this->parse_sequence_.push_back(&rhs);

            return *this;
        }

        ProductionRule<G> &operator+(NonTerminal<G> &rhs)
        {
            this->parse_sequence_.push_back(&rhs);

            return *this;
        }

        std::optional<typename G::ValueType> Produce(Tokenizer<G>::TokenStream &stream)
        {
            //static_assert(false, "unimplemented");
            return std::nullopt;
        }

        ProductionRule(Terminal<G> &start)
        {
            this->parse_sequence_.push_back(&start);
        }

        ProductionRule(NonTerminal<G> &start)
        {
            this->parse_sequence_.push_back(&start);
        }
    };

    template<IGrammar G>
    class NonTerminal
    {
        friend class Parser<G>;

    protected:
        std::vector<ProductionRule<G>> rules_;

    public:
        const id_t id = G::SymbolId();

        NonTerminal<G> &operator|(ProductionRule<G> const &rhs)
        {
            this->rules_.push_back(rhs);
            return *this;
        }

        NonTerminal(ProductionRule<G> const &rule) : rules_({rule}) {}
        NonTerminal(std::initializer_list<ProductionRule<G>> const &rules) : rules_(rules) {}
    };

#pragma region ProductionRule Composition Functions
    template<IGrammar G>
    ProductionRule<G> operator+(Terminal<G> &lhs, Terminal<G> &rhs)
    {
        return ProductionRule(lhs) + rhs;
    }

    template<IGrammar G>
    ProductionRule<G> operator+(Terminal<G> &lhs, NonTerminal<G> *rhs)
    {
        return ProductionRule(lhs) + rhs;
    }

    template<IGrammar G>
    ProductionRule<G> operator+(NonTerminal<G> &lhs, NonTerminal<G> &rhs)
    {
        return ProductionRule(lhs) + rhs;
    }

    template<IGrammar G>
    ProductionRule<G> operator+(NonTerminal<G> &lhs, Terminal<G> &rhs)
    {
        return ProductionRule(lhs) + rhs;
    }

    template<IGrammar G>
    NonTerminal<G> operator|(ProductionRule<G> const &lhs, ProductionRule<G> const &rhs)
    {
        return {lhs, rhs};
    }
#pragma endregion

    template<IGrammar G>
    class Parser
    {
        using SymbolType = ProductionRule<G>::SymbolType;

        Tokenizer<G> const &tokenizer_;
        NonTerminal<G> const &start_;

        enum class ActionType
        {
            kShift,
            kReduce,
            kAccept,
        };

        struct Item
        {
            ProductionRule<G> const *rule;
            std::size_t index;

            bool FullyMatched() const
            {
                return this->index > this->rule->parse_sequence_.size() - 1;
            }

            SymbolType const &NextSymbol() const
            {
                return this->rule->parse_sequence_[this->index];
            }

            Item Advance() const
            {
                return Item(this->rule, this->index + 1);
            }
        };

        struct State
        {
            std::optional<id_t> self = std::nullopt;
            std::vector<Item> items;

            void Append(State const &state)
            {
                this->branches.insert_range(items.end(), state.branches);
            }

            void Close()
            {
                for(auto const &item : this->items)
                {
                    if(!item.FullyMatched())
                    {
                        std::visit(overload{
                                [&](NonTerminal<G> *nonterminal)
                                {
                                    if(this->self && nonterminal->id == *self) return;

                                    State state(*nonterminal);
                                    state.Close();

                                    this->Append(state);
                                },
                                [](Terminal<G> *none) {},
                        }, item.NextSymbol());
                    }
                }
            }

            State() = default;

            State(NonTerminal<G> const &nonterminal)
            {
                this->self = nonterminal.id;

                for(auto const &rule : nonterminal.rules_)
                {
                    this->items.push_back({
                        .rule = &rule,
                        .index = 0,
                    });
                }
            }
        };

        struct Transition
        {
            Item from;
            std::shared_ptr<State> to;
            id_t on;
        };

        std::vector<Transition> transitions_;
        std::vector<State> states_;

        // void goto_;
        // void action_;

    public:
        /**
         * @param input
         * @return
         */
        std::expected<typename G::ValueType, ParseError> Parse(std::string_view input) const
        {
            using StackType = std::variant<Token, NonTerminal<G>>;

            auto stream = this->tokenizer_.Stream(input);
            std::stack<StackType> stack;

            // to shift we call stream.Next();
        }

        Parser(Tokenizer<G> const &tokenizer, NonTerminal<G> const &start) : tokenizer_(tokenizer), start_(start)
        {
            /*
             * Steps:
             *  1. Generate States
             *  2.
             */
            // Generate first state
            auto &first_state = this->states_.emplace_back(start);
            first_state.Close();

            for(int i = 0; i < this->states_.size(); i++)
            {
            }
        }
    };
}

#endif //BUFFALO_H
