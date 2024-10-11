#ifndef BUFFALO2_H
#define BUFFALO2_H

#include <cctype>
#include <exception>
#include <expected>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <stack>
#include <stdexcept>
#include <variant>
#include <vector>

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
    };

    /*
     * FORWARD-DECLS
     */
    template<IGrammar G>
    class Grammar;

    template<IGrammar G>
    class ProductionRule;

    template<IGrammar G>
    class Tokenizer;

    template<IGrammar G>
    class Terminal;

    template<IGrammar G, typename SemanticType>
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
     * ERROR
     */
    class Error : public std::runtime_error
    {
    public:
        Error() : runtime_error("Buffalo Generic Error") {}
    };

    template<IGrammar G, typename SemanticType>
    class SemanticConversionError : public Error
    {
        std::string error_message = "";

    public:
        char const *what() const noexcept override
        {
            return this->error_message.c_str();
        }

        SemanticConversionError(Symbol<G> symbol)
        {
        }
    };

    class ShiftShiftError : public Error {};
    class ShiftReduceError : public Error {};
    class ReduceReduceError : public Error {};

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
    template<IGrammar G>
    struct Token
    {
        Terminal<G> terminal;
        std::string_view raw;
        Location location;

        std::size_t Size() const
        {
            return this->location.end - this->location.begin;
        }
    };

    /**
     * TOKENIZER
     * Virtual class to expose the Tokenizer API.
     * Child classes EXPECTED to create Terminal<G>'s.
     */
    template<IGrammar G>
    class Tokenizer
    {
    public:
        using LexxerType = std::optional<Token<G>>(*)(Terminal<G>, std::string_view);

        /**
         * Unique Terminal<G> (ensured uniqueness through StaticallyIdentifiedObject) that marks the end of a
         * stream of tokens.
         * @return Unique EOS (End of Stream) terminal.
         */
        Terminal<G> const EOS;

        /**
         * Creates a new generic Terminal
         * @return
         */
        Terminal<G> Generic(Terminal<G>::ReasonerType reasoner = nullptr) const
        {
            return Terminal<G>(reasoner);
        }

        /**
         * Gets the first token on the input stream.
         * @param input
         * @return
         */
        virtual std::expected<Token<G>, Error> First(std::string_view input) const = 0;

        /**
         * Helper to create a stream of tokens from string_view.
         */
        class TokenStream
        {
            std::size_t index_ = 0;
            std::string_view input_;

            Tokenizer<G> const &tokenizer_;

            std::deque<Token<G>> buffer_;

        public:
            std::optional<Token<G>> Peek(std::size_t lookahead = 1)
            {
                // Fill buffer to requested lookahead
                for(int i = 0; i < lookahead - buffer_.size(); i++)
                {
                    while(!this->input_.empty() && std::isspace(this->input_[0]))
                    {
                        this->input_ = this->input_.substr(1);
                        this->index_++;
                    }

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

                return std::nullopt;
            }

            std::optional<Token<G>> Consume()
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
        TokenStream StreamInput(std::string_view input) const
        {
            return TokenStream(*this, input);
        }

        virtual ~Tokenizer() = default;
    };

    /**
     * GRAMMAR DEFINITION
     * @tparam GValueType
     */
    template<typename GValueType>
    requires std::constructible_from<GValueType>
    class GrammarDefinition
    {
    public:
        /*
         * TYPES
         */
        using ValueType = GValueType;
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

    enum class Associativity
    {
        None,
        Left,
        Right,
    };

    /**
     * Struct to hold debug symbol names.
     */
    struct DebugSymbol
    {
        char const *debug_name = "Generic Symbol";
    };

    /**
     * TERMINAL
     */
    template<IGrammar G>
    class Terminal : public StaticallyIdentifiedObject, public DebugSymbol
    {
        friend class Grammar<G>;
        friend class Tokenizer<G>;

    public:
        using ReasonerType = typename G::ValueType(*)(Token<G> const&);

    protected:
        ReasonerType reasoner_ = nullptr;

        Terminal(ReasonerType reasoner = nullptr) : reasoner_(reasoner) {}

    public:
        std::size_t precedence = this->id;
        Associativity associativity = Associativity::None;

        Terminal &operator|(Associativity new_associativity)
        {
            this->associativity = new_associativity;

            return *this;
        }

        std::optional<typename G::ValueType> Reason(Token<G> const &token) const
        {
            if(this->reasoner_)
            {
                return this->reasoner_(token);
            }

            return std::nullopt;
        }
    };

    /**
     * DEFINE TERMINAL
     * @tparam G
     */
    template<IGrammar G, typename SemanticValue = void>
    class DefineTerminal : public Terminal<G>
    {
    public:
        SemanticValue operator()(typename G::ValueType &value)
        {
            if constexpr(std::variant_size<typename G::ValueType>::value != 0)
            {
                if(!std::holds_alternative<SemanticValue>(value))
                {
                    throw SemanticConversionError();
                }

                return std::move(std::get<SemanticValue>(value));
            }
            else
            {
                return std::move(reinterpret_cast<SemanticValue>(value));
            }
        }

        DefineTerminal() = delete;

        DefineTerminal(Terminal<G>  &terminal) : Terminal<G>(terminal) {}
        DefineTerminal(Terminal<G> &&terminal) : Terminal<G>(terminal) {}
    };

    /**
     * NON-TERMINAL
     */
    template<IGrammar G>
    class NonTerminal : public StaticallyIdentifiedObject
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
                    throw SemanticConversionError();
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
    using Symbol = std::variant<Terminal<G>, NonTerminal<G>*>;

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

        ProductionRule &operator<=>(typename NonTerminal<G>::TransductorType tranductor)
        {
            this->transductor_ = tranductor;

            return *this;
        }

        bool operator==(ProductionRule<G> const &other) const
        {
            if(this->non_terminal_ && other.non_terminal_)
            {
                if(*this->non_terminal_ != *other.non_terminal_) return false;
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

        ProductionRule(Terminal<G> const &terminal) : sequence_({ terminal }) {}
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
        std::set<NonTerminal<G>*> registered_nonterminals_;
        std::map<NonTerminal<G>*, std::set<Terminal<G>>> first_;
        std::map<NonTerminal<G>*, std::set<Terminal<G>>> follow_;

        /// List of all production rules along with their respective NonTerminal
        std::vector<std::pair<NonTerminal<G>*, ProductionRule<G>>> production_rules_;

    public:
        Tokenizer<G> const &tokenizer;
        NonTerminal<G> &root;

        /**
         * Simple, but somewhat inefficient algorithm for generating FIRST sets.
         * The FIRST set is the set of all Terminals that a NonTerminal can begin with.
         */
        void GenerateFirstSet()
        {
            bool has_change = false;
            do
            {
                for(auto const &[nonterminal, rule] : this->production_rules_)
                {
                    if(rule.sequence_.empty()) continue;

                    has_change = std::visit(overload{
                        [&](Terminal<G> terminal)
                        {
                            auto [it, inserted] = this->first_[nonterminal].insert(terminal);
                            return inserted;
                        },
                        [&](NonTerminal<G> *child_nonterminal)
                        {
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
            this->follow_[&root] = { this->tokenizer.EOS };

            bool has_change = false;
            do
            {
                for(auto &[nonterminal, rule] : this->production_rules_)
                {
                    for(int i = 0; i < rule.sequence_.size(); i++)
                    {
                        // Skip over Terminals
                        if(std::holds_alternative<Terminal<G>>(rule.sequence_[i])) continue;

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

                        has_change = std::visit(overload{
                            [&](Terminal<G> terminal)
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

        void RegisterNonTerminals(NonTerminal<G> *nonterminal)
        {
            this->registered_nonterminals_.insert(nonterminal);

            for(auto &rule : nonterminal->rules_)
            {
                rule.non_terminal_ = nonterminal;
                Symbol<G> *last_terminal = nullptr;

                this->production_rules_.push_back({nonterminal, rule});

                for(auto &symbol : rule.sequence_)
                {
                    std::visit(overload{
                        [&](Terminal<G> terminal)
                        {
                            last_terminal = &symbol;
                        },
                        [&](NonTerminal<G> *child_nonterminal)
                        {
                            if(this->registered_nonterminals_.contains(child_nonterminal)) return;

                            this->RegisterNonTerminals(child_nonterminal);
                        }
                    }, symbol);
                }

                // Rule precedence defaults to the precedence of the LAST terminal in sequence.
                if(last_terminal)
                {
                    rule.precedence = std::get<Terminal<G>>(*last_terminal).precedence;
                }
            }
        }

        Grammar(Tokenizer<G> const &tokenizer, NonTerminal<G> &start) : tokenizer(tokenizer), root(start)
        {
            this->RegisterNonTerminals(&start);

            this->GenerateFirstSet();
            this->GenerateFollowSet();
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
        virtual std::expected<typename G::ValueType, Error> Parse(std::string_view input) = 0;

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

        std::map<lrstate_id_t, std::map<Terminal<G>, LRAction<G>>> action_;
        std::map<lrstate_id_t, std::map<NonTerminal<G> *, lrstate_id_t>> goto_;

        struct ParseStackItem
        {
            lrstate_id_t state;
            typename G::ValueType value;

            ParseStackItem(lrstate_id_t state) : state(state) {}
            ParseStackItem(lrstate_id_t state, typename G::ValueType value) : state(state), value(std::move(value)) {}
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
                        [&](Terminal<G> terminal)
                        {
                            switch(this->action_[i][terminal].type)
                            {
                                case LRActionType::kShift:
                                {
                                    throw ShiftShiftError();
                                }

                                case LRActionType::kReduce:
                                {
                                    throw ShiftReduceError();
                                }

                                default:;
                            }

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
                                    if(item.rule->precedence < follow_terminal.precedence)
                                    {
                                        this->action_[i][follow_terminal] = {
                                            .type = LRActionType::kReduce,
                                            .rule = item.rule,
                                        };
                                        break;
                                    }

                                    // Shift due to lower precedence
                                    if(item.rule->precedence > follow_terminal.precedence)
                                    {
                                        break;
                                    }

                                    // Reduce due to associativity rule
                                    if(follow_terminal.associativity == Associativity::Left)
                                    {
                                        this->action_[i][follow_terminal] = {
                                            .type = LRActionType::kReduce,
                                            .rule = item.rule,
                                        };
                                        break;
                                    }

                                    // Shift due to associativity rule
                                    if(follow_terminal.associativity == Associativity::Right)
                                    {
                                        break;
                                    }

                                    // Unable to resolve conflict
                                    throw ShiftReduceError();
                                }

                                case LRActionType::kReduce:
                                {
                                    throw ReduceReduceError();
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

            this->action_[0][this->grammar_.tokenizer.EOS] = {
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
        SLRParser(Tokenizer<G> const &tokenizer, NonTerminal<G> &start) : grammar_(tokenizer, start) {}

    public:
        std::expected<typename G::ValueType, Error> Parse(std::string_view input) override
        {
            auto token_stream = this->grammar_.tokenizer.StreamInput(input);

            std::stack<ParseStackItem> parse_stack;
            parse_stack.emplace(0);

            while(true)
            {
                std::optional<Token<G>> lookahead = token_stream.Peek();
                if(!lookahead)
                {
                    return std::unexpected(Error());
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
                        auto value = std::move(lookahead->terminal.Reason(*lookahead));

                        if(value)
                        {
                            parse_stack.emplace(action.state, std::move(*value));
                        }
                        else
                        {
                            parse_stack.emplace(action.state);
                        }

                        token_stream.Consume();
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

                        auto value = std::move(action.rule->Transduce(args));

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
                        return std::unexpected(Error());
                    }
                }
            }
        }

        static std::expected<SLRParser, Error> Build(Tokenizer<G> const &tokenizer, NonTerminal<G> &start)
        {
            SLRParser parser(tokenizer, start);

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
