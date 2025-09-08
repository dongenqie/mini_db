// =============================================
// sql_compiler/lexer.cpp
// =============================================
#include "lexer.hpp"
#include <unordered_set>

namespace minidb {

    static std::unordered_set<std::string> KW = {
        "SELECT","FROM","WHERE","CREATE","TABLE",
        "INSERT","INTO","VALUES","DELETE",
        "INT","VARCHAR"
    };

    bool is_keyword(const std::string& up) { return KW.count(up) > 0; }

    void Lexer::adv() {
        if (i_ < s_.size()) {
            if (s_[i_] == '\n') { line_++; col_ = 1; }
            else { col_++; }
            i_++;
        }
    }

    void Lexer::skip_ws() {
        while (i_ < s_.size()) {
            char c = s_[i_];
            // SQL ÐÐ×¢ÊÍ: --
            if (c == '-' && i_ + 1 < s_.size() && s_[i_ + 1] == '-') {
                while (i_ < s_.size() && s_[i_] != '\n') adv();
            }
            else if (isspace((unsigned char)c)) {
                adv();
            }
            else break;
        }
    }

    Token Lexer::ident_or_kw() {
        int sl = line_, sc = col_;
        std::string buf;
        while (i_ < s_.size() && (isalnum((unsigned char)s_[i_]) || s_[i_] == '_')) {
            buf.push_back(s_[i_]); adv();
        }
        std::string up = buf; for (auto& ch : up) ch = toupper(ch);
        if (is_keyword(up)) return { TokenType::KEYWORD, up, sl, sc };
        return { TokenType::IDENT, buf, sl, sc };
    }

    Token Lexer::number() {
        int sl = line_, sc = col_;
        std::string buf;
        while (i_ < s_.size() && isdigit((unsigned char)s_[i_])) { buf.push_back(s_[i_]); adv(); }
        return { TokenType::INTCONST, buf, sl, sc };
    }

    Token Lexer::string() {
        int sl = line_, sc = col_;
        adv(); // skip opening '
        std::string buf; bool closed = false;
        while (i_ < s_.size()) {
            char c = look();
            if (c == '\\' && i_ + 1 < s_.size()) { // ×ªÒå
                adv(); buf.push_back(look()); adv();
            }
            else if (c == '\'') { adv(); closed = true; break; }
            else { buf.push_back(c); adv(); }
        }
        if (!closed) return { TokenType::INVALID, "Unterminated string", sl, sc };
        return { TokenType::STRCONST, buf, sl, sc };
    }

    Token Lexer::next() {
        if (has_) { has_ = false; return la_; }
        skip_ws();
        if (i_ >= s_.size()) return { TokenType::END, "", line_, col_ };
        int sl = line_, sc = col_;
        char c = look();
        if (isalpha((unsigned char)c) || c == '_') return ident_or_kw();
        if (isdigit((unsigned char)c))          return number();

        switch (c) {
        case ',': adv(); return { TokenType::COMMA, ",", sl, sc };
        case '(': adv(); return { TokenType::LPAREN, "(", sl, sc };
        case ')': adv(); return { TokenType::RPAREN, ")", sl, sc };
        case ';': adv(); return { TokenType::SEMI,   ";", sl, sc };
        case '*': adv(); return { TokenType::STAR,   "*", sl, sc };
        case '.': adv(); return { TokenType::DOT,    ".", sl, sc };
        case '=': adv(); return { TokenType::EQ,     "=", sl, sc };
        case '!':
            if (i_ + 1 < s_.size() && s_[i_ + 1] == '=') { adv(); adv(); return { TokenType::NEQ, "!=", sl, sc }; }
            break;
        case '<':
            if (i_ + 1 < s_.size() && s_[i_ + 1] == '=') { adv(); adv(); return { TokenType::LE, "<=", sl, sc }; }
            adv(); return { TokenType::LT, "<", sl, sc };
        case '>':
            if (i_ + 1 < s_.size() && s_[i_1] == '=') {} // ·ÀÎóÐ´
            if (i_ + 1 < s_.size() && s_[i_ + 1] == '=') { adv(); adv(); return { TokenType::GE, ">=", sl, sc }; }
            adv(); return { TokenType::GT, ">", sl, sc };
        case '\'': return string();
        }
        adv();
        return { TokenType::INVALID, "Unexpected", sl, sc };
    }

    Token Lexer::peek() {
        if (!has_) { la_ = next(); has_ = true; }
        return la_;
    }

} // namespace minidb