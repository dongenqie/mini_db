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

	// ---------- Token ��ӡ ----------
	const char* TokName(TokenType t);
	void PrintTokens(const std::string& sql, std::ostream& os);

	// ---------- AST ��ӡ��S-���ʽ�� ----------
	std::string ExprSexpr(const Expr* e);
	std::string StmtSexpr(const Stmt* s);
	void PrintAST(const Stmt* s, std::ostream& os);

	// ---------- ִ�мƻ���ӡ��JSON ��� ----------
	std::string PlanJson(const PlanNode* n, int indent = 0);
	void PrintPlan(const Plan& p, std::ostream& os);

} // namespace minidb