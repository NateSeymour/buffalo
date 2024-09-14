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
    IDENTIFIER,

    KW_VAR,

    OP_ADDITION,
    OP_SUBTRACTION,
    OP_DIVISION,
    OP_MULTIPLICATION,

    OP_ASSIGNMENT,

    LEFT_PARENTHESIS,
    RIGHT_PARENTHESIS,

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
using G = bf::GrammarDefinition<ValueType, Token>;

bf::Terminal<G> NUMBER(Token::NUMBER, [](auto &tok) -> ValueType {
    return 0.0;
});

bf::Terminal<G> IDENTIFIER(Token::IDENTIFIER);

bf::Terminal<G> KW_VAR(Token::KW_VAR);

bf::Terminal<G> OP_ADDITION(Token::OP_ADDITION);
bf::Terminal<G> OP_ASSIGNMENT(Token::OP_ASSIGNMENT);

bf::Terminal<G> END(Token::END, [](auto &tok) -> ValueType {
    return 0.0;
});

bf::NonTerminal<G> expression
    = bf::ProductionRule(NUMBER)<=>[](auto &$) -> ValueType
    {
        return std::get<double>($[0]);
    }
    | bf::ProductionRule(IDENTIFIER)<=>[](auto &$) -> ValueType
    {
        return 0.0;
    }
    | (expression + OP_ADDITION + NUMBER)<=>[](auto &$) -> ValueType
    {
        return 0.0;
    };

bf::NonTerminal<G> variable_declaration
    = (KW_VAR + IDENTIFIER + OP_ASSIGNMENT + expression)<=>[](auto &$) -> ValueType
    {
        return 0.0;
    };

bf::NonTerminal<G> statement
    = bf::ProductionRule(expression)
    | bf::ProductionRule(variable_declaration)
    ;

bf::Grammar grammar(statement);
bf::SLRParser calculator(grammar);

TEST(Lang, BasicParsing)
{

}
