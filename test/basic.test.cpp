#include <buffalo/buffalo.h>
#include <buffalo/spex.h>
#include <gtest/gtest.h>
#include <string>
#include <variant>

/*
 * Grammar Definition
 */
using ValueType = std::variant<std::monostate, double, std::string>;
using G = bf::GrammarDefinition<ValueType>;

spex::CTRETokenizer<G> tok;

bf::Terminal<G> NUMBER(tok, tok.GenLex<R"(\d+(\.\d+)?)">(), [](auto const &tok) -> ValueType {
    return std::stod(std::string(tok.raw));
});

bf::Terminal<G> OP_ADDITION(tok, tok.GenLex<R"(\+)">());

bf::NonTerminal<G> expression
    = bf::ProductionRule(NUMBER)<=>[](auto &$) -> ValueType
    {
        return std::get<double>($[0]);
    }
    | (expression + OP_ADDITION + NUMBER)<=>[](auto &$) -> ValueType
    {
        return std::get<double>($[0]) + std::get<double>($[2]);
    };

bf::NonTerminal<G> statement
    = bf::ProductionRule(expression)<=>[](auto &$) -> ValueType
    {
        return std::get<double>($[0]);
    }
    ;

bf::Grammar grammar(tok, statement);
bf::SLRParser calculator(grammar);

TEST(Tokenizer, Tokenization)
{
    auto stream = tok.StreamInput("32 + 32");

    ASSERT_EQ(stream.Consume()->terminal, NUMBER);
    ASSERT_EQ(stream.Consume()->terminal, OP_ADDITION);
    ASSERT_EQ(stream.Consume()->terminal, NUMBER);
}

TEST(Lang, BasicParsing)
{
    auto res = calculator.Parse("32 + 32");

    ASSERT_TRUE(res);
    ASSERT_EQ(std::get<double>(*res), 64.0);
}
