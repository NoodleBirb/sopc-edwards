#include "FrontEnd.h"

#include "ASTBuilder.h"
#include "ASTVisualizer.h"
#include "ParseError.h"
#include "PrettyPrinter.h"
#include "SemanticError.h"
#include "TOPBaseVisitor.h"
#include "TOPLexer.h"
#include "TOPParser.h"

#include "loguru.hpp"
#include <any>

using namespace std;
using namespace antlr4;

namespace {
// The inherited AST visitor would otherwise silently visit children of SOP
// nodes that it does not implement, producing an incomplete or incorrect AST.
class RequireTOPAst : public TOPBaseVisitor {
  std::any unsupported() {
    throw SemanticError("SOP AST construction is not implemented; use --parse-only");
  }

public:
  std::any visitSequenceExpr(TOPParser::SequenceExprContext *) override {
    return unsupported();
  }
  std::any visitIndexExpr(TOPParser::IndexExprContext *) override {
    return unsupported();
  }
  std::any visitSliceExpr(TOPParser::SliceExprContext *) override {
    return unsupported();
  }
  std::any visitLengthExpr(TOPParser::LengthExprContext *) override {
    return unsupported();
  }
  std::any visitLambdaExpr(TOPParser::LambdaExprContext *) override {
    return unsupported();
  }
  std::any visitForStmt(TOPParser::ForStmtContext *) override {
    return unsupported();
  }
};
} // namespace

void LexerErrorListener::syntaxError(Recognizer *recognizer,
                                     Token *offendingSymbol, size_t line,
                                     size_t charPositionInLine,
                                     const std::string &msg,
                                     std::exception_ptr e) {
  throw ParseError(msg + "@" + std::to_string(line) + ":" +
                   std::to_string(charPositionInLine));
}

void ParserErrorListener::syntaxError(Recognizer *recognizer,
                                      Token *offendingSymbol, size_t line,
                                      size_t charPositionInLine,
                                      const std::string &msg,
                                      std::exception_ptr e) {
  throw ParseError(msg + "@" + std::to_string(line) + ":" +
                   std::to_string(charPositionInLine));
}

std::string FrontEnd::parseSyntax(std::istream &stream) {
  ANTLRInputStream input(stream);
  TOPLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  TOPParser parser(&tokens);
  LexerErrorListener lexerErrorListener;
  ParserErrorListener parserErrorListener;
  lexer.removeErrorListeners();
  lexer.addErrorListener(&lexerErrorListener);
  parser.removeErrorListeners();
  parser.addErrorListener(&parserErrorListener);
  auto *tree = parser.program();
  return tree->toStringTree(&parser, false);
}

std::shared_ptr<ASTProgram> FrontEnd::parse(std::istream &stream) {
  ANTLRInputStream input(stream);
  TOPLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  TOPParser parser(&tokens);
  LexerErrorListener lexerErrorListener;
  ParserErrorListener parserErrorListener;

  // Add error listeners
  lexer.removeErrorListeners();
  lexer.addErrorListener(&lexerErrorListener);

  parser.removeParseListeners();
  parser.removeErrorListeners();
  parser.addErrorListener(&parserErrorListener);

  LOG_S(1) << "Parsing program";

  TOPParser::ProgramContext *tree = parser.program();

  RequireTOPAst supported;
  supported.visit(tree);

  LOG_S(1) << "Building AST";

  ASTBuilder ab(&parser);
  return ab.build(tree);
}

void FrontEnd::prettyprint(ASTProgram *program, std::ostream &os) {
  PrettyPrinter::print(program, os, ' ', 2);
}

void FrontEnd::astVisualize(std::shared_ptr<ASTNode> node, std::ostream &os,
                            const std::string &format) {
  ASTVisualizer visualizer(os);
  SyntaxTree syntaxTree(node);
  if (format == "ascii") {
    visualizer.buildAscii(syntaxTree);
  } else {
    visualizer.buildGraph(syntaxTree);
  }
}
