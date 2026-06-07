// test_main.cpp — Exocortex AST C++ tests (10+ test cases)
#include "exocortex_ast.hpp"
#include <cassert>
#include <iostream>
#include <string>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        std::cout << "  TEST " << #name << " ... "; \
    } while(0)

#define PASS() \
    do { \
        tests_passed++; \
        std::cout << "OK\n"; \
    } while(0)

#define FAIL(msg) \
    do { \
        std::cout << "FAIL: " << msg << "\n"; \
    } while(0)

using namespace exocortex::ast;

void test_empty_source() {
    TEST(empty_source);
    ASTDecomposer d;
    auto root = d.parse("");
    assert(root != nullptr);
    assert(root->kind == NodeKind::TranslationUnit);
    assert(root->children.empty());
    PASS();
}

void test_simple_function() {
    TEST(simple_function);
    ASTDecomposer d;
    auto root = d.parse("void hello() { return; }");
    auto funcs = root->find_all(NodeKind::FunctionDecl);
    assert(funcs.size() == 1);
    assert(funcs[0]->name == "hello");
    PASS();
}

void test_function_with_params() {
    TEST(function_with_params);
    ASTDecomposer d;
    auto root = d.parse("int add(int a, int b) { return a + b; }");
    auto funcs = root->find_all(NodeKind::FunctionDecl);
    assert(funcs.size() == 1);
    assert(funcs[0]->name == "add");
    PASS();
}

void test_struct_definition() {
    TEST(struct_definition);
    ASTDecomposer d;
    auto root = d.parse("struct Point { int x; int y; };");
    auto structs = root->find_all(NodeKind::StructDef);
    assert(structs.size() == 1);
    assert(structs[0]->name == "Point");
    // Should have field children
    auto fields = structs[0]->find_all(NodeKind::FieldDecl);
    assert(fields.size() == 2);
    PASS();
}

void test_class_definition() {
    TEST(class_definition);
    ASTDecomposer d;
    auto root = d.parse("class Engine { void start(); void stop(); };");
    auto classes = root->find_all(NodeKind::ClassDef);
    assert(classes.size() == 1);
    assert(classes[0]->name == "Engine");
    PASS();
}

void test_namespace_block() {
    TEST(namespace_block);
    ASTDecomposer d;
    auto root = d.parse("namespace core { void init(); void shutdown(); }");
    auto namespaces = root->find_all(NodeKind::NamespaceBlock);
    assert(namespaces.size() == 1);
    assert(namespaces[0]->name == "core");
    auto funcs = namespaces[0]->find_all(NodeKind::FunctionDecl);
    assert(funcs.size() == 2);
    PASS();
}

void test_nested_namespaces() {
    TEST(nested_namespaces);
    ASTDecomposer d;
    auto root = d.parse(
        "namespace outer { "
        "  namespace inner { "
        "    void deep(); "
        "  } "
        "  void shallow(); "
        "}"
    );
    auto outer = root->find_all(NodeKind::NamespaceBlock);
    assert(outer.size() >= 1);
    // outer should contain inner namespace and shallow function
    auto inner = outer[0]->find_all(NodeKind::NamespaceBlock);
    assert(inner.size() == 1);
    assert(inner[0]->name == "inner");
    PASS();
}

void test_mixed_declarations() {
    TEST(mixed_declarations);
    ASTDecomposer d;
    auto root = d.parse(R"(
        namespace app {
            struct Config {
                int port;
                int workers;
            };
            void load(Config& cfg);
            void save(const Config& cfg);
        }
        int main() { return 0; }
    )");
    auto funcs = root->find_all(NodeKind::FunctionDecl);
    assert(funcs.size() == 3); // load, save, main
    auto structs = root->find_all(NodeKind::StructDef);
    assert(structs.size() == 1);
    assert(structs[0]->name == "Config");
    PASS();
}

void test_tokenizer_identifiers() {
    TEST(tokenizer_identifiers);
    Tokenizer t("hello world foo_bar _baz");
    auto tokens = t.tokenize();
    // Should have 4 idents + EOF
    int idents = 0;
    for (auto& tok : tokens) {
        if (tok.kind == TokKind::Ident) idents++;
    }
    assert(idents == 4);
    PASS();
}

void test_tokenizer_strings_and_numbers() {
    TEST(tokenizer_strings_and_numbers);
    Tokenizer t(R"(42 "hello" 3.14)");
    auto tokens = t.tokenize();
    assert(tokens[0].kind == TokKind::Number);
    assert(tokens[0].text == "42");
    assert(tokens[1].kind == TokKind::String);
    assert(tokens[1].text == "hello");
    assert(tokens[2].kind == TokKind::Number);
    assert(tokens[2].text == "3.14");
    PASS();
}

void test_source_dependency_graph() {
    TEST(source_dependency_graph);
    std::string src = R"(
        void serve(int port) { bind(port); listen(); }
        void init() { load_config(); serve(8080); }
    )";
    SourceDependencyGraph graph;
    auto deps = graph.extract(src);
    assert(deps.count("serve") > 0);
    assert(deps.count("init") > 0);
    // serve calls bind and listen
    bool has_bind = false, has_listen = false;
    for (auto& c : deps["serve"]) {
        if (c == "bind") has_bind = true;
        if (c == "listen") has_listen = true;
    }
    assert(has_bind && has_listen);
    PASS();
}

void test_node_count_and_find_by_name() {
    TEST(node_count_and_find_by_name);
    ASTDecomposer d;
    auto root = d.parse(R"(
        struct Vec3 { float x; float y; float z; };
        float length(Vec3 v);
        void normalize(Vec3& v);
    )");
    // Should have struct + 2 functions = 3 top-level + fields
    assert(root->count() > 3);
    auto* len = root->find_by_name("length");
    assert(len != nullptr);
    assert(len->kind == NodeKind::FunctionDecl);
    PASS();
}

void test_comments_ignored() {
    TEST(comments_ignored);
    ASTDecomposer d;
    auto root = d.parse(R"(
        // This is a comment
        /* block comment */
        void documented();
        // another comment
        int value;
    )");
    auto funcs = root->find_all(NodeKind::FunctionDecl);
    assert(funcs.size() == 1);
    assert(funcs[0]->name == "documented");
    PASS();
}

int main() {
    std::cout << "=== Exocortex AST C++ Tests ===\n\n";

    test_empty_source();
    test_simple_function();
    test_function_with_params();
    test_struct_definition();
    test_class_definition();
    test_namespace_block();
    test_nested_namespaces();
    test_mixed_declarations();
    test_tokenizer_identifiers();
    test_tokenizer_strings_and_numbers();
    test_source_dependency_graph();
    test_node_count_and_find_by_name();
    test_comments_ignored();

    std::cout << "\n  " << tests_passed << "/" << tests_run << " tests passed\n";
    if (tests_passed != tests_run) {
        std::cout << "  SOME TESTS FAILED!\n";
        return 1;
    }
    std::cout << "  ALL TESTS PASSED!\n";
    return 0;
}
