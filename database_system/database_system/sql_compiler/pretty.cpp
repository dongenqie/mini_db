// =============================================
// sql_compiler/pretty.cpp
// =============================================
#include "pretty.h"
#include <sstream>

namespace minidb {

    // 将 TokenType + 词面，映射到课程“种别码”
    int LexCategoryCode(TokenType t, const std::string& lexeme_upper) {
        switch (t) {
        case TokenType::KEYWORD:   return 1;
        case TokenType::IDENT:     return 2;
        case TokenType::INTCONST:
        case TokenType::STRCONST:  return 3;
        case TokenType::PLUS: case TokenType::MINUS:
        case TokenType::STAR: case TokenType::SLASH: case TokenType::PERCENT:
        case TokenType::EQ: case TokenType::NEQ: case TokenType::LT:
        case TokenType::LE: case TokenType::GT: case TokenType::GE:
            return 4; // 运算符
        case TokenType::COMMA: case TokenType::SEMI: case TokenType::DOT:
        case TokenType::LPAREN: case TokenType::RPAREN:
        case TokenType::LBRACE: case TokenType::RBRACE:
        case TokenType::LBRACKET: case TokenType::RBRACKET:
            return 5; // 界符
        case TokenType::COMMENT: return 6;       // 注释
        default: return 99;
        }
    }

    static const char* DtName(minidb::DataType dt) {
        using minidb::DataType;
        switch (dt) {
        case DataType::INT32:     return "INT";
        case DataType::TINYINT:   return "TINYINT";
        case DataType::FLOAT:     return "FLOAT";
        case DataType::CHAR:      return "CHAR";
        case DataType::VARCHAR:   return "VARCHAR";
        case DataType::DECIMAL:   return "DECIMAL";
        case DataType::TIMESTAMP: return "TIMESTAMP";
        default:                  return "VARCHAR";
        }
    }


    // 打印四元式 [种别码, "词素", 行, 列]
    void PrintTokenQuads(const std::string& sql, int start_line, std::ostream& os) {
        // 你的 Lexer 支持 (sql, start_line) 构造
        Lexer lx(sql, start_line, /*start_col*/0, /*keep_comments*/true);
        while (true) {
            Token t = lx.next();
            if (t.type == TokenType::END) break;
            if (t.type == TokenType::INVALID) {
                os << "[LEX ERROR, \"" << t.lexeme << "\", " << t.line << ", " << t.col << "]\n";
                break;
            }
            // 词素大写副本，用于关键字分类显示（不改变原词素）
            std::string up = t.lexeme;
            for (auto& ch : up) ch = std::toupper((unsigned char)ch);

            os << "[" << LexCategoryCode(t.type, up) << ", \"" << t.lexeme
                << "\", " << t.line << ", " << t.col << "]\n";
        }
    }


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
        case TokenType::COMMENT: return "COMMENT"; 
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
                << "\", " << t.line << ", " << t.col << ")\n";
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
                os << "(" << Quote(c.name) << " " << DtName(c.type) << ")";
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
        else if (auto up = dynamic_cast<const UpdateStmt*>(s)) {
            os << "(update " << Quote(up->table) << " (set ";
            for (size_t i = 0; i < up->sets.size(); ++i) {
                os << "(" << Quote(up->sets[i].first) << " " << ExprSexpr(up->sets[i].second.get()) << ")";
                if (i + 1 < up->sets.size()) os << " ";
            }
            os << ")";
            if (up->where) os << " (where " << ExprSexpr(up->where.get()) << ")";
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

    // JSON 字符串转义
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

    static const char* OpName(PlanOp op) {
        switch (op) {
        case PlanOp::CREATE:  return "CreateTable";
        case PlanOp::INSERT:  return "Insert";
        case PlanOp::SEQSCAN: return "SeqScan";
        case PlanOp::FILTER:  return "Filter";
        case PlanOp::PROJECT: return "Project";
        case PlanOp::DELETE_: return "Delete";
        case PlanOp::DROP:    return "Drop";
        case PlanOp::UPDATE:  return "Update";
        default:              return "Error";
        }
    }

    // —— JSON 渲染 —— //
    static void to_json(const PlanNode* n, std::ostream& os, int ind) {
        auto pad = [&](int k) { for (int i = 0; i < k; ++i) os << ' '; };
        pad(ind); os << "{ \"type\": \"" << OpName(n->op) << "\"";

        switch (n->op) {
        case PlanOp::CREATE:
            os << ", \"create\": { \"name\": \"" << n->create_def.name << "\", \"cols\": [";
            for (size_t i = 0; i < n->create_def.columns.size(); ++i) {
                auto& c = n->create_def.columns[i];
                os << "{ \"name\": \"" << c.name << "\", \"type\": \"" << DtName(c.type) << "\" }";
                if (i + 1 < n->create_def.columns.size()) os << ", ";
            }
            os << "] }";
            break;

        case PlanOp::INSERT:
            os << ", \"table\": \"" << n->table << "\"";
            os << ", \"columns\": [";
            for (size_t i = 0; i < n->insert_cols.size(); ++i) {
                os << '\"' << n->insert_cols[i] << '\"';
                if (i + 1 < n->insert_cols.size()) os << ", ";
            }
            os << "]";
            os << ", \"values\": [";
            for (size_t i = 0; i < n->insert_values.size(); ++i) {
                auto* e = n->insert_values[i].get();
                if (auto ii = dynamic_cast<IntLit*>(e)) os << ii->v;
                else if (auto ss = dynamic_cast<StrLit*>(e)) os << '\"' << ss->v << '\"';
                else if (auto cc = dynamic_cast<ColRef*>(e)) os << '\"' << cc->name << '\"';
                else os << "\"expr\"";
                if (i + 1 < n->insert_values.size()) os << ", ";
            }
            os << "]";
            break;

        case PlanOp::SEQSCAN:
            os << ", \"table\": \"" << n->table << "\"";
            break;

        case PlanOp::FILTER:
            os << ", \"filter\": \"(predicate)\""; // 简化显示
            break;

        case PlanOp::PROJECT:
            os << ", \"project\": [";
            for (size_t i = 0; i < n->project.size(); ++i) {
                os << '\"' << n->project[i] << '\"';
                if (i + 1 < n->project.size()) os << ", ";
            }
            os << "]";
            break;

        case PlanOp::DELETE_:
            os << ", \"table\": \"" << n->table << "\"";
            if (n->predicate) os << ", \"filter\": \"(predicate)\"";
            break;
        case PlanOp::ERROR:
            os << ", \"message\": \"" << n->error_msg << "\"";
            break;
        case PlanOp::DROP:
            os << ", \"table\": \"" << n->table << "\"";
            break;
        case PlanOp::UPDATE:
            os << ", \"table\": \"" << n->table << "\"";
            // sets
            os << ", \"sets\": [";
            for (size_t i = 0; i < n->update_sets.size(); ++i) {
                const auto& kv = n->update_sets[i];
                os << "{ \"col\": \"" << kv.first << "\", \"val\": ";
                if (auto ii = dynamic_cast<IntLit*>(kv.second.get()))       os << ii->v;
                else if (auto ss = dynamic_cast<StrLit*>(kv.second.get()))  os << '\"' << JsonEscape(ss->v) << '\"';
                else if (auto cc = dynamic_cast<ColRef*>(kv.second.get()))  os << '\"' << JsonEscape(cc->name) << '\"';
                else os << "\"expr\"";
                os << " }";
                if (i + 1 < n->update_sets.size()) os << ", ";
            }
            os << "]";
            if (n->predicate) os << ", \"filter\": \"(predicate)\"";
            break;
        }

        if (!n->children.empty()) {
            os << ", \"children\": [\n";
            for (size_t i = 0; i < n->children.size(); ++i) {
                to_json(n->children[i].get(), os, ind + 2);
                if (i + 1 < n->children.size()) os << ",\n";
            }
            os << "\n"; pad(ind); os << "]";
        }
        os << " }";
    }

    // —— S-表达式渲染 —— //
    static void to_sexpr(const PlanNode* n, std::ostream& os) {
        os << "(" << OpName(n->op);
        switch (n->op) {
        case PlanOp::CREATE:
            os << " \"" << n->create_def.name << "\" (";
            for (size_t i = 0; i < n->create_def.columns.size(); ++i) {
                auto& c = n->create_def.columns[i];
                os << "(" << c.name << " " << DtName(c.type) << ")";
                if (i + 1 < n->create_def.columns.size()) os << " ";
            }
            os << ")";
            break;
        case PlanOp::INSERT:
            os << " \"" << n->table << "\" (";
            for (size_t i = 0; i < n->insert_cols.size(); ++i) {
                os << n->insert_cols[i];
                if (i + 1 < n->insert_cols.size()) os << " ";
            }
            os << ") (";
            for (size_t i = 0; i < n->insert_values.size(); ++i) {
                auto* e = n->insert_values[i].get();
                if (auto ii = dynamic_cast<IntLit*>(e)) os << ii->v;
                else if (auto ss = dynamic_cast<StrLit*>(e)) os << "\"" << ss->v << "\"";
                else if (auto cc = dynamic_cast<ColRef*>(e)) os << cc->name;
                else os << "expr";
                if (i + 1 < n->insert_values.size()) os << " ";
            }
            os << ")";
            break;
        case PlanOp::SEQSCAN:  os << " \"" << n->table << "\""; break;
        case PlanOp::FILTER:   os << " (predicate)"; break;
        case PlanOp::PROJECT:
            os << " (";
            for (size_t i = 0; i < n->project.size(); ++i) {
                os << n->project[i];
                if (i + 1 < n->project.size()) os << " ";
            }
            os << ")";
            break;
        case PlanOp::DELETE_:
            os << " \"" << n->table << "\"";
            if (n->predicate) os << " (predicate)";
            break;
        case PlanOp::DROP:
            os << " \"" << n->table << "\"";
            break;
        case PlanOp::UPDATE:
            os << " \"" << n->table << "\" (";
            for (size_t i = 0; i < n->update_sets.size(); ++i) {
                const auto& kv = n->update_sets[i];
                os << "(" << kv.first << " ";
                if (auto ii = dynamic_cast<IntLit*>(kv.second.get()))       os << ii->v;
                else if (auto ss = dynamic_cast<StrLit*>(kv.second.get()))  os << "\"" << ss->v << "\"";
                else if (auto cc = dynamic_cast<ColRef*>(kv.second.get()))  os << cc->name;
                else os << "expr";
                os << ")";
                if (i + 1 < n->update_sets.size()) os << " ";
            }
            os << ")";
            if (n->predicate) os << " (predicate)";
            break;
        case PlanOp::ERROR:
            os << " \"ERROR: " << n->error_msg << "\"";
            break;
        }
        for (auto& ch : n->children) {
            os << " ";
            to_sexpr(ch.get(), os);
        }
        os << ")";
    }

    void PrintPlan(const Plan& p, std::ostream& os, const char* fmt) {
        if (!p.root) { os << "(plan nil)\n"; return; }
        if (std::string(fmt) == "sexpr") {
            to_sexpr(p.root.get(), os);
            os << "\n";
        }
        else {
            // 默认 JSON
            to_json(p.root.get(), os, 0);
            os << "\n";
        }
    }

} // namespace minidb