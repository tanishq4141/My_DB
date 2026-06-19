#pragma once
#include <iostream>
#include <string>
#include <vector>

// Represents every type of token the lexer can produce
enum class TokenType {
  // SQL Keywords
  TOKEN_SELECT,
  TOKEN_INSERT,
  TOKEN_INTO,
  TOKEN_VALUES,
  TOKEN_FROM,
  TOKEN_WHERE,
  TOKEN_CREATE,
  TOKEN_TABLE,
  TOKEN_DELETE,
  TOKEN_INT,
  TOKEN_TEXT,

  // Literals & identifiers
  TOKEN_NUMBER,     // e.g. 42
  TOKEN_STRING,     // e.g. 'hello'
  TOKEN_IDENTIFIER, // e.g. column_name, table_name

  // Symbols
  TOKEN_STAR,      // *
  TOKEN_COMMA,     // ,
  TOKEN_LPAREN,    // (
  TOKEN_RPAREN,    // )
  TOKEN_SEMICOLON, // ;
  TOKEN_EQUALS,    // =

  // Special
  TOKEN_EOF,
  TOKEN_UNKNOWN
};

// A single token: its type + the raw text it came from
struct Token {
  TokenType type;
  std::string value;

  Token(TokenType type, const std::string &value) : type(type), value(value) {}
};

// Returns a human-readable name for a token type (useful for debugging)
std::string tokenTypeName(TokenType type);

class Lexer {
public:
  Lexer();

  // Tokenize an input SQL string into a vector of tokens
  std::vector<Token> handleInput(const std::string &input);

private:
  // Helper: check if a word is a SQL keyword and return its TokenType
  TokenType keywordType(const std::string &word);
};
