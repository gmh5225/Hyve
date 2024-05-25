#pragma once

#include "parser/HAstNode.hxx"
#include "parser/nodes/HAstTypeNode.hxx"
#include <string>
#include <vector>

namespace Hyve::Parser {
    struct HAstClassNode : HAstNode {
        std::string Name;
        std::shared_ptr<HAstInheritanceNode> Inheritance;
    };
}