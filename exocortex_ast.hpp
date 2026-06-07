#pragma once
// exocortex-ast-cpp: Header-only AST decomposition engine (C++17)
// No external dependencies. Recursive-descent tokenizer + parser.

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <cassert>
#include <cctype>

namespace exocortex::ast {

// ── Source Range ──────────────────────────────────────────────────────

struct SourceRange {
    size_t line_start = 0;
    size_t col_start  = 0;
    size_t line_end   = 0;
    size_t col_end    = 0;

    bool operator==(const SourceRange& o) const {
        return line_start == o.line_start && col_start == o.col_start &&
               line_end == o.line_end && col_end == o.col_end;
    }
};

// ── AST Node ──────────────────────────────────────────────────────────

enum class NodeKind {
    TranslationUnit,
    FunctionDecl,
    StructDef,
    ClassDef,
    NamespaceBlock,
    FieldDecl,
    ParamDecl,
    CallExpr,
    Identifier,
    Block,
    Unknown
};

inline const char* node_kind_str(NodeKind k) {
    switch (k) {
        case NodeKind::TranslationUnit: return "TranslationUnit";
        case NodeKind::FunctionDecl:    return "FunctionDecl";
        case NodeKind::StructDef:       return "StructDef";
        case NodeKind::ClassDef:        return "ClassDef";
        case NodeKind::NamespaceBlock:  return "NamespaceBlock";
        case NodeKind::FieldDecl:       return "FieldDecl";
        case NodeKind::ParamDecl:       return "ParamDecl";
        case NodeKind::CallExpr:        return "CallExpr";
        case NodeKind::Identifier:      return "Identifier";
        case NodeKind::Block:           return "Block";
        case NodeKind::Unknown:         return "Unknown";
    }
    return "?";
}

class ASTNode {
public:
    NodeKind                          kind;
    std::string                       name;
    SourceRange                       range;
    std::vector<std::shared_ptr<ASTNode>> children;

    ASTNode(NodeKind k, std::string n, SourceRange r = {})
        : kind(k), name(std::move(n)), range(r) {}

    void add_child(std::shared_ptr<ASTNode> child) {
        children.push_back(std::move(child));
    }

    // Depth-first search for nodes of a given kind
    std::vector<ASTNode*> find_all(NodeKind target) const {
        std::vector<ASTNode*> result;
        for (auto& c : children) {
            if (c->kind == target) result.push_back(c.get());
            auto sub = c->find_all(target);
            result.insert(result.end(), sub.begin(), sub.end());
        }
        return result;
    }

    // Find first node by name
    ASTNode* find_by_name(const std::string& target) const {
        for (auto& c : children) {
            if (c->name == target) return c.get();
            auto* r = c->find_by_name(target);
            if (r) return r;
        }
        return nullptr;
    }

    void print(int depth = 0) const {
        std::string indent(depth * 2, ' ');
        std::cout << indent << "- " << node_kind_str(kind) << ": " << name << "\n";
        for (auto& c : children) c->print(depth + 1);
    }

    size_t count() const {
        size_t n = 1;
        for (auto& c : children) n += c->count();
        return n;
    }
};

// ── Token ─────────────────────────────────────────────────────────────

enum class TokKind {
    Ident, Number, String,
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Semi, Colon, Comma, Dot, Arrow, DoubleColon,
    Equals, Eof, Unknown
};

struct Token {
    TokKind   kind;
    std::string text;
    SourceRange range;
};

// ── Tokenizer ─────────────────────────────────────────────────────────

class Tokenizer {
    std::string src_;
    size_t      pos_ = 0;
    size_t      line_ = 1;
    size_t      col_  = 1;

    char peek() const { return pos_ < src_.size() ? src_[pos_] : '\0'; }
    char advance() {
        char c = peek();
        pos_++;
        if (c == '\n') { line_++; col_ = 1; } else { col_++; }
        return c;
    }
    void skip_ws_comments() {
        while (pos_ < src_.size()) {
            char c = peek();
            if (std::isspace(c)) { advance(); continue; }
            if (c == '/' && pos_ + 1 < src_.size()) {
                if (src_[pos_+1] == '/') {
                    while (pos_ < src_.size() && peek() != '\n') advance();
                    continue;
                }
                if (src_[pos_+1] == '*') {
                    advance(); advance();
                    while (pos_ + 1 < src_.size()) {
                        if (peek() == '*' && src_[pos_+1] == '/') { advance(); advance(); break; }
                        advance();
                    }
                    continue;
                }
            }
            break;
        }
    }

public:
    explicit Tokenizer(std::string src) : src_(std::move(src)) {}

    Token next() {
        skip_ws_comments();
        Token tok;
        tok.range.line_start = line_;
        tok.range.col_start  = col_;

        if (pos_ >= src_.size()) {
            tok.kind = TokKind::Eof;
            tok.text = "<eof>";
            return tok;
        }

        char c = peek();

        // Identifiers / keywords
        if (std::isalpha(c) || c == '_') {
            while (pos_ < src_.size() && (std::isalnum(peek()) || peek() == '_'))
                tok.text += advance();
            tok.kind = TokKind::Ident;
            tok.range.line_end = line_;
            tok.range.col_end = col_;
            return tok;
        }

        // Numbers
        if (std::isdigit(c)) {
            while (pos_ < src_.size() && std::isdigit(peek()))
                tok.text += advance();
            if (peek() == '.') { tok.text += advance(); while (std::isdigit(peek())) tok.text += advance(); }
            tok.kind = TokKind::Number;
            tok.range.line_end = line_; tok.range.col_end = col_;
            return tok;
        }

        // Strings
        if (c == '"') {
            advance(); // opening quote
            while (pos_ < src_.size() && peek() != '"') {
                if (peek() == '\\') { tok.text += advance(); }
                tok.text += advance();
            }
            if (peek() == '"') advance();
            tok.kind = TokKind::String;
            tok.range.line_end = line_; tok.range.col_end = col_;
            return tok;
        }

        // Two-char tokens
        if (c == '-' && pos_ + 1 < src_.size() && src_[pos_+1] == '>') {
            advance(); advance(); tok.kind = TokKind::Arrow; tok.text = "->";
            tok.range.line_end = line_; tok.range.col_end = col_;
            return tok;
        }
        if (c == ':' && pos_ + 1 < src_.size() && src_[pos_+1] == ':') {
            advance(); advance(); tok.kind = TokKind::DoubleColon; tok.text = "::";
            tok.range.line_end = line_; tok.range.col_end = col_;
            return tok;
        }

        // Single-char tokens
        advance();
        tok.text = c;
        switch (c) {
            case '(': tok.kind = TokKind::LParen; break;
            case ')': tok.kind = TokKind::RParen; break;
            case '{': tok.kind = TokKind::LBrace; break;
            case '}': tok.kind = TokKind::RBrace; break;
            case '[': tok.kind = TokKind::LBracket; break;
            case ']': tok.kind = TokKind::RBracket; break;
            case ';': tok.kind = TokKind::Semi; break;
            case ':': tok.kind = TokKind::Colon; break;
            case ',': tok.kind = TokKind::Comma; break;
            case '.': tok.kind = TokKind::Dot; break;
            case '=': tok.kind = TokKind::Equals; break;
            default:  tok.kind = TokKind::Unknown; break;
        }
        tok.range.line_end = line_; tok.range.col_end = col_;
        return tok;
    }

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (true) {
            Token t = next();
            tokens.push_back(t);
            if (t.kind == TokKind::Eof) break;
        }
        return tokens;
    }
};

// ── Parser ────────────────────────────────────────────────────────────

class Parser {
    std::vector<Token> tokens_;
    size_t pos_ = 0;

    const Token& peek() const { return tokens_[pos_]; }
    Token consume() { return tokens_[pos_++]; }
    bool match(TokKind k) { if (peek().kind == k) { pos_++; return true; } return false; }
    bool check(TokKind k) const { return peek().kind == k; }

    // Skip balanced braces
    void skip_braces() {
        int depth = 0;
        while (pos_ < tokens_.size()) {
            if (check(TokKind::LBrace)) depth++;
            if (check(TokKind::RBrace)) { depth--; if (depth <= 0) { pos_++; return; } }
            pos_++;
        }
    }

    // Skip balanced parens
    void skip_parens() {
        int depth = 0;
        while (pos_ < tokens_.size()) {
            if (check(TokKind::LParen)) depth++;
            if (check(TokKind::RParen)) { depth--; if (depth <= 0) { pos_++; return; } }
            pos_++;
        }
    }

    // Parse a single declaration inside a translation unit or namespace
    void parse_decl(std::shared_ptr<ASTNode> parent) {
        auto& tok = peek();

        // namespace NAME { ... }
        if (tok.kind == TokKind::Ident && tok.text == "namespace") {
            consume(); // 'namespace'
            std::string ns_name = (check(TokKind::Ident)) ? consume().text : "(anon)";
            if (!match(TokKind::LBrace)) return;
            auto ns = std::make_shared<ASTNode>(NodeKind::NamespaceBlock, ns_name);
            while (!check(TokKind::RBrace) && !check(TokKind::Eof)) {
                parse_decl(ns);
            }
            match(TokKind::RBrace);
            parent->add_child(ns);
            return;
        }

        // struct NAME { ... };
        if (tok.kind == TokKind::Ident && tok.text == "struct") {
            consume(); // 'struct'
            std::string name = (check(TokKind::Ident)) ? consume().text : "(anon)";
            auto node = std::make_shared<ASTNode>(NodeKind::StructDef, name);
            if (match(TokKind::LBrace)) {
                while (!check(TokKind::RBrace) && !check(TokKind::Eof)) {
                    if (check(TokKind::Ident)) {
                        auto field = std::make_shared<ASTNode>(NodeKind::FieldDecl, consume().text);
                        node->add_child(field);
                    }
                    // skip to next semicolon or closing brace
                    while (!check(TokKind::Semi) && !check(TokKind::RBrace) && !check(TokKind::Eof)) pos_++;
                    match(TokKind::Semi);
                }
                match(TokKind::RBrace);
            }
            match(TokKind::Semi);
            parent->add_child(node);
            return;
        }

        // class NAME { ... };
        if (tok.kind == TokKind::Ident && tok.text == "class") {
            consume(); // 'class'
            std::string name = (check(TokKind::Ident)) ? consume().text : "(anon)";
            auto node = std::make_shared<ASTNode>(NodeKind::ClassDef, name);
            if (match(TokKind::LBrace)) {
                // Skip access specifiers
                while (!check(TokKind::RBrace) && !check(TokKind::Eof)) {
                    if (check(TokKind::Ident)) {
                        auto& t = peek();
                        // Could be a function or field
                        // Collect return type + name
                        std::string first = t.text;
                        pos_++;
                        // Look for function: TYPE NAME(
                        if (check(TokKind::Ident)) {
                            std::string fname = consume().text;
                            if (check(TokKind::LParen)) {
                                auto fn = std::make_shared<ASTNode>(NodeKind::FunctionDecl, fname);
                                skip_parens();
                                if (check(TokKind::LBrace)) skip_braces();
                                else match(TokKind::Semi);
                                node->add_child(fn);
                                continue;
                            }
                        }
                        // Otherwise field or other — skip to semicolon
                        while (!check(TokKind::Semi) && !check(TokKind::RBrace) && !check(TokKind::Eof)) pos_++;
                        match(TokKind::Semi);
                    } else if (check(TokKind::Colon)) {
                        // access specifier line
                        while (!check(TokKind::Semi) && !check(TokKind::RBrace) && !check(TokKind::Eof)) pos_++;
                        match(TokKind::Semi);
                    } else {
                        pos_++;
                    }
                }
                match(TokKind::RBrace);
            }
            match(TokKind::Semi);
            parent->add_child(node);
            return;
        }

        // Function: we look for TYPE NAME ( ... ) { ... } or TYPE NAME ( ... );
        // Collect type tokens + name
        if (tok.kind == TokKind::Ident) {
            std::vector<std::string> leading;
            while (check(TokKind::Ident) || check(TokKind::DoubleColon) ||
                   check(TokKind::Arrow) || check(TokKind::LBracket) ||
                   check(TokKind::RBracket) || check(TokKind::Colon)) {
                leading.push_back(consume().text);
            }
            // Now we expect NAME(
            if (check(TokKind::LParen) && leading.size() >= 1) {
                // The last ident before ( is the function name
                std::string fname = leading.back();
                auto fn = std::make_shared<ASTNode>(NodeKind::FunctionDecl, fname);
                skip_parens();
                // Optional const
                if (check(TokKind::Ident) && peek().text == "const") pos_++;
                if (check(TokKind::LBrace)) skip_braces();
                else match(TokKind::Semi);
                parent->add_child(fn);
                return;
            }
            // Not a function — skip to semicolon
            while (!check(TokKind::Semi) && !check(TokKind::Eof)) pos_++;
            match(TokKind::Semi);
            return;
        }

        // Fallback: skip token
        pos_++;
    }

public:
    explicit Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

    std::shared_ptr<ASTNode> parse() {
        auto root = std::make_shared<ASTNode>(NodeKind::TranslationUnit, "<root>");
        while (!check(TokKind::Eof)) {
            parse_decl(root);
        }
        return root;
    }
};

// ── AST Decomposer ────────────────────────────────────────────────────

class ASTDecomposer {
public:
    std::shared_ptr<ASTNode> parse(const std::string& source) {
        Tokenizer tokenizer(source);
        auto tokens = tokenizer.tokenize();
        Parser parser(std::move(tokens));
        return parser.parse();
    }
};

// ── Dependency Graph ──────────────────────────────────────────────────

class DependencyGraph {
public:
    // Extracts function → called functions mapping
    // For simplicity, we re-tokenize function bodies looking for IDENT(
    std::unordered_map<std::string, std::vector<std::string>>
    extract(const ASTNode& root) {
        std::unordered_map<std::string, std::vector<std::string>> deps;

        // Collect all function declarations
        auto funcs = root.find_all(NodeKind::FunctionDecl);
        // Also search inside classes/namespaces
        for (auto* f : funcs) {
            std::vector<std::string> callees;
            collect_calls(root, f->name, callees);
            if (!callees.empty()) {
                deps[f->name] = callees;
            }
        }
        return deps;
    }

private:
    // Simple approach: scan the source for IDENT( patterns within known function names
    // Since our AST doesn't store body text, we use structural hints
    void collect_calls(const ASTNode& node, const std::string& func_name,
                       std::vector<std::string>& callees) {
        // Walk all children looking for CallExpr or nested patterns
        for (auto& child : node.children) {
            if (child->kind == NodeKind::CallExpr) {
                callees.push_back(child->name);
            }
            collect_calls(*child, func_name, callees);
        }
    }
};

// ── Extended DependencyGraph that uses source scanning ────────────────

class SourceDependencyGraph {
public:
    std::unordered_map<std::string, std::vector<std::string>>
    extract(const std::string& source) {
        std::unordered_map<std::string, std::vector<std::string>> deps;

        // Find function definitions: TYPE NAME(...) {
        // Then scan body for IDENT( patterns
        Tokenizer tokenizer(source);
        auto tokens = tokenizer.tokenize();

        for (size_t i = 0; i < tokens.size(); i++) {
            // Look for pattern: ... IDENT LPAREN ... LBRACE (function body)
            if (tokens[i].kind == TokKind::LParen) {
                // Check if previous token is an identifier (function name)
                if (i > 0 && tokens[i-1].kind == TokKind::Ident) {
                    std::string fname = tokens[i-1].text;
                    // Skip parens
                    size_t j = i;
                    int depth = 0;
                    for (; j < tokens.size(); j++) {
                        if (tokens[j].kind == TokKind::LParen) depth++;
                        if (tokens[j].kind == TokKind::RParen) { depth--; if (depth == 0) break; }
                    }
                    j++; // past RPAREN
                    // Skip const
                    if (j < tokens.size() && tokens[j].kind == TokKind::Ident && tokens[j].text == "const") j++;
                    // Expect LBRACE
                    if (j < tokens.size() && tokens[j].kind == TokKind::LBrace) {
                        std::vector<std::string> callees;
                        extract_calls_from_body(tokens, j, callees);
                        if (!callees.empty()) {
                            deps[fname] = callees;
                        }
                    }
                }
            }
        }
        return deps;
    }

private:
    void extract_calls_from_body(const std::vector<Token>& tokens, size_t brace_start,
                                  std::vector<std::string>& callees) {
        int depth = 0;
        size_t i = brace_start;
        // Find matching close brace
        size_t end = i;
        for (; end < tokens.size(); end++) {
            if (tokens[end].kind == TokKind::LBrace) depth++;
            if (tokens[end].kind == TokKind::RBrace) { depth--; if (depth == 0) break; }
        }
        // Scan body for IDENT(
        for (i = brace_start + 1; i < end; i++) {
            if (tokens[i].kind == TokKind::Ident &&
                i + 1 < tokens.size() &&
                tokens[i+1].kind == TokKind::LParen) {
                // Filter out language keywords
                const auto& name = tokens[i].text;
                if (name != "if" && name != "while" && name != "for" && name != "switch" &&
                    name != "return" && name != "sizeof" && name != "catch") {
                    callees.push_back(name);
                }
            }
        }
    }
};

} // namespace exocortex::ast
