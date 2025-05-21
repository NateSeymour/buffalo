#include <gtest/gtest.h>
#include <buffalo/buffalo.h>

TEST(Parser, Construction)
{
    auto parser = bf::SLRParser<G>::Build(statement);
    ASSERT_TRUE(parser.has_value());
}

TEST(Parser, Evaluation)
{
    auto parser = *bf::SLRParser<G>::Build(statement);

    auto res = parser.Parse("3 * 3 + 4^2 - (9 / 3)");
    ASSERT_TRUE(res.has_value());

    ASSERT_EQ(res->GetValue(), 22.0);
}
