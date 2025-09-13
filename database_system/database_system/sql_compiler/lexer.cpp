// =============================================
// sql_compiler/lexer.cpp 词法分析
// =============================================
#include "lexer.h"
#include <cassert>
#pragma execution_character_set("utf-8")

namespace minidb {

    static std::unordered_set<std::string> KW = {
        "SELECT","FROM","WHERE","CREATE","TABLE",
        "INSERT","INTO","VALUES","DELETE",
        // DML/查询扩展
        "UPDATE","SET",
        "JOIN","INNER","LEFT","RIGHT","FULL","ON",
        "ORDER","BY","GROUP","HAVING","ASC","DESC",
        "LIMIT",
        // 新增（本次修复的关键）
        "DROP","IF","EXISTS",
        // 类型
        "INT","VARCHAR",
        // 逻辑
        "AND","OR","NOT"
    };

    bool is_keyword_upper(const std::string& up) { return KW.count(up) > 0; }

    // 工具：把 TokenType 映射为“课程四元式”的 LexCategory
    static LexCategory category_of(TokenType t, const std::string& lexeme_upper) {
        switch (t) {
        case TokenType::IDENT:     return LexCategory::IDENTIFIER;
        case TokenType::INTCONST:
        case TokenType::STRCONST:  return LexCategory::CONSTANT;

        case TokenType::PLUS: case TokenType::MINUS:
        case TokenType::STAR: case TokenType::SLASH: case TokenType::PERCENT:
        case TokenType::EQ: case TokenType::NEQ: case TokenType::LT:
        case TokenType::LE: case TokenType::GT:  case TokenType::GE:
            return LexCategory::OPERATOR;

        case TokenType::COMMA: case TokenType::SEMI: case TokenType::DOT:
        case TokenType::LPAREN: case TokenType::RPAREN:
        case TokenType::LBRACE: case TokenType::RBRACE:
        case TokenType::LBRACKET: case TokenType::RBRACKET:
            return LexCategory::DELIMITER;

        case TokenType::COMMENT:   return LexCategory::COMMENT;

        case TokenType::END:       return LexCategory::UNKNOWN;
        case TokenType::INVALID:   return LexCategory::UNKNOWN;

        case TokenType::KEYWORD:   // 关键字在 ident_or_kw 里设置
            return LexCategory::KEYWORD;
        }
        return LexCategory::UNKNOWN;
    }

    void Lexer::adv() {
        if (i_ < s_.size()) {
            if (s_[i_] == '\n') { line_++; col_ = 1; }
            else { col_++; }
            i_++;
        }
    }

    // 空白/注释：加入对 /* ... */ 的跳过
    void Lexer::skip_ws() {
        while (i_ < s_.size()) {
            char c = s_[i_];
            // -- 行注释
            if (c == '-' && i_ + 1 < s_.size() && s_[i_ + 1] == '-') {
                while (i_ < s_.size() && s_[i_] != '\n') adv();
                continue;
            }
            // /* 块注释 */
            if (c == '/' && i_ + 1 < s_.size() && s_[i_ + 1] == '*') {
                adv(); adv(); // 吃掉 "/*"
                while (i_ < s_.size()) {
                    if (s_[i_] == '*' && i_ + 1 < s_.size() && s_[i_ + 1] == '/') { adv(); adv(); break; }
                    adv();
                }
                continue;
            }
            if (isspace((unsigned char)c)) { adv(); continue; }
            break;
        }
    }

    // 如果 keep_comments_==true，并且当前位置是注释，就返回 COMMENT token；否则返回 nullopt
    std::optional<Token> Lexer::try_comment() {
        int sl = line_, sc = col_;
        // 行注释
        if (look() == '-' && look_ahead(1) == '-') {
            std::string buf;
            while (i_ < s_.size() && look() != '\n') { buf.push_back(look()); adv(); }
            return Token{ TokenType::COMMENT, buf, sl, sc };
        }
        // 块注释（不支持嵌套）
        if (look() == '/' && look_ahead(1) == '*') {
            std::string buf;
            buf.push_back(look()); adv();
            buf.push_back(look()); adv(); // "/*"
            bool closed = false;
            while (i_ < s_.size()) {
                if (look() == '*' && look_ahead(1) == '/') {
                    buf.push_back(look()); adv();
                    buf.push_back(look()); adv();
                    closed = true; break;
                }
                buf.push_back(look()); adv();
            }
            if (!closed) return Token{ TokenType::INVALID, "Unterminated block comment", sl, sc };
            return Token{ TokenType::COMMENT, buf, sl, sc };
        }
        return std::nullopt;
    }

    Token Lexer::make_simple(TokenType t, const char* lx, int sl, int sc, LexCategory cat) {
        return Token{ t, lx, sl, sc, cat };
    }

    Token Lexer::ident_or_kw() {
        int sl = line_, sc = col_;
        std::string buf;
        while (i_ < s_.size() && (std::isalnum((unsigned char)look()) || look() == '_')) {
            buf.push_back(look()); adv();
        }
        std::string up = buf;
        for (auto& ch : up) ch = std::toupper((unsigned char)ch);

        if (is_keyword_upper(up)) {
            return Token{ TokenType::KEYWORD, up, sl, sc, LexCategory::KEYWORD };
        }
        return Token{ TokenType::IDENT, buf, sl, sc, LexCategory::IDENTIFIER };
    }

    Token Lexer::number() {
        int sl = line_, sc = col_;
        std::string buf;
        while (i_ < s_.size() && std::isdigit((unsigned char)look())) {
            buf.push_back(look()); adv();
        }
        return Token{ TokenType::INTCONST, buf, sl, sc, LexCategory::CONSTANT };
    }

    // 支持：
    // 1) 反斜杠转义：\' \\ \n \t ...
    // 2) SQL 单引号转义：'' -> 一个单引号
    Token Lexer::string() {
        int sl = line_, sc = col_;
        // 当前 look()=='\''
        adv(); // 吃掉开引号
        std::string buf;
        bool closed = false;

        while (i_ < s_.size()) {
            char c = look();
            if (c == '\\') {
                // C 风格转义
                adv();
                char e = look();
                switch (e) {
                case 'n': buf.push_back('\n'); break;
                case 't': buf.push_back('\t'); break;
                case '\\': buf.push_back('\\'); break;
                case '\'': buf.push_back('\''); break;
                default: buf.push_back(e); break;
                }
                adv();
                continue;
            }
            if (c == '\'') {
                // SQL 风格：'' -> '
                if (look_ahead(1) == '\'') {
                    buf.push_back('\''); adv(); adv();
                    continue;
                }
                // 关闭
                adv();
                closed = true;
                break;
            }
            buf.push_back(c);
            adv();
        }

        if (!closed) {
            return Token{ TokenType::INVALID, "Unterminated string", sl, sc, LexCategory::CONSTANT };
        }
        return Token{ TokenType::STRCONST, buf, sl, sc, LexCategory::CONSTANT };
    }

    Token Lexer::next() {
        if (has_) { has_ = false; return la_; }

        if (keep_comments_) {
            // 保留注释：先跳过空白（不跨过注释），再尝试读注释
            while (i_ < s_.size() && std::isspace((unsigned char)look())) adv();
            if (auto com = try_comment(); com.has_value()) return *com;
        }
        else {
            skip_ws();
        }

        if (i_ >= s_.size()) return { TokenType::END, "", line_, col_ };

        if (keep_comments_) { // 再尝试一次注释（例如行首就是注释）
            if (auto com = try_comment(); com.has_value()) return *com;
        }

        int sl = line_, sc = col_;
        char c = look();

        // 标识符/关键字
        if (std::isalpha((unsigned char)c) || c == '_') {
            return ident_or_kw();
        }

        // 常数（仅整数）
        if (std::isdigit((unsigned char)c)) {
            return number();
        }

        // 单字符 / 双字符运算符 & 分隔符
        switch (c) {
        case ',': adv(); return make_simple(TokenType::COMMA, ",", sl, sc, LexCategory::DELIMITER);
        case ';': adv(); return make_simple(TokenType::SEMI, ";", sl, sc, LexCategory::DELIMITER);
        case '.': adv(); return make_simple(TokenType::DOT, ".", sl, sc, LexCategory::DELIMITER);
        case '(': adv(); return make_simple(TokenType::LPAREN, "(", sl, sc, LexCategory::DELIMITER);
        case ')': adv(); return make_simple(TokenType::RPAREN, ")", sl, sc, LexCategory::DELIMITER);
        case '{': adv(); return make_simple(TokenType::LBRACE, "{", sl, sc, LexCategory::DELIMITER);
        case '}': adv(); return make_simple(TokenType::RBRACE, "}", sl, sc, LexCategory::DELIMITER);
        case '[': adv(); return make_simple(TokenType::LBRACKET, "[", sl, sc, LexCategory::DELIMITER);
        case ']': adv(); return make_simple(TokenType::RBRACKET, "]", sl, sc, LexCategory::DELIMITER);

        case '+': adv(); return make_simple(TokenType::PLUS, "+", sl, sc, LexCategory::OPERATOR);
        case '-':
            if (look_ahead(1) == '-' && keep_comments_ == false) {
                // 行注释（在 keep=false 且 skip_ws 没吃掉的极端情形），吃到行尾并再取 token
                while (i_ < s_.size() && look() != '\n') adv();
                return next();
            }
            else {
                adv(); return make_simple(TokenType::MINUS, "-", sl, sc, LexCategory::OPERATOR);
            }
        case '*': adv(); return make_simple(TokenType::STAR, "*", sl, sc, LexCategory::OPERATOR);
        case '/':
            if (look_ahead(1) == '*' && keep_comments_ == false) {
                // 块注释；吃掉再 next（理论上 skip_ws 已经处理）
                adv(); adv();
                while (i_ < s_.size()) {
                    if (look() == '*' && look_ahead(1) == '/') { adv(); adv(); break; }
                    adv();
                }
                return next();
            }
            else {
                adv(); return make_simple(TokenType::SLASH, "/", sl, sc, LexCategory::OPERATOR);
            }
        case '%': adv(); return make_simple(TokenType::PERCENT, "%", sl, sc, LexCategory::OPERATOR);

        case '=': adv(); return make_simple(TokenType::EQ, "=", sl, sc, LexCategory::OPERATOR);
        case '!':
            if (look_ahead(1) == '=') { adv(); adv(); return make_simple(TokenType::NEQ, "!=", sl, sc, LexCategory::OPERATOR); }
            break;
        case '<':
            if (look_ahead(1) == '=') { adv(); adv(); return make_simple(TokenType::LE, "<=", sl, sc, LexCategory::OPERATOR); }
            adv(); return make_simple(TokenType::LT, "<", sl, sc, LexCategory::OPERATOR);
        case '>':
            if (look_ahead(1) == '=') { adv(); adv(); return make_simple(TokenType::GE, ">=", sl, sc, LexCategory::OPERATOR); }
            adv(); return make_simple(TokenType::GT, ">", sl, sc, LexCategory::OPERATOR);

        case '\'': return string();
        }

        // 其它非法字符
        adv();
        return Token{ TokenType::INVALID, "Unexpected character", sl, sc, LexCategory::UNKNOWN };
    }

    Token Lexer::peek() {
        if (!has_) { la_ = next(); has_ = true; }
        return la_;
    }

} // namespace minidb