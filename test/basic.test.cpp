#include <buffalo/buffalo.h>
#include <buffalo/spex.h>
#include <gtest/gtest.h>
#include <string>
#include <variant>
#include <cmath>

/*
 * Grammar Definition
 */
using ValueType = std::variant<std::monostate, double, std::string>;
using G = bf::GrammarDefinition<ValueType>;

spex::CTRETokenizer<G> tok;

bf::DefineTerminal<G, double> NUMBER = tok.Terminal<R"(\d+(\.\d+)?)">([](auto const &tok) -> ValueType {
    return std::stod(std::string(tok.raw));
});

bf::DefineTerminal<G> OP_EXP = tok.Terminal<R"(\^)", bf::Associativity::Right>();

bf::DefineTerminal<G> OP_MUL = tok.Terminal<R"(\*)", bf::Associativity::Left>();
bf::DefineTerminal<G> OP_DIV = tok.Terminal<R"(\/)", bf::Associativity::Left>();
bf::DefineTerminal<G> OP_ADD = tok.Terminal<R"(\+)", bf::Associativity::Left>();
bf::DefineTerminal<G> OP_SUB = tok.Terminal<R"(\-)", bf::Associativity::Left>();

bf::DefineTerminal<G> PAR_OPEN = tok.Terminal<R"(\()">();
bf::DefineTerminal<G> PAR_CLOSE = tok.Terminal<R"(\))">();

bf::DefineNonTerminal<G, double> expression
    = bf::PR<G>(NUMBER)<=>[](auto &$) -> ValueType { return NUMBER($[0]); }
    | (PAR_OPEN + expression + PAR_CLOSE)<=>[](auto &$) -> ValueType { return expression($[1]); }
    | (expression + OP_EXP + expression)<=>[](auto &$) -> ValueType { return std::pow(expression($[0]), expression($[2])); }
    | (expression + OP_MUL + expression)<=>[](auto &$) -> ValueType { return expression($[0]) * expression($[2]); }
    | (expression + OP_DIV + expression)<=>[](auto &$) -> ValueType { return expression($[0]) / expression($[2]); }
    | (expression + OP_ADD + expression)<=>[](auto &$) -> ValueType { return expression($[0]) + expression($[2]); }
    | (expression + OP_SUB + expression)<=>[](auto &$) -> ValueType { return expression($[0]) - expression($[2]); }
    ;

bf::DefineNonTerminal<G, double> statement
    = bf::PR<G>(expression)<=>[](auto &$) -> ValueType
    {
        return expression($[0]);
    }
    ;

bf::Grammar grammar(tok, statement);
bf::SLRParser calculator(grammar);

TEST(Tokenizer, Tokenization)
{
    auto stream = tok.StreamInput("32 + 32");

    ASSERT_EQ(stream.Consume()->terminal, NUMBER);
    ASSERT_EQ(stream.Consume()->terminal, OP_ADD);
    ASSERT_EQ(stream.Consume()->terminal, NUMBER);
}

TEST(Lang, BasicParsing)
{
    std::map<std::string, double> math_problems = {
        { "32 + 32 + 32 + 32", 128.0 },
        { "3 * 4 + 2", 14.0 },
        { "3 * (4 + 2)", 18.0 },
        { "3 * (4 + 2)", 18.0 },
        { "2^(1 + 1)", 4.0 },
        { "18 + 2^(1 + 1) * 4", 34.0 },
    };

    for(auto const &[problem, answer] : math_problems)
    {
        auto result = *calculator.Parse(problem);
        ASSERT_EQ(statement(result), answer);
    }
}
