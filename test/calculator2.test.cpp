#include <gtest/gtest.h>
#include <buffalo/buffalo2.h>
#include <string>
#include <variant>

/*
 * Grammar Definition
 */
enum class Token
{
    NUMBER,
    OPERATOR,
    END,
};

enum class Operator
{
    Addition,
    Subtraction,
    Multiplication,
    Division,
};

using ValueType = std::variant<double, std::string, Operator>;
using GrammarType = bf::GrammarDefinition<ValueType, Token>;

bf::Terminal<GrammarType> NUMBER(Token::NUMBER, [](auto &tok) -> ValueType {
    return 0.0;
});

bf::Terminal<GrammarType> OPERATOR(Token::OPERATOR, [](auto &tok) -> ValueType {
    return  Operator::Addition;
});

bf::Terminal<GrammarType> END(Token::END, [](auto &tok) -> ValueType {
    return 0.0;
});

bf::NonTerminal<GrammarType> expression
    = bf::ProductionRule(NUMBER)<=>[](auto &$) -> ValueType
    {
        return std::get<double>($[0]);
    }
    | (expression + OPERATOR + NUMBER)<=>[](auto &$) -> ValueType
    {
        return 0.0;
    };

bf::Grammar grammar(expression);
bf::SLRParser calculator(grammar);

TEST(Parser, BasicParsing)
{

}