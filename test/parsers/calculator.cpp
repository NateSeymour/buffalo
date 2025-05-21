#include <buffalo/buffalo.h>
#include <cmath>
#include "Parser.h"

/*
 * Grammar Definition
 */
using G = bf::test::CalculatorG;

/*
 * Terminals
 */
static bf::DefineTerminal<G, R"(\d+(\.\d+)?)", double> NUMBER([](auto const &tok) {
    return std::stod(std::string(tok.raw));
});

static bf::DefineTerminal<G, R"(\^)"> OP_EXP(bf::Right);

static bf::DefineTerminal<G, R"(\*)"> OP_MUL(bf::Left);
static bf::DefineTerminal<G, R"(\/)"> OP_DIV(bf::Left);
static bf::DefineTerminal<G, R"(\+)"> OP_ADD(bf::Left);
static bf::DefineTerminal<G, R"(\-)"> OP_SUB(bf::Left);
static bf::DefineTerminal<G, R"(\()"> PAR_OPEN;
static bf::DefineTerminal<G, R"(\))"> PAR_CLOSE;

/*
 * Non-Terminals
 */
static bf::DefineNonTerminal<G, "expression"> expression
    = bf::PR<G>(NUMBER)
    | (PAR_OPEN + expression + PAR_CLOSE)<=>[](auto &$) { $ = $[1]; }
    | (expression + OP_EXP + expression)<=>[](auto &$) { $ = std::pow($[0], $[2]); }
    | (expression + OP_MUL + expression)<=>[](auto &$) { $ = $[0] * $[2]; }
    | (expression + OP_DIV + expression)<=>[](auto &$) { $ = $[0] / $[2]; }
    | (expression + OP_ADD + expression)<=>[](auto &$) { $ = $[0] + $[2]; }
    | (expression + OP_SUB + expression)<=>[](auto &$) { $ = $[0] - $[2]; }
    ;

bf::DefineNonTerminal<G> bf::test::calculator
    = bf::PR<G>(expression)
    ;
