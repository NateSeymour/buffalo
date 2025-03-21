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
bf::DefineNonTerminal<G, "expression"> expression
    = bf::PR<G>(NUMBER)
    | (PAR_OPEN + expression + PAR_CLOSE)<=>[](auto &$) { $ = $[1]; }
    | (expression + OP_EXP + expression)<=>[](auto &$) { $ = std::pow($[0], $[2]); }
    | (expression + OP_MUL + expression)<=>[](auto &$) { $ = $[0] * $[2]; }
    | (expression + OP_DIV + expression)<=>[](auto &$) { $ = $[0] / $[2]; }
    | (expression + OP_ADD + expression)<=>[](auto &$) { $ = $[0] + $[2]; }
    | (expression + OP_SUB + expression)<=>[](auto &$) { $ = $[0] - $[2]; }
    ;

bf::DefineNonTerminal<G, "statement"> statement
    = bf::PR<G>(expression)
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

    ASSERT_EQ(res->GetValue(), 22.0);
}
