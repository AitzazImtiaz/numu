#ifndef NUMU_CORE_AST_H
#define NUMU_CORE_AST_H

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

namespace numu {
namespace ast {

enum class NodeType {
    NUMBER,
    BOOLEAN,
    STRING,
    VARIABLE,
    BINARY_OP,
    UNARY_OP,
    FUNCTION,
    MATRIX,
    TENSOR,
    ASSIGNMENT,
    BLOCK,
    IF,
    WHILE,
    FOR,
    RETURN
};

enum class BinaryOp {
    ADD, SUB, MUL, DIV, MOD, POW,
    EQ, NEQ, LT, LEQ, GT, GEQ,
    AND, OR
};

enum class UnaryOp {
    NEGATE, NOT,
    SIN, COS, TAN, ASIN, ACOS, ATAN,
    EXP, LOG, SQRT,
    TRANSPOSE, DETERMINANT, INVERSE
};

struct Node {
    NodeType type;
    virtual ~Node() = default;
protected:
    Node(NodeType type) : type(type) {}
};

struct NumberNode : Node {
    double value;
    NumberNode(double value);
    static NumberNode* create(double value);
};

struct BooleanNode : Node {
    bool value;
    BooleanNode(bool value);
    static BooleanNode* create(bool value);
};

struct StringNode : Node {
    std::string value;
    StringNode(const std::string& value);
    static StringNode* create(const std::string& value);
};

struct VariableNode : Node {
    std::string name;
    VariableNode(const std::string& name);
    static VariableNode* create(const std::string& name);
};

struct BinaryOpNode : Node {
    BinaryOp op;
    Node* left;
    Node* right;
    BinaryOpNode(BinaryOp op, Node* left, Node* right);
    static BinaryOpNode* create(BinaryOp op, Node* left, Node* right);
};

struct UnaryOpNode : Node {
    UnaryOp op;
    Node* operand;
    UnaryOpNode(UnaryOp op, Node* operand);
    static UnaryOpNode* create(UnaryOp op, Node* operand);
};

struct FunctionNode : Node {
    std::string name;
    std::vector<Node*> args;
    FunctionNode(const std::string& name, std::vector<Node*> args);
    static FunctionNode* create(const std::string& name, std::vector<Node*> args);
};

struct MatrixNode : Node {
    std::vector<std::vector<Node*>> elements;
    MatrixNode(std::vector<std::vector<Node*>> elements);
    static MatrixNode* create(std::vector<std::vector<Node*>> elements);
};

struct TensorNode : Node {
    std::vector<size_t> dims;
    std::vector<Node*> values;
    TensorNode(std::vector<size_t> dims, std::vector<Node*> values);
    static TensorNode* create(std::vector<size_t> dims, std::vector<Node*> values);
};

struct AssignmentNode : Node {
    std::string name;
    Node* value;
    AssignmentNode(const std::string& name, Node* value);
    static AssignmentNode* create(const std::string& name, Node* value);
};

struct BlockNode : Node {
    std::vector<Node*> statements;
    BlockNode(std::vector<Node*> statements);
    static BlockNode* create(std::vector<Node*> statements);
};

struct IfNode : Node {
    Node* condition;
    Node* then_branch;
    Node* else_branch;
    IfNode(Node* condition, Node* then_branch, Node* else_branch);
    static IfNode* create(Node* condition, Node* then_branch, Node* else_branch);
};

struct WhileNode : Node {
    Node* condition;
    Node* body;
    WhileNode(Node* condition, Node* body);
    static WhileNode* create(Node* condition, Node* body);
};

struct ForNode : Node {
    Node* initializer;
    Node* condition;
    Node* increment;
    Node* body;
    ForNode(Node* initializer, Node* condition, Node* increment, Node* body);
    static ForNode* create(Node* initializer, Node* condition, Node* increment, Node* body);
};

struct ReturnNode : Node {
    Node* value;
    ReturnNode(Node* value);
    static ReturnNode* create(Node* value);
};

Node* clone(Node* node);
bool equals(Node* a, Node* b);
size_t hash(Node* node);
void traverse(Node* node, const std::function<void(Node*)>& visitor);
Node* simplify(Node* node);

} // namespace ast
} // namespace numu

#endif // NUMU_CORE_AST_H
