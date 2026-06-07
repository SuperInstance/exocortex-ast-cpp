# exocortex-ast-cpp

A C++17 header-only AST decomposition engine for parsing C/C++/Rust-like source code into structured nodes and extracting dependency graphs.

## Features

- **ASTNode**: Represents source constructs with kind, name, source range, and children
- **ASTDecomposer**: Recursive-descent tokenizer + parser (no external dependencies)
- **DependencyGraph**: Extracts call references between functions
- Parses: function declarations, struct/class definitions, namespace blocks
- Header-only: just `#include "exocortex_ast.hpp"`

## Building

```bash
mkdir build && cd build
cmake ..
make
./exocortex_ast_tests
```

## Usage

```cpp
#include "exocortex_ast.hpp"

int main() {
    exocortex::ast::ASTDecomposer decomposer;
    auto root = decomposer.parse(R"(
        namespace core {
            struct Config {
                int port;
            };
            void init(Config& cfg) {
                serve(cfg.port);
            }
        }
    )");
    root->print(0);

    exocortex::ast::DependencyGraph graph;
    auto deps = graph.extract(*root);
    for (auto& [caller, callees] : deps) {
        for (auto& callee : callees) {
            std::cout << caller << " -> " << callee << "\n";
        }
    }
}
```

## License

MIT
