#include "parser/IHParser.hxx"
#include "parser/HParser.hxx"
#include "parser/parsers/HClassParser.hxx"
#include "parser/parsers/HEnumParser.hxx"
#include "parser/parsers/HExpressionParser.hxx"
#include "parser/parsers/HFuncParser.hxx"
#include "parser/parsers/HInheritanceParser.hxx"
#include "parser/parsers/HInitParser.hxx"
#include "parser/parsers/HModuleParser.hxx"
#include "parser/parsers/HPropertyParser.hxx"
#include "parser/parsers/HProtocolParser.hxx"
#include "parser/parsers/HPrototypeParser.hxx"
#include "parser/parsers/HStructParser.hxx"
#include "parser/parsers/HVariableParser.hxx"
#include <ast/HAstOperatorType.hxx>
#include <ast/HAstLiteralType.hxx>
#include <ast/nodes/HAstArrayNode.hxx>
#include <ast/nodes/HAstInheritanceNode.hxx>
#include <ast/nodes/HAstLiteralNode.hxx>
#include <lexer/HToken.hxx>
#include <core/HCompilerError.hxx>
#include <core/HErrorHandler.hxx>
#include <sstream>
#include <string_view>

namespace Hyve::Parser {
	using namespace AST;

	void IHParser::Panic(Lexer::HTokenStream& stream, Lexer::HTokenType type) const {
		if (stream.IsEmpty()) {
			return;
		}
		while(stream.Peek().Type != type && !stream.IsEmpty()) {
			stream.Consume();
		}
	}

	void IHParser::Panic(Lexer::HTokenStream& stream, Lexer::HTokenFamily family) const {
		if (stream.IsEmpty()) {
			return;
		}
		while (stream.Peek().Family != family && !stream.IsEmpty()) {
			stream.Consume();
		}
	}

	bool IHParser::CanStartStatement(const Lexer::HToken& token) const {
		using enum Lexer::HTokenType;

		switch (token.Type) {
			case RETURN:
			case VAR:
			case LET:
			case IDENTIFIER:
			case SELF:
			case DEALLOC:
				return true;
			default:
				return false;
		}
	}

	bool IHParser::CanBeInExpression(const Lexer::HToken& token) const {
		using enum Lexer::HTokenType;

		switch (token.Type) {
			case IDENTIFIER:
			case STRING:
			case TRUE:
			case FALSE:
			case NULL_LITERAL:
			case INTEGER:
			case FLOAT:
			case BOOLEAN:
			case ALLOC:
				return true;
			default:
				return false;
		}
	}

	bool IHParser::IsAccessLevel(Lexer::HTokenStream& stream) const {
		using enum Lexer::HTokenType;

		auto token = stream.PeekUntilNonLineBreak();

		switch (token.Type) {
			case PUBLIC:
			case PRIVATE:
			case INTERNAL:
			case FILEPRIVATE:
				return true;
			default:
				return false;
		}
	}

	bool IHParser::IsExpression(const std::array<Lexer::HToken, 2>& tokens) const {
		using enum Lexer::HTokenType;
		using enum Lexer::HTokenFamily;

		if (!CanBeInExpression(tokens.front())) {
			return false;
		}

		// What is an expression?
		// - Literals
		// - Function calls
		// - Identifiers
		// Followed by:
		// - Operators(+, -, *, /, %)
		// - Parentheses("(")
		const Lexer::HToken& next = tokens.back();

		// Simple: If we start with an identifier or a literal this is an expression
		if (tokens.front().Family == LITERAL || tokens.front().Type == IDENTIFIER) {
			return true;
		}

		if(tokens.front().Type == ALLOC) {
			return true;
		}

		return false;
	}

	bool IHParser::IsStatement(const std::array<Lexer::HToken, 2>& tokens) const {
		using enum Lexer::HTokenType;

		if (!CanStartStatement(tokens.front())) {
			return false;
		}

		if(tokens.front().Type == VAR || tokens.front().Type == LET || tokens.front().Type == RETURN) {
			return true;
		}

		if (tokens.front().Type == IDENTIFIER) {
			// Check if the next token is an assignment operator
			const Lexer::HToken& next = tokens.back();

			if (next.Type == ASSIGNMENT) {
				return true;
			} else if(next.Type == PLUS_ASSIGN) {
				return true;
			} else if(next.Type == MINUS_ASSIGN) {
				return true;
			} else if(next.Type == MULTIPLY_ASSIGN) {
				return true;
			} else if(next.Type == DIVIDE_ASSIGN) {
				return true;
			}
		} else if(tokens.front().Type == RETURN) {
			return true;
		}

		return false;
	}

	bool IHParser::IsEnum(Lexer::HTokenStream& stream) const {
		using enum Lexer::HTokenType;
		
		auto token = stream.PeekUntilNonLineBreak();

		if (token.Type == PUBLIC || token.Type == PRIVATE || token.Type == INTERNAL) {
			// We need to skip the access modifier, as it is optional. Peek two tokens ahead
			auto tokens = stream.Peek(2);

			if (tokens[1].Type == ENUM) {
				return true;
			}
		}

		if (token.Type == ENUM) {
			return true;
		}

		return false;
	}

	bool IHParser::IsFunc(Lexer::HTokenStream& stream) const {
		using enum Lexer::HTokenType;
		auto token = stream.PeekUntilNonLineBreak();

		if (token.Type == PUBLIC || token.Type == PRIVATE || token.Type == INTERNAL) {
			// We need to skip the access modifier, as it is optional. Peek two tokens ahead
			auto tokens = stream.Peek(3);

			if (tokens[1].Type == FUNC) {
				return true;
			}

			if (tokens[1].Type == STATIC && tokens[2].Type == FUNC) {
				return true;
			}
		}

		if (token.Type == FUNC) {
			return true;
		}

		return false;
	}
	bool IHParser::IsInit(Lexer::HTokenStream& stream) const {
		using enum Lexer::HTokenType;
		auto token = stream.PeekUntilNonLineBreak();

		if (token.Type == PUBLIC || token.Type == PRIVATE || token.Type == INTERNAL) {
			// We need to skip the access modifier, as it is optional. Peek two tokens ahead
			auto tokens = stream.Peek(2);

			if (tokens[1].Type == INIT) {
				return true;
			}
		}

		if (token.Type == INIT) {
			return true;
		}

		return false;
	}

	bool IHParser::IsProperty(Lexer::HTokenStream& stream) const {
		using enum Lexer::HTokenType;
		using enum HParserContext;
		
		auto token = stream.PeekUntilNonLineBreak();

		if (GetContext() != Class && GetContext() != Struct) {
			return false;
		}

		if (token.Type == PUBLIC || token.Type == PRIVATE || token.Type == INTERNAL) {
			// We need to skip the access modifier, as it is optional. Peek two tokens ahead
			auto tokens = stream.Peek(3);

			if (tokens[1].Type == VAR || tokens[1].Type == LET) {
				return true;
			}

			if (tokens[1].Type == STATIC && (tokens[1].Type == VAR || tokens[1].Type == LET)) {
				return true;
			}
		}

		if (token.Type == VAR || token.Type == LET) {
			return true;
		}

		return false;
	}

	bool IHParser::IsContract(Lexer::HTokenStream& stream) const {
		using enum Lexer::HTokenType;
		
		auto token = stream.PeekUntilNonLineBreak();

		if (token.Type == PUBLIC || token.Type == PRIVATE || token.Type == INTERNAL) {
			// We need to skip the access modifier, as it is optional. Peek two tokens ahead
			auto tokens = stream.Peek(2);

			if (tokens[1].Type == CONTRACT) {
				return true;
			}
		}

		if (token.Type == CONTRACT) {
			return true;
		}

		return false;
	}

	bool IHParser::IsStruct(Lexer::HTokenStream& stream) const {
		using enum Lexer::HTokenType;
		
		auto token = stream.PeekUntilNonLineBreak();

		if (token.Type == PUBLIC || token.Type == PRIVATE || token.Type == INTERNAL) {
			// We need to skip the access modifier, as it is optional. Peek two tokens ahead
			auto tokens = stream.Peek(2);

			if (tokens[1].Type == STRUCT) {
				return true;
			}
		}
		if (token.Type == STRUCT) {
			return true;
		}

		return false;
	}

	bool IHParser::IsVariable(Lexer::HTokenStream& stream) const {
		using enum Lexer::HTokenType;
		using enum HParserContext;
		
		auto token = stream.PeekUntilNonLineBreak();

		if (GetContext() == Class || GetContext() == Struct) {
			return false;
		}

		if (token.Type == VAR || token.Type == LET) {
			return true;
		}

		return false;
	}

	std::int8_t IHParser::GetOperatorPrecedence(HAstOperatorType type) const {
		using enum HAstOperatorType;

		switch (type) {
			case MULTIPLY:
			case DIVIDE:
			case MODULO:  // Modulo operator
				return 14;

			case ADD:
			case SUBTRACT:
				return 12;

			case BITWISE_LEFT_SHIFT:
			case BITWISE_RIGHT_SHIFT:
				return 10;  // Bit shift operators

			case LESS_THAN:
			case GREATER_THAN:
			case LESS_THAN_OR_EQUAL:
			case GREATER_THAN_OR_EQUAL:
				return 8;

			case EQUAL:
			case NOT_EQUAL:
				return 6;

			case BITWISE_AND:
				return 4;

			case BITWISE_XOR:
				return 3;

			case BITWISE_OR:
				return 2;

			case BITWISE_NOT:
				return 1;   // Bitwise inversion operator

			default:
				return 0;   // Non-operator tokens have the lowest precedence
		}
	}

	bool IHParser::IsOperator(Lexer::HTokenStream& stream) const {
		using enum Lexer::HTokenType;

		auto token = stream.PeekUntilNonLineBreak();
		auto type = GetOperatorType(token);

		return IsStatementOperator(type) || IsUnaryOperator(type) || IsBinaryOperator(type);
	}

	bool IHParser::IsStatementOperator(HAstOperatorType type) const {
		using enum HAstOperatorType;

		switch (type) {
			case ASSIGN:
			case ADD_ASSIGN:
			case SUBTRACT_ASSIGN:
			case MULTIPLY_ASSIGN:
			case DIVIDE_ASSIGN:
				return true;
			default:
				return false;
		}
	}

	bool IHParser::IsUnaryOperator(HAstOperatorType type) const {
		using enum HAstOperatorType;

		switch (type) {
			case NOT:
			case ADD:
			case SUBTRACT:
			case INCREMENT:
			case DECREMENT:
			case NEGATE:
				return true;
			default:
				return false;
		}
	}

	bool IHParser::IsBinaryOperator(HAstOperatorType type) const {
		using enum HAstOperatorType;

		switch (type) {
			case ADD:
			case SUBTRACT:
			case MULTIPLY:
			case DIVIDE:
			case MODULO:
			case LESS_THAN:
			case GREATER_THAN:
			case EQUAL:
			case NOT_EQUAL:
			case AND:
			case OR:
			case BITWISE_AND:
			case BITWISE_OR:
			case BITWISE_XOR:
			case BITWISE_LEFT_SHIFT:
			case BITWISE_RIGHT_SHIFT:
				return true;
			default:
				return false;
		}
	}

	HAstOperatorType IHParser::GetOperatorType(const Lexer::HToken& token) const {
		using enum Lexer::HTokenType;

		switch (token.Type) {
			case MULTIPLY:
				return HAstOperatorType::MULTIPLY;
			case DIVIDE:
				return HAstOperatorType::DIVIDE;
			case MODULO:
				return HAstOperatorType::MODULO;
			case PLUS:
				return HAstOperatorType::ADD;
			case MINUS:
				return HAstOperatorType::SUBTRACT;
			case LESS:
				return HAstOperatorType::LESS_THAN;
			case GREATER:
				return HAstOperatorType::GREATER_THAN;
			case EQUAL:
				return HAstOperatorType::EQUAL;
			default:
				return HAstOperatorType::INVALID_OPERATOR;
		}
	}


	std::shared_ptr<HAstExpressionNode> IHParser::ParseLiteral(Lexer::HTokenStream& stream) const {
		using enum Lexer::HTokenType;
		using enum AST::HAstLiteralType;

		auto token = stream.Consume();
		auto lit = std::make_shared<HAstLiteralNode>();
		// Set the type
		switch (token.Type) {
			case INTEGER:
				lit->LiteralType = Integer;
				break;
			case FLOAT:
				lit->LiteralType = Float;
				break;
			case BOOLEAN:
				lit->LiteralType = Boolean;
				break;
			case STRING:
				lit->LiteralType = String;
				break;
			case NULL_LITERAL:
				lit->LiteralType = Null;
				break;
			default:
				throw Core::HCompilerError(
					Core::HCompilerError::ErrorCode::UnexpectedToken,
					token.FileName,
					token.Line
				);
		}
		lit->Value = token.Value;

		return lit;
	}

	std::shared_ptr<HAstLiteralNode> IHParser::ParseString(std::string_view literal) const {
		using enum AST::HAstLiteralType;


		auto ast = std::make_shared<HAstLiteralNode>();
		ast->LiteralType = String;
		ast->Value = literal;

		return ast;
	}

	Core::HAccessLevel IHParser::IHParser::ParseAccessLevel(Lexer::HTokenStream& stream) const {
		using enum Lexer::HTokenType;
		using enum Core::HAccessLevel;

		// Safeguard to we indeed have an access level
		if(!IsAccessLevel(stream)) {
			return Internal;
		}

		auto token = stream.Consume();

		switch (token.Type) {
			case PUBLIC:
				return Public;
			case PRIVATE:
				return Private;
			case INTERNAL:
				return Internal;
			case FILEPRIVATE:
				return Fileprivate;
			default:
				// This should never happen
				return Internal;
		}
	}

	std::shared_ptr<HAstTypeNode> IHParser::ParseType(
		Lexer::HTokenStream& stream, 
		std::shared_ptr<HAstTypeNode> parent
	) {
		auto token = stream.Consume();

		// We expect an identifier -> Regular type
		if (token.Type == Lexer::HTokenType::IDENTIFIER) {
			auto type = std::make_shared<HAstTypeNode>();
			type->Name = token.Value;

			if (parent != nullptr) {
				if (stream.Peek().Type == Lexer::HTokenType::BRACKET_RIGHT && parent->Name == "Array") {
					stream.Consume();
					std::shared_ptr<HAstArrayNode> array = dynamic_pointer_cast<HAstArrayNode>(parent);
					array->ItemType = type;
					type = array;
				}
				else {
					throw Core::HCompilerError(
						Core::HCompilerError::ErrorCode::TypeMismatch,
						token.FileName,
						token.Line
					);
				}
			}

			return type;
		}
		// We expect an open bracket -> Array type
		else if (token.Type == Lexer::HTokenType::BRACKET_LEFT) {
			auto array = std::make_shared<HAstArrayNode>();
			array->Name = "Array";
			array->ItemType = ParseType(stream, array);
			return array;
		}
		else {
			throw Core::HCompilerError(
				Core::HCompilerError::ErrorCode::TypeMismatch,
				token.FileName,
				token.Line
			);
		}
	}

	HParserContext IHParser::GetContext() const {
		return _context;
	}

	void IHParser::SetContext(HParserContext ctx) {
		_context = ctx;
	}

	std::unique_ptr<IHParser> Create() {
		auto errorHandler = std::make_shared<Core::HErrorHandler>();
		auto classParser = std::make_shared<HClassParser>();
		auto enumParser = std::make_shared<HEnumParser>();
		auto exprParser = std::make_shared<HExpressionParser>(errorHandler);
		auto varParser = std::make_shared<HVariableParser>(errorHandler, exprParser);
		auto statementParser = std::make_shared<HStatementParser>(errorHandler, exprParser, varParser);
		auto funcParser = std::make_shared<HFuncParser>(errorHandler, exprParser, statementParser);
		auto inheritanceParser = std::make_shared<HInheritanceParser>(errorHandler);
		auto initParser = std::make_shared<HInitParser>(errorHandler);
		auto propParser = std::make_shared<HPropertyParser>(errorHandler, exprParser);
		auto protocolParser = std::make_shared<HProtocolParser>();
		auto prototypeParser = std::make_shared<HPrototypeParser>();
		auto structParser = std::make_shared<HStructParser>(
			errorHandler, 
			funcParser, 
			inheritanceParser, 
			initParser,
			propParser
		);
		auto moduleParser = std::make_shared<HModuleParser>(
			errorHandler,
			classParser,
			enumParser,
			funcParser,
			propParser,
			protocolParser,
			prototypeParser,
			statementParser,
			structParser,
			varParser
		);

		return std::make_unique<Hyve::Parser::HParser>(errorHandler, moduleParser);
	}
}