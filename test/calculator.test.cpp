#include <gtest/gtest.h>
#include <variant>
#include <buffalo/buffalo.h>

/*
 * Grammar Definition
 */
using ValueType = std::variant<double, std::string>;
using Calculator = buffalo::GrammarDefinition<ValueType>;

/*
 * Terminals & Tokenizer
 */
buffalo::DefineTerminal<Calculator, R"((\-?\d+(\.\d+)?))", double> NUMBER([](auto &tok){
    return std::stod(std::string(tok.raw));
});

buffalo::DefineTerminal<Calculator, R"(\+|\-|\*|\/)", std::string> OPERATOR([](auto &tok){
    return std::string(tok.raw);
});

buffalo::DefineTerminal<Calculator, R"(\s*$)", double> END([](auto &tok){
    return 0.0;
});

/*
 * Non-Terminals
 */
buffalo::NonTerminal<Calculator> expression
    = (expression + OPERATOR + NUMBER)<=>[](auto &$)
    {
        return std::get<double>($[0]) + NUMBER($[2]);
    }
    | buffalo::ProductionRule(NUMBER)<=>[](auto &$)
    {
        return NUMBER($[0]);
    };

buffalo::NonTerminal<Calculator> program
    = (expression + END)<=>[](auto &$)
    {
        return std::get<double>($[0]);
    };

buffalo::Grammar<Calculator> grammar({
    // Terminals
    &NUMBER,
    &OPERATOR,
    &END,

    // NonTerminals
    &expression,
    &program,
});

TEST(Buffalo, Calculator)
{
}