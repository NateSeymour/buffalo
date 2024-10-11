#include <iostream>
#include <buffalo/buffalo.h>
#include <buffalo/spex.h>

/*
 * Grammar Definition
 */
using ValueType = std::variant<std::monostate, double, std::string>;
using G = bf::GrammarDefinition<ValueType>;

spex::CTRETokenizer<G> tok;

/*
 * Terminals
 */
bf::DefineTerminal<G, double> NUMBER = tok.Terminal<R"(\d+(\.\d+)?)">([](auto const &tok) -> ValueType {
    return std::stod(std::string(tok.raw));
});

bf::DefineTerminal<G> OP_EXP = tok.Terminal<R"(\^)", bf::Associativity::Right>();

bf::DefineTerminal<G> OP_MUL = tok.Terminal<R"(\*)", bf::Associativity::Left>();
bf::DefineTerminal<G> OP_DIV = tok.Terminal<R"(\/)", bf::Associativity::Left>();
bf::DefineTerminal<G> OP_ADD = tok.Terminal<R"(\+)", bf::Associativity::Left>();
bf::DefineTerminal<G> OP_SUB = tok.Terminal<R"(\-)", bf::Associativity::Left>();

bf::DefineTerminal<G> PAR_OPEN = tok.Terminal<R"(\()">();
bf::DefineTerminal<G> PAR_CLOSE = tok.Terminal<R"(\))">();

/*
 * Non-Terminals
 */
bf::DefineNonTerminal<G, double> expression
    = bf::PR<G>(NUMBER)<=>[](auto &$) -> ValueType { return NUMBER($[0]); }
    | (PAR_OPEN + expression + PAR_CLOSE)<=>[](auto &$) -> ValueType { return expression($[1]); }
    | (expression + OP_EXP + expression)<=>[](auto &$) -> ValueType { return std::pow(expression($[0]), expression($[2])); }
    | (expression + OP_MUL + expression)<=>[](auto &$) -> ValueType { return expression($[0]) * expression($[2]); }
    | (expression + OP_DIV + expression)<=>[](auto &$) -> ValueType { return expression($[0]) / expression($[2]); }
    | (expression + OP_ADD + expression)<=>[](auto &$) -> ValueType { return expression($[0]) + expression($[2]); }
    | (expression + OP_SUB + expression)<=>[](auto &$) -> ValueType { return expression($[0]) - expression($[2]); }
    ;

bf::DefineNonTerminal<G, double> statement
    = bf::PR<G>(expression)<=>[](auto &$) -> ValueType
    {
        return expression($[0]);
    }
    ;

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        std::cerr << "Usage: calculator expression" << std::endl;
        return 1;
    }

    auto calculator = *bf::SLRParser<G>::Build(tok, statement);

    auto result = calculator.Parse(argv[1]);
    if(!result)
    {
        std::cerr << result.error().what() << std::endl;
    }

    std::cout << statement(*result) << std::endl;
}