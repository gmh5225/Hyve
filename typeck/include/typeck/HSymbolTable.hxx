#pragma once

#include "typeck/HSymbol.hxx"
#include <memory>
#include <vector>

namespace Hyve::Typeck {
    struct HSymbolTable {
    public:
        HSymbolTable() = default;

        HSymbol* globalScope;
        std::vector<std::unique_ptr<HSymbol>> children;
	};
}