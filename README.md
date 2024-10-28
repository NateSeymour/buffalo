# buffalo
> *buffalo: to bewilder, baffle (also: **bamboozle**)* [2024 Merriam-Webster Dictionary]

Bewilder, baffle (even bamboozle!) your grammars with `buffalo`, a C++23 header-only SLR parser generator with
`yacc/bison`-like syntax.

It supports the definition of terminals, non-terminals and grammar structures all within the same C++ file.

## Features
- Terminal definition with builtin scanning based on `compile-time-regular-expressions` (`spex`).
- Grammar definition in pseudo BNF notation.
- Shift/Reduce conflict resolution through precedence (based on definition order) and associativity (left/right/none).

## Compiler Support
`buffalo` officially supports the following compilers:
- GCC 14
- Clang 18
- MSVC

## Examples
### Calculator
```c++
/* calculator.cpp */
#include <buffalo/buffalo.h>
#include <buffalo/spex.h>

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

bf::Terminal<G> terminals[] = { NUMBER, OP_EXP, OP_MUL, OP_DIV, OP_ADD, OP_SUB, PAR_OPEN, PAR_CLOSE };
bf::CTRETokenizer tokenizer(std::to_array(terminals));

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

/*
 * Calculations
 */
auto calculator = bf::SLRParser<G>::Build(tok, statement);
double result = *calculator.Parse("18 + 2^(1 + 1) * 4");
```