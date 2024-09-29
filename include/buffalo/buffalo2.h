#ifndef BUFFALO2_H
#define BUFFALO2_H

#include <functional>
#include <variant>
#include <expected>
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
     * GRAMMAR DEFINITION
     */
    template<typename T>
    concept IGrammar = requires(T)
    {
        /*
         * TYPES
         */
        typename T::ValueType;
        typename T::ReasonerType;
        typename T::TransductorType;
    };

    /*
     * FORWARD-DECLS
     */
    template<IGrammar G>
    class Grammar;

    template<IGrammar G>
    class ProductionRule;

    template<IGrammar G>
    class Terminal;

    template<IGrammar G>
    class NonTerminal;

    template<IGrammar G>
    struct LRItem;

    template<IGrammar G>
    struct LRState;

    /**
     * ERROR
     */
    class Error {};

    /**
     * LOCATION
     */
     struct Location
     {
        std::size_t begin;
        std::size_t end;
     };

    /**
     * TOKEN
     */
    struct Token
    {
        std::size_t terminal_id;
        Location location;

        std::size_t Size() const
        {
            return this->location.end - this->location.begin;
        }
    };

    /**
     * TOKENIZER
     * Virtual class to expose the Tokenizer API.
     */
    template<IGrammar G>
    class Tokenizer
    {
    public:
        using LexxerType = std::optional<Token>(*)(Terminal<G>*, std::string_view);

        /**
         * Optional method to register a terminal relationship to a lexxer.
         * Default implementation does nothing.
         * @param terminal
         * @param lexxer
         */
        virtual void RegisterTerminal(Terminal<G> *terminal, LexxerType lexxer) { /* Do Nothing */ }

        /**
         * Gets the first token on the input stream.
         * @param input
         * @return
         */
        virtual std::expected<Token, Error> First(std::string_view input) const = 0;

        /**
         * Helper to create a stream of tokens from string_view.
         */
        class TokenStream
        {
            std::size_t index_ = 0;
            std::string_view input_;

            Tokenizer<G> const &tokenizer_;

            std::deque<Token> buffer_;

        public:
            std::optional<Token> Peek(std::size_t lookahead = 1)
            {
                // Fill buffer to requested lookahead
                for(int i = 0; i < lookahead - buffer_.size(); i++)
                {
                    auto token = this->tokenizer_.First(this->input_);
                    if(!token)
                    {
                        return std::nullopt;
                    }

                    token->location.begin += this->index_;
                    token->location.end += this->index_;

                    this->index_ += token->Size();
                    this->input_ = this->input_.substr(token->Size());

                    this->buffer_.push_back(*token);
                }

                // Feed from buffer first
                if(this->buffer_.size() >= lookahead)
                {
                    return this->buffer_[lookahead - 1];
                }
            }

            std::optional<Token> Consume()
            {
                auto token = this->Peek(1);
                if(token)
                {
                    this->buffer_.pop_front();
                }

                return token;
            }

            TokenStream(Tokenizer<G> const &tokenizer, std::string_view input) : tokenizer_(tokenizer), input_(input) {}
        };

        /**
         * Generate stream of tokens from an std::string_view.
         * @param input
         * @return
         */
        TokenStream StreamInput(std::string_view input)
        {
            return TokenStream(*this, input);
        }
    };

    /**
     * GRAMMAR DEFINITION
     * @tparam GValueType
     */
    template<typename GValueType>
    class GrammarDefinition
    {
    public:
        /*
         * TYPES
         */
        using ValueType = GValueType;
        using ReasonerType = ValueType(*)(Token&);
        using TransductorType = std::function<ValueType(std::vector<ValueType> const&)>;
    };

    /**
     * STATIC IDENTIFIER
     * Provides a unique static identifier for all object holders.
     * NOTE: Only valid within the same translation unit!
     */
    class StaticIdentifier
    {
        inline static std::size_t last_id_ = 0;

    public:
        std::size_t id = StaticIdentifier::last_id_++;

        operator std::size_t() const
        {
            return this->id;
        }

        bool operator==(StaticIdentifier const &other) const
        {
            return this->id == other.id;
        }

        bool operator<(StaticIdentifier const &other) const
        {
            return this->id < other.id;
        }
    };

    /**
     * STATICALLY IDENTIFIED OBJECT
     */
    struct StaticallyIdentifiedObject
    {
        StaticIdentifier id;

        bool operator==(StaticallyIdentifiedObject const &other) const
        {
            return this->id == other.id;
        }

        bool operator<(StaticallyIdentifiedObject const &other) const
        {
            return this->id < other.id;
        }
    };

    /**
     * TERMINAL
     */
    template<IGrammar G>
    class Terminal : public StaticallyIdentifiedObject
    {
        friend class Grammar<G>;

        typename G::ReasonerType reasoner_ = nullptr;

    public:
        Terminal() = default;

        Terminal(Tokenizer<G> &tok, typename Tokenizer<G>::LexxerType lexxer = nullptr, typename G:: ReasonerType reasoner = nullptr) : reasoner_(reasoner)
        {
            tok.RegisterTerminal(this, lexxer);
        }
    };

    /**
     * NON-TERMINAL
     */
    template<IGrammar G>
    class NonTerminal : StaticallyIdentifiedObject
    {
        friend class Grammar<G>;
        friend struct LRState<G>;

    protected:
        std::vector<ProductionRule<G>> rules_;

    public:
        // WARNING: This will soon cause many issues and headaches!!!
        NonTerminal &operator|(ProductionRule<G> const &rhs)
        {
            this->rules_.push_back(rhs);
            return *this;
        }
        NonTerminal() = default;

        NonTerminal(ProductionRule<G> const &rule) : rules_({rule}) {}
        NonTerminal(std::initializer_list<ProductionRule<G>> const &rules) : rules_(rules) {}
    };

    /**
     * SYMBOL
     */
    template<IGrammar G>
    using Symbol = std::variant<Terminal<G>, NonTerminal<G>*>;

    /*
     * PRODUCTION RULES
     */
    template<IGrammar G>
    class ProductionRule
    {
        friend class Grammar<G>;
        friend struct LRItem<G>;

        typename G::TransductorType transductor_;
        std::vector<Symbol<G>> sequence_;

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

        ProductionRule(Terminal<G> const &terminal) : sequence_({ terminal }) {}
        ProductionRule(NonTerminal<G> &nonterminal) : sequence_({ &nonterminal }) {}
    };

    /*
     * GRAMMAR
     */
    template<IGrammar G>
    class Grammar
    {
        std::map<NonTerminal<G>*, std::set<Terminal<G>>> first_;
        std::map<NonTerminal<G>*, std::set<Terminal<G>>> follow_;

    public:
        NonTerminal<G> const &root;

        void ProcessNonTerminalFirstSet(NonTerminal<G> *nonterminal)
        {
            for(auto &rule : nonterminal->rules_)
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
                        if(nonterminal == child_nonterminal) return;

                        this->ProcessNonTerminalFirstSet(child_nonterminal);
                        this->first_[nonterminal].insert(this->first_[child_nonterminal].begin(), this->first_[child_nonterminal].end());
                    }
                }, symbol);
            }
        }

        void ProcessNonTerminalFollowSet()
        {
            bool has_change = false;

            do
            {
                for(auto &[nonterminal, follow_set] : this->follow_)
                {
                    for(auto const &rule : nonterminal->rules_)
                    {
                        for(int i = 0; i < rule.sequence_.size() - 1; i++)
                        {
                            auto &symbol = rule.sequence_[i];
                            auto &follow = rule.sequence_[i + 1];

                            // Skip over terminals
                            if(std::holds_alternative<Terminal<G>>(symbol)) continue;

                            // Add to NonTerminal FOLLOW set
                            auto observed_nonterminal = std::get<NonTerminal<G>*>(symbol);

                            has_change = std::visit(overload{
                                [&](Terminal<G> terminal)
                                {
                                    if(auto const [it, inserted] = this->follow_[observed_nonterminal].insert(terminal); inserted)
                                    {
                                        return true;
                                    }

                                    return false;
                                },
                                [&](NonTerminal<G> *child_nonterminal)
                                {
                                    std::size_t follow_set_size = this->follow_[observed_nonterminal].size();

                                    this->follow_[observed_nonterminal].insert(this->first_[child_nonterminal].begin(), this->first_[child_nonterminal].end());

                                    if(this->follow_[observed_nonterminal].size() > follow_set_size)
                                    {
                                        return true;
                                    }

                                    return false;
                                }
                            }, follow);
                        }
                    }
                }
            } while(has_change);
        }

        void RegisterAllSymbols(NonTerminal<G> *nonterminal)
        {
            this->first_[nonterminal] = {};
            this->follow_[nonterminal] = {};

            for(auto &rule : nonterminal->rules_)
            {
                for(auto &symbol : rule.sequence_)
                {
                    std::visit(overload{
                        [](Terminal<G> terminal) { /* Do Nothing */ },
                        [&](NonTerminal<G> *child_nonterminal)
                        {
                            if(nonterminal == child_nonterminal) return;

                            this->RegisterAllSymbols(child_nonterminal);
                        }
                    }, symbol);
                }
            }
        }

        Grammar(NonTerminal<G> &start) : root(start)
        {
            this->RegisterAllSymbols(&start);
            this->ProcessNonTerminalFirstSet(&start);
            this->ProcessNonTerminalFollowSet();
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

    /**
     * LR ITEM
     * @tparam G
     */
    template<IGrammar G>
    struct LRItem
    {
        ProductionRule<G> const *rule;
        std::size_t position;

        [[nodiscard]] bool Complete() const
        {
            return this->position >= this->rule->sequence_.size();
        }

        [[nodiscard]] LRItem Advance() const
        {
            return LRItem(this->rule, this->position + 1);
        }

        [[nodiscard]] Symbol<G> NextSymbol() const
        {
            return this->rule->sequence_[this->position];
        }

        bool operator==(LRItem const &other) const
        {
            return this->rule == other.rule && this->position == other.position;
        }

        bool operator<(LRItem const &other) const
        {
            return this->rule == other.rule && this->position < other.position;
        }

        LRItem(ProductionRule<G> const *rule, int position = 0) : rule(rule), position(position) {}

        LRItem() = delete;
    };

    /**
     * LR State
     * @tparam G
     */
    template<IGrammar G>
    struct LRState
    {
        std::vector<LRItem<G>> kernel_items;

        std::vector<LRItem<G>> GenerateClosure() const
        {
            std::vector<LRItem<G>> closure = this->kernel_items;
            std::set<NonTerminal<G>*> closed_nonterminals;

            for(int i = 0; i < closure.size(); i++)
            {
                if(closure[i].Complete()) continue;

                std::visit(overload{
                    [](Terminal<G> terminal) { /* Do Nothing */ },
                    [&](NonTerminal<G> *non_terminal)
                    {
                        if(closed_nonterminals.contains(non_terminal)) return;
                        closed_nonterminals.insert(non_terminal);

                        for(auto const &rule : non_terminal->rules_)
                        {
                            closure.emplace_back(&rule);
                        }
                    }
                }, closure[i].NextSymbol());
            }

            return closure;
        }

        std::map<Symbol<G>, LRState<G>> GenerateTransitions() const
        {
            std::map<Symbol<G>, LRState<G>> transitions;

            auto closure = this->GenerateClosure();

            for(auto const &item : closure)
            {
                if(item.Complete()) continue;

                transitions[item.NextSymbol()].kernel_items.push_back(item.Advance());
            }

            return transitions;
        }

        /**
         * This is very slow and needs to be remedied.
         * @param other
         * @return
         */
        bool operator==(LRState const &other) const
        {
            if(this->kernel_items.size() != other.kernel_items.size()) return false;

            for(int i = 0; i < this->kernel_items.size(); i++)
            {
                if(this->kernel_items[i] != other.kernel_items[i]) return false;
            }

            return true;
        }

        bool operator<(LRState const &other) const
        {
            if(this->kernel_items.size() < other.kernel_items.size()) return true;

            for(int i = 0; i < this->kernel_items.size(); i++)
            {
                if(this->kernel_items[i] < other.kernel_items[i]) return true;
            }

            return false;
        }

        LRState(NonTerminal<G> const *start)
        {
            for(auto const &rule : start->rules_)
            {
                this->kernel_items.emplace_back(&rule);
            }
        }

        LRState() = default;
    };

    /**
     * PARSER
     * @tparam G
     */
    template<IGrammar G>
    class Parser
    {
    public:
        virtual std::expected<typename G::ValueType, Error> Parse(std::string_view input) = 0;
    };

    /**
     * SLR PARSER
     * @tparam G
     */
    template<IGrammar G>
    class SLRParser : public Parser<G>
    {
        Tokenizer<G> const &tokenizer_;
        Grammar<G> const &grammar_;

        std::map<LRState<G>, std::map<Symbol<G>, LRState<G>>> lr0_fsm_;

    public:
        std::expected<typename G::ValueType, Error> Parse(std::string_view input) override
        {
        }

        void GenerateStates(LRState<G> const &state)
        {
            auto transitions = state.GenerateTransitions();
            this->lr0_fsm_[state] = transitions;

            for(auto const &[symbol, new_state] : transitions)
            {
                if(state == new_state) continue;

                this->GenerateStates(new_state);
            }
        }

        SLRParser(Tokenizer<G> const &tokenizer, Grammar<G> const &grammar) : tokenizer_(tokenizer), grammar_(grammar)
        {
            // Generate parser tables
            LRState<G> root_state(&grammar.root);
            this->GenerateStates(root_state);
        }
    };
}

#endif //BUFFALO2_H
