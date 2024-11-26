#ifndef BUFFALO2_H
#define BUFFALO2_H

#include <algorithm>
#include <cctype>
#include <expected>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <stack>
#include <stdexcept>
#include <variant>
#include <vector>
#include <ctre.hpp>
#include <ctll.hpp>

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
        typename T::ValueType;
        typename T::UserDataType;
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

    template<IGrammar G, ctll::fixed_string regex, typename SemanticType>
    class DefineTerminal;

    template<IGrammar G>
    class NonTerminal;

    template<IGrammar G, typename SemanticType>
    class DefineNonTerminal;

    template<IGrammar G>
    struct LRItem;

    template<IGrammar G>
    struct LRState;

    template<IGrammar G>
    class Parser;

    template<IGrammar G>
    class SLRParser;

    /**
     * LOCATION
     * Represents the location of a string of text in `buffer`.
     * `begin` and `end` represent iterator positions in `buffer`. Each iterator position is in between two characters.
     */
    struct Location
    {
        std::string_view buffer;
        std::size_t begin;
        std::size_t end;

        /**
         *
         * @param padding Amount of characters before and after snippet to print.
         * @return
         */
        std::string SnippetString(std::size_t padding = 10)
        {
            return "";
        }
    };

    struct Error
    {
        std::string message;

        Error(std::string message) : message(std::move(message)) {}
    };

    struct GrammarDefinitionError : Error
    {
        GrammarDefinitionError(std::string message) : Error(std::move(message)) {}
    };

    struct ParsingError : Error
    {
        Location location;

        ParsingError(Location location, std::string message) : Error(std::move(message)), location(location) {}
    };

    /**
     * PRODUCTION RULE LIST
     * @tparam G
     */
    template<IGrammar G>
    struct ProductionRuleList
    {
        std::vector<ProductionRule<G>> rules;

        ProductionRuleList &operator|(ProductionRule<G> const &rule)
        {
            this->rules.push_back(rule);

            return *this;
        }
    };

    /**
     * TOKEN
     */
    template<IGrammar G>
    struct Token
    {
        Terminal<G> *terminal;
        std::string_view raw;
        Location location;

        std::size_t Size() const
        {
            return this->location.end - this->location.begin;
        }
    };

    /**
     * Empty dummy struct to act as default for Grammar user data.
     */
    struct Dummy {};

    /**
     * GRAMMAR DEFINITION
     * @tparam GValueType
     */
    template<typename GValueType, typename GUserDataType = Dummy>
    requires std::constructible_from<GValueType>
    class GrammarDefinition
    {
    public:
        /*
         * TYPES
         */
        using ValueType = GValueType;
        using UserDataType = GUserDataType;
    };

    /**
     * Struct to hold debug symbol names.
     */
    struct DebugSymbol
    {
        char const *debug_name = "Generic Symbol";
    };

    /**
     * Used by the parser to resolve shift-reduce conflicts when terminals have the same precedence.
     */
    enum Associativity
    {
        None,
        Left,
        Right,
    };

    /**
     * TERMINAL
     */
    template<IGrammar G>
    class Terminal : public DebugSymbol
    {
        friend class Grammar<G>;

    public:
        using ReasonerType = typename G::ValueType(*)(Token<G> const&);

    protected:
        inline static std::size_t counter = 0;

        ReasonerType reasoner_ = nullptr;

        Terminal() = default;

    public:
        std::size_t precedence = Terminal::counter++;
        Associativity associativity = Associativity::None;
        typename G::UserDataType user_data;

        std::optional<typename G::ValueType> Reason(Token<G> const &token) const
        {
            if(this->reasoner_)
            {
                return this->reasoner_(token);
            }

            return std::nullopt;
        }

        virtual std::optional<Token<G>> Lex(std::string_view input) const
        {
            return std::nullopt;
        }

        Terminal(Terminal<G>  &&) = delete;
        Terminal(Terminal<G> const &) = delete;
    };

    /**
     * DEFINE TERMINAL
     * @tparam G
     */
    template<IGrammar G, ctll::fixed_string regex, typename SemanticType = void>
    class DefineTerminal : public Terminal<G>
    {
    public:
        SemanticType operator()(typename G::ValueType &value)
        {
            if constexpr(std::variant_size<typename G::ValueType>::value != 0)
            {
                if(!std::holds_alternative<SemanticType>(value))
                {
                    throw std::runtime_error("failed to convert type");
                }

                return std::move(std::get<SemanticType>(value));
            }
            else
            {
                return std::move(reinterpret_cast<SemanticType>(value));
            }
        }

        std::optional<Token<G>> Lex(std::string_view input) const override
        {
            auto match = ctre::starts_with<regex>(input);

            if(!match)
            {
                return std::nullopt;
            }

            return Token<G> {
                    .terminal = (Terminal<G>*)this,
                    .raw = match.view(),
                    .location = {
                            .begin = 0,
                            .end = match.size(),
                    },
            };
        }

        constexpr DefineTerminal(Associativity assoc = bf::None, typename G::UserDataType user_data = {}, typename Terminal<G>::ReasonerType reasoner = nullptr)
        {
            this->associativity = assoc;
            this->user_data = user_data;
            this->reasoner_ = reasoner;
        }

        constexpr DefineTerminal(Associativity assoc, typename Terminal<G>::ReasonerType reasoner) : DefineTerminal(assoc, {}, reasoner) {}

        constexpr DefineTerminal(typename G::UserDataType user_data) : DefineTerminal(bf::None, user_data, nullptr) {}
        constexpr DefineTerminal(typename G::UserDataType user_data, typename Terminal<G>::ReasonerType reasoner) : DefineTerminal(bf::None, user_data, reasoner) {}

        constexpr DefineTerminal(typename Terminal<G>::ReasonerType reasoner) : DefineTerminal(bf::None, {}, reasoner) {}
    };

    /**
     * NON-TERMINAL
     */
    template<IGrammar G>
    class NonTerminal
    {
        friend class Grammar<G>;
        friend struct LRState<G>;

    public:
        using TransductorType = typename G::ValueType(*)(std::vector<typename G::ValueType> &);

    protected:
        std::vector<ProductionRule<G>> rules_;

    public:
        NonTerminal(NonTerminal  &) = delete;
        NonTerminal(NonTerminal &&) = delete;

        NonTerminal() = default;

        NonTerminal(ProductionRule<G> const &rule) : rules_({rule}) {}
        NonTerminal(ProductionRuleList<G> const &rule_list) : rules_(rule_list.rules) {}
    };

    /**
     * DEFINE NON-TERMINAL
     */
    template<IGrammar G, typename SemanticValue = void>
    class DefineNonTerminal : public NonTerminal<G>
    {
    public:
        SemanticValue operator()(typename G::ValueType &value)
        {
            if constexpr(std::variant_size<typename G::ValueType>::value != 0)
            {
                if(!std::holds_alternative<SemanticValue>(value))
                {
                    throw std::runtime_error("failed to convert type");
                }

                return std::move(std::get<SemanticValue>(value));
            }
            else
            {
                return std::move(reinterpret_cast<SemanticValue>(value));
            }
        }

        DefineNonTerminal() = delete;

        DefineNonTerminal(ProductionRule<G> const &rule) : NonTerminal<G>(rule) {}
        DefineNonTerminal(ProductionRuleList<G> const &rule_list) : NonTerminal<G>(rule_list) {}
    };

    /**
     * SYMBOL
     */
    template<IGrammar G>
    using Symbol = std::variant<Terminal<G>*, NonTerminal<G>*>;

    /*
     * PRODUCTION RULES
     */
    template<IGrammar G>
    class ProductionRule
    {
        friend class Grammar<G>;
        friend struct LRItem<G>;
        friend class Parser<G>;
        friend class SLRParser<G>;

    protected:
        typename NonTerminal<G>::TransductorType transductor_ = nullptr;
        std::vector<Symbol<G>> sequence_;
        std::size_t precedence = -1;

        NonTerminal<G> *non_terminal_ = nullptr;

    public:
        ProductionRule &operator+(Terminal<G> &rhs)
        {
            this->sequence_.push_back(&rhs);

            return *this;
        }

        ProductionRule &operator+(NonTerminal<G> &rhs)
        {
            this->sequence_.push_back(&rhs);

            return *this;
        }

        ProductionRule &operator<=>(typename NonTerminal<G>::TransductorType tranductor)
        {
            this->transductor_ = tranductor;

            return *this;
        }

        bool operator==(ProductionRule<G> const &other) const
        {
            if(this->non_terminal_ && other.non_terminal_)
            {
                if(this->non_terminal_ != other.non_terminal_) return false;
            }
            else if(this->non_terminal_ != other.non_terminal_)
            {
                return false;
            }

            if(this->sequence_.size() != other.sequence_.size()) return false;

            for(auto const &[first, second] : std::views::zip(this->sequence_, other.sequence_))
            {
                if(first != second) return false;
            }

            return true;
        }

        std::optional<typename G::ValueType> Transduce(std::vector<typename G::ValueType> &args) const
        {
            if(this->transductor_)
            {
                return std::move(this->transductor_(args));
            }

            return std::nullopt;
        }

        ProductionRule(Terminal<G>       &terminal) : sequence_({ &terminal }) {}
        ProductionRule(NonTerminal<G> &nonterminal) : sequence_({ &nonterminal }) {}
    };

    template<IGrammar G>
    using PR = ProductionRule<G>;

    /*
     * GRAMMAR
     */
    template<IGrammar G>
    class Grammar
    {
        friend class Parser<G>;
        friend class SLRParser<G>;

    protected:
        /**
         * Unique pointer to an EOS terminal for this grammar. This allows the grammar to be moved while still
         * retaining the ability to perform pointer comparison on the EOS terminal.
         */
        std::unique_ptr<Terminal<G>> EOS;

        std::set<NonTerminal<G>*> nonterminals_;
        std::set<Terminal<G>*> terminals_;

        std::map<NonTerminal<G>*, std::set<Terminal<G>*>> first_;
        std::map<NonTerminal<G>*, std::set<Terminal<G>*>> follow_;

        /// List of all production rules along with their respective NonTerminal
        std::vector<std::pair<NonTerminal<G>*, ProductionRule<G>>> production_rules_;

    public:
        NonTerminal<G> &root;

        bool HasNonTerminal(NonTerminal<G> &non_terminal) const
        {
            return this->nonterminals_.contains(&non_terminal);
        }

        bool NonTerminalHasFollow(NonTerminal<G> &non_terminal, Terminal<G> &terminal) const
        {
            return this->follow_.at(&non_terminal).contains(&terminal);
        }

        bool NonTerminalHasFirst(NonTerminal<G> &non_terminal, Terminal<G> &terminal) const
        {
            return this->first_.at(&non_terminal).contains(&terminal);
        }

        /**
         * Simple, but somewhat inefficient algorithm for generating FIRST sets.
         * The FIRST set is the set of all Terminals that a NonTerminal can begin with.
         */
        void GenerateFirstSet()
        {
            bool has_change;
            do
            {
                has_change = false;

                for(auto const &[nonterminal, rule] : this->production_rules_)
                {
                    if(rule.sequence_.empty()) continue;

                    has_change |= std::visit(overload{
                        [&](Terminal<G> *terminal)
                        {
                            auto [it, inserted] = this->first_[nonterminal].insert(terminal);
                            return inserted;
                        },
                        [&](NonTerminal<G> *child_nonterminal)
                        {
                            if(child_nonterminal == nonterminal)
                            {
                                return false;
                            }

                            auto &parent_first = this->first_[nonterminal];
                            auto &child_first = this->first_[child_nonterminal];

                            std::size_t parent_size = parent_first.size();
                            parent_first.insert(child_first.begin(), child_first.end());

                            return parent_size != parent_first.size();
                        }
                    }, rule.sequence_[0]);
                }
            } while(has_change);
        }

        /**
         * Simple, but somewhat inefficient algorithm for generating FOLLOW sets.
         * The FOLLOW set is the set of all Terminals that can follow a NonTerminal.
         */
        void GenerateFollowSet()
        {
            this->follow_[&root] = { this->EOS.get() };

            bool has_change;
            do
            {
                has_change = false;

                for(auto &[nonterminal, rule] : this->production_rules_)
                {
                    for(int i = 0; i < rule.sequence_.size(); i++)
                    {
                        // Skip over Terminals
                        if(std::holds_alternative<Terminal<G>*>(rule.sequence_[i])) continue;

                        // Process NonTerminal
                        auto symbol = std::get<NonTerminal<G>*>(rule.sequence_[i]);

                        // If this is the last NonTerminal in the sequence, then it gets all of the FOLLOW of parent.
                        if(i == rule.sequence_.size() - 1)
                        {
                            auto &parent_follow = this->follow_[nonterminal];
                            auto &child_follow = this->follow_[symbol];

                            std::size_t child_size = child_follow.size();
                            child_follow.insert(parent_follow.begin(), parent_follow.end());

                            has_change = child_size != child_follow.size();
                            continue;
                        }

                        // ELSE process the next token
                        auto follow = rule.sequence_[i + 1];

                        has_change |= std::visit(overload{
                            [&](Terminal<G> *terminal)
                            {
                                auto [it, inserted] = this->follow_[symbol].insert(terminal);
                                return inserted;
                            },
                            [&](NonTerminal<G> *follow_nonterminal)
                            {
                                auto &symbol_follow = this->follow_[symbol];
                                auto &follow_first = this->first_[follow_nonterminal];

                                std::size_t symbol_size = symbol_follow.size();
                                symbol_follow.insert(follow_first.begin(), follow_first.end());

                                return symbol_size != symbol_follow.size();
                            }
                        }, follow);
                    }
                }
            } while(has_change);
        }

        void RegisterSymbols(NonTerminal<G> *nonterminal)
        {
            this->nonterminals_.insert(nonterminal);

            for(auto &rule : nonterminal->rules_)
            {
                rule.non_terminal_ = nonterminal;
                Terminal<G> *last_terminal = nullptr;

                this->production_rules_.push_back({nonterminal, rule});

                for(auto &symbol : rule.sequence_)
                {
                    std::visit(overload{
                        [&](Terminal<G> *terminal)
                        {
                            last_terminal = terminal;
                            this->terminals_.insert(terminal);
                        },
                        [&](NonTerminal<G> *child_nonterminal)
                        {
                            if(this->nonterminals_.contains(child_nonterminal)) return;

                            this->RegisterSymbols(child_nonterminal);
                        }
                    }, symbol);
                }

                // Rule precedence defaults to the precedence of the LAST terminal in sequence.
                if(last_terminal)
                {
                    rule.precedence = last_terminal->precedence;
                }
            }
        }

        Grammar(NonTerminal<G> &start) : root(start)
        {
            this->EOS = std::make_unique<DefineTerminal<G, R"(\Z)">>();
            this->terminals_.insert(this->EOS.get());

            this->RegisterSymbols(&start);

            this->GenerateFirstSet();
            this->GenerateFollowSet();
        }

        Grammar() = delete;
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
    ProductionRuleList<G> operator|(ProductionRule<G> const &lhs, ProductionRule<G> const &rhs)
    {
        return ProductionRuleList<G>() | lhs | rhs;
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
            return *this->rule == *other.rule && this->position == other.position;
        }

        LRItem(ProductionRule<G> const *rule, int position = 0) : rule(rule), position(position) {}

        LRItem() = delete;
    };

    using lrstate_id_t = std::size_t;

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
                    [](Terminal<G> *terminal) { /* Do Nothing */ },
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
     * LR ACTION TYPE
     */
    enum class LRActionType
    {
        kError,
        kAccept,
        kShift,
        kReduce,
    };

    /**
     * LR ACTION
     * @tparam G
     */
    template<IGrammar G>
    struct LRAction
    {
        LRActionType type = LRActionType::kError;

        union
        {
            lrstate_id_t state;
            ProductionRule<G> const *rule;
        };
    };

    /**
     * PARSER
     * @tparam G
     */
    template<IGrammar G>
    class Parser
    {
    public:
        virtual std::expected<typename G::ValueType, Error> Parse(std::string_view input,  std::vector<Token<G>> *tokens) = 0;

        virtual ~Parser() = default;
    };

    /**
     * SLR PARSER
     * @tparam G
     */
    template<IGrammar G>
    class SLRParser final : public Parser<G>
    {
        Grammar<G> grammar_;

        std::map<lrstate_id_t, std::map<Terminal<G>*, LRAction<G>>> action_;
        std::map<lrstate_id_t, std::map<NonTerminal<G>*, lrstate_id_t>> goto_;

        struct ParseStackItem
        {
            lrstate_id_t state;
            typename G::ValueType value;

            ParseStackItem(lrstate_id_t state) : state(state) {}
            ParseStackItem(lrstate_id_t state, typename G::ValueType value) : state(state), value(std::move(value)) {}
        };

        struct Tokenizer
        {
            SLRParser<G> const &parser;
            std::string_view input;
            std::size_t index = 0;

            std::vector<Token<G>> *tokens;

            std::optional<Token<G>> Peek(lrstate_id_t state = 0, bool permissive = false)
            {
                while(this->index < this->input.size() && std::isspace(this->input[this->index])) this->index++;

                // IMPORTANT: No need to check for EOF, because it is checked for by special EOF terminal!

                if(permissive)
                {
                    for(auto terminal : this->parser.grammar_.terminals_)
                    {
                        auto token = terminal->Lex(this->input.substr(this->index));
                        if(token)
                        {
                            token->location.begin += this->index;
                            token->location.end += this->index;

                            return token;
                        }
                    }

                    // No token was matched. So we increment the index to skip this character.
                    index++;
                }
                else
                {
                    for(auto terminal : this->parser.action_.at(state) | std::views::keys)
                    {
                        auto token = terminal->Lex(this->input.substr(this->index));
                        if(token)
                        {
                            token->location.begin += this->index;
                            token->location.end += this->index;

                            return token;
                        }
                    }
                }

                return std::nullopt;
            }

            void Consume(Token<G> const &token)
            {
                this->index += token.Size();
                if(tokens)
                {
                    tokens->push_back(token);
                }
            }

            Tokenizer(SLRParser<G> const &parser, std::string_view input, std::vector<Token<G>> *tokens = nullptr) : parser(parser), input(input), tokens(tokens) {}
        };

        /**
         * Inserts state into list if does not exist, otherwise returns the index of existing equal state.
         * @param state_list
         * @param state LRState
         * @return
         */
        lrstate_id_t FindOrInsertLRState(std::vector<LRState<G>> &state_list, LRState<G> const &state)
        {
            auto it = std::ranges::find(state_list, state);

            // Does not exist, add it in.
            if(it == state_list.end())
            {
                state_list.push_back(state);
                return state_list.size() - 1;
            }

            // Otherwise return index.
            return std::distance(state_list.begin(), it);
        }

        /**
         * Constructs ACTION and GOTO for table-based SLR parsing.
         */
        std::optional<Error> BuildParsingTables()
        {
            std::vector<LRState<G>> states;

            // Generate first state
            states.clear();
            states.emplace_back(&this->grammar_.root);

            // Finite State Machine
            std::map<lrstate_id_t, std::map<Symbol<G>, lrstate_id_t>> fsm;

            for(lrstate_id_t i = 0; i < states.size(); i++)
            {
                // Process all transitions
                for(auto const &[symbol, new_state] : states[i].GenerateTransitions())
                {
                    lrstate_id_t new_state_id = this->FindOrInsertLRState(states, new_state);
                    fsm[i][symbol] = new_state_id;

                    // Create SHIFT/GOTO entries in parsing tables
                    std::visit(overload{
                        // Create ACTION
                        [&](Terminal<G> *terminal)
                        {
                            this->action_[i][terminal] = {
                                .type = LRActionType::kShift,
                                .state = new_state_id,
                            };
                        },

                        // Create GOTO
                        [&](NonTerminal<G> *non_terminal)
                        {
                            this->goto_[i][non_terminal] = new_state_id;
                        },
                    }, symbol);
                }

                // Create REDUCE/ACCEPT entries in parsing tables
                for(auto const &item : states[i].kernel_items)
                {
                    if(item.Complete())
                    {
                        for(auto follow_terminal : this->grammar_.follow_[item.rule->non_terminal_])
                        {
                            switch(this->action_[i][follow_terminal].type)
                            {
                                /* SHIFT-REDUCE CONFLICT */
                                case LRActionType::kShift:
                                {
                                    // Reduce due to higher precedence
                                    if(item.rule->precedence < follow_terminal->precedence)
                                    {
                                        this->action_[i][follow_terminal] = {
                                            .type = LRActionType::kReduce,
                                            .rule = item.rule,
                                        };
                                        break;
                                    }

                                    // Shift due to lower precedence
                                    if(item.rule->precedence > follow_terminal->precedence)
                                    {
                                        break;
                                    }

                                    // Reduce due to associativity rule
                                    if(follow_terminal->associativity == Associativity::Left)
                                    {
                                        this->action_[i][follow_terminal] = {
                                            .type = LRActionType::kReduce,
                                            .rule = item.rule,
                                        };
                                        break;
                                    }

                                    // Shift due to associativity rule
                                    if(follow_terminal->associativity == Right)
                                    {
                                        break;
                                    }

                                    // Unable to resolve conflict
                                    return GrammarDefinitionError("ShiftReduce");
                                }

                                case LRActionType::kReduce:
                                {
                                    return GrammarDefinitionError("ReduceReduce");
                                }

                                default:
                                {
                                    this->action_[i][follow_terminal] = {
                                        .type = LRActionType::kReduce,
                                        .rule = item.rule,
                                    };
                                }
                            }
                        }
                    }
                }
            }

            this->action_[0][this->grammar_.EOS.get()] = {
                .type = LRActionType::kAccept,
            };

            return std::nullopt;
        }

    protected:
        /**
         * Simply constructs the SLRParser's grammar and empty tables. SLRParser<G>::BuildParsingTables
         * MUST be called before attempting to parse.
         * @param tokenizer
         * @param start
         */
        SLRParser(NonTerminal<G> &start) : grammar_(start) {}

    public:
        Grammar<G> const &GetGrammar() const
        {
            return this->grammar_;
        }

        std::expected<typename G::ValueType, Error> Parse(std::string_view input, std::vector<Token<G>> *tokens = nullptr) override
        {
            Tokenizer tokenizer(*this, input, tokens);

            std::stack<ParseStackItem> parse_stack;
            parse_stack.emplace(0);

            while(true)
            {
                lrstate_id_t state = parse_stack.top().state;

                std::optional<Token<G>> lookahead = tokenizer.Peek(state);
                if(!lookahead)
                {
                    // Permissively consume rest of tokens
                    if(tokens)
                    {
                        while(true)
                        {
                            lookahead = tokenizer.Peek(0, true);
                            if(!lookahead) continue;
                            if(lookahead->terminal == this->grammar_.EOS.get())
                            {
                                break;
                            }
                            tokenizer.Consume(*lookahead);
                        }
                    }

                    return std::unexpected(Error{"Unexpected Token!"});
                }

                LRAction<G> action = this->action_[parse_stack.top().state][lookahead->terminal];
                switch(action.type)
                {
                    case LRActionType::kAccept:
                    {
                        return std::move(parse_stack.top().value);
                    }

                    case LRActionType::kShift:
                    {
                        std::optional<typename G::ValueType> value = std::move(lookahead->terminal->Reason(*lookahead));

                        if(value)
                        {
                            parse_stack.emplace(action.state, std::move(*value));
                        }
                        else
                        {
                            parse_stack.emplace(action.state);
                        }

                        tokenizer.Consume(*lookahead);
                        break;
                    }

                    case LRActionType::kReduce:
                    {
                        std::vector<typename G::ValueType> args(action.rule->sequence_.size());

                        for(int i = action.rule->sequence_.size() - 1; i >= 0; i--)
                        {
                            auto &top = parse_stack.top();

                            args[i] = std::move(top.value);

                            parse_stack.pop();
                        }

                        std::optional<typename G::ValueType> value = std::move(action.rule->Transduce(args));
                        if(value)
                        {
                            parse_stack.emplace(this->goto_[parse_stack.top().state][action.rule->non_terminal_], std::move(*value));
                        }
                        else
                        {
                            parse_stack.emplace(this->goto_[parse_stack.top().state][action.rule->non_terminal_]);
                        }
                        break;
                    }

                    default:
                    {
                        return std::unexpected(ParsingError(lookahead->location, "Unexpected Token"));
                    }
                }
            }
        }

        static std::expected<SLRParser, Error> Build(NonTerminal<G> &start)
        {
            SLRParser parser(start);

            auto error = parser.BuildParsingTables();
            if(error)
            {
                return std::unexpected(*error);
            }

            return std::move(parser);
        }

        /**
         * Construction of a parser can generate grammar errors. Use SLRParser<G>::Build to create.
         */
        SLRParser() = delete;
    };
}

#endif //BUFFALO2_H
