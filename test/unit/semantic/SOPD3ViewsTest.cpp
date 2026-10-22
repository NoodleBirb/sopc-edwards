#include "SOPD3Cases.h"

#include "ASTBorrowExpr.h"
#include "ASTForStmt.h"
#include "ASTLambdaExpr.h"
#include "ConstraintRenderer.h"
#include "FrontEnd.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>

namespace {
std::string compact(std::string text) {
  text.erase(std::remove_if(text.begin(), text.end(),
                            [](unsigned char c) { return std::isspace(c); }),
             text.end());
  return text;
}

std::string quote(const std::string &text) {
  std::string result = "'";
  for (char c : text)
    result += c == '\'' ? "'\\''" : std::string(1, c);
  return result + "'";
}

std::string read(const std::filesystem::path &path) {
  std::ifstream input(path);
  if (!input)
    throw std::runtime_error("cannot read " + path.string());
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

class Invocation {
  std::filesystem::path directory;

public:
  int status = -1;
  std::string output;
  std::string error;
  std::string callGraph;

  Invocation(const std::string &source,
             const std::vector<std::string> &options) {
    auto pattern =
        (std::filesystem::temp_directory_path() / "sopc-d3-XXXXXX").string();
    std::vector<char> buffer(pattern.begin(), pattern.end());
    buffer.push_back('\0');
    auto *created = mkdtemp(buffer.data());
    if (!created)
      throw std::runtime_error("cannot create D3 test directory");
    directory = created;
    try {
      const auto inputPath = directory / "fixture.sop";
      const auto stdoutPath = directory / "stdout.txt";
      const auto stderrPath = directory / "stderr.txt";
      std::ofstream input(inputPath);
      input << source;
      input.close();
      if (!input)
        throw std::runtime_error("cannot write D3 fixture");
      std::string command = quote(SOP_D3_TOPC);
      for (const auto &option : options)
        command += " " + quote(option);
      command += " " + quote(inputPath.string()) + " > " +
                 quote(stdoutPath.string()) + " 2> " +
                 quote(stderrPath.string());
      const int rawStatus = std::system(command.c_str());
      if (rawStatus != -1 && WIFEXITED(rawStatus))
        status = WEXITSTATUS(rawStatus);
      output = read(stdoutPath);
      error = read(stderrPath);
      const auto graphPath = directory / "fixture.sop.callgraph.txt";
      if (std::filesystem::exists(graphPath))
        callGraph = read(graphPath);
    } catch (...) {
      std::error_code ignored;
      std::filesystem::remove_all(directory, ignored);
      throw;
    }
  }

  ~Invocation() {
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
  }
  Invocation(const Invocation &) = delete;
  Invocation &operator=(const Invocation &) = delete;
};

void checkSuccess(const Invocation &run) {
  INFO("stdout:\n" << run.output);
  INFO("stderr:\n" << run.error);
  REQUIRE(run.status == 0);
  CHECK_THAT(run.error, !Catch::Matchers::ContainsSubstring("semantic error"));
  CHECK_THAT(run.error, !Catch::Matchers::ContainsSubstring("not implemented"));
}

void contains(const std::string &output, const std::string &expected) {
  INFO("expected output fact: " << expected);
  CHECK_THAT(compact(output),
             Catch::Matchers::ContainsSubstring(compact(expected)));
}

bool matches(const std::string &text, const std::string &pattern) {
  return std::regex_search(text, std::regex(pattern, std::regex::icase));
}

// Find the additional plan section, independently of whitespace and its exact
// heading spelling. Existing type/ownership summaries cannot satisfy this
// check.
std::string cleanupDetail(const std::string &output) {
  std::istringstream input(output);
  std::string line, result;
  bool collecting = false;
  while (std::getline(input, line)) {
    if (!line.empty() && line.front() == '[') {
      collecting =
          matches(line, "cleanup|destruction") && matches(line, "plan|detail");
    }
    if (collecting)
      result += line + "\n";
  }
  INFO("SL18/SL19 require a separate, labeled cleanup-plan detail section");
  REQUIRE_FALSE(result.empty());
  return result;
}

const sopd3::Case &namedCase(const std::string &name) {
  const auto &all = sopd3::cases();
  const auto found = std::find_if(
      all.begin(), all.end(), [&](const auto &c) { return c.name == name; });
  if (found == all.end())
    throw std::runtime_error("unknown D3 fixture: " + name);
  return *found;
}
} // namespace

TEST_CASE("SOP D3 view assertions work on existing TOP", "[SOP][D3][harness]") {
  Invocation run("main() { var p, q; p = alloc 7; q = p; return *q; }",
                 {"--ptype", "--pownership", "--pborrow", "--constraint"});
  checkSuccess(run);
  contains(run.output, "local main.q : own&int");
  contains(run.output, "local main.q : Own");
  contains(run.output, "main : 1 destroy");
  CHECK(matches(run.output, "\\[move\\][^\\n]*\\bmove[ \\t]+p\\b"));
}

TEST_CASE("SOP D3 production inspection views", "[SOP][D3][views]") {
  for (const auto &example : sopd3::cases()) {
    if (!example.diagnostic.empty())
      continue;
    DYNAMIC_SECTION(example.group << ": " << example.name << " ["
                                  << example.requirements << "]") {
      Invocation run(example.source,
                     {"--ptype", "--pownership", "--pborrow", "--constraint",
                      "--pcallgraph=ascii", "--pcfg=ascii"});
      checkSuccess(run);
      for (const auto &local : example.locals) {
        // The production --constraint type records qualify locals by function.
        if (!local.type.empty())
          contains(run.output, "local " + local.function + "." + local.name +
                                   " : " + local.type);
        contains(run.output, "local " + local.function + "." + local.name +
                                 " : " + (local.own ? "Own" : "Copy"));
      }
      for (const auto &expected : example.destructions) {
        const std::string pattern =
            "(^|\\n)[ \\t]*" + expected.function + "[ \\t]*:[ \\t]*" +
            std::to_string(expected.targets.size()) + "[ \\t]+destroys?\\b";
        INFO("named-destruction count in " << expected.function);
        CHECK(matches(run.output, pattern));
      }
      for (const auto &event : example.moves)
        CHECK(matches(run.output, "\\[move\\][^\\n]*\\b" + event.kind +
                                      "[ \\t]+" + event.variable + "\\b"));
      for (const auto &name : example.neverMoved)
        CHECK_FALSE(matches(run.output,
                            "\\[move\\][^\\n]*\\bmove[ \\t]+" + name + "\\b"));

      std::istringstream input(example.source);
      auto ast = FrontEnd::parse(input);
      std::function<void(ASTNode *, bool)> inspect = [&](ASTNode *node,
                                                         bool loopHeader) {
        if (auto *borrow = dynamic_cast<ASTBorrowExpr *>(node)) {
          std::ostringstream expression;
          expression << *borrow;
          const auto span = ConstraintRenderer::formatSpan(borrow->getLine(),
                                                           borrow->getColumn());
          contains(run.output, span + " " + expression.str() + " -> approved");
          // The detailed context must accompany the borrow's own expression.
          std::istringstream lines(run.output);
          std::string line;
          bool context = false;
          while (std::getline(lines, line)) {
            if (compact(line).find(compact(expression.str())) ==
                std::string::npos)
              continue;
            if (matches(line, loopHeader ? "for iterable|loop iterable"
                                         : "direct argument"))
              context = true;
          }
          CHECK(context);
        }
        if (auto *lambda = dynamic_cast<ASTLambdaExpr *>(node)) {
          std::ostringstream expression;
          expression << *lambda;
          contains(run.output, expression.str());
          contains(run.output, ConstraintRenderer::formatSpan(
                                   lambda->getLine(), lambda->getColumn()));
          for (auto *parameter : lambda->getParameters())
            CHECK(matches(run.output,
                          "\\b" + parameter->getName() + "\\s*:\\s*"));
        }
        auto *loop = dynamic_cast<ASTForStmt *>(node);
        for (const auto &child : node->getChildren())
          inspect(child.get(), loop && child.get() == loop->getInput());
      };
      inspect(ast.get(), false);
      if (example.loopCFG) {
        contains(run.output, "for");
        contains(run.output, "true");
        contains(run.output, "false");
      }
      if (!example.callTargets.empty()) {
        REQUIRE_FALSE(run.callGraph.empty());
        contains(run.callGraph, "main");
        contains(run.output, "call-target");
      }
    }
  }
}

TEST_CASE("SOP D3 recursive cleanup plan detail", "[SOP][D3][cleanup-plan]") {
  const auto &example = namedCase("retained recursive owners");
  Invocation run(example.source, {"--pownership", "--constraint"});
  checkSuccess(run);
  const auto detail = cleanupDetail(run.output);
  for (const auto &local : example.locals) {
    contains(detail, local.name);
    contains(detail, local.type);
  }
  CHECK(matches(detail, "element"));
  const auto element = detail.find("element");
  const auto storage = detail.find("storage");
  CHECK((matches(detail, "before|after|then|post.?order") ||
         (element != std::string::npos && storage != std::string::npos &&
          element < storage)));
  // Distinguish Copy-element omission from recursively cleaned Own contents.
  CHECK_FALSE(matches(detail, "(destroy|free)[ \\t]+Copy[ \\t]+elements?"));
  CHECK(matches(detail, "(own&int|owned|Own)[^\\n]*(element|recursive)|"
                        "(element|recursive)[^\\n]*(own&int|owned|Own)"));
  contains(run.output, "main : 3 destroys");
}

TEST_CASE("SOP D3 temporary cleanup points and return ordering",
          "[SOP][D3][cleanup-plan]") {
  const auto &example = namedCase("named and unnamed cleanup");
  Invocation run(example.source, {"--pownership", "--constraint"});
  checkSuccess(run);
  const auto detail = cleanupDetail(run.output);
  contains(detail, "mk()");
  contains(detail, "s[1:3]");
  contains(detail, "Seq(int)");
  CHECK(matches(detail, "[0-9]+:[0-9]+"));
  CHECK(matches(
      detail, "after[^\\n]*(length|#)|(length|#)[^\\n]*before[^\\n]*cleanup"));
  CHECK(matches(detail, "return[^\\n]*(comput|evaluat)|"
                        "(comput|evaluat)[^\\n]*return"));
  contains(run.output, "main : 1 destroy");
  contains(run.output, "mk : 0 destroys");
}

TEST_CASE("SOP D3 transferred temporary has one cleanup owner",
          "[SOP][D3][cleanup-plan]") {
  for (const auto &name : {"temporary transferred to binder",
                           "temporary transferred into sequence"}) {
    DYNAMIC_SECTION(name) {
      const auto &example = namedCase(name);
      Invocation run(example.source, {"--pownership", "--constraint"});
      checkSuccess(run);
      const auto detail = cleanupDetail(run.output);
      contains(detail, example.locals.front().name);
      contains(detail, example.locals.front().type);
      // A plan may display transfer records, but must distinguish them from
      // independent cleanup of an untransferred temporary.
      CHECK(matches(detail, "transfer|recipient|enclos|element"));
      CHECK_FALSE(matches(detail,
                          "(cleanup|destroy)[^\\n]*(untransferred|discarded)"
                          "[^\\n]*mk\\(\\)"));
      contains(run.output, "main : 1 destroy");
      contains(run.output, "mk : 0 destroys");
    }
  }
}
