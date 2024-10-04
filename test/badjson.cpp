#include <map>
#include <string>
#include <variant>
#include <gtest/gtest.h>
#include <buffalo/buffalo.h>
#include <buffalo/spex.h>

namespace badjson
{
    struct json
    {
        std::variant<std::string, double, bool, std::map<std::string, json>> value;
    };
}

using ValueType = std::variant<std::monostate, badjson::json, std::pair<std::string, badjson::json>, std::vector<std::pair<std::string, badjson::json>>>;
using G = bf::GrammarDefinition<ValueType>;

spex::CTRETokenizer<G> tok;

bf::Terminal<G> STRING(tok, tok.GenLex<R"(\"[a-zA-Z\s]+\")">(), [](auto &tok) -> ValueType {
    return badjson::json {
        .value = std::string(tok.raw.substr(1, -1)),
    };
});

bf::Terminal<G> NUMBER(tok, tok.GenLex<R"(\d+(\.\d+)?)">(), [](auto &tok) -> ValueType {
    return badjson::json {
        .value = std::stod(std::string(tok.raw)),
    };
});

bf::Terminal<G> BOOLEAN(tok, tok.GenLex<"true|false">(), [](auto &tok) -> ValueType {
    return badjson::json {
        .value = tok.raw == "true",
    };
});

bf::Terminal<G> OBJ_OPEN(tok, tok.GenLex<"\\{">());
bf::Terminal<G> OBJ_CLOSE(tok, tok.GenLex<"\\}">());
bf::Terminal<G> PROPERTY_DELIMITER(tok, tok.GenLex<",">());
bf::Terminal<G> PROPERTY_SEPERATOR(tok, tok.GenLex<":">());

extern bf::NonTerminal<G> json_object;

bf::NonTerminal<G> json_basic_value
    = bf::ProductionRule(STRING)<=>[](auto &$) { return $[0]; }
    | bf::ProductionRule(NUMBER)<=>[](auto &$) { return $[0]; }
    | bf::ProductionRule(BOOLEAN)<=>[](auto &$) { return $[0]; }
    | bf::ProductionRule(json_object)<=>[](auto &$) { return $[0]; };

bf::NonTerminal<G> property
    = (STRING + PROPERTY_SEPERATOR + json_basic_value)<=>[](auto &$) -> ValueType
    {
        return std::pair<std::string, badjson::json> {
            std::get<std::string>(std::get<badjson::json>($[0]).value),
            std::get<badjson::json>($[2]),
        };
    };

bf::NonTerminal<G> property_list
    = bf::ProductionRule(property)<=>[](auto &$) -> ValueType
    {
        return std::vector<std::pair<std::string, badjson::json>> { std::get<std::pair<std::string, badjson::json>>($[0]) };
    }
    | (property_list + PROPERTY_DELIMITER + property)<=>[](auto &$) -> ValueType
    {
        auto list = std::get<std::vector<std::pair<std::string, badjson::json>>>($[0]);
        list.push_back(std::get<std::pair<std::string, badjson::json>>($[2]));
        return list;
    };

bf::NonTerminal<G> json_object
    = (OBJ_OPEN + OBJ_CLOSE)<=>[](auto &$) -> ValueType { return badjson::json{}; }
    | (OBJ_OPEN + property_list + OBJ_CLOSE)<=>[](auto &$) -> ValueType
    {
        std::map<std::string, badjson::json> values;

        auto list = std::get<std::vector<std::pair<std::string, badjson::json>>>($[1]);
        for(auto const &[name, value] : list)
        {
            values[name] = value;
        }

        return badjson::json { .value = values };
    };

bf::NonTerminal<G> document
    = bf::ProductionRule(json_basic_value)<=>[](auto &$) { return $[0]; };

bf::Grammar grammar(tok, document);
bf::SLRParser json(grammar);

TEST(BadJSON, SimpleDocument)
{
    auto obj = json.Parse(R"(
        {
            "My Key": "My Value",
            "Number": 32.5,
            "boolean": false
        }
    )");

    ASSERT_TRUE(obj);
}