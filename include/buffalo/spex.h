#ifndef SPEX_H
#define SPEX_H

#include <buffalo/buffalo.h>
#include <ctll.hpp>
#include <ctre.hpp>
#include <map>
#include <string_view>
#include <cctype>

namespace spex
{
    template<bf::IGrammar G>
    class CTRETokenizer : public bf::Tokenizer<G>
    {
        std::map<bf::Terminal<G>*, typename bf::Tokenizer<G>::LexxerType> lexxers_;

    public:
        void RegisterTerminal(bf::Terminal<G> *terminal, bf::Tokenizer<G>::LexxerType lexxer) override
        {
            this->lexxers_[terminal] = lexxer;
        }

        std::expected<bf::Token<G>, bf::Error> First(std::string_view input) const override
        {
            if(input.empty())
            {
                return bf::Token<G> {
                    .terminal = this->EOS(),
                    .location = {
                        .begin = 0,
                        .end = 0,
                    },
                };
            }

            for(auto const [terminal, lexxer] : this->lexxers_)
            {
                auto token = lexxer(terminal, input);

                if(token)
                {
                    return *token;
                }
            }

            return std::unexpected(bf::Error{});
        }

        template<ctll::fixed_string regex>
        constexpr typename bf::Tokenizer<G>::LexxerType GenLex() const
        {
            return [](bf::Terminal<G> *terminal, std::string_view input) -> std::optional<bf::Token<G>>
            {
                auto match = ctre::starts_with<regex>(input);

                if(!match)
                {
                    return std::nullopt;
                }

                return bf::Token<G> {
                    .terminal = *terminal,
                    .raw = match.view(),
                    .location = {
                        .begin = 0,
                        .end = match.size(),
                    },
                };
            };
        }
    };
}

#endif //SPEX_H
