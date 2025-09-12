// =============================================
// sql_compiler/pretty.h
// =============================================
#pragma once
#include <string>
#include <memory>
#include <vector>
#include <iostream>

#include "lexer.h"
#include "ast.h"
#include "planner.h"

namespace minidb {

	// 四元式：种别码（1关键字/2标识符/3常量/4运算符/5界符/6注释）
	int  LexCategoryCode(TokenType t, const std::string& lexeme_upper);
	// 打印词法四元式（支持起始行）
	void PrintTokenQuads(const std::string& sql, int start_line, std::ostream& os);

	// ---------- Token 打印 ----------
	const char* TokName(TokenType t);
	void PrintTokens(const std::string& sql, std::ostream& os);

	// ---------- AST 打印（S-表达式） ----------
	std::string ExprSexpr(const Expr* e);
	std::string StmtSexpr(const Stmt* s);
	void PrintAST(const Stmt* s, std::ostream& os);

	// ---------- 执行计划打印（JSON 风格） ----------
	std::string PlanJson(const PlanNode* n, int indent = 0);
	// fmt: "json" 或 "sexpr"
	void PrintPlan(const Plan& p, std::ostream& os, const char* fmt = "json");

} // namespace minidb