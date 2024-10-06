#include <buffalo/buffalo.h>
#include <buffalo/spex.h>
#include <gtest/gtest.h>

TEST(Tokenizer, TerminalComparison)
{
    using ValueType = std::variant<std::monostate, double>;
    using G = bf::GrammarDefinition<ValueType>;

    spex::CTRETokenizer<G> tokenizer;

    auto NUMBER = tokenizer.Terminal<"\\d">([](auto &tok) -> ValueType {
        return std::stod(std::string(tok.raw));
    });
    auto GENERIC = tokenizer.Generic();
    auto GENERIC2 = tokenizer.Generic();

    ASSERT_EQ(NUMBER, NUMBER);
    ASSERT_NE(NUMBER, GENERIC);
    ASSERT_NE(GENERIC, GENERIC2);
    ASSERT_NE(NUMBER, tokenizer.EOS);
    ASSERT_NE(GENERIC, tokenizer.EOS);
    ASSERT_NE(GENERIC2, tokenizer.EOS);

    bf::Symbol<G> NUMBER_SYMBOL = NUMBER;
    bf::Symbol<G> GENERIC_SYMBOL = GENERIC;
    bf::Symbol<G> GENERIC2_SYMBOL = GENERIC2;
    bf::Symbol<G> EOS_SYMBOL = tokenizer.EOS;

    ASSERT_EQ(NUMBER_SYMBOL, NUMBER_SYMBOL);
    ASSERT_NE(NUMBER_SYMBOL, GENERIC_SYMBOL);
    ASSERT_NE(GENERIC_SYMBOL, GENERIC2_SYMBOL);
    ASSERT_NE(NUMBER_SYMBOL, EOS_SYMBOL);
    ASSERT_NE(GENERIC_SYMBOL,EOS_SYMBOL);
    ASSERT_NE(GENERIC2_SYMBOL, EOS_SYMBOL);
}
