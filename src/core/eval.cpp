#include "numu/core/ast.h"
#include "numu/core/eval.h"
#include <unordered_map>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <memory>

namespace numu {
namespace core {

namespace {
struct EvalContext {
    std::unordered_map<std::string, double> variables;
    std::unordered_map<std::string, std::function<double(const std::vector<double>&)>> functions;
    
    EvalContext() {
        functions["sin"] = [](const auto& args) { return std::sin(args[0]); };
        functions["cos"] = [](const auto& args) { return std::cos(args[0]); };
        functions["tan"] = [](const auto& args) { return std::tan(args[0]); };
        functions["exp"] = [](const auto& args) { return std::exp(args[0]); };
        functions["log"] = [](const auto& args) { return std::log(args[0]); };
        functions["sqrt"] = [](const auto& args) { return std::sqrt(args[0]); };
        functions["pow"] = [](const auto& args) { return std::pow(args[0], args[1]); };
    }
};

thread_local EvalContext global_context;

struct EvaluationError : std::runtime_error {
    using std::runtime_error::runtime_error;
};

void validate_args(const std::string& name, size_t expected, size_t actual) {
    if (expected != actual) {
        throw EvaluationError("Function " + name + " expects " + 
                            std::to_string(expected) + " arguments, got " + 
                            std::to_string(actual));
    }
}

double eval_binary_op(BinaryOp op, double left, double right) {
    switch(op) {
        case BinaryOp::ADD: return left + right;
        case BinaryOp::SUB: return left - right;
        case BinaryOp::MUL: return left * right;
        case BinaryOp::DIV: 
            if (right == 0.0) {
                throw EvaluationError("Division by zero");
            }
            return left / right;
        case BinaryOp::POW: return std::pow(left, right);
        case BinaryOp::MOD: 
            if (right == 0.0) {
                throw EvaluationError("Modulo by zero");
            }
            return std::fmod(left, right);
        default:
            throw EvaluationError("Unknown binary operator");
    }
}

double eval_unary_op(UnaryOp op, double operand) {
    switch(op) {
        case UnaryOp::NEGATE: return -operand;
        case UnaryOp::SIN: return std::sin(operand);
        case UnaryOp::COS: return std::cos(operand);
        case UnaryOp::TAN: return std::tan(operand);
        case UnaryOp::EXP: return std::exp(operand);
        case UnaryOp::LOG: 
            if (operand <= 0.0) {
                throw EvaluationError("Logarithm of non-positive number");
            }
            return std::log(operand);
        case UnaryOp::SQRT: 
            if (operand < 0.0) {
                throw EvaluationError("Square root of negative number");
            }
            return std::sqrt(operand);
        default:
            throw EvaluationError("Unknown unary operator");
    }
}

double eval_matrix_op(const MatrixNode* node, const EvalContext& ctx) {
    throw EvaluationError("Matrix operations not yet implemented");
}

double eval_tensor_op(const TensorNode* node, const EvalContext& ctx) {
    throw EvaluationError("Tensor operations not yet implemented");
}

} // namespace

double evaluate(ast::Node* node, const EvalContext& ctx) {
    if (!node) {
        throw EvaluationError("Null node in evaluation");
    }

    switch(node->type) {
        case ast::NodeType::NUMBER:
            return static_cast<ast::NumberNode*>(node)->value;
            
        case ast::NodeType::VARIABLE: {
            const auto& name = static_cast<ast::VariableNode*>(node)->name;
            auto it = ctx.variables.find(name);
            if (it == ctx.variables.end()) {
                throw EvaluationError("Undefined variable: " + name);
            }
            return it->second;
        }
            
        case ast::NodeType::BINARY_OP: {
            auto* bin = static_cast<ast::BinaryOpNode*>(node);
            double left = evaluate(bin->left, ctx);
            double right = evaluate(bin->right, ctx);
            return eval_binary_op(bin->op, left, right);
        }
            
        case ast::NodeType::UNARY_OP: {
            auto* un = static_cast<ast::UnaryOpNode*>(node);
            double operand = evaluate(un->operand, ctx);
            return eval_unary_op(un->op, operand);
        }
            
        case ast::NodeType::FUNCTION: {
            auto* fn = static_cast<ast::FunctionNode*>(node);
            const auto& name = fn->name;
            
            std::vector<double> args;
            args.reserve(fn->args.size());
            for (auto* arg : fn->args) {
                args.push_back(evaluate(arg, ctx));
            }
            
            auto it = ctx.functions.find(name);
            if (it != ctx.functions.end()) {
                return it->second(args);
            }
            
            throw EvaluationError("Unknown function: " + name);
        }
            
        case ast::NodeType::MATRIX:
            return eval_matrix_op(static_cast<ast::MatrixNode*>(node), ctx);
            
        case ast::NodeType::TENSOR:
            return eval_tensor_op(static_cast<ast::TensorNode*>(node), ctx);
            
        default:
            throw EvaluationError("Unknown node type in evaluation");
    }
}

void set_variable(const std::string& name, double value) {
    global_context.variables[name] = value;
}

double get_variable(const std::string& name) {
    auto it = global_context.variables.find(name);
    if (it == global_context.variables.end()) {
        throw EvaluationError("Undefined variable: " + name);
    }
    return it->second;
}

void register_function(const std::string& name, 
                      std::function<double(const std::vector<double>&)> func,
                      size_t arity) {
    if (global_context.functions.count(name)) {
        throw EvaluationError("Function already registered: " + name);
    }
    global_context.functions[name] = [func, arity, name](const auto& args) {
        validate_args(name, arity, args.size());
        return func(args);
    };
}

double evaluate(ast::Node* node) {
    return evaluate(node, global_context);
}

namespace builtin {
void initialize() {
    register_function("abs", [](const auto& args) { return std::abs(args[0]); }, 1);
    register_function("min", [](const auto& args) { 
        return *std::min_element(args.begin(), args.end()); 
    }, -1); // -1 means variable arguments
    register_function("max", [](const auto& args) { 
        return *std::max_element(args.begin(), args.end()); 
    }, -1);
    register_function("sum", [](const auto& args) { 
        return std::accumulate(args.begin(), args.end(), 0.0); 
    }, -1);
    register_function("avg", [](const auto& args) { 
        return std::accumulate(args.begin(), args.end(), 0.0) / args.size(); 
    }, -1);
    
    // Constants
    set_variable("pi", 3.14159265358979323846);
    set_variable("e", 2.71828182845904523536);
    set_variable("inf", std::numeric_limits<double>::infinity());
}
} // namespace builtin

} // namespace core
} // namespace numu
