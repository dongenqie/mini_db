// =============================================
// sql_compiler/lexer.h
// =============================================
#pragma once
#include "../utils/common.h"
#include <cctype>
#include <string>
#include <unordered_set>
#include <optional>

namespace minidb {

    // 词法“种别码”类别（课程四元式里要用）
    enum class LexCategory {
        KEYWORD = 1,      // 1
        IDENTIFIER = 2,   // 2
        CONSTANT = 3,     // 3
        OPERATOR = 4,     // 4
        DELIMITER = 5,    // 5
        COMMENT = 6,      // 扩展：注释
        UNKNOWN = 99
    };

    enum class TokenType {
        // 基本标识
        IDENT, KEYWORD,
        INTCONST, STRCONST,

        // 分隔符 & 括号
        COMMA, SEMI, DOT,
        LPAREN, RPAREN,
        LBRACE, RBRACE,       // { }
        LBRACKET, RBRACKET,   // [ ]

        // 算术运算符
        PLUS, MINUS, STAR, SLASH, PERCENT,

        // 比较运算符
        EQ, NEQ, LT, LE, GT, GE,

        // 逻辑运算符（词形：AND/OR/NOT —— 仍以 KEYWORD 发出；这里只保留枚举给扩展）
        // AND, OR, NOT,   //（解析阶段仍以 KEYWORD 处理）

        // 其它
        COMMENT,     // 当 keep_comments=true 时返回注释 token；否则跳过
        END,
        INVALID
    };

    struct Token {
        TokenType type{ TokenType::INVALID };
        std::string lexeme;     // 原始词素（keyword 我们会保留大写形式）
        int line{ 1 };
        int col{ 1 };
        // 词法类别（用于四元式打印）
        LexCategory category{ LexCategory::UNKNOWN };
    };

    bool is_keyword_upper(const std::string& up);

    // --------- Lexer -----------
    class Lexer {
    public:
        // 默认从 1:1 开始；支持指定起始行列用于“多语句 + 正确行号”场景
        explicit Lexer(const std::string& input, int start_line = 1, int start_col = 1, bool keep_comments = false)
            : s_(input), line_(start_line), col_(start_col), keep_comments_(keep_comments) {}

        Token next();
        Token peek();

        // 运行时开关：是否把注释作为 Token 返回（默认 false）
        void set_keep_comments(bool k) { keep_comments_ = k; }

    private:
        // 扫描子过程
        void skip_ws();                   // 跳过空白与（当 keep=false）注释
        std::optional<Token> try_comment();  // 识别注释；若 keep=true 则返回 COMMENT Token
        Token ident_or_kw();
        Token number();
        Token string();
        Token make_simple(TokenType t, const char* lx, int sl, int sc, LexCategory cat);

        // 游标
        char look() const { return i_ < s_.size() ? s_[i_] : '\0'; }
        char look_ahead(size_t k) const { return (i_ + k < s_.size()) ? s_[i_ + k] : '\0'; }
        void adv();

    private:
        std::string s_;
        size_t i_{ 0 };
        int line_{ 1 };
        int col_{ 1 };

        bool has_{ false };
        Token la_{};

        bool keep_comments_{ false };
    };

} // namespace minidb