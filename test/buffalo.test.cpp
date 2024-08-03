#include <gtest/gtest.h>
#include <variant>
#include <buffalo/buffalo.h>

enum class TerminalType
{
    NUMBER,
    IDENTIFIER,
    KEYWORD,
    OPERATOR,
    SYMBOL,
    END,
};

typedef std::variant<double, std::string> ValueType;

buffalo::Tokenizer<TerminalType, ValueType> tokenizer;

buffalo::DefineTerminal<TerminalType, ValueType, R"(\-?\d+(\.\d+)?)", double> NUMBER(tokenizer, TerminalType::NUMBER, [](auto tok)
{
    return std::stod(std::string(tok.raw));
});

buffalo::DefineTerminal<TerminalType, ValueType, R"(\+|\-|\*|\/)", std::string> OPERATOR(tokenizer, TerminalType::OPERATOR, [](auto tok)
{
    return std::string(tok.raw);
});

buffalo::DefineTerminal<TerminalType, ValueType, R"([a-zA-Z\d]+)", std::string> IDENTIFIER(tokenizer, TerminalType::IDENTIFIER, [](auto tok)
{
    return std::string(tok.raw);
});

buffalo::DefineTerminal<TerminalType, ValueType, R"(\s*$)", double> END(tokenizer, TerminalType::END, [](auto tok) {
    return 0.0;
});

buffalo::NonTerminal<TerminalType, ValueType> atomic
    = buffalo::ProductionRule(NUMBER) <=> [](auto &$)
    {
        return std::get<double>($[0]);
    }
    | buffalo::ProductionRule(IDENTIFIER) <=> [](auto &$)
    {
        return 0.0;
    };

buffalo::NonTerminal<TerminalType, ValueType> expression
    = (expression + OPERATOR + atomic)<=>[](auto &$)
    {
        return std::get<double>($[0]) + std::get<double>($[2]);
    };

buffalo::NonTerminal<TerminalType, ValueType> program
    = (expression + END)<=>[](auto &$)
    {
        return std::get<double>($[0]);
    };

TEST(Parser, BasicParsing)
{
    buffalo::Parser<TerminalType, ValueType> p(tokenizer, program);

    //ASSERT_EQ(81.5, std::get<double>(result));
}