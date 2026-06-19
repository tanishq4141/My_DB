#include "parser.h"
#include <iostream>
#include <sstream>

Parser::Parser() : tokens_(nullptr), pos_(0) {}

const Token &Parser::current() const { return (*tokens_)[pos_]; }

const Token &Parser::advance() {
  const Token &tok = (*tokens_)[pos_];
  if (tok.type != TokenType::TOKEN_EOF) {
    pos_++;
  }
  return tok;
}

bool Parser::check(TokenType type) const { return current().type == type; }

bool Parser::match(TokenType type) {
  if (check(type)) {
    advance();
    return true;
  }
  return false;
}

const Token &Parser::expect(TokenType type, const std::string &errorMsg) {
  if (check(type)) {
    return advance();
  }
  throw std::runtime_error("Parse error: " + errorMsg + " (got '" +
                           current().value + "')");
}

Statement Parser::parse(const std::vector<Token> &tokens) {
  tokens_ = &tokens;
  pos_ = 0;

  if (check(TokenType::TOKEN_CREATE)) {
    return parseCreateTable();
  }
  if (check(TokenType::TOKEN_INSERT)) {
    return parseInsert();
  }
  if (check(TokenType::TOKEN_SELECT)) {
    return parseSelect();
  }
  if (check(TokenType::TOKEN_DELETE)) {
    return parseDelete();
  }

  throw std::runtime_error(
      "Parse error: expected CREATE, INSERT, SELECT, or DELETE (got '" +
      current().value + "')");
}

// CREATE TABLE table_name (col1 TYPE1, col2 TYPE2, ...);
Statement Parser::parseCreateTable() {
  Statement stmt;
  stmt.type = StatementType::CREATE_TABLE;

  expect(TokenType::TOKEN_CREATE, "expected CREATE");
  expect(TokenType::TOKEN_TABLE, "expected TABLE after CREATE");

  const Token &name =
      expect(TokenType::TOKEN_IDENTIFIER, "expected table name");
  stmt.tableName = name.value;

  expect(TokenType::TOKEN_LPAREN, "expected '(' after table name");

  // Parse column definitions: name TYPE, name TYPE, ...
  while (!check(TokenType::TOKEN_RPAREN) &&
         !check(TokenType::TOKEN_EOF)) {
    ColumnDef col;
    const Token &colName =
        expect(TokenType::TOKEN_IDENTIFIER, "expected column name");
    col.name = colName.value;

    // Column type: INT or TEXT
    if (check(TokenType::TOKEN_INT)) {
      col.type = "INT";
      advance();
    } else if (check(TokenType::TOKEN_TEXT)) {
      col.type = "TEXT";
      advance();
    } else {
      throw std::runtime_error("Parse error: expected column type INT or TEXT "
                               "(got '" +
                               current().value + "')");
    }

    stmt.columns.push_back(col);

    // Consume comma between columns, but not after the last one
    if (check(TokenType::TOKEN_COMMA)) {
      advance();
    }
  }

  expect(TokenType::TOKEN_RPAREN, "expected ')' after column definitions");
  match(TokenType::TOKEN_SEMICOLON); // optional semicolon

  return stmt;
}

// INSERT INTO table_name VALUES (val1, val2, ...);
Statement Parser::parseInsert() {
  Statement stmt;
  stmt.type = StatementType::INSERT;

  expect(TokenType::TOKEN_INSERT, "expected INSERT");
  expect(TokenType::TOKEN_INTO, "expected INTO after INSERT");

  const Token &name =
      expect(TokenType::TOKEN_IDENTIFIER, "expected table name");
  stmt.tableName = name.value;

  expect(TokenType::TOKEN_VALUES, "expected VALUES");
  expect(TokenType::TOKEN_LPAREN, "expected '(' after VALUES");

  // Parse values: can be numbers, strings, or identifiers
  while (!check(TokenType::TOKEN_RPAREN) &&
         !check(TokenType::TOKEN_EOF)) {
    if (check(TokenType::TOKEN_NUMBER) || check(TokenType::TOKEN_STRING) ||
        check(TokenType::TOKEN_IDENTIFIER)) {
      stmt.values.push_back(current().value);
      advance();
    } else {
      throw std::runtime_error("Parse error: expected a value (got '" +
                               current().value + "')");
    }

    if (check(TokenType::TOKEN_COMMA)) {
      advance();
    }
  }

  expect(TokenType::TOKEN_RPAREN, "expected ')' after values");
  match(TokenType::TOKEN_SEMICOLON);

  return stmt;
}

// SELECT (* | col1, col2, ...) FROM table_name [WHERE col = val];
Statement Parser::parseSelect() {
  Statement stmt;
  stmt.type = StatementType::SELECT;

  expect(TokenType::TOKEN_SELECT, "expected SELECT");

  // Column list or *
  if (match(TokenType::TOKEN_STAR)) {
    stmt.selectAll = true;
  } else {
    stmt.selectAll = false;
    // Parse comma-separated column names
    while (true) {
      const Token &col =
          expect(TokenType::TOKEN_IDENTIFIER, "expected column name");
      stmt.selectColumns.push_back(col.value);
      if (!match(TokenType::TOKEN_COMMA)) {
        break;
      }
    }
  }

  expect(TokenType::TOKEN_FROM, "expected FROM");

  const Token &name =
      expect(TokenType::TOKEN_IDENTIFIER, "expected table name");
  stmt.tableName = name.value;

  // Optional WHERE clause
  if (check(TokenType::TOKEN_WHERE)) {
    stmt.hasWhere = true;
    stmt.where = parseWhere();
  }

  match(TokenType::TOKEN_SEMICOLON);
  return stmt;
}

// DELETE FROM table_name [WHERE col = val];
Statement Parser::parseDelete() {
  Statement stmt;
  stmt.type = StatementType::DELETE_STMT;

  expect(TokenType::TOKEN_DELETE, "expected DELETE");
  expect(TokenType::TOKEN_FROM, "expected FROM after DELETE");

  const Token &name =
      expect(TokenType::TOKEN_IDENTIFIER, "expected table name");
  stmt.tableName = name.value;

  if (check(TokenType::TOKEN_WHERE)) {
    stmt.hasWhere = true;
    stmt.where = parseWhere();
  }

  match(TokenType::TOKEN_SEMICOLON);
  return stmt;
}

// WHERE column = value
WhereClause Parser::parseWhere() {
  WhereClause wc;
  expect(TokenType::TOKEN_WHERE, "expected WHERE");

  const Token &col =
      expect(TokenType::TOKEN_IDENTIFIER, "expected column name in WHERE");
  wc.column = col.value;

  expect(TokenType::TOKEN_EQUALS, "expected '=' in WHERE clause");

  // Value can be a number, string, or identifier
  if (check(TokenType::TOKEN_NUMBER) || check(TokenType::TOKEN_STRING) ||
      check(TokenType::TOKEN_IDENTIFIER)) {
    wc.value = current().value;
    advance();
  } else {
    throw std::runtime_error(
        "Parse error: expected a value after '=' in WHERE clause (got '" +
        current().value + "')");
  }

  return wc;
}

// Pretty-print a parsed statement for debugging
std::string statementToString(const Statement &stmt) {
  std::ostringstream ss;

  switch (stmt.type) {
  case StatementType::CREATE_TABLE:
    ss << "CREATE TABLE " << stmt.tableName << " (\n";
    for (size_t i = 0; i < stmt.columns.size(); i++) {
      ss << "  " << stmt.columns[i].name << " " << stmt.columns[i].type;
      if (i + 1 < stmt.columns.size())
        ss << ",";
      ss << "\n";
    }
    ss << ")";
    break;

  case StatementType::INSERT:
    ss << "INSERT INTO " << stmt.tableName << " VALUES (";
    for (size_t i = 0; i < stmt.values.size(); i++) {
      ss << stmt.values[i];
      if (i + 1 < stmt.values.size())
        ss << ", ";
    }
    ss << ")";
    break;

  case StatementType::SELECT:
    ss << "SELECT ";
    if (stmt.selectAll) {
      ss << "*";
    } else {
      for (size_t i = 0; i < stmt.selectColumns.size(); i++) {
        ss << stmt.selectColumns[i];
        if (i + 1 < stmt.selectColumns.size())
          ss << ", ";
      }
    }
    ss << " FROM " << stmt.tableName;
    if (stmt.hasWhere) {
      ss << " WHERE " << stmt.where.column << " = " << stmt.where.value;
    }
    break;

  case StatementType::DELETE_STMT:
    ss << "DELETE FROM " << stmt.tableName;
    if (stmt.hasWhere) {
      ss << " WHERE " << stmt.where.column << " = " << stmt.where.value;
    }
    break;
  }

  return ss.str();
}
