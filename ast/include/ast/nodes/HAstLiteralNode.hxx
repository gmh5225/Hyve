#pragma once

#include "ast/HAstNode.hxx"
#include "ast/nodes/HAstExpressionNode.hxx"
#include <string>

namespace Hyve::AST {
    struct HAstLiteralNode : HAstExpressionNode {
        std::string LiteralType;
        std::string Value;
    };
}