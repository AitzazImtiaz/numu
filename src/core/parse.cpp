#include "numu/core/parse.h"
#include "numu/core/ast.h"
#include <memory>
#include <vector>
#include <stdexcept>
#include <unordered_map>
#include <algorithm>

namespace numu {
namespace parse {

namespace {
using Precedence = int;
const Precedence PREC_NONE = 0;
const Precedence PREC_ASSIGNMENT = 1;
const Precedence PREC_TERNARY = 2;
const Precedence PREC_OR = 3;
const Precedence PREC_AND = 4;
const Precedence PREC_EQUALITY = 5;
const Precedence PREC_COMPARISON = 6;
const Precedence PREC_TERM = 7;
const Precedence PREC_FACTOR = 8;
const Precedence PREC_UNARY = 9;
const Precedence PREC_POWER = 10;
const Precedence PREC_CALL = 11;
const Precedence PREC_PRIMARY = 12;

struct ParseRule {
    std::function<ast::Node*(Parser*)> prefix;
    std::function<ast::Node*(Parser*, ast::Node*)> infix;
    Precedence precedence;
};

class Parser {
public:
    Parser(lex::Lexer& lexer) : lexer_(lexer) {
        current_ = lexer_.next();
        next_ = lexer_.next();
        setup_rules();
    }

    ast::Node* parse() {
        return parse_expression();
    }

private:
    lex::Lexer& lexer_;
    lex::Token current_;
    lex::Token next_;
    std::unordered_map<lex::TokenType, ParseRule> rules_;

    void setup_rules() {
        rules_[lex::TokenType::NUMBER] = {&Parser::number, nullptr, PREC_NONE};
        rules_[lex::TokenType::IDENTIFIER] = {&Parser::variable, &Parser::call, PREC_CALL};
        rules_[lex::TokenType::STRING] = {&Parser::string, nullptr, PREC_NONE};
        rules_[lex::TokenType::LPAREN] = {&Parser::grouping, &Parser::call, PREC_CALL};
        rules_[lex::TokenType::LBRACKET] = {&Parser::matrix, nullptr, PREC_NONE};
        rules_[lex::TokenType::MINUS] = {&Parser::unary, &Parser::binary, PREC_TERM};
        rules_[lex::TokenType::PLUS] = {nullptr, &Parser::binary, PREC_TERM};
        rules_[lex::TokenType::STAR] = {nullptr, &Parser::binary, PREC_FACTOR};
        rules_[lex::TokenType::SLASH] = {nullptr, &Parser::binary, PREC_FACTOR};
        rules_[lex::TokenType::CARET] = {nullptr, &Parser::binary, PREC_POWER};
        rules_[lex::TokenType::EQUAL] = {nullptr, &Parser::assignment, PREC_ASSIGNMENT};
        rules_[lex::TokenType::EQEQ] = {nullptr, &Parser::binary, PREC_EQUALITY};
        rules_[lex::TokenType::NEQ] = {nullptr, &Parser::binary, PREC_EQUALITY};
        rules_[lex::TokenType::LESS] = {nullptr, &Parser::binary, PREC_COMPARISON};
        rules_[lex::TokenType::LEQ] = {nullptr, &Parser::binary, PREC_COMPARISON};
        rules_[lex::TokenType::GREATER] = {nullptr, &Parser::binary, PREC_COMPARISON};
        rules_[lex::TokenType::GEQ] = {nullptr, &Parser::binary, PREC_COMPARISON};
        rules_[lex::TokenType::BANG] = {&Parser::unary, nullptr, PREC_NONE};
        rules_[lex::TokenType::TRUE] = {&Parser::boolean, nullptr, PREC_NONE};
        rules_[lex::TokenType::FALSE] = {&Parser::boolean, nullptr, PREC_NONE};
        rules_[lex::TokenType::PI] = {&Parser::constant, nullptr, PREC_NONE};
        rules_[lex::TokenType::E] = {&Parser::constant, nullptr, PREC_NONE};
        rules_[lex::TokenType::INF] = {&Parser::constant, nullptr, PREC_NONE};
        rules_[lex::TokenType::NAN] = {&Parser::constant, nullptr, PREC_NONE};
    }

    void advance() {
        current_ = next_;
        next_ = lexer_.next();
    }

    void consume(lex::TokenType type, const std::string& message) {
        if (current_.type == type) {
            advance();
            return;
        }
        throw_error(message);
    }

    [[noreturn]] void throw_error(const std::string& message) const {
        throw ParseError{message, current_.line, current_.column};
    }

    bool match(lex::TokenType type) {
        if (current_.type == type) {
            advance();
            return true;
        }
        return false;
    }

    ast::Node* parse_expression(Precedence precedence = PREC_ASSIGNMENT) {
        ast::Node* expr = parse_prefix();
        while (precedence <= get_rule(current_.type).precedence) {
            expr = parse_infix(expr);
        }
        return expr;
    }

    ast::Node* parse_prefix() {
        auto rule = get_rule(current_.type);
        if (!rule.prefix) {
            throw_error("Expected expression");
        }
        return rule.prefix(this);
    }

    ast::Node* parse_infix(ast::Node* left) {
        auto rule = get_rule(current_.type);
        return rule.infix(this, left);
    }

    ParseRule get_rule(lex::TokenType type) const {
        auto it = rules_.find(type);
        if (it != rules_.end()) {
            return it->second;
        }
        return {nullptr, nullptr, PREC_NONE};
    }

    ast::Node* number() {
        double value = current_.value;
        advance();
        return ast::NumberNode::create(value);
    }

    ast::Node* string() {
        std::string value(current_.text);
        advance();
        return ast::StringNode::create(value);
    }

    ast::Node* boolean() {
        bool value = current_.type == lex::TokenType::TRUE;
        advance();
        return ast::BooleanNode::create(value);
    }

    ast::Node* constant() {
        double value;
        switch(current_.type) {
            case lex::TokenType::PI: value = 3.14159265358979323846; break;
            case lex::TokenType::E: value = 2.71828182845904523536; break;
            case lex::TokenType::INF: value = std::numeric_limits<double>::infinity(); break;
            case lex::TokenType::NAN: value = std::numeric_limits<double>::quiet_NaN(); break;
            default: throw_error("Unknown constant");
        }
        advance();
        return ast::NumberNode::create(value);
    }

    ast::Node* variable() {
        std::string name(current_.text);
        advance();
        return ast::VariableNode::create(name);
    }

    ast::Node* grouping() {
        advance();
        ast::Node* expr = parse_expression();
        consume(lex::TokenType::RPAREN, "Expect ')' after expression");
        return expr;
    }

    ast::Node* matrix() {
        advance();
        std::vector<std::vector<ast::Node*>> rows;
        
        if (!match(lex::TokenType::RBRACKET)) {
            do {
                std::vector<ast::Node*> row;
                if (match(lex::TokenType::LBRACKET)) {
                    if (!match(lex::TokenType::RBRACKET)) {
                        do {
                            row.push_back(parse_expression());
                        } while (match(lex::TokenType::COMMA));
                        consume(lex::TokenType::RBRACKET, "Expect ']' after row elements");
                    }
                } else {
                    row.push_back(parse_expression());
                }
                rows.push_back(std::move(row));
            } while (match(lex::TokenType::COMMA));
            
            consume(lex::TokenType::RBRACKET, "Expect ']' after matrix rows");
        }
        
        return ast::MatrixNode::create(std::move(rows));
    }

    ast::Node* unary() {
        lex::Token op = current_;
        advance();
        ast::Node* right = parse_expression(PREC_UNARY);
        
        switch(op.type) {
            case lex::TokenType::MINUS:
                return ast::UnaryOpNode::create(ast::UnaryOp::NEGATE, right);
            case lex::TokenType::BANG:
                return ast::UnaryOpNode::create(ast::UnaryOp::NOT, right);
            default:
                throw_error("Invalid unary operator");
        }
    }

    ast::Node* binary(ast::Node* left) {
        lex::Token op = current_;
        advance();
        
        ParseRule rule = get_rule(op.type);
        ast::Node* right = parse_expression(rule.precedence);
        
        switch(op.type) {
            case lex::TokenType::PLUS:
                return ast::BinaryOpNode::create(ast::BinaryOp::ADD, left, right);
            case lex::TokenType::MINUS:
                return ast::BinaryOpNode::create(ast::BinaryOp::SUB, left, right);
            case lex::TokenType::STAR:
                return ast::BinaryOpNode::create(ast::BinaryOp::MUL, left, right);
            case lex::TokenType::SLASH:
                return ast::BinaryOpNode::create(ast::BinaryOp::DIV, left, right);
            case lex::TokenType::CARET:
                return ast::BinaryOpNode::create(ast::BinaryOp::POW, left, right);
            case lex::TokenType::EQEQ:
                return ast::BinaryOpNode::create(ast::BinaryOp::EQ, left, right);
            case lex::TokenType::NEQ:
                return ast::BinaryOpNode::create(ast::BinaryOp::NEQ, left, right);
            case lex::TokenType::LESS:
                return ast::BinaryOpNode::create(ast::BinaryOp::LT, left, right);
            case lex::TokenType::LEQ:
                return ast::BinaryOpNode::create(ast::BinaryOp::LEQ, left, right);
            case lex::TokenType::GREATER:
                return ast::BinaryOpNode::create(ast::BinaryOp::GT, left, right);
            case lex::TokenType::GEQ:
                return ast::BinaryOpNode::create(ast::BinaryOp::GEQ, left, right);
            default:
                throw_error("Invalid binary operator");
        }
    }

    ast::Node* assignment(ast::Node* left) {
        if (left->type != ast::NodeType::VARIABLE) {
            throw_error("Invalid assignment target");
        }
        
        auto* var = static_cast<ast::VariableNode*>(left);
        advance();
        ast::Node* value = parse_expression();
        
        return ast::AssignmentNode::create(var->name, value);
    }

    ast::Node* call(ast::Node* left) {
        if (left->type != ast::NodeType::VARIABLE) {
            throw_error("Can only call functions and methods");
        }
        
        auto* var = static_cast<ast::VariableNode*>(left);
        std::vector<ast::Node*> args;
        
        if (!match(lex::TokenType::RPAREN)) {
            do {
                args.push_back(parse_expression());
            } while (match(lex::TokenType::COMMA));
            
            consume(lex::TokenType::RPAREN, "Expect ')' after arguments");
        }
        
        return ast::FunctionNode::create(var->name, std::move(args));
    }
};

} // namespace

ast::Node* parse(lex::Lexer& lexer) {
    Parser parser(lexer);
    return parser.parse();
}

} // namespace parse
} // namespace numu
