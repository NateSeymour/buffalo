#include <gtest/gtest.h>
#include <variant>
#include <buffalo/buffalo.h>

/*
 * Grammar Definition
 */
using ValueType = std::variant<double, std::string>;
using CalculatorGrammarType = buffalo::Grammar<ValueType>;

/*
 * Terminals & Tokenizer
 */
buffalo::DefineTerminal<CalculatorGrammarType, R"((\-?\d+(\.\d+)?))", double> NUMBER([](auto &tok){
    return std::stod(std::string(tok.raw));
});

buffalo::DefineTerminal<CalculatorGrammarType, R"(\+|\-|\*|\/)", std::string> OPERATOR([](auto &tok){
    return std::string(tok.raw);
});

buffalo::DefineTerminal<CalculatorGrammarType, R"(\s*$)", double> END([](auto &tok){
    return 0.0;
});

/*
 * Non-Terminals
 */
buffalo::NonTerminal<CalculatorGrammarType> expression
    = (expression + OPERATOR + NUMBER)<=>[](auto &$)
    {
        return std::get<double>($[0]) + NUMBER($[2]);
    }
    | buffalo::ProductionRule(NUMBER)<=>[](auto &$)
    {
        return NUMBER($[0]);
    };

buffalo::NonTerminal<CalculatorGrammarType> program
    = (expression + END)<=>[](auto &$)
    {
        return std::get<double>($[0]);
    };

CalculatorGrammarType CalculatorGrammar;

TEST(Buffalo, Calculator)
{
}