//
// Created by Nathan on 9/8/2024.
//

#ifndef BUFFALO2_H
#define BUFFALO2_H

#include <list>
#include <map>
#include <vector>

namespace bf
{
    enum class DefaultTokenType {};

    template<typename TokenType>
    struct Token {};

    template<typename ValueType, typename TokenType = DefaultTokenType>
    class Grammar
    {
        /*
         * TYPES
         */
        using ReasonerType = ValueType(*)(Token<TokenType>&);
        using TransductorType = ValueType(*)(std::vector<ValueType> const&);

        /*
         * FORWARD-DECLS
         */
        class ProductionRule;

        /*
         * TERMINALS
         */
        class Terminal
        {
            inline static std::size_t last_id_ = 0;

            std::size_t id_ = Terminal::last_id_++;

            TokenType type_;
            ReasonerType reasoner_;

        public:
            bool operator==(Terminal const &other) const
            {
                return this->id_ == other.id_;
            }

            Terminal(TokenType type, ReasonerType transductor) : type_(type), reasoner_(transductor) {}
            Terminal(ReasonerType transductor) : reasoner_(transductor) {}
        };

        template<typename SemanticType>
        struct TerminalDefinition
        {
            Terminal &terminal;

            SemanticType operator()(ValueType value) const
            {
                return std::get<SemanticType>(value);
            }
        };

        /*
         * NON-TERMINALS
         */
        class NonTerminal
        {
        public:
            std::vector<ProductionRule> rules;
        };

        template<typename SemanticType>
        struct NonTerminalDefinition
        {
            NonTerminal &nonterminal;

            NonTerminalDefinition &operator[](ProductionRule const &rule)
            {
                nonterminal.rules = { rule };

                return *this;
            }

            NonTerminalDefinition &operator[](std::vector<ProductionRule> const &rules)
            {
                nonterminal.rules = rules;

                return *this;
            }

            NonTerminalDefinition(NonTerminal &nonterminal) : nonterminal(nonterminal) {}
        };

        /*
         * PRODUCTION RULES
         */
        class ProductionRule
        {

        };

        /*
         * MEMBERS
         */
        std::list<Terminal> terminals_;
        std::list<NonTerminal> nonterminals_;

    protected:
        template<typename SemanticType>
        TerminalDefinition<SemanticType> MakeTerminal(ReasonerType reasoner)
        {
            return {
                .terminal = this->terminals_.emplace_back(reasoner)
            };
        }

        template<typename SemanticType>
        NonTerminalDefinition<SemanticType> MakeNonTerminal()
        {
            return this->nonterminals_.emplace_back();
        }

    public:
        virtual void Build() = 0;

        virtual ~Grammar() = default;
    };
}

#endif //BUFFALO2_H
