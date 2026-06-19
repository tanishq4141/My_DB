#pragma once
#include "lexer.h"
#include <stdexcept>
#include <string>
#include <vector>

// A column definition for CREATE TABLE, e.g. "id INT"
struct ColumnDef {
  std::string name;
  std::string type; // "INT" or "TEXT"
};

// A simple WHERE clause: column = value
struct WhereClause {
  std::string column;
  std::string value; // the right-hand side (number or string)
};

// The type of SQL statement that was parsed
enum class StatementType {
  CREATE_TABLE,
  INSERT,
  SELECT,
  DELETE_STMT // avoid conflict with C delete keyword
};

// A parsed SQL statement — tagged union style
struct Statement {
  StatementType type;
  std::string tableName;

  // CREATE TABLE fields
  std::vector<ColumnDef> columns;

  // INSERT fields
  std::vector<std::string> values;

  // SELECT fields
  std::vector<std::string> selectColumns; // empty means SELECT *
  bool selectAll = false;

  // WHERE clause (used by SELECT and DELETE)
  bool hasWhere = false;
  WhereClause where;
};

// Returns a human-readable description of a parsed statement
std::string statementToString(const Statement &stmt);

class Parser {
public:
  Parser();

  // Parse a token stream into a Statement. Throws std::runtime_error on failure.
  Statement parse(const std::vector<Token> &tokens);

private:
  const std::vector<Token> *tokens_;
  size_t pos_;

  // Get current token without advancing
  const Token &current() const;

  // Advance and return the previous token
  const Token &advance();

  // Check if current token matches expected type
  bool check(TokenType type) const;

  // If current matches, advance and return true
  bool match(TokenType type);

  // Expect current token to be of given type, or throw an error
  const Token &expect(TokenType type, const std::string &errorMsg);

  // Parse each statement type
  Statement parseCreateTable();
  Statement parseInsert();
  Statement parseSelect();
  Statement parseDelete();

  // Parse an optional WHERE clause
  WhereClause parseWhere();
};
