#pragma once

#include "ASTProgram.h"

#include "antlr4-runtime.h"

#include <fstream>
#include <iostream>
#include <string>

//! \brief Lexer error listener for redirecting ANTLR4 errors to ParseError.
class LexerErrorListener : public antlr4::BaseErrorListener {
public:
  LexerErrorListener() = default;

  virtual void syntaxError(antlr4::Recognizer *recognizer,
                           antlr4::Token *offendingSymbol, size_t line,
                           size_t charPositionInLine, const std::string &msg,
                           std::exception_ptr e) override;
};

//! \brief Parse error listener for redirecting ANTLR4 errors to ParseError.
class ParserErrorListener : public antlr4::BaseErrorListener {
public:
  ParserErrorListener() = default;

  virtual void syntaxError(antlr4::Recognizer *recognizer,
                           antlr4::Token *offendingSymbol, size_t line,
                           size_t charPositionInLine, const std::string &msg,
                           std::exception_ptr e) override;
};

/*! \class FrontEnd
 *  \brief A collection of routines implementing the compiler front end.
 */
class FrontEnd {
public:
  // Validate a complete program and return its ANTLR tree as text. No AST,
  // name resolution, type checking, or execution is performed.
  [[nodiscard]] static std::string parseSyntax(std::istream &stream);

  /*! \fn parse
   *  \brief Parse an input stream and return an AST.
   *
   * Parsing can detect errors, which are reported via throw of a ParseError
   * exception.  In the absence of errors, ownership of the generated AST is
   * transfered to the caller.
   * Valid SOP constructs without AST support throw SemanticError; use
   * parseSyntax to validate those without entering AST construction.
   * \param stream the input stream holding the program text.
   * \return the generated AST.
   */
  [[nodiscard]] static std::shared_ptr<ASTProgram> parse(std::istream &stream);

  /*! \fn print
   *  \brief Print program in a standard form to cout.
   *
   * \param program the root of an AST for a TOP program.
   */
  static void prettyprint(ASTProgram *program, std::ostream &os);

  static void astVisualize(std::shared_ptr<ASTNode> node, std::ostream &os,
                           const std::string &format = "dot");
};
