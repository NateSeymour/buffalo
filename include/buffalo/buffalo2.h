//
// Created by Nathan on 9/8/2024.
//

#ifndef BUFFALO2_H
#define BUFFALO2_H

#include <functional>
#include <variant>
#include <vector>
#include <map>
#include <set>
#include <stack>

namespace bf
{
    /*
     * Helper for std::visit provided by Andreas Fertig.
     * Apple-Clang 15 isn't C++23 compliant enough for the prettier solution, so C++17 style it is.
     * https://andreasfertig.blog/2023/07/visiting-a-stdvariant-safely/
     */
    template<class...>
    constexpr bool always_false_v = false;

    template<class... Ts>
    struct overload : Ts...
    {
        using Ts::operator()...;

        template<typename T>
        constexpr void operator()(T) const
        {
            static_assert(always_false_v<T>, "Unsupported type");
        }
    };

    template<class... Ts>
    overload(Ts...) -> overload<Ts...>;

    /*
     * TOKEN
     */
    template<typename TokenType>
    struct Token {};

    /*
     * GRAMMAR DEFINITION
     */
    template<typename T>
    concept IGrammar = requires(T)
    {
        /*
         * TYPES
         */
        typename T::ValueType;
        typename T::TokenType;
        typename T::ReasonerType;
        typename T::TransductorType;
    };

    template<typename GValueType, typename GTokenType>
    class GrammarDefinition
    {
    public:
        /*
         * TYPES
         */
        using ValueType = GValueType;
        using TokenType = GTokenType;
        using ReasonerType = ValueType(*)(Token<TokenType>&);
        using TransductorType = std::function<ValueType(std::vector<ValueType> const&)>;
    };

    /*
     * FORWARD-DECLS
     */
    template<IGrammar G>
    class Grammar;

    template<IGrammar G>
    class ProductionRule;

    /*
     * TERMINALS
     */
    template<IGrammar G>
    class Terminal
    {
        friend class Grammar<G>;

        typename G::TokenType type_;
        typename G::ReasonerType reasoner_ = nullptr;

    public:
        bool operator==(Terminal const &other) const
        {
            return this->type_ == other.type_;
        }

        bool operator<(Terminal const &other) const
        {
            return this->type_ < other.type_;
        }

        Terminal(typename G::TokenType const type) : type_(type) {}
        Terminal(typename G::TokenType const type, typename G:: ReasonerType reasoner) : type_(type), reasoner_(reasoner) {}
    };

    /*
     * NON-TERMINALS
     */
    template<IGrammar G>
    class NonTerminal
    {
        friend class Grammar<G>;

        inline static std::size_t last_id_ = 0;
        std::size_t id_ = NonTerminal::last_id_++;

        std::vector<ProductionRule<G>> rules_;

    public:
        // WARNING: This will soon cause many issues and headaches!!!
        NonTerminal &operator|(ProductionRule<G> const &rhs)
        {
            this->rules_.push_back(rhs);
            return *this;
        }

        bool operator==(NonTerminal const &other) const
        {
            return this->id_ == other.id_;
        }

        bool operator<(NonTerminal const &other) const
        {
            return this->id_ < other.id_;
        }

        NonTerminal() = default;

        NonTerminal(ProductionRule<G> const &rule) : rules_({rule}) {}
        NonTerminal(std::initializer_list<ProductionRule<G>> const &rules) : rules_(rules) {}
    };

    /*
     * PRODUCTION RULES
     */
    template<IGrammar G>
    class ProductionRule
    {
        friend class Grammar<G>;

        typename G::TransductorType transductor_;
        std::vector<std::variant<Terminal<G>, NonTerminal<G>*>> sequence_;

    public:
        ProductionRule &operator+(Terminal<G> const &rhs)
        {
            this->sequence_.push_back(rhs);

            return *this;
        }

        ProductionRule &operator+(NonTerminal<G> &rhs)
        {
            this->sequence_.push_back(&rhs);

            return *this;
        }

        ProductionRule &operator<=>(typename G::TransductorType tranductor)
        {
            this->transductor_ = tranductor;

            return *this;
        }

        ProductionRule(typename G::TokenType anonymous_token) : ProductionRule(Terminal<G>(anonymous_token)) {};
        ProductionRule(Terminal<G> const &terminal) : sequence_({ terminal }) {}
        ProductionRule(NonTerminal<G> &nonterminal) : sequence_({ &nonterminal }) {}
    };

    /*
     * GRAMMAR
     */
    template<IGrammar G>
    class Grammar
    {
        std::set<Terminal<G>> terminals_;
        std::set<NonTerminal<G>> nonterminals_;

        std::map<NonTerminal<G>, std::set<Terminal<G>>> first_;
        std::map<NonTerminal<G>, std::set<Terminal<G>>> follow_;

    public:
        void ProcessNonTerminalFirstSet(NonTerminal<G> &nonterminal)
        {
            for(auto &rule : nonterminal.rules_)
            {
                if(rule.sequence_.empty()) continue;

                auto &symbol = rule.sequence_[0];
                std::visit(overload{
                    [&](Terminal<G> terminal)
                    {
                        this->first_[nonterminal].insert(terminal);
                    },
                    [&](NonTerminal<G> *child_nonterminal)
                    {
                        if(nonterminal == *child_nonterminal) return;

                        this->ProcessNonTerminalFirstSet(*child_nonterminal);
                        this->first_[nonterminal].insert_range(this->first_[*child_nonterminal]);
                    }
                }, symbol);
            }
        }

        Grammar(NonTerminal<G> &start)
        {
            this->nonterminals_.insert(start);

            /*
             * GENERATE FIRST SET
             */
            this->ProcessNonTerminalFirstSet(start);
        }
    };

    /*
     * PRODUCTION RULE COMPOSITION FUNCTIONS
     */
    template<IGrammar G>
    ProductionRule<G> operator+(Terminal<G> &lhs, Terminal<G> &rhs)
    {
        return ProductionRule(lhs) + rhs;
    }

    template<IGrammar G>
    ProductionRule<G> operator+(Terminal<G> &lhs, NonTerminal<G> &rhs)
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

    /*
     * PARSER
     */
    template<IGrammar G>
    class Parser {};

    template<IGrammar G>
    class SLRParser : public Parser<G>
    {
    public:
        SLRParser(Grammar<G> grammar) {}
    };
}

#endif //BUFFALO2_H
