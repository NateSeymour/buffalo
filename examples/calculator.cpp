#include <iostream>
#include <cmath>
#include <buffalo/buffalo.h>

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

int main(int argc, char const **argv)
{
    if(argc < 2)
    {
        std::cerr << "Usage: calculator expression" << std::endl;
        return 1;
    }

    auto calculator = *bf::SLRParser<G>::Build(statement);

    auto result = calculator.Parse(argv[1]);
    if(!result)
    {
        std::cerr << result.error().what() << std::endl;
        return 1;
    }

    std::cout << result->root.value << std::endl;
}