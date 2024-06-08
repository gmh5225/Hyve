#pragma once

#include "typeck/HSymbol.hxx"
#include "typeck/HType.hxx"
#include <memory>

namespace Hyve::Typeck {
    struct HVariableSymbol : public HSymbol {
    public:
        std::shared_ptr<HType> Type;
    };
}