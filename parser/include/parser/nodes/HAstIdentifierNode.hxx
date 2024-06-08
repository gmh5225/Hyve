#pragma once

#include "parser/HAstNode.hxx"
#include "parser/nodes/HAstExpressionNode.hxx"
#include "parser/HAstNodeType.hxx"
#include <memory>
#include <string>
#include <vector>

namespace Hyve::Parser {
    struct HAstIdentifierNode : HAstExpressionNode {
        std::string Name;

        HAstIdentifierNode() {
            ExpressionType = ExpressionType::Identifier;
        }
    };
}