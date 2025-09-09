// =============================================
// sql_compiler/pretty.cpp
// =============================================
#include "pretty.h"
#include <sstream>

namespace minidb {

    // ---------------- Token ----------------
    const char* TokName(TokenType t) {
        switch (t) {
        case TokenType::IDENT:    return "IDENT";
        case TokenType::KEYWORD:  return "KEYWORD";
        case TokenType::INTCONST: return "INTCONST";
        case TokenType::STRCONST: return "STRCONST";
        case TokenType::COMMA:    return "COMMA";
        case TokenType::LPAREN:   return "LPAREN";
        case TokenType::RPAREN:   return "RPAREN";
        case TokenType::SEMI:     return "SEMI";
        case TokenType::STAR:     return "STAR";
        case TokenType::EQ:       return "EQ";
        case TokenType::NEQ:      return "NEQ";
        case TokenType::LT:       return "LT";
        case TokenType::GT:       return "GT";
        case TokenType::LE:       return "LE";
        case TokenType::GE:       return "GE";
        case TokenType::DOT:      return "DOT";
        case TokenType::END:      return "END";
        default:                  return "INVALID";
        }
    }

    void PrintTokens(const std::string& sql, std::ostream& os) {
        os << "TOKENS:\n";
        Lexer lx(sql);
        while (true) {
            Token t = lx.next();
            os << "  (" << TokName(t.type) << ", \"" << t.lexeme
                << "\", " << t.line << ":" << t.col << ")\n";
            if (t.type == TokenType::END) break;
            if (t.type == TokenType::INVALID) {
                os << "  [LEX ERROR] at " << t.line << ":" << t.col
                    << " : " << t.lexeme << "\n";
                break;
            }
        }
    }

    // ---------------- AST (S-Expr) ----------------
    static std::string Quote(const std::string& s) {
        std::string o = "\"";
        for (char c : s) { if (c == '"') o += "\\\""; else o += c; }
        o += "\""; return o;
    }

    std::string ExprSexpr(const Expr* e) {
        if (!e) return "nil";
        std::ostringstream os;
        if (auto c = dynamic_cast<const ColRef*>(e)) {
            os << "(col " << Quote(c->name) << ")";
        }
        else if (auto i = dynamic_cast<const IntLit*>(e)) {
            os << "(int " << i->v << ")";
        }
        else if (auto s = dynamic_cast<const StrLit*>(e)) {
            os << "(str " << Quote(s->v) << ")";
        }
        else if (auto cmp = dynamic_cast<const CmpExpr*>(e)) {
            const char* op =
                (cmp->op == CmpOp::EQ) ? "=" :
                (cmp->op == CmpOp::NEQ) ? "!=" :
                (cmp->op == CmpOp::LT) ? "<" :
                (cmp->op == CmpOp::LE) ? "<=" :
                (cmp->op == CmpOp::GT) ? ">" : ">=";
            os << "(" << op << " " << ExprSexpr(cmp->lhs.get())
                << " " << ExprSexpr(cmp->rhs.get()) << ")";
        }
        else {
            os << "(unknown-expr)";
        }
        return os.str();
    }

    std::string StmtSexpr(const Stmt* s) {
        std::ostringstream os;
        if (auto ct = dynamic_cast<const CreateTableStmt*>(s)) {
            os << "(create-table " << Quote(ct->def.name) << " (";
            for (size_t i = 0; i < ct->def.columns.size(); ++i) {
                const auto& c = ct->def.columns[i];
                os << "(" << Quote(c.name) << " "
                    << (c.type == DataType::INT32 ? "INT" : "VARCHAR") << ")";
                if (i + 1 < ct->def.columns.size()) os << " ";
            }
            os << "))";
        }
        else if (auto ins = dynamic_cast<const InsertStmt*>(s)) {
            os << "(insert " << Quote(ins->table) << " (";
            for (size_t i = 0; i < ins->columns.size(); ++i) {
                os << Quote(ins->columns[i]);
                if (i + 1 < ins->columns.size()) os << " ";
            }
            os << ") (";
            for (size_t i = 0; i < ins->values.size(); ++i) {
                os << ExprSexpr(ins->values[i].get());
                if (i + 1 < ins->values.size()) os << " ";
            }
            os << "))";
        }
        else if (auto sel = dynamic_cast<const SelectStmt*>(s)) {
            os << "(select " << (sel->star ? "*" : "");
            if (!sel->star) {
                os << "(";
                for (size_t i = 0; i < sel->columns.size(); ++i) {
                    os << Quote(sel->columns[i]);
                    if (i + 1 < sel->columns.size()) os << " ";
                }
                os << ")";
            }
            os << " (from " << Quote(sel->table) << ")";
            if (sel->where) os << " (where " << ExprSexpr(sel->where.get()) << ")";
            os << ")";
        }
        else if (auto del = dynamic_cast<const DeleteStmt*>(s)) {
            os << "(delete (from " << Quote(del->table) << ")";
            if (del->where) os << " (where " << ExprSexpr(del->where.get()) << ")";
            os << ")";
        }
        else {
            os << "(unknown-stmt)";
        }
        return os.str();
    }

    void PrintAST(const Stmt* s, std::ostream& os) {
        os << "AST (S-Expr):\n  " << StmtSexpr(s) << "\n";
    }

    // ---------------- Plan (JSON-ish) ----------------
    static std::string Indent(int n) { return std::string(n, ' '); }

    // JSON ×Ö·û´®×ªÒå
    static std::string JsonEscape(const std::string& s) {
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
            switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                }
                else {
                    out += c;
                }
            }
        }
        return out;
    }

    static const char* PlanName(PlanType t) {
        switch (t) {
        case PlanType::CREATE:  return "CreateTable";
        case PlanType::INSERT:  return "Insert";
        case PlanType::SEQSCAN: return "SeqScan";
        case PlanType::FILTER:  return "Filter";
        case PlanType::PROJECT: return "Project";
        case PlanType::DELETE:  return "Delete";
        default: return "Unknown";
        }
    }

    std::string PlanJson(const PlanNode* n, int indent) {
        std::ostringstream os;
        os << Indent(indent) << "{ \"type\": \"" << PlanName(n->type) << "\"";
        // fields
        if (!n->table.empty()) os << ", \"table\": \"" << n->table << "\"";
        if (!n->project_cols.empty()) {
            os << ", \"project\": [";
            for (size_t i = 0; i < n->project_cols.size(); ++i) {
                os << "\"" << n->project_cols[i] << "\"";
                if (i + 1 < n->project_cols.size()) os << ", ";
            }
            os << "]";
        }
        if (n->type == PlanType::CREATE) {
            os << ", \"create\": { \"name\": \"" << n->create_def.name << "\", \"cols\": [";
            for (size_t i = 0; i < n->create_def.columns.size(); ++i) {
                auto& c = n->create_def.columns[i];
                os << "{ \"name\": \"" << c.name << "\", \"type\": \""
                    << (c.type == DataType::INT32 ? "INT" : "VARCHAR") << "\" }";
                if (i + 1 < n->create_def.columns.size()) os << ", ";
            }
            os << "] }";
        }
        if (n->filter) {
            os << ", \"filter\": \"" << JsonEscape(ExprSexpr(n->filter.get())) << "\"";
        }
        // children
        if (!n->children.empty()) {
            os << ", \"children\": [\n";
            for (size_t i = 0; i < n->children.size(); ++i) {
                os << PlanJson(n->children[i].get(), indent + 2);
                if (i + 1 < n->children.size()) os << ",\n";
            }
            os << "\n" << Indent(indent) << "]";
        }
        os << " }";
        return os.str();
    }

    void PrintPlan(const Plan& p, std::ostream& os) {
        os << "PLAN:\n" << PlanJson(p.root.get(), 2) << "\n";
    }

} // namespace minidb