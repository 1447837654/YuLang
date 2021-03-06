#include "front/lexer.h"

#include <iostream>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <cctype>

#include "define/token.h"

using namespace yulang::front;
using namespace yulang::define;

namespace {

enum class NumberType {
  Normal, Hex, Bin, Float
};

const char *kKeywords[] = {YULANG_KEYWORDS(YULANG_EXPAND_SECOND)};
const char *kOperators[] = {YULANG_OPERATORS(YULANG_EXPAND_SECOND)};

// get index of a string in string array
template <typename T, std::size_t N>
int GetIndex(const char *str, T (&str_array)[N]) {
  for (std::size_t i = 0; i < N; ++i) {
    if (!std::strcmp(str, str_array[i])) return i;
  }
  return -1;
}

bool IsOperatorChar(char c) {
  const char op_chars[] = "`~!@#$%^&*-=+\\|:<.>/?";
  for (const auto &i : op_chars) {
    if (i == c) return true;
  }
  return false;
}

}  // namespace

Token Lexer::LogError(std::string_view message) {
  logger_.LogError(message);
  return Token::Error;
}

int Lexer::ReadEscape() {
  // eat '\'
  NextChar();
  if (IsEOL()) return -1;
  switch (last_char_) {
    case 'a': return '\a';
    case 'b': return '\b';
    case 'f': return '\f';
    case 'n': return '\n';
    case 'r': return '\r';
    case 't': return '\t';
    case 'v': return '\v';
    case '\\': return '\\';
    case '\'': return '\'';
    case '"': return '"';
    case '0': return '\0';
    case 'x': {
      char hex[3] = {0};
      char *end_pos;
      // read 2 hex digits
      for (int i = 0; i < 2; ++i) {
        NextChar();
        if (IsEOL()) return -1;
        hex[i] = last_char_;
      }
      // convert to character
      auto ret = std::strtol(hex, &end_pos, 16);
      return *end_pos ? -1 : ret;
    }
    default: return -1;
  }
}

void Lexer::SkipSpaces() {
  while (!IsEOL() && std::isspace(last_char_)) NextChar();
}

Token Lexer::HandleId() {
  // read string
  std::string id;
  do {
    id += last_char_;
    NextChar();
  } while (!IsEOL() && (std::isalnum(last_char_) || last_char_ == '_'));
  // check if string is keyword
  int index = GetIndex(id.c_str(), kKeywords);
  if (index < 0) {
    id_val_ = id;
    return Token::Id;
  }
  else {
    key_val_ = static_cast<Keyword>(index);
    return Token::Keyword;
  }
}

Token Lexer::HandleNum() {
  std::string num;
  NumberType num_type = NumberType::Normal;
  // check if is hexadecimal/binary/floating-point number
  if (last_char_ == '0') {
    NextChar();
    switch (last_char_) {
      // hexadecimal
      case 'x': case 'X': num_type = NumberType::Hex; break;
      // binary
      case 'b': case 'B': num_type = NumberType::Bin; break;
      // floating point
      case '.': num = "0."; num_type = NumberType::Float; break;
      default: {
        if (IsEOL() || !std::isdigit(last_char_)) {
          // just zero
          int_val_ = 0;
          return Token::Int;
        }
        else {
          return LogError("invalid number literal");
        }
        break;
      }
    }
    NextChar();
  }
  // read number string
  while (!IsEOL() && (std::isxdigit(last_char_) || last_char_ == '.' ||
                      last_char_ == '_')) {
    if (num_type != NumberType::Float && last_char_ == '.') {
      num_type = NumberType::Float;
    }
    if (last_char_ != '_') num += last_char_;
    NextChar();
  }
  // convert to number
  char *end_pos;
  Token ret;
  switch (num_type) {
    case NumberType::Hex: {
      int_val_ = std::strtoull(num.c_str(), &end_pos, 16);
      ret = Token::Int;
      break;
    }
    case NumberType::Bin: {
      int_val_ = std::strtoull(num.c_str(), &end_pos, 2);
      ret = Token::Int;
      break;
    }
    case NumberType::Normal: {
      int_val_ = std::strtoull(num.c_str(), &end_pos, 10);
      ret = Token::Int;
      break;
    }
    case NumberType::Float: {
      fp_val_ = std::strtod(num.c_str(), &end_pos);
      ret = Token::Float;
      break;
    }
    default:;
  }
  // check if conversion is valid
  return *end_pos ? LogError("invalid number literal") : ret;
}

Token Lexer::HandleString() {
  std::string str;
  // start with quotes
  NextChar();
  while (last_char_ != '"') {
    if (last_char_ == '\\') {
      // read escape char
      int ret = ReadEscape();
      if (ret < 0) return LogError("invalid escape character");
      str += ret;
    }
    else {
      str += last_char_;
    }
    NextChar();
    if (IsEOL()) return LogError("expected '\"'");
  }
  // eat right quotation mark
  NextChar();
  str_val_ = str;
  return Token::String;
}

Token Lexer::HandleChar() {
  // start with quotes
  NextChar();
  if (last_char_ == '\\') {
    // read escape char
    int ret = ReadEscape();
    if (ret < 0) return LogError("invalid escape character");
    char_val_ = ret;
  }
  else {
    char_val_ = last_char_;
  }
  NextChar();
  // check & eat right quotation mark
  if (last_char_ != '\'') return LogError("expected \"'\"");
  NextChar();
  return Token::Char;
}

Token Lexer::HandleOperator() {
  std::string op;
  // read first char
  op += last_char_;
  NextChar();
  // check if is comment
  if (op[0] == '/') {
    if (!IsEOL()) {
      switch (last_char_) {
        case '/': return HandleComment();
        case '*': return HandleBlockComment();
      }
    }
  }
  // read rest chars
  while (!IsEOL() && IsOperatorChar(last_char_)) {
    op += last_char_;
    NextChar();
  }
  // check if operator is valid
  int index = GetIndex(op.c_str(), kOperators);
  if (index < 0) {
    // treat unknown operator as identifier
    id_val_ = op;
    return Token::Id;
  }
  else {
    op_val_ = static_cast<Operator>(index);
    return Token::Operator;
  }
}

Token Lexer::HandleComment() {
  // eat '/'
  NextChar();
  while (!IsEOL()) NextChar();
  return NextToken();
}

Token Lexer::HandleBlockComment() {
  // eat '*'
  NextChar();
  // read until there is '*/' in stream
  bool star = false;
  while (!in_.eof() && !(star && last_char_ == '/')) {
    star = last_char_ == '*';
    if (IsEOL() && !in_.eof()) logger_.IncreaseLinePos();
    NextChar();
  }
  // check unclosed block comment
  if (in_.eof()) return LogError("comment unclosed at EOF");
  // eat '/'
  NextChar();
  return NextToken();
}

Token Lexer::HandleEOL() {
  do {
    logger_.IncreaseLinePos();
    NextChar();
  } while (IsEOL() && !in_.eof());
  return Token::EOL;
}

void Lexer::Reset() {
  logger_.Reset();
  last_char_ = ' ';
  // check if file was opened
  if (!in_.is_open()) {
    LogError("failed to open file");
  }
  else {
    // reset status of file stream
    in_.clear();
    in_.seekg(0, std::ios::beg);
    in_ >> std::noskipws;
  }
}

Token Lexer::NextToken() {
  // end of file
  if (in_.eof()) return Token::End;
  // skip spaces
  SkipSpaces();
  // id or keyword
  if (std::isalpha(last_char_) || last_char_ == '_') return HandleId();
  // number
  if (std::isdigit(last_char_)) return HandleNum();
  // string
  if (last_char_ == '"') return HandleString();
  // character
  if (last_char_ == '\'') return HandleChar();
  // operator or id
  if (IsOperatorChar(last_char_)) return HandleOperator();
  // end of line (line break or delimiter)
  if (IsEOL()) return HandleEOL();
  if (last_char_ == ';') {
    NextChar();
    return Token::EOL;
  }
  // other characters
  other_val_ = last_char_;
  NextChar();
  return Token::Other;
}
