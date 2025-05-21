#include <buffalo/buffalo.h>
#include <string_view>

using ValueType = std::string_view;
using G = bf::GrammarDefinition<ValueType>;

static bf::DefineTerminal<G, "[a-zA-Z]+", std::string_view> VALUE([](auto const &tok) -> ValueType {
    return tok.raw;
});

static bf::DefineTerminal<G, ";"> DELIMITER;

bf::DefineNonTerminal<G> statement_list = bf::