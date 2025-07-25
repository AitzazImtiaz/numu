#include "numu/core/ast.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <numeric>
#include <cmath>
#include <stack>
#include <queue>
#include <functional>

namespace numu {
namespace ast {

namespace {
struct NodePool {
    std::vector<std::unique_ptr<Node>> nodes;
    
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        auto ptr = std::make_unique<T>(std::forward<Args>(args)...);
        auto* raw = ptr.get();
        nodes.push_back(std::move(ptr));
        return raw;
    }
};

thread_local NodePool global_pool;
}

Node::Node(NodeType type) : type(type) {}
Node::~Node() = default;

NumberNode::NumberNode(double value) : Node(NodeType::NUMBER), value(value) {}
NumberNode* NumberNode::create(double value) {
    return global_pool.create<NumberNode>(value);
}

VariableNode::VariableNode(const std::string& name) 
    : Node(NodeType::VARIABLE), name(name) {}
VariableNode* VariableNode::create(const std::string& name) {
    return global_pool.create<VariableNode>(name);
}

BinaryOpNode::BinaryOpNode(BinaryOp op, Node* left, Node* right)
    : Node(NodeType::BINARY_OP), op(op), left(left), right(right) {}
BinaryOpNode* BinaryOpNode::create(BinaryOp op, Node* left, Node* right) {
    return global_pool.create<BinaryOpNode>(op, left, right);
}

UnaryOpNode::UnaryOpNode(UnaryOp op, Node* operand)
    : Node(NodeType::UNARY_OP), op(op), operand(operand) {}
UnaryOpNode* UnaryOpNode::create(UnaryOp op, Node* operand) {
    return global_pool.create<UnaryOpNode>(op, operand);
}

FunctionNode::FunctionNode(const std::string& name, std::vector<Node*> args)
    : Node(NodeType::FUNCTION), name(name), args(std::move(args)) {}
FunctionNode* FunctionNode::create(const std::string& name, std::vector<Node*> args) {
    return global_pool.create<FunctionNode>(name, std::move(args));
}

MatrixNode::MatrixNode(std::vector<std::vector<Node*>> elements)
    : Node(NodeType::MATRIX), elements(std::move(elements)) {}
MatrixNode* MatrixNode::create(std::vector<std::vector<Node*>> elements) {
    return global_pool.create<MatrixNode>(std::move(elements));
}

TensorNode::TensorNode(std::vector<size_t> dims, std::vector<Node*> values)
    : Node(NodeType::TENSOR), dims(std::move(dims)), values(std::move(values)) {}
TensorNode* TensorNode::create(std::vector<size_t> dims, std::vector<Node*> values) {
    return global_pool.create<TensorNode>(std::move(dims), std::move(values));
}

Node* clone(Node* node) {
    if (!node) return nullptr;
    
    switch(node->type) {
        case NodeType::NUMBER:
            return NumberNode::create(static_cast<NumberNode*>(node)->value);
        case NodeType::VARIABLE:
            return VariableNode::create(static_cast<VariableNode*>(node)->name);
        case NodeType::BINARY_OP: {
            auto* bin = static_cast<BinaryOpNode*>(node);
            return BinaryOpNode::create(bin->op, clone(bin->left), clone(bin->right));
        }
        case NodeType::UNARY_OP: {
            auto* un = static_cast<UnaryOpNode*>(node);
            return UnaryOpNode::create(un->op, clone(un->operand));
        }
        case NodeType::FUNCTION: {
            auto* fn = static_cast<FunctionNode*>(node);
            std::vector<Node*> args;
            args.reserve(fn->args.size());
            for (auto* arg : fn->args) {
                args.push_back(clone(arg));
            }
            return FunctionNode::create(fn->name, std::move(args));
        }
        case NodeType::MATRIX: {
            auto* mat = static_cast<MatrixNode*>(node);
            std::vector<std::vector<Node*>> elems;
            elems.reserve(mat->elements.size());
            for (const auto& row : mat->elements) {
                std::vector<Node*> new_row;
                new_row.reserve(row.size());
                for (auto* elem : row) {
                    new_row.push_back(clone(elem));
                }
                elems.push_back(std::move(new_row));
            }
            return MatrixNode::create(std::move(elems));
        }
        case NodeType::TENSOR: {
            auto* tensor = static_cast<TensorNode*>(node);
            std::vector<Node*> vals;
            vals.reserve(tensor->values.size());
            for (auto* val : tensor->values) {
                vals.push_back(clone(val));
            }
            return TensorNode::create(tensor->dims, std::move(vals));
        }
        default:
            throw std::runtime_error("Unknown node type in clone");
    }
}

bool equals(Node* a, Node* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->type != b->type) return false;
    
    switch(a->type) {
        case NodeType::NUMBER: {
            auto* na = static_cast<NumberNode*>(a);
            auto* nb = static_cast<NumberNode*>(b);
            return na->value == nb->value;
        }
        case NodeType::VARIABLE: {
            auto* va = static_cast<VariableNode*>(a);
            auto* vb = static_cast<VariableNode*>(b);
            return va->name == vb->name;
        }
        case NodeType::BINARY_OP: {
            auto* ba = static_cast<BinaryOpNode*>(a);
            auto* bb = static_cast<BinaryOpNode*>(b);
            return ba->op == bb->op && 
                   equals(ba->left, bb->left) && 
                   equals(ba->right, bb->right);
        }
        case NodeType::UNARY_OP: {
            auto* ua = static_cast<UnaryOpNode*>(a);
            auto* ub = static_cast<UnaryOpNode*>(b);
            return ua->op == ub->op && 
                   equals(ua->operand, ub->operand);
        }
        case NodeType::FUNCTION: {
            auto* fa = static_cast<FunctionNode*>(a);
            auto* fb = static_cast<FunctionNode*>(b);
            if (fa->name != fb->name || fa->args.size() != fb->args.size()) {
                return false;
            }
            for (size_t i = 0; i < fa->args.size(); ++i) {
                if (!equals(fa->args[i], fb->args[i])) {
                    return false;
                }
            }
            return true;
        }
        case NodeType::MATRIX: {
            auto* ma = static_cast<MatrixNode*>(a);
            auto* mb = static_cast<MatrixNode*>(b);
            if (ma->elements.size() != mb->elements.size()) {
                return false;
            }
            for (size_t i = 0; i < ma->elements.size(); ++i) {
                if (ma->elements[i].size() != mb->elements[i].size()) {
                    return false;
                }
                for (size_t j = 0; j < ma->elements[i].size(); ++j) {
                    if (!equals(ma->elements[i][j], mb->elements[i][j])) {
                        return false;
                    }
                }
            }
            return true;
        }
        case NodeType::TENSOR: {
            auto* ta = static_cast<TensorNode*>(a);
            auto* tb = static_cast<TensorNode*>(b);
            if (ta->dims != tb->dims || ta->values.size() != tb->values.size()) {
                return false;
            }
            for (size_t i = 0; i < ta->values.size(); ++i) {
                if (!equals(ta->values[i], tb->values[i])) {
                    return false;
                }
            }
            return true;
        }
        default:
            throw std::runtime_error("Unknown node type in equals");
    }
}

size_t hash(Node* node) {
    if (!node) return 0;
    
    static constexpr size_t prime = 0x100000001B3ull;
    size_t h = static_cast<size_t>(node->type);
    
    switch(node->type) {
        case NodeType::NUMBER: {
            auto* num = static_cast<NumberNode*>(node);
            h ^= std::hash<double>{}(num->value) + prime;
            break;
        }
        case NodeType::VARIABLE: {
            auto* var = static_cast<VariableNode*>(node);
            h ^= std::hash<std::string>{}(var->name) + prime;
            break;
        }
        case NodeType::BINARY_OP: {
            auto* bin = static_cast<BinaryOpNode*>(node);
            h ^= (static_cast<size_t>(bin->op) + prime);
            h ^= (hash(bin->left) * prime);
            h ^= (hash(bin->right) * prime);
            break;
        }
        case NodeType::UNARY_OP: {
            auto* un = static_cast<UnaryOpNode*>(node);
            h ^= (static_cast<size_t>(un->op) + prime);
            h ^= (hash(un->operand) * prime);
            break;
        }
        case NodeType::FUNCTION: {
            auto* fn = static_cast<FunctionNode*>(node);
            h ^= std::hash<std::string>{}(fn->name) + prime;
            for (auto* arg : fn->args) {
                h ^= (hash(arg) * prime);
            }
            break;
        }
        case NodeType::MATRIX: {
            auto* mat = static_cast<MatrixNode*>(node);
            for (const auto& row : mat->elements) {
                for (auto* elem : row) {
                    h ^= (hash(elem) * prime);
                }
            }
            break;
        }
        case NodeType::TENSOR: {
            auto* tensor = static_cast<TensorNode*>(node);
            for (auto dim : tensor->dims) {
                h ^= (std::hash<size_t>{}(dim) + prime);
            }
            for (auto* val : tensor->values) {
                h ^= (hash(val) * prime);
            }
            break;
        }
        default:
            throw std::runtime_error("Unknown node type in hash");
    }
    
    return h;
}

void traverse(Node* node, const std::function<void(Node*)>& visitor) {
    if (!node) return;
    
    std::stack<Node*> stack;
    stack.push(node);
    
    while (!stack.empty()) {
        Node* current = stack.top();
        stack.pop();
        
        visitor(current);
        
        switch(current->type) {
            case NodeType::BINARY_OP: {
                auto* bin = static_cast<BinaryOpNode*>(current);
                if (bin->right) stack.push(bin->right);
                if (bin->left) stack.push(bin->left);
                break;
            }
            case NodeType::UNARY_OP: {
                auto* un = static_cast<UnaryOpNode*>(current);
                if (un->operand) stack.push(un->operand);
                break;
            }
            case NodeType::FUNCTION: {
                auto* fn = static_cast<FunctionNode*>(current);
                for (auto it = fn->args.rbegin(); it != fn->args.rend(); ++it) {
                    if (*it) stack.push(*it);
                }
                break;
            }
            case NodeType::MATRIX: {
                auto* mat = static_cast<MatrixNode*>(current);
                for (auto row_it = mat->elements.rbegin(); row_it != mat->elements.rend(); ++row_it) {
                    for (auto elem_it = row_it->rbegin(); elem_it != row_it->rend(); ++elem_it) {
                        if (*elem_it) stack.push(*elem_it);
                    }
                }
                break;
            }
            case NodeType::TENSOR: {
                auto* tensor = static_cast<TensorNode*>(current);
                for (auto it = tensor->values.rbegin(); it != tensor->values.rend(); ++it) {
                    if (*it) stack.push(*it);
                }
                break;
            }
            default:
                break;
        }
    }
}

Node* simplify(Node* node) {
    if (!node) return nullptr;
    
    switch(node->type) {
        case NodeType::BINARY_OP: {
            auto* bin = static_cast<BinaryOpNode*>(node);
            Node* left = simplify(bin->left);
            Node* right = simplify(bin->right);
            
            if (left->type == NodeType::NUMBER && right->type == NodeType::NUMBER) {
                double lval = static_cast<NumberNode*>(left)->value;
                double rval = static_cast<NumberNode*>(right)->value;
                double result = 0;
                
                switch(bin->op) {
                    case BinaryOp::ADD: result = lval + rval; break;
                    case BinaryOp::SUB: result = lval - rval; break;
                    case BinaryOp::MUL: result = lval * rval; break;
                    case BinaryOp::DIV: result = lval / rval; break;
                    case BinaryOp::POW: result = std::pow(lval, rval); break;
                    default: break;
                }
                
                return NumberNode::create(result);
            }
            
            return BinaryOpNode::create(bin->op, left, right);
        }
        case NodeType::UNARY_OP: {
            auto* un = static_cast<UnaryOpNode*>(node);
            Node* operand = simplify(un->operand);
            
            if (operand->type == NodeType::NUMBER) {
                double val = static_cast<NumberNode*>(operand)->value;
                double result = 0;
                
                switch(un->op) {
                    case UnaryOp::NEGATE: result = -val; break;
                    case UnaryOp::SIN: result = std::sin(val); break;
                    case UnaryOp::COS: result = std::cos(val); break;
                    case UnaryOp::TAN: result = std::tan(val); break;
                    case UnaryOp::EXP: result = std::exp(val); break;
                    case UnaryOp::LOG: result = std::log(val); break;
                    case UnaryOp::SQRT: result = std::sqrt(val); break;
                    default: break;
                }
                
                return NumberNode::create(result);
            }
            
            return UnaryOpNode::create(un->op, operand);
        }
        default:
            return clone(node);
    }
}

} // namespace ast
} // namespace numu
