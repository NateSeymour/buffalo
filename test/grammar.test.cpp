#include <buffalo/buffalo.h>
#include <buffalo/spex.h>
#include <gtest/gtest.h>

TEST(Grammar, OnlyAcceptTriviallyConstructableValues)
{
    // UNCOMMENT TO TEST. SHOULD NOT COMPILE.
    /*
    class Foo
    {
    public:
        Foo() = delete;
    };

    using ValueType = Foo;
    using G = bf::GrammarDefinition<Foo>;

    spex::CTRETokenizer<G> tokenizer;
     */
}