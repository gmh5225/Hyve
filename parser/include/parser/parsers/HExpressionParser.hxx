#pragma once

#include "parser/IHParser.hxx"
#include <core/HErrorHandler.hxx>
#include <memory>

namespace Hyve::AST {
	struct HAstNode;
	struct HAstBinaryExpressionNode;
	struct HAstExpressionNode;
	struct HAstFuncCallNode;
	struct HAstPropAccessNode;
	struct HAstMemberAccessNode;
	struct HAstObjectLiteralNode;
	struct HAstUnaryExpressionNode;
}

namespace Hyve::Parser {
	class HExpressionParser : public IHParser {
	public:
		explicit HExpressionParser(std::shared_ptr<Core::HErrorHandler> errorHandler);
		~HExpressionParser() final = default;

		std::shared_ptr<AST::HAstNode> Parse(Lexer::HTokenStream& stream) override;

	private:
		std::shared_ptr<Core::HErrorHandler> _errorHandler;

		// Primary expression parser
		std::shared_ptr<AST::HAstExpressionNode> ParseExpression(
			Lexer::HTokenStream& stream, 
			uint16_t precedence = 0
		);
		// Func call parser
		std::shared_ptr<AST::HAstFuncCallNode> ParseFuncCall(Lexer::HTokenStream& stream);
		// Object literal parser
		std::shared_ptr<AST::HAstObjectLiteralNode> ParseObjectLiteral(Lexer::HTokenStream& stream);
		// Unary expression parser
		std::shared_ptr<AST::HAstUnaryExpressionNode> ParseUnaryExpression(Lexer::HTokenStream& stream);
		// Binary expression parser
		std::shared_ptr<AST::HAstBinaryExpressionNode> ParseBinaryExpression(Lexer::HTokenStream& stream);
		// Property and member access parser
		std::shared_ptr<AST::HAstPropAccessNode> ParsePropAccess(Lexer::HTokenStream& stream) const;
		// Member access parser
		std::shared_ptr<AST::HAstExpressionNode> ParseMemberAccess(Lexer::HTokenStream& stream);
	};
}