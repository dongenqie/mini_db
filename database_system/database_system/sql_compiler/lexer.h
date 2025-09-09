// =============================================
// sql_compiler/lexer.h 
// =============================================
#pragma once
#include "../utils/common.h"
#include <cctype>

namespace minidb {

    enum class TokenType {
        IDENT, KEYWORD, INTCONST, STRCONST,
        COMMA, LPAREN, RPAREN, SEMI, STAR,
        EQ, NEQ, LT, GT, LE, GE, DOT,
        END, INVALID
    };

    struct Token {
        TokenType type{ TokenType::INVALID };
        std::string lexeme;
        int line{ 1 };
        int col{ 1 };
    };

    class Lexer {
    public:
        explicit Lexer(const std::string& input) : s_(input) {}
        Token next();          // 消费一个
        Token peek();          // 预读一个

    private:
        void skip_ws();
        Token ident_or_kw();
        Token number();
        Token string();
        char look() const { return i_ < s_.size() ? s_[i_] : '\0'; }
        void adv();

    private:
        std::string s_;
        size_t i_{ 0 };
        int line_{ 1 };
        int col_{ 1 };
        bool has_{ false };
        Token la_{};
    };

    bool is_keyword(const std::string& upper);

} // namespace minidb