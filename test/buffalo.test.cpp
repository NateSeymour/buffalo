#include <gtest/gtest.h>
#include <buffalo/buffalo.h>

TEST(Tokenizer, ConstructTokenizer)
{
    using G = bf::GrammarDefinition<double>;

    bf::DefineTerminal<G, R"(\d+)"> NUMBER;
    bf::DefineTerminal<G, R"(\+)"> OP_PLUS;

    ASSERT_NE(NUMBER, OP_PLUS);

    bf::Terminal<G> terminals[] = { NUMBER, OP_PLUS };
    bf::CTRETokenizer tokenizer(std::to_array(terminals));
}
