#include <buffalo/buffalo2.h>
#include <string>
#include <variant>

/*
 * Grammar Definition
 */
using ValueType = std::variant<double, std::string>;

class CalculatorGrammar : public bf::Grammar<ValueType>
{
public:
    void Build() override
    {
        auto NUMBER = this->MakeTerminal<double>([](auto &tok) -> ValueType {
            return 0.0;
        });

        auto expression = this->MakeNonTerminal<double>()
        [
            (expression + OPERATOR + NUMBER)<=>[]()
            {

            }
            | (NUMBER)<=>[]() {}
        ];
    }
};