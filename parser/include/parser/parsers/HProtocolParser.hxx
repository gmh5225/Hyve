#pragma once

#include "parser/IHParser.hxx"
#include "parser/parsers/HFuncParser.hxx"
#include "parser/parsers/HVariableParser.hxx"

namespace Hyve::Parser {
	class HProtocolParser : public IHParser {
	public:
		HProtocolParser() = default;
		~HProtocolParser() final = default;

		virtual std::shared_ptr<HAstNode> Parse(
			std::string_view fileName,
			std::vector<Lexer::HToken>& tokens
		) override;

	private:
		// Different parsers per context
		std::shared_ptr<HFuncParser> _funcParser;
		std::shared_ptr<HVariableParser> _varParser;
	};
}