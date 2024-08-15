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
    };
#pragma endregion

#pragma region Forwards Decls
    template<IGrammar G>
    struct LRState;

    template<IGrammar G>
    class Parser;

    template<IGrammar G>
    class SLRParser;

    template<IGrammar G>
    class Symbol;

    template<IGrammar G>
    class Terminal;

    template<IGrammar G>
    class NonTerminal;
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

    template<typename T>
    class Grammar
    {
    protected:
        id_t symbol_count = 1;

    public:
        using ValueType = T;
        using TransductorType = std::function<T(std::vector<T> const&)>;

        id_t SymbolId()
        {
            return this->symbol_count++;
        }

        const id_t epsilon = 0;

        const int x;

        consteval Grammar() : x(0)
        {
        }
    };

    template<typename T, std::size_t terminal_count, std::size_t nonterminal_count>
    class DefineGrammar
    {

    public:
        consteval DefineGrammar()
        {

        }
    };

    enum class SymbolType
    {
        kTerminal,
        kNonTerminal,
    };

    template<IGrammar G>
    class Symbol
    {
    public:
        const id_t id = 0; //G::SymbolId();
        const SymbolType symbol_type;

        consteval Symbol(SymbolType symbol_type) : symbol_type(symbol_type) {}
    };

    template<IGrammar G>
    class Terminal : public Symbol<G>
    {
    public:
        [[nodiscard]] virtual G::ValueType SemanticValue(Token const &token) const = 0;

        [[nodiscard]] virtual constexpr std::optional<Token> Match(std::string_view input) const = 0;

        consteval Terminal() : Symbol<G>(SymbolType::kTerminal) {}
    };

    template<IGrammar G, ctll::fixed_string pattern, typename SemanticType>
    class DefineTerminal : public Terminal<G>
    {
        using SemanticReasonerType = SemanticType(*)(Token const &);
        SemanticReasonerType semantic_reasoner_;

    public:
        [[nodiscard]] G::ValueType SemanticValue(Token const &token) const override
        {
            return this->semantic_reasoner_(token);
        }

        [[nodiscard]] constexpr std::optional<Token> Match(std::string_view input) const override
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

        [[nodiscard]] SemanticType operator()(G::ValueType const &value)
        {
            return std::get<SemanticType>(value);
        }

        consteval explicit DefineTerminal(SemanticReasonerType semantic_reasoner) : semantic_reasoner_(semantic_reasoner) {}
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
        friend struct LRState<G>;
        friend class Parser<G>;
        friend class SLRParser<G>;

    protected:
        std::optional<typename G::TransductorType> tranductor_ = std::nullopt;

        std::vector<Symbol<G>*> parse_sequence_;

    public:
        template<typename F>
        ProductionRule<G> operator<=>(F transductor)
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
    class NonTerminal : public Symbol<G>
    {
        friend struct LRState<G>;
        friend class Parser<G>;
        friend class SLRParser<G>;

    protected:
        std::vector<ProductionRule<G>> rules_;

    public:
        NonTerminal<G> &operator|(ProductionRule<G> const &rhs)
        {
            this->rules_.push_back(rhs);
            return *this;
        }

        NonTerminal(ProductionRule<G> const &rule) : Symbol<G>(SymbolType::kNonTerminal), rules_({rule}) {}
        NonTerminal(std::initializer_list<ProductionRule<G>> const &rules) : Symbol<G>(SymbolType::kNonTerminal), rules_(rules) {}
    };

    template<IGrammar G, typename SemanticType, std::size_t rule_count>
    class DefineNonTerminal : public NonTerminal<G>
    {

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

    enum class ActionType
    {
        kShift,
        kReduce,
        kGoto,
        kAccept,
    };

    struct Action
    {
        ActionType type;
    };

    template<IGrammar G>
    struct LRState
    {
        struct Item
        {
            ProductionRule<G> const *rule;
            int position;

            [[nodiscard]] bool Complete() const
            {
                return this->position >= this->rule->parse_sequence_.size();
            }

            [[nodiscard]] Item Advance() const
            {
                return Item(this->rule, this->position + 1);
            }

            [[nodiscard]] Symbol<G> *NextSymbol() const
            {
                return this->rule->parse_sequence_[this->position];
            }

            Item() = delete;

            explicit Item(ProductionRule<G> const *rule, int position = 0) : rule(rule), position(position) {}
        };

        std::map<id_t, Action> actions;

        std::vector<Item> items;

        std::vector<Item> Closure()
        {
            std::vector<Item> closure;
            std::set<id_t> closed_nonterminals;

            for(int i = 0; i < items.size(); i++)
            {
                Item const &item = this->items[i];

                if(item.Complete()) continue;

                Symbol<G> *symbol = item.NextSymbol();
                if(symbol->symbol_type == SymbolType::kTerminal) continue;
                if(closed_nonterminals.contains(symbol->id)) continue;

                closed_nonterminals.insert(symbol->id);

                auto *nonterminal = reinterpret_cast<NonTerminal<G>*>(symbol);
                for(ProductionRule<G> const &rule : nonterminal->rules_)
                {
                    this->items.emplace_back(&rule);
                }
            }

            return closure;
        }

        std::map<Symbol<G>*, LRState<G>> Transitions()
        {
            /*
             * 1. Get set of all lookaheads
             * 2. Generate transition + state for each lookahead, which will contain the advanced rules items from this (closed) state
             */
            std::map<Symbol<G>*, LRState<G>> transitions;

            // Get set of all lookaheads
            auto closure = this->Closure();

            for(Item const &item : closure)
            {
                if(item.Comlete()) continue;
                if(!transitions.contains(item.NextSymbol()))
                {
                    transitions[item.NextSymbol()] = LRState<G>{};
                }

                transitions[item.NextSymbol()].items.push_back(item.Advance());
            }

            return transitions;
        }

        LRState(NonTerminal<G> const &nonterminal)
        {
            for(ProductionRule<G> const &rule : nonterminal.rules_)
            {
                this->items.emplace_back(&rule);
            }
        }
    };

    template<IGrammar G>
    class Parser
    {
    public:
        virtual std::expected<typename G::ValueType, ParseError> Parse(std::string_view input) const = 0;
    };

    template<IGrammar G>
    class SLRParser : public Parser<G>
    {
        Tokenizer<G> const &tok_;

        std::vector<LRState<G>> states_;

    public:
        std::expected<typename G::ValueType, ParseError> Parse(std::string_view input) const override
        {
        }

        SLRParser(Tokenizer<G> const &tok, NonTerminal<G> const &start) : tok_(tok)
        {

            LRState<G> &start_state = this->states_.emplace_back(start);
        }
    };
}

#endif //BUFFALO_H
