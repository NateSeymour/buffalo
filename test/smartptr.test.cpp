#include <buffalo/buffalo.h>
#include <buffalo/spex.h>
#include <gtest/gtest.h>

#include <utility>

class Node
{
public:
    virtual int Codegen() const = 0;

    virtual ~Node() = default;
};

class ConstantNode : public Node
{
    double value_;

public:
    int Codegen() const override
    {
        return (int)this->value_;
    }

    ConstantNode(double value) : value_(value) {}
};

class StringNode : public Node
{
    std::string value_;

public:
    int Codegen() const override
    {
        return 0;
    }

    StringNode(std::string value) : value_(std::move(value)) {}
};

using ValueType = std::unique_ptr<Node>;
using G = bf::GrammarDefinition<ValueType>;

static spex::CTRETokenizer<G> tokenizer;

static bf::DefineTerminal<G, std::unique_ptr<ConstantNode>> NUMBER = tokenizer.Terminal<R"(\d+(\.\d+)?)">([](auto const &tok) -> ValueType {
    auto value = std::stod(std::string(tok.raw));
    return std::make_unique<ConstantNode>(value);
});

static bf::DefineNonTerminal<G, std::unique_ptr<Node>> expression
    = bf::PR<G>(NUMBER)<=>[](auto &$) -> ValueType { return std::move($[0]); }
    ;

static bf::Grammar<G> grammar(tokenizer, expression);
static bf::SLRParser<G> parser(grammar);

TEST(Buffalo, SmartPointerSupport)
{
    auto res = std::move(*parser.Parse("355"));

    ASSERT_EQ(res->Codegen(), 355);
}
