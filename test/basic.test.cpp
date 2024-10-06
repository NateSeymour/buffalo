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

bf::DefineTerminal<G, double> NUMBER = tok.Terminal<R"(\d+(\.\d+)?)">([](auto const &tok) -> ValueType {
    return std::stod(std::string(tok.raw));
});

bf::DefineTerminal<G> OP_ADDITION = tok.Terminal<R"(\+)">();

bf::DefineNonTerminal<G, double> expression
    = bf::PR<G>(NUMBER)<=>[](auto &$) -> ValueType
    {
        return NUMBER($[0]);
    }
    | (expression + OP_ADDITION + NUMBER)<=>[](auto &$) -> ValueType
    {
        return expression($[0]) + NUMBER($[2]);
    };

bf::DefineNonTerminal<G, double> statement
    = bf::PR<G>(expression)<=>[](auto &$) -> ValueType
    {
        return expression($[0]);
    }
    ;

bf::Grammar grammar(tok, (bf::NonTerminal<G>&)statement);
bf::SLRParser calculator(grammar);

TEST(Tokenizer, Tokenization)
{
    auto stream = tok.StreamInput("32 + 32");

    ASSERT_EQ(stream.Consume()->terminal, (bf::Terminal<G>)NUMBER);
    ASSERT_EQ(stream.Consume()->terminal, (bf::Terminal<G>)OP_ADDITION);
    ASSERT_EQ(stream.Consume()->terminal, (bf::Terminal<G>)NUMBER);
}

TEST(Lang, BasicParsing)
{
    auto res = calculator.Parse("32 + 32 + 32 + 32");

    ASSERT_TRUE(res);

    ASSERT_EQ(statement(*res), 128.0);
}
