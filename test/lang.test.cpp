#include <string>
#include <variant>
#include <gtest/gtest.h>
#include <buffalo/buffalo2.h>
#include <buffalo/spex.h>

/*
 * Grammar Definition
 */
using ValueType = std::variant<double, std::string>;
using G = bf::GrammarDefinition<ValueType>;

spex::CTRETokenizer<G> tok;

bf::Terminal<G> NUMBER(tok, tok.GenLex<R"(\d*\.?\d*)">(), [](auto &tok) -> ValueType {
    return 0.0;
});

bf::Terminal<G> IDENTIFIER;
bf::Terminal<G> KW_VAR;
bf::Terminal<G> OP_ADDITION;
bf::Terminal<G> OP_ASSIGNMENT;
bf::Terminal<G> END;

bf::NonTerminal<G> expression =
    bf::ProductionRule(NUMBER)<=>[](auto &$) -> ValueType
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
