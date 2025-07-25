#include "numu/core/lex.h"
#include <cctype>
#include <unordered_map>
#include <stdexcept>
#include <string_view>
#include <charconv>

namespace numu {
namespace lex {

namespace {
const std::unordered_map<std::string_view, TokenType> keywords = {
    {"let", TokenType::LET},
    {"fn", TokenType::FN},
    {"if", TokenType::IF},
    {"else", TokenType::ELSE},
    {"for", TokenType::FOR},
    {"while", TokenType::WHILE},
    {"return", TokenType::RETURN},
    {"true", TokenType::TRUE},
    {"false", TokenType::FALSE},
    {"inf", TokenType::INF},
    {"nan", TokenType::NAN},
    {"pi", TokenType::PI},
    {"e", TokenType::E}
};

const std::unordered_map<char, TokenType> single_char_tokens = {
    {'+', TokenType::PLUS},
    {'-', TokenType::MINUS},
    {'*', TokenType::STAR},
    {'/', TokenType::SLASH},
    {'%', TokenType::PERCENT},
    {'^', TokenType::CARET},
    {'=', TokenType::EQUAL},
    {'<', TokenType::LESS},
    {'>', TokenType::GREATER},
    {'!', TokenType::BANG},
    {'(', TokenType::LPAREN},
    {')', TokenType::RPAREN},
    {'[', TokenType::LBRACKET},
    {']', TokenType::RBRACKET},
    {'{', TokenType::LBRACE},
    {'}', TokenType::RBRACE},
    {',', TokenType::COMMA},
    {'.', TokenType::DOT},
    {':', TokenType::COLON},
    {';', TokenType::SEMI}
};

const std::unordered_map<std::string_view, TokenType> multi_char_ops = {
    {"==", TokenType::EQEQ},
    {"!=", TokenType::NEQ},
    {"<=", TokenType::LEQ},
    {">=", TokenType::GEQ},
    {"->", TokenType::ARROW},
    {"**", TokenType::POW}
};

bool is_identifier_start(char c) {
    return std::isalpha(c) || c == '_';
}

bool is_identifier_continue(char c) {
    return std::isalnum(c) || c == '_';
}

double parse_number(std::string_view text, size_t& pos) {
    const char* start = text.data() + pos;
    char* end;
    double value = std::strtod(start, &end);
    pos += (end - start);
    return value;
}

} // namespace

Lexer::Lexer(std::string_view source) 
    : source_(source), pos_(0), line_(1), col_(1) {}

Token Lexer::next() {
    skip_whitespace();
    if (pos_ >= source_.length()) {
        return make_token(TokenType::EOF);
    }

    char c = source_[pos_];
    
    // Handle numbers
    if (std::isdigit(c) || (c == '.' && pos_ + 1 < source_.length() && std::isdigit(source_[pos_ + 1]))) {
        return lex_number();
    }
    
    // Handle identifiers and keywords
    if (is_identifier_start(c)) {
        return lex_identifier();
    }
    
    // Handle string literals
    if (c == '"') {
        return lex_string();
    }
    
    // Handle multi-character operators
    if (pos_ + 1 < source_.length()) {
        std::string_view op = source_.substr(pos_, 2);
        auto it = multi_char_ops.find(op);
        if (it != multi_char_ops.end()) {
            pos_ += 2;
            col_ += 2;
            return make_token(it->second);
        }
    }
    
    // Handle single character tokens
    auto it = single_char_tokens.find(c);
    if (it != single_char_tokens.end()) {
        pos_++;
        col_++;
        return make_token(it->second);
    }
    
    // Handle comments
    if (c == '#') {
        skip_comment();
        return next();
    }
    
    throw_error("Unexpected character: " + std::string(1, c));
}

Token Lexer::lex_number() {
    size_t start = pos_;
    bool has_decimal = false;
    bool has_exponent = false;
    
    while (pos_ < source_.length()) {
        char c = source_[pos_];
        if (std::isdigit(c)) {
            pos_++;
            col_++;
        } else if (c == '.' && !has_decimal) {
            has_decimal = true;
            pos_++;
            col_++;
        } else if ((c == 'e' || c == 'E') && !has_exponent) {
            has_exponent = true;
            pos_++;
            col_++;
            if (pos_ < source_.length() && (source_[pos_] == '+' || source_[pos_] == '-')) {
                pos_++;
                col_++;
            }
        } else {
            break;
        }
    }
    
    std::string_view num_text = source_.substr(start, pos_ - start);
    double value;
    auto result = std::from_chars(num_text.data(), num_text.data() + num_text.size(), value);
    
    if (result.ec == std::errc::invalid_argument) {
        throw_error("Invalid number format");
    }
    
    return Token{TokenType::NUMBER, num_text, value, line_, col_ - num_text.length()};
}

Token Lexer::lex_identifier() {
    size_t start = pos_;
    while (pos_ < source_.length() && is_identifier_continue(source_[pos_])) {
        pos_++;
        col_++;
    }
    
    std::string_view text = source_.substr(start, pos_ - start);
    auto it = keywords.find(text);
    TokenType type = (it != keywords.end()) ? it->second : TokenType::IDENTIFIER;
    
    return Token{type, text, 0.0, line_, col_ - text.length()};
}

Token Lexer::lex_string() {
    pos_++; // Skip opening quote
    col_++;
    size_t start = pos_;
    
    while (pos_ < source_.length() && source_[pos_] != '"') {
        if (source_[pos_] == '\\') {
            pos_++; // Skip escape character
            col_++;
        }
        if (source_[pos_] == '\n') {
            line_++;
            col_ = 1;
        }
        pos_++;
        col_++;
    }
    
    if (pos_ >= source_.length()) {
        throw_error("Unterminated string literal");
    }
    
    std::string_view text = source_.substr(start, pos_ - start);
    pos_++; // Skip closing quote
    col_++;
    
    return Token{TokenType::STRING, text, 0.0, line_, col_ - text.length() - 2};
}

void Lexer::skip_whitespace() {
    while (pos_ < source_.length()) {
        char c = source_[pos_];
        if (c == ' ' || c == '\t') {
            pos_++;
            col_++;
        } else if (c == '\n') {
            pos_++;
            line_++;
            col_ = 1;
        } else if (c == '\r') {
            if (pos_ + 1 < source_.length() && source_[pos_ + 1] == '\n') {
                pos_ += 2;
            } else {
                pos_++;
            }
            line_++;
            col_ = 1;
        } else {
            break;
        }
    }
}

void Lexer::skip_comment() {
    while (pos_ < source_.length() && source_[pos_] != '\n') {
        pos_++;
        col_++;
    }
}

Token Lexer::make_token(TokenType type) const {
    return Token{type, std::string_view(), 0.0, line_, col_};
}

[[noreturn]] void Lexer::throw_error(const std::string& message) const {
    throw LexError{message, line_, col_};
}

} // namespace lex
} // namespace numu
