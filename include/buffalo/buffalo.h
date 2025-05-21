#ifndef BUFFALO2_H
#define BUFFALO2_H

#include <algorithm>
#include <cctype>
#include <ctll.hpp>
#include <ctre.hpp>
#include <expected>
#include <format>
#include <list>
#include <map>
#include <optional>
#include <ranges>
#include <set>
#include <stdexcept>
#include <variant>
#include <vector>
#include <sstream>
#include <memory>

namespace bf
{
    /*
     * Helper for std::visit provided by Andreas Fertig.
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

    /**
     * Empty dummy struct to act as default for Grammar user data.
     */
    struct Dummy {};

    /**
     * Helper for turning ctll string literals into std::string.
     * @param utf32
     * @param size
     * @return
     */
    [[nodiscard]] inline std::string utf32_to_string(char32_t const *utf32, std::size_t size)
    {
        std::string buffer(size, '0');

        for (std::size_t i = 0; i < size; i++)
        {
            buffer[i] = static_cast<char>(utf32[i]);
        }

        return std::move(buffer);
    }

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
    class ValueTokenReference;

    template<IGrammar G>
    class ValueTokenAccessor;

    template<IGrammar G>
    class ProductionRule;

    template<IGrammar G>
    class ProductionRuleList;

    template<IGrammar G>
    class Terminal;

    template<IGrammar G, ctll::fixed_string regex, typename SemanticType>
    class DefineTerminal;

    template<IGrammar G>
    class NonTerminal;

    template<IGrammar G, ctll::fixed_string DebugName, typename SemanticType>
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
        [[nodiscard]] std::string_view SnippetString(std::size_t padding = 10) const
        {
            std::size_t start = std::max(static_cast<std::size_t>(0), this->begin - padding);
            std::size_t n = this->end - this->begin + (padding * 2);

            return buffer.substr(start, n);
        }
    };

    class Error : public std::exception
    {
    protected:
        std::string message_;

    public:
        [[nodiscard]] char const *what() const noexcept override
        {
            return this->message_.c_str();
        }

        Error(std::string message) : message_(std::move(message)) {}
        Error() = default;
    };

    template<IGrammar G>
    class GrammarDefinitionError : public Error
    {
        static void AppendRule(std::stringstream &stream, ProductionRule<G> const *rule, int dot = -1)
        {
            stream << "\t" << rule->non_terminal_->GetName() << " -> ";

            for (int i = 0; i < rule->sequence_.size(); i++)
            {
                if (i == dot)
                {
                    stream << ". ";
                }

                char const *name = std::visit(overload{
                    [](Terminal<G> *terminal)
                    {
                        return terminal->GetName();
                    },
                    [](NonTerminal<G> *non_terminal)
                    {
                        return non_terminal->GetName();
                    }
                }, rule->sequence_[i]);

                stream << name << " ";

                if (dot == rule->sequence_.size() && i == rule->sequence_.size() - 1)
                {
                    stream << ".";
                }
            }

            stream << "\n";
        }

    public:
        static GrammarDefinitionError ShiftReduce(LRState<G> &state, LRState<G> &shift, ProductionRule<G> const *reduce, Terminal<G> *lookahead)
        {
            char const *reduce_name = reduce->non_terminal_->GetName();
            char const *lookahead_name = lookahead->GetName();

            std::stringstream message;
            message << "Grammar contains an irreconcilable shift-reduce conflict when deciding to reduce " << reduce_name << " or shift " << lookahead_name << ".\n";
            message << "Shift-reduce conflicts can be solved by:\n";
            message << "\t1. Refactoring your grammar.\n";
            message << "\t2. Adding precedence to your terminals.\n";
            message << "\t3. Adding Associativity to your terminals.\n\n";
            message << "The conflict arose in the following state with the following closure:\n";

            for (LRItem<G> &item : state.GenerateClosure())
            {
                GrammarDefinitionError::AppendRule(message, item.rule, static_cast<int>(item.position));
            }

            message << "When deciding to reduce the following rule:\n";
            GrammarDefinitionError::AppendRule(message, reduce);

            message << "Or shift " << lookahead_name << " to the following state:\n";

            for (LRItem<G> &item : shift.GenerateClosure())
            {
                GrammarDefinitionError::AppendRule(message, item.rule, static_cast<int>(item.position));
            }

            return {message.str()};
        }

        static GrammarDefinitionError ReduceReduce(LRState<G> &state, ProductionRule<G> const *a, ProductionRule<G> const *b, Terminal<G> *lookahead)
        {
            char const *a_name = a->non_terminal_->GetName();
            char const *b_name = b->non_terminal_->GetName();

            std::stringstream message;
            message << "Grammar contains an irreconcilable reduce-reduce conflict between " << a_name << "/" << b_name << ".\n";
            message << "Reduce-reduce conflicts are normally solved by refactoring your grammar.\n";
            message << "The conflict arose in the following two rules:\n\n";

            for (auto const rule : {a, b})
            {
                GrammarDefinitionError::AppendRule(message, rule);
            }

            message << "With lookahead " << lookahead->GetName() << "\n";

            message << "In the state with the following closure:\n";
            for (LRItem<G> &item : state.GenerateClosure())
            {
                GrammarDefinitionError::AppendRule(message, item.rule, static_cast<int>(item.position));
            }

            return {message.str()};
        }

        GrammarDefinitionError(std::string message) : Error(std::move(message)) {}
        GrammarDefinitionError() : Error("Unknown Grammar Definition Error") {}
    };

    class ParsingError : public Error
    {
        Location location_;

    public:
        ParsingError(Location location, std::string_view message) : location_(location)
        {
            std::stringstream mstream;
            mstream << message << "\n";
            mstream << "\t" << location.SnippetString(10) << "\n";
            mstream << "\t" << std::string(10, ' ') << '^' << std::string(location.end - location.begin, '~') << "\n";

            this->message_ = mstream.str();
        }
    };

    /**
     * TOKEN
     * Resolved by the lexxer at scan time.
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
     * VALUE TOKEN
     * Passed in array to NonTerminals at transduction time.
     */
    template<IGrammar G>
    struct ValueToken
    {
        std::string_view raw;
        Location location;
        typename G::ValueType value;

        [[nodiscard]] operator typename G::ValueType()
        {
            return this->value;
        }

        ValueToken(std::string_view raw, Location location, typename G::ValueType &&value) : raw(raw), location(location), value(std::move(value)) {}
    };

    template<IGrammar G>
    class ValueTokenStore
    {
        friend class ValueTokenReference<G>;
        friend class ValueTokenAccessor<G>;
        friend class SLRParser<G>;

        std::vector<ValueToken<G>> values_;

    public:
        ValueTokenStore()
        {
            this->values_.reserve(100);
        }
    };

    template<IGrammar G>
    class ValueTokenReference
    {
        std::size_t index_;
        std::shared_ptr<ValueTokenStore<G>> store_;

    public:
        [[nodiscard]] ValueToken<G> &GetValueToken() const
        {
            return this->store_->values_[this->index_];
        }

        [[nodiscard]] typename G::ValueType &GetValue() const
        {
            return this->store_->values_[this->index_].value;
        }

        ValueTokenReference(std::size_t index, std::shared_ptr<ValueTokenStore<G>> store) : index_(index), store_(store) {}
    };

    template<IGrammar G>
    class ValueTokenAccessor
    {
        std::span<std::size_t> indices_;
        std::shared_ptr<ValueTokenStore<G>> store_;

        std::string_view raw_;
        Location location_;

    public:
        [[nodiscard]] typename G::ValueType &operator[](std::size_t i)
        {
            return this->store_->values_[this->indices_[i]].value;
        }

        [[nodiscard]] ValueTokenReference<G> operator()(std::size_t i)
        {
            return {this->indices_[i], this->store_};
        }

        ValueTokenAccessor &operator=(typename G::ValueType value)
        {
            this->store_->values_.emplace_back(this->raw_, this->location_, std::move(value));

            return *this;
        }

        ValueTokenAccessor(std::span<std::size_t> indices, std::shared_ptr<ValueTokenStore<G>> store) : indices_(indices), store_(store) {}
    };

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
    class Terminal
    {
        friend class Grammar<G>;

    public:
        using ReasonerType = typename G::ValueType(*)(Token<G> const&);

    protected:
        std::string name_ = "\"UNKNOWN\"";

        inline static std::size_t counter_ = 0;

        ReasonerType reasoner_ = nullptr;

        Terminal() = default;

    public:
        std::size_t precedence = Terminal::counter_++;
        Associativity associativity = Associativity::None;
        typename G::UserDataType user_data;

        /**
         * Takes a token and turns it into a value. Default constructs the value if no reasoner is defined.
         * @param token Token to turn into a value
         * @return Value
         */
        typename G::ValueType Reason(Token<G> const &token) const
        {
            if(this->reasoner_)
            {
                return this->reasoner_(token);
            }

            return {};
        }

        virtual std::optional<Token<G>> Lex(std::string_view input) const
        {
            return std::nullopt;
        }

        [[nodiscard]] char const *GetName() const noexcept
        {
            return this->name_.c_str();
        }

        Terminal(Terminal<G>  &&) = delete;
        Terminal(Terminal<G> const &) = delete;
    };

    /**
     * DEFINE TERMINAL
     * @tparam G
     */
    template<IGrammar G, ctll::fixed_string regex, typename SemanticType = Dummy>
    class DefineTerminal : public Terminal<G>
    {
    public:
        SemanticType &operator()(typename G::ValueType &value)
        {
            return std::get<SemanticType>(value);
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
            this->name_ = std::format("\"{}\"", utf32_to_string(regex.content, regex.size()));
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
        using TransductorType = void(*)(ValueTokenAccessor<G> &);

    protected:
        std::string name_ = "unknown";
        std::vector<ProductionRule<G>> rules_;

    public:
        [[nodiscard]] char const *GetName() const noexcept
        {
            return this->name_.c_str();
        }

        NonTerminal(NonTerminal  &) = delete;
        NonTerminal(NonTerminal &&) = delete;

        NonTerminal() = default;

        NonTerminal(ProductionRule<G> const &rule, std::string name = "unknown") : rules_({rule}), name_(std::move(name)) {}
        NonTerminal(ProductionRuleList<G> const &rule_list, std::string name = "unknown") : rules_(rule_list.rules), name_(std::move(name)) {}
    };

    /**
     * DEFINE NON-TERMINAL
     */
    template<IGrammar G, ctll::fixed_string DebugName = "Generic", typename SemanticValue = Dummy>
    class DefineNonTerminal : public NonTerminal<G>
    {
    public:
        SemanticValue &operator()(typename G::ValueType &value)
        {
            return std::get<SemanticValue>(value);
        }

        DefineNonTerminal() = delete;

        DefineNonTerminal(Terminal<G> &single_terminal) : NonTerminal<G>({single_terminal}, utf32_to_string(DebugName.content, DebugName.size())) {}
        DefineNonTerminal(NonTerminal<G> &single_non_terminal) : NonTerminal<G>({single_non_terminal}, utf32_to_string(DebugName.content, DebugName.size())) {}

        DefineNonTerminal(ProductionRule<G> const &rule) : NonTerminal<G>(rule, utf32_to_string(DebugName.content, DebugName.size())) {}
        DefineNonTerminal(ProductionRuleList<G> const &rule_list) : NonTerminal<G>(rule_list, utf32_to_string(DebugName.content, DebugName.size())) {}
    };

    /**
     * SYMBOL
     */
    template<IGrammar G>
    using Symbol = std::variant<Terminal<G>*, NonTerminal<G>*>;

    /*
     * PRODUCTION RULE MODIFIERS
     */

    /**
     * ProductionRule attribute class for creating short circuits.
     * @tparam G
     */
    template<IGrammar G>
    struct Short
    {
        std::set<Terminal<G> *> terminals;

        Short(Terminal<G> *terminal) : terminals(terminal) {}
        Short(std::span<Terminal<G>*> terminals) : terminals(terminals) {}
    };

    /**
     * Marks a ProductionRule unproductive, so that no
     */
    struct Unproductive {};

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
        friend class GrammarDefinitionError<G>;

    protected:
        typename NonTerminal<G>::TransductorType transductor_ = nullptr;
        std::vector<Symbol<G>> sequence_;
        std::size_t precedence = -1;

        NonTerminal<G> *non_terminal_ = nullptr;

        std::set<Terminal<G>*> short_circuit_;

        struct {} attr_;

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

        /**
         * "Short Circuits" the rule for a particular lookahead. This will ensure that this rule is not considered
         * when a specific lookahead is ahead.
         * @param lookahead Lookahead to short circuit on
         * @return
         */
        ProductionRule &Short(Terminal<G> &lookahead)
        {
            this->mod_.short_circuit.insert(&lookahead);

            return *this;
        }

        ProductionRule &operator<=>(typename NonTerminal<G>::TransductorType tranductor)
        {
            this->transductor_ = tranductor;

            return *this;
        }

        bool operator==(ProductionRule<G> const &other) const
        {
            // NonTerminal
            if(this->non_terminal_ != other.non_terminal_)
            {
                return false;
            }

            if (this->mod_ != other.mod_)
            {
                return false;
            }

            // Sequence
            if(this->sequence_.size() != other.sequence_.size()) return false;
            for(auto const &[first, second] : std::views::zip(this->sequence_, other.sequence_))
            {
                if(first != second) return false;
            }

            return true;
        }

        void Transduce(ValueTokenAccessor<G> &$) const
        {
            if(this->transductor_)
            {
                this->transductor_($);
            }
            // If no transductor is defined, return the first element in sequence.
            else if (this->sequence_.size() > 0)
            {
                $ = $[0];
            }
        }

        ProductionRule(Terminal<G>       &terminal) : sequence_({ &terminal }) {}
        ProductionRule(NonTerminal<G> &nonterminal) : sequence_({ &nonterminal }) {}
    };

    /**
     * Helper for defining single-element production rules.
     */
    template<IGrammar G>
    using PR = ProductionRule<G>;

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
        ProductionRuleModifiers<G> mod;
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
            return *this->rule == *other.rule && this->mod == other.mod && this->position == other.position;
        }

        LRItem(ProductionRule<G> const *rule, int position = 0) : rule(rule), mod(rule->mod_), position(position) {}

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
                    [&](Terminal<G> *terminal)
                    {
                        // Remove this rule if it is present it item's short-circuit list
                        if (closure[i].mod.short_circuit.contains(terminal))
                        {
                            closure.erase(closure.begin() + (i--));
                        }
                    },
                    [&](NonTerminal<G> *non_terminal)
                    {
                        if(closed_nonterminals.contains(non_terminal)) return;
                        closed_nonterminals.insert(non_terminal);

                        for(auto const &rule : non_terminal->rules_)
                        {
                            auto &new_item = closure.emplace_back(&rule);
                            new_item.mod += closure[i].mod;
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
                if (item.Complete()) continue;

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
        virtual std::expected<ValueTokenReference<G>, Error> Parse(std::string_view input) const = 0;

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

        [[nodiscard]] LRAction<G> const &LookupAction(lrstate_id_t state, Terminal<G> *lookahead) const
        {
            return this->action_.at(state).at(lookahead);
        }

        [[nodiscard]] lrstate_id_t LookupGoto(lrstate_id_t state, NonTerminal<G> *non_terminal) const
        {
            return this->goto_.at(state).at(non_terminal);
        }

        struct Tokenizer
        {
            SLRParser<G> const &parser;
            std::string_view input;
            std::size_t index = 0;

            std::optional<Token<G>> Next(lrstate_id_t state = 0)
            {
                while(this->index < this->input.size() && std::isspace(this->input[this->index])) this->index++;

                for(auto terminal : this->parser.action_.at(state) | std::views::keys)
                {
                    auto token = terminal->Lex(this->input.substr(this->index));
                    if(token)
                    {
                        token->location.begin += this->index;
                        token->location.end += this->index;

                        this->index += token->Size();

                        return token;
                    }
                }

                return std::nullopt;
            }

            Tokenizer(SLRParser<G> const &parser, std::string_view input) : parser(parser), input(input) {}
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
            std::vector<LRState<G>> states{};
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
                    if(!item.Complete()) continue;

                    for(auto follow_terminal : this->grammar_.follow_[item.rule->non_terminal_])
                    {
                        // There will be no conflict
                        if (!this->action_[i].contains(follow_terminal))
                        {
                            this->action_[i][follow_terminal] = {
                                .type = LRActionType::kReduce,
                                .rule = item.rule,
                            };
                            continue;
                        }

                        // There will be conflict (FIGHT!)
                        LRAction<G> conflict = this->action_[i][follow_terminal];
                        if (conflict.type == LRActionType::kShift) // SHIFT-REDUCE
                        {
                            // Reduce due to higher precedence
                            if(item.rule->precedence < follow_terminal->precedence)
                            {
                                this->action_[i][follow_terminal] = {
                                    .type = LRActionType::kReduce,
                                    .rule = item.rule,
                                };
                                continue;
                            }

                            // Shift due to lower precedence
                            if(item.rule->precedence > follow_terminal->precedence)
                            {
                                continue;
                            }

                            // Reduce due to associativity rule
                            if(follow_terminal->associativity == Associativity::Left)
                            {
                                this->action_[i][follow_terminal] = {
                                    .type = LRActionType::kReduce,
                                    .rule = item.rule,
                                };
                                continue;
                            }

                            // Shift due to associativity rule
                            if(follow_terminal->associativity == Right)
                            {
                                continue;
                            }

                            // Unable to resolve conflict
                            return GrammarDefinitionError<G>::ShiftReduce(states[i], states[conflict.state], item.rule, follow_terminal);
                        }
                        else if (conflict.type == LRActionType::kReduce) // REDUCE-REDUCE
                        {
                            return GrammarDefinitionError<G>::ReduceReduce(states[i], conflict.rule, item.rule, follow_terminal);
                        }
                    }
                }
            }

            this->action_[0][this->grammar_.EOS.get()] = {
                .type = LRActionType::kAccept,
            };

            this->goto_[0][&this->grammar_.root] = 0;

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

        std::expected<ValueTokenReference<G>, Error> Parse(std::string_view input) const override
        {
            constexpr int expected_max_pr_length = 50;

            Tokenizer tokenizer(*this, input);

            std::vector<lrstate_id_t> stack;
            stack.reserve(expected_max_pr_length);
            stack.push_back(0);

            std::vector<std::size_t> value_stack;
            value_stack.reserve(expected_max_pr_length);

            auto store = std::make_shared<ValueTokenStore<G>>();

            std::optional<Token<G>> lookahead = tokenizer.Next(stack.back());
            while(true)
            {
                if(!lookahead)
                {
                    Location location = {
                        .buffer = input,
                        .begin = tokenizer.index,
                        .end = tokenizer.index,
                    };
                    return std::unexpected(ParsingError{location, "Unexpected Token!"});
                }

                auto &action = this->LookupAction(stack.back(), lookahead->terminal);
                if (action.type == LRActionType::kAccept)
                {
                    return ValueTokenReference<G>(store->values_.size() - 1, store);
                }
                else if (action.type == LRActionType::kShift)
                {
                    typename G::ValueType value = std::move(lookahead->terminal->Reason(*lookahead));
                    store->values_.emplace_back(lookahead->raw, lookahead->location, std::move(value));

                    stack.push_back(action.state);
                    value_stack.push_back(store->values_.size() - 1);

                    lookahead = tokenizer.Next(stack.back());
                }
                else if (action.type == LRActionType::kReduce)
                {
                    auto sequence_size = action.rule->sequence_.size();
                    ValueTokenAccessor<G> accessor{std::span{value_stack.end() - sequence_size, sequence_size}, store};

                    action.rule->Transduce(accessor);

                    stack.erase(stack.end() - sequence_size, stack.end());
                    stack.push_back(this->LookupGoto(stack.back(), action.rule->non_terminal_));

                    value_stack.erase(value_stack.end() - sequence_size, value_stack.end());
                    value_stack.push_back(store->values_.size() - 1);
                }
                else
                {
                    return std::unexpected(ParsingError(lookahead->location, "Unexpected Token!"));
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
