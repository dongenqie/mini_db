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

	// ---------- Token 打印 ----------
	const char* TokName(TokenType t);
	void PrintTokens(const std::string& sql, std::ostream& os);

	// ---------- AST 打印（S-表达式） ----------
	std::string ExprSexpr(const Expr* e);
	std::string StmtSexpr(const Stmt* s);
	void PrintAST(const Stmt* s, std::ostream& os);

	// ---------- 执行计划打印（JSON 风格） ----------
	std::string PlanJson(const PlanNode* n, int indent = 0);
	void PrintPlan(const Plan& p, std::ostream& os);

} // namespace minidb