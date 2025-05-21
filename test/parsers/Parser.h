#ifndef PARSER_H
#define PARSER_H

#include <buffalo/buffalo.h>

namespace bf::test
{
    using CalculatorG = bf::GrammarDefinition<double>;
    extern bf::DefineNonTerminal<CalculatorG> calculator;

}

#endif //PARSER_H
