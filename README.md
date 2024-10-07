# buffalo
> *buffalo: to bewilder, baffle (also: **bamboozle**)* [2024 Merriam-Webster Dictionary]

Bewilder, baffle (even bamboozle!) your grammars with `buffalo`, a C++23 header-only SLR parser generator with 
 `yacc/bison`-like syntax.

It supports the definition of terminals, non-terminals and grammar structures all within the same C++ file.

## Features
- Terminal definition with builtin scanning based on `compile-time-regular-expressions` (`spex`).
- Grammar definition in pseudo BNF notation.
- Shift/Reduce conflict resolution through precedence (based on definition order) and associativity (left/right/none). 

## Examples
### Calculator
```c++
/* calculator.cpp */
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

bf::Grammar grammar(tok, statement);
bf::SLRParser calculator(grammar);

/*
 * Calculations
 */
double result = statement(*calculator.Parse("18 + 2^(1 + 1) * 4"));
```