// =============================================
// sql_compiler/lexer.h �ʷ�����
// =============================================
#pragma once
#include "../utils/common.h"
#include <cctype>
#include <string>
#include <unordered_set>
#include <optional>

namespace minidb {

    // �ʷ����ֱ��롱��𣨿γ���Ԫʽ��Ҫ�ã�
    enum class LexCategory {
        KEYWORD = 1,      // 1
        IDENTIFIER = 2,   // 2
        CONSTANT = 3,     // 3
        OPERATOR = 4,     // 4
        DELIMITER = 5,    // 5
        COMMENT = 6,      // ��չ��ע��
        UNKNOWN = 99
    };

    enum class TokenType {
        // ������ʶ
        IDENT, KEYWORD,
        INTCONST, STRCONST,

        // �ָ��� & ����
        COMMA, SEMI, DOT,
        LPAREN, RPAREN,
        LBRACE, RBRACE,       // { }
        LBRACKET, RBRACKET,   // [ ]

        // ���������
        PLUS, MINUS, STAR, SLASH, PERCENT,

        // �Ƚ������
        EQ, NEQ, LT, LE, GT, GE,

        // �߼�����������Σ�AND/OR/NOT ���� ���� KEYWORD ����������ֻ����ö�ٸ���չ��
        // AND, OR, NOT,   //�������׶����� KEYWORD ����

        // ����
        COMMENT,     // �� keep_comments=true ʱ����ע�� token����������
        END,
        INVALID
    };

    struct Token {
        TokenType type{ TokenType::INVALID };
        std::string lexeme;     // ԭʼ���أ�keyword ���ǻᱣ����д��ʽ��
        int line{ 1 };   //�к���ʼ
        int col{ 0 };    //�к���ʼ
        // �ʷ����������Ԫʽ��ӡ��
        LexCategory category{ LexCategory::UNKNOWN };
    };

    bool is_keyword_upper(const std::string& up);

    // ���������¹ؼ��ֲ�ȫ��ֻ��������ʵ�֣�

    // --------- Lexer -----------
    class Lexer {
    public:
        // ---------- ���﷨���ٿ���ʹ�õı���/�ָ� ----------
        struct State {
            size_t i;
            int line;
            int col;
            bool has;
            Token la;
        };
        State save() const {
            return State{ i_, line_, col_, has_, la_ };
        }
        void restore(const State& st) {
            i_ = st.i;
            line_ = st.line;
            col_ = st.col;
            has_ = st.has;
            la_ = st.la;
        }

        // ������֧��������ʼ��/�У��Լ��Ƿ��ע����Ϊ token ����
        explicit Lexer(const std::string& input,
            int start_line = 1,
            int start_col = 0,
            bool keep_comments = false)
            : s_(input), line_(start_line), col_(start_col), keep_comments_(keep_comments) {}

        Token next();
        Token peek();

        // ����ʱ���أ��Ƿ��ע����Ϊ Token ���أ�Ĭ�� false��
        void set_keep_comments(bool k) { keep_comments_ = k; }

    private:
        // ɨ���ӹ���
        void skip_ws();                   // �����հ��루�� keep=false��ע��
        std::optional<Token> try_comment();  // ʶ��ע�ͣ��� keep=true �򷵻� COMMENT Token
        Token ident_or_kw();
        Token number();

        Token string();                 // �ɵģ�����������������
        Token string_with(char quote);  // ������֧���������ţ���/˫��
        // ͳһ�������ַ���������֧�� '...' �� "..."��
        Token quoted_string(char quote_char);
        Token make_simple(TokenType t, const char* lx, int sl, int sc, LexCategory cat);

        // �α�
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