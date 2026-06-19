#include "parser/lexer.h"
#include "parser/parser.h"
#include <iostream>
#include <string>

int main() {
  Lexer lexer;
  Parser parser;

  std::string input;
  while (true) {
    std::cout << "db > ";
    std::getline(std::cin, input);

    // Allow the user to exit
    if (input == ".exit" || input == "quit" || input == "exit") {
      std::cout << "Goodbye!" << std::endl;
      break;
    }

    // Skip empty input
    if (input.empty()) {
      continue;
    }

    // Tokenize
    std::vector<Token> tokens = lexer.handleInput(input);

    // Debug: print tokens
    std::cout << "Tokens: ";
    for (const auto &tok : tokens) {
      if (tok.type == TokenType::TOKEN_EOF)
        break;
      std::cout << "[" << tokenTypeName(tok.type) << " '" << tok.value
                << "'] ";
    }
    std::cout << std::endl;

    // Parse
    try {
      Statement stmt = parser.parse(tokens);
      std::cout << "Parsed: " << statementToString(stmt) << std::endl;
    } catch (const std::runtime_error &e) {
      std::cerr << "Error: " << e.what() << std::endl;
    }
  }

  return 0;
}
