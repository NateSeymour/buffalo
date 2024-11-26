#include <gtest/gtest.h>
#include <buffalo/buffalo.h>
#include <cmath>

/*
 * Grammar Definition
 */
using G = bf::GrammarDefinition<double>;

/*
 * Terminals
 */
bf::DefineTerminal<G, R"(\d+(\.\d+)?)", double> NUMBER([](auto const &tok) {
    return std::stod(std::string(tok.raw));
});

bf::DefineTerminal<G, R"(\^)"> OP_EXP(bf::Right);

bf::DefineTerminal<G, R"(\*)"> OP_MUL(bf::Left);
bf::DefineTerminal<G, R"(\/)"> OP_DIV(bf::Left);
bf::DefineTerminal<G, R"(\+)"> OP_ADD(bf::Left);
bf::DefineTerminal<G, R"(\-)"> OP_SUB(bf::Left);

bf::DefineTerminal<G, R"(\()"> PAR_OPEN;
bf::DefineTerminal<G, R"(\))"> PAR_CLOSE;

/*
 * Non-Terminals
 */
bf::DefineNonTerminal<G> expression
    = bf::PR<G>(NUMBER)<=>[](auto &$) { return $[0]; }
    | (PAR_OPEN + expression + PAR_CLOSE)<=>[](auto &$) { return $[1]; }
    | (expression + OP_EXP + expression)<=>[](auto &$) { return std::pow($[0], $[2]); }
    | (expression + OP_MUL + expression)<=>[](auto &$) { return $[0] * $[2]; }
    | (expression + OP_DIV + expression)<=>[](auto &$) { return $[0] / $[2]; }
    | (expression + OP_ADD + expression)<=>[](auto &$) { return $[0] + $[2]; }
    | (expression + OP_SUB + expression)<=>[](auto &$) { return $[0] - $[2]; }
    ;

bf::DefineNonTerminal<G> statement
    = bf::PR<G>(expression)<=>[](auto &$)
    {
        return $[0];
    }
    ;

TEST(Parser, Construction)
{
    auto parser = bf::SLRParser<G>::Build(statement);
    ASSERT_TRUE(parser.has_value());
}

TEST(Parser, Evaluation)
{
    auto parser = *bf::SLRParser<G>::Build(statement);

    auto res = parser.Parse("3 * 3 + 4^2 - (9 / 3)");
    ASSERT_TRUE(res.has_value());

    ASSERT_EQ(*res, 22.0);
}

TEST(Tokenization, Strict)
{
    auto parser = *bf::SLRParser<G>::Build(statement);

    std::vector<bf::Token<G>> tokens;
    auto result = parser.Parse("3 + 5 - 2", &tokens);

    ASSERT_TRUE(result.has_value());

    ASSERT_EQ(tokens.size(), 5);

    ASSERT_EQ(tokens[0].terminal, &NUMBER);
    ASSERT_EQ(tokens[0].location.begin, 0);

    ASSERT_EQ(tokens[1].terminal, &OP_ADD);
    ASSERT_EQ(tokens[1].location.begin, 2);

    ASSERT_EQ(tokens[2].terminal, &NUMBER);
    ASSERT_EQ(tokens[2].location.begin, 4);

    ASSERT_EQ(tokens[3].terminal, &OP_SUB);
    ASSERT_EQ(tokens[3].location.begin, 6);

    ASSERT_EQ(tokens[4].terminal, &NUMBER);
    ASSERT_EQ(tokens[4].location.begin, 8);
}

TEST(Tokenization, Permissive)
{
    auto parser = *bf::SLRParser<G>::Build(statement);

    std::vector<bf::Token<G>> tokens;
    auto result = parser.Parse("3[[[+]]]&0", &tokens);

    ASSERT_FALSE(result.has_value());

    ASSERT_EQ(tokens.size(), 3);

    ASSERT_EQ(tokens[0].terminal, &NUMBER);
    ASSERT_EQ(tokens[0].location.begin, 0);

    ASSERT_EQ(tokens[1].terminal, &OP_ADD);
    ASSERT_EQ(tokens[1].location.begin, 4);

    ASSERT_EQ(tokens[2].terminal, &NUMBER);
    ASSERT_EQ(tokens[2].location.begin, 9);
}
