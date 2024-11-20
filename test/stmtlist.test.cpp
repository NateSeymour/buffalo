/**
 * Tests a subset of the `unlogic` grammar for certain properties and consistency.
 */
#include <gtest/gtest.h>
#include <buffalo/buffalo.h>

struct Node {};

using ValueType = std::variant<
    double,
    std::string,
    std::vector<std::string>,
    std::unique_ptr<Node>,
    std::vector<std::unique_ptr<Node>>
>;
using G = bf::GrammarDefinition<ValueType>;

static bf::DefineTerminal<G, R"(given)"> KW_GIVEN;
static bf::DefineTerminal<G, R"(calc)"> KW_CALC;
static bf::DefineTerminal<G, R"(plot)"> KW_PLOT;

static bf::DefineTerminal<G, R"(on)"> KW_ON;
static bf::DefineTerminal<G, R"(as)"> KW_AS;

static bf::DefineTerminal<G, R"(\d+(\.\d+)?)", double> NUMBER([](auto const &tok) -> ValueType {
    return std::stod(std::string(tok.raw));
});

static bf::DefineTerminal<G, R"([a-zA-Z]+)", std::string> IDENTIFIER([](auto const &tok) -> ValueType {
    return std::string(tok.raw);
});

static bf::DefineTerminal<G, R"(\^)"> OP_EXP(bf::Right);

static bf::DefineTerminal<G, R"(\*)"> OP_MUL(bf::Left);
static bf::DefineTerminal<G, R"(\/)"> OP_DIV(bf::Left);
static bf::DefineTerminal<G, R"(\+)"> OP_ADD(bf::Left);
static bf::DefineTerminal<G, R"(\-)"> OP_SUB(bf::Left);

static bf::DefineTerminal<G, R"(:=)"> OP_ASN(bf::Left);

static bf::DefineTerminal<G, R"(\()"> PAR_OPEN;
static bf::DefineTerminal<G, R"(\))"> PAR_CLOSE;

static bf::DefineTerminal<G, R"(\[)"> BRK_OPEN;
static bf::DefineTerminal<G, R"(\])"> BRK_CLOSE;

static bf::DefineTerminal<G, R"(;)"> STMT_DELIMITER;

static bf::DefineTerminal<G, R"(,)"> SEPARATOR;

static bf::Terminal<G> terminals[] = {
    KW_GIVEN,
    KW_CALC,
    KW_PLOT,
    KW_ON,
    KW_AS,

    NUMBER,
    IDENTIFIER,

    OP_EXP,
    OP_MUL,
    OP_DIV,
    OP_ADD,
    OP_SUB,
    OP_ASN,

    PAR_OPEN,
    PAR_CLOSE,
    BRK_OPEN,
    BRK_CLOSE,

    STMT_DELIMITER,

    SEPARATOR,
};

static bf::CTRETokenizer tokenzier(std::to_array(terminals));

static bf::DefineNonTerminal<G, std::unique_ptr<Node>> expression
    = bf::PR<G>(NUMBER)<=>[](auto &$) -> ValueType
    {
        return std::make_unique<Node>();
    }
    | bf::PR<G>(IDENTIFIER)<=>[](auto &$) -> ValueType
    {
        return std::make_unique<Node>();
    }
    | (PAR_OPEN + expression + PAR_CLOSE)<=>[](auto &$) -> ValueType
    {
        return std::make_unique<Node>();
    }
    | (expression + OP_EXP + expression)<=>[](auto &$) -> ValueType
    {
        return std::make_unique<Node>();
    }
    | (expression + OP_MUL + expression)<=>[](auto &$) -> ValueType
    {
        return std::make_unique<Node>();
    }
    | (expression + OP_DIV + expression)<=>[](auto &$) -> ValueType
    {
        return std::make_unique<Node>();
    }
    | (expression + OP_ADD + expression)<=>[](auto &$) -> ValueType
    {
        return std::make_unique<Node>();
    }
    | (expression + OP_SUB + expression)<=>[](auto &$) -> ValueType
    {
        return std::make_unique<Node>();
    }
    ;

static bf::DefineNonTerminal<G, std::vector<std::string>> identifier_list
    = bf::PR<G>(IDENTIFIER)<=>[](auto &$) -> ValueType
    {
        return std::vector<std::string>{ IDENTIFIER($[0]) };
    }
    | (identifier_list + SEPARATOR + IDENTIFIER)<=>[](auto &$) -> ValueType
    {
        auto list = identifier_list($[0]);
        list.push_back(IDENTIFIER($[2]));

        return std::move(list);
    }
    ;

static bf::DefineNonTerminal<G, std::unique_ptr<Node>> function_definition
    = (KW_GIVEN + IDENTIFIER + PAR_OPEN + identifier_list + PAR_CLOSE + OP_ASN + expression)<=>[](auto &$) -> ValueType
    {
        return std::make_unique<Node>();
    }
    | (KW_GIVEN + IDENTIFIER + PAR_OPEN + PAR_CLOSE + OP_ASN + expression)<=>[](auto &$) -> ValueType
    {
        return std::make_unique<Node>();
    }
    ;

static bf::DefineNonTerminal<G, std::unique_ptr<Node>> plot_command
    = (KW_PLOT + IDENTIFIER)<=>[](auto &$) -> ValueType
    {
        return std::make_unique<Node>();
    }
    ;

static bf::DefineNonTerminal<G, std::unique_ptr<Node>> statement
    = (function_definition + STMT_DELIMITER)<=>[](auto &$) -> ValueType
    {
        return std::move($[0]);
    }
    | (plot_command + STMT_DELIMITER)<=>[](auto &$) -> ValueType
    {
        return std::move($[0]);
    }
    ;

static bf::DefineNonTerminal<G, std::vector<std::unique_ptr<Node>>> statement_list
    = bf::PR<G>(statement)<=>[](auto &$) -> ValueType
    {
        std::vector<std::unique_ptr<Node>> list;
        list.push_back(std::move(statement($[0])));

        return std::move(list);
    }
    | (statement_list + statement)<=>[](auto &$) -> ValueType
    {
        auto list = statement_list($[0]);
        list.push_back(std::move(statement($[1])));

        return std::move(list);
    }
    ;

static bf::DefineNonTerminal<G, std::unique_ptr<Node>> program
    = bf::PR<G>(statement_list)<=>[](auto &$) -> ValueType
    {
        return std::make_unique<Node>();
    }
    ;

TEST(StmtList, FollowSet)
{
    auto parser = bf::SLRParser<G>::Build(tokenzier, program);
    ASSERT_TRUE(parser);

    auto &grammar = parser->GetGrammar();
    ASSERT_TRUE(grammar.NonTerminalHasFollow(function_definition, STMT_DELIMITER));

    ASSERT_TRUE(grammar.HasNonTerminal(function_definition));
    ASSERT_TRUE(grammar.HasNonTerminal(plot_command));
    ASSERT_TRUE(grammar.HasNonTerminal(statement));

    ASSERT_TRUE(grammar.NonTerminalHasFirst(function_definition, KW_GIVEN));
    ASSERT_TRUE(grammar.NonTerminalHasFirst(plot_command, KW_PLOT));

    ASSERT_TRUE(grammar.NonTerminalHasFirst(statement, KW_GIVEN));
    ASSERT_TRUE(grammar.NonTerminalHasFirst(statement, KW_PLOT));

    ASSERT_TRUE(grammar.NonTerminalHasFollow(statement_list, KW_GIVEN));
    ASSERT_TRUE(grammar.NonTerminalHasFollow(statement_list, KW_PLOT));

    auto result = parser->Parse("given f(x) := x^2;\nplot f;");
    ASSERT_TRUE(result);
}