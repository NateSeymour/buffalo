//
// Created by Nathan on 9/8/2024.
//

#ifndef BUFFALO2_H
#define BUFFALO2_H

#include <variant>
#include <vector>

namespace buffalo
{
    struct Token {};

    template<typename T>
    concept IGrammar = requires(T)
    {
        typename T::ValueType;
        typename T::TransductorType;
        typename T::SemanticReasonerType;
    };

    template<typename T>
    class GrammarType
    {
    public:
        using ValueType = T;
        using TransductorType = T(*)(std::vector<T> const&);
        using SemanticReasonerType = T(*)(Token const &);

        GrammarType() = delete;
    };

#pragma region Terminal

    template<IGrammar G>
    class Terminal
    {
    protected:
        typename G::SemanticReasonerType reasoner_;

    public:

    };

    template<IGrammar G, typename SemanticType>
    class DefineTerminal : public Terminal<G>
    {
    public:
        SemanticType operator()(typename G::ValueType &value)
        {
            return std::get<SemanticType>(value);
        }

        DefineTerminal(typename G::SemanticReasonerType reasoner) : reasoner_(reasoner) {}
    };

#pragma endregion

    class NonTerminal {};

    class Parser {};
}

#endif //BUFFALO2_H
