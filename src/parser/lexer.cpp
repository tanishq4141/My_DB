#include "lexer.h"
#include <algorithm>
#include <cctype>
#include <iostream>

Lexer::Lexer() { std::cout << "Initializing lexer..." << std::endl; }

// Convert a string to uppercase for case-insensitive keyword matching
static std::string toUpper(const std::string &s) {
  std::string result = s;
  std::transform(result.begin(), result.end(), result.begin(), ::toupper);
  return result;
}

TokenType Lexer::keywordType(const std::string &word) {
  std::string upper = toUpper(word);
  if (upper == "SELECT") return TokenType::TOKEN_SELECT;
  if (upper == "INSERT") return TokenType::TOKEN_INSERT;
  if (upper == "INTO") return TokenType::TOKEN_INTO;
  if (upper == "VALUES") return TokenType::TOKEN_VALUES;
  if (upper == "FROM") return TokenType::TOKEN_FROM;
  if (upper == "WHERE") return TokenType::TOKEN_WHERE;
  if (upper == "CREATE") return TokenType::TOKEN_CREATE;
  if (upper == "TABLE") return TokenType::TOKEN_TABLE;
  if (upper == "DELETE") return TokenType::TOKEN_DELETE;
  if (upper == "INT") return TokenType::TOKEN_INT;
  if (upper == "TEXT") return TokenType::TOKEN_TEXT;
  return TokenType::TOKEN_IDENTIFIER;
}

std::vector<Token> Lexer::handleInput(const std::string &input) {
  std::vector<Token> tokens;
  size_t pos = 0;
  size_t len = input.length();

  while (pos < len) {
    // Skip whitespace
    if (std::isspace(input[pos])) {
      pos++;
      continue;
    }

    // Single-character symbols
    char c = input[pos];
    if (c == '*') {
      tokens.emplace_back(TokenType::TOKEN_STAR, "*");
      pos++;
      continue;
    }
    if (c == ',') {
      tokens.emplace_back(TokenType::TOKEN_COMMA, ",");
      pos++;
      continue;
    }
    if (c == '(') {
      tokens.emplace_back(TokenType::TOKEN_LPAREN, "(");
      pos++;
      continue;
    }
    if (c == ')') {
      tokens.emplace_back(TokenType::TOKEN_RPAREN, ")");
      pos++;
      continue;
    }
    if (c == ';') {
      tokens.emplace_back(TokenType::TOKEN_SEMICOLON, ";");
      pos++;
      continue;
    }
    if (c == '=') {
      tokens.emplace_back(TokenType::TOKEN_EQUALS, "=");
      pos++;
      continue;
    }

    // String literals: 'hello world'
    if (c == '\'') {
      pos++; // skip opening quote
      std::string str;
      while (pos < len && input[pos] != '\'') {
        str += input[pos];
        pos++;
      }
      if (pos < len) {
        pos++; // skip closing quote
      }
      tokens.emplace_back(TokenType::TOKEN_STRING, str);
      continue;
    }

    // Numbers: 123, 456
    if (std::isdigit(c)) {
      std::string num;
      while (pos < len && std::isdigit(input[pos])) {
        num += input[pos];
        pos++;
      }
      tokens.emplace_back(TokenType::TOKEN_NUMBER, num);
      continue;
    }

    // Keywords and identifiers: SELECT, table_name, col1
    if (std::isalpha(c) || c == '_') {
      std::string word;
      while (pos < len && (std::isalnum(input[pos]) || input[pos] == '_')) {
        word += input[pos];
        pos++;
      }
      TokenType type = keywordType(word);
      tokens.emplace_back(type, word);
      continue;
    }

    // Unknown character
    tokens.emplace_back(TokenType::TOKEN_UNKNOWN, std::string(1, c));
    pos++;
  }

  tokens.emplace_back(TokenType::TOKEN_EOF, "");
  return tokens;
}

std::string tokenTypeName(TokenType type) {
  switch (type) {
  case TokenType::TOKEN_SELECT: return "SELECT";
  case TokenType::TOKEN_INSERT: return "INSERT";
  case TokenType::TOKEN_INTO: return "INTO";
  case TokenType::TOKEN_VALUES: return "VALUES";
  case TokenType::TOKEN_FROM: return "FROM";
  case TokenType::TOKEN_WHERE: return "WHERE";
  case TokenType::TOKEN_CREATE: return "CREATE";
  case TokenType::TOKEN_TABLE: return "TABLE";
  case TokenType::TOKEN_DELETE: return "DELETE";
  case TokenType::TOKEN_INT: return "INT";
  case TokenType::TOKEN_TEXT: return "TEXT";
  case TokenType::TOKEN_NUMBER: return "NUMBER";
  case TokenType::TOKEN_STRING: return "STRING";
  case TokenType::TOKEN_IDENTIFIER: return "IDENTIFIER";
  case TokenType::TOKEN_STAR: return "STAR";
  case TokenType::TOKEN_COMMA: return "COMMA";
  case TokenType::TOKEN_LPAREN: return "LPAREN";
  case TokenType::TOKEN_RPAREN: return "RPAREN";
  case TokenType::TOKEN_SEMICOLON: return "SEMICOLON";
  case TokenType::TOKEN_EQUALS: return "EQUALS";
  case TokenType::TOKEN_EOF: return "EOF";
  case TokenType::TOKEN_UNKNOWN: return "UNKNOWN";
  }
  return "UNKNOWN";
}
