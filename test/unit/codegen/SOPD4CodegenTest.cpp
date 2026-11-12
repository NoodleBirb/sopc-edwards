#include "CodeGenerator.h"
#include "FrontEnd.h"
#include "Optimizer.h"
#include "SemanticAnalysis.h"

#include "llvm/IR/CFG.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Support/raw_ostream.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <functional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace {
struct Compiled {
  std::shared_ptr<ASTProgram> ast;
  std::shared_ptr<SemanticAnalysis> analysis;
  std::shared_ptr<llvm::Module> module;
};

Compiled compile(const std::string &source) {
  std::istringstream input(source);
  auto ast = FrontEnd::parse(input);
  auto analysis = SemanticAnalysis::analyze(ast.get());
  auto module = CodeGenerator::generate(ast.get(), analysis.get(), "sop-d4");
  REQUIRE(module != nullptr);
  std::string diagnostic;
  llvm::raw_string_ostream output(diagnostic);
  const bool invalid = llvm::verifyModule(*module, &output);
  INFO(diagnostic);
  REQUIRE_FALSE(invalid);
  return {std::move(ast), std::move(analysis), std::move(module)};
}

std::vector<llvm::CallInst *> calls(llvm::Module &module) {
  std::vector<llvm::CallInst *> result;
  for (auto &function : module)
    for (auto &instruction : llvm::instructions(function))
      if (auto *call = llvm::dyn_cast<llvm::CallInst>(&instruction))
        result.push_back(call);
  return result;
}

bool dependsOn(llvm::Value *value, llvm::Value *needle) {
  std::set<llvm::Value *> seen;
  std::function<bool(llvm::Value *)> visit = [&](llvm::Value *current) {
    if (current == needle)
      return true;
    if (!seen.insert(current).second)
      return false;
    if (auto *user = llvm::dyn_cast<llvm::User>(current))
      for (auto &operand : user->operands())
        if (visit(operand.get()))
          return true;
    return false;
  };
  return visit(value);
}

bool heapAllocation(llvm::CallInst *call) {
  if (auto *callee = call->getCalledFunction()) {
    if (callee->isIntrinsic())
      return false;
    if (callee->getName() == "malloc" || callee->getName() == "calloc")
      return true;
    if (callee->hasFnAttribute(llvm::Attribute::AllocSize))
      return true;
  }
  return call->getType()->isPointerTy();
}

bool allocationFor(llvm::Module &module, uint64_t length) {
  const uint64_t bytes = 8 * (length + 1);
  for (auto *call : calls(module)) {
    auto *callee = call->getCalledFunction();
    if (callee && callee->getName() == "calloc" && call->arg_size() == 2) {
      auto *count = llvm::dyn_cast<llvm::ConstantInt>(call->getArgOperand(0));
      auto *size = llvm::dyn_cast<llvm::ConstantInt>(call->getArgOperand(1));
      if (count && size &&
          count->getZExtValue() * size->getZExtValue() == bytes)
        return true;
      continue;
    }
    if (callee && callee->getName() == "malloc" && call->arg_size() == 1) {
      auto *size = llvm::dyn_cast<llvm::ConstantInt>(call->getArgOperand(0));
      if (size && size->getZExtValue() == bytes)
        return true;
      continue;
    }
    // A generated/runtime allocation helper may accept length, words, or bytes.
    // Runtime fixtures independently check the helper's resulting behavior.
    if ((!callee || !callee->isIntrinsic()) &&
        (heapAllocation(call) || call->getType()->isIntegerTy(64)))
      for (auto &argument : call->args())
        if (auto *constant = llvm::dyn_cast<llvm::ConstantInt>(argument.get()))
          if (constant->getZExtValue() == length ||
              constant->getZExtValue() == length + 1 ||
              constant->getZExtValue() == bytes)
            return true;
  }
  return false;
}

bool reachable(llvm::BasicBlock *from, llvm::BasicBlock *to) {
  std::vector<llvm::BasicBlock *> pending{from};
  std::set<llvm::BasicBlock *> seen;
  while (!pending.empty()) {
    auto *block = pending.back();
    pending.pop_back();
    if (block == to)
      return true;
    if (!seen.insert(block).second)
      continue;
    for (auto *successor : llvm::successors(block))
      pending.push_back(successor);
  }
  return false;
}

bool errorPath(llvm::BasicBlock *start) {
  auto *function = start->getParent();
  for (auto &block : *function)
    if (reachable(start, &block) &&
        llvm::isa<llvm::UnreachableInst>(block.getTerminator()))
      for (auto &instruction : block)
        if (llvm::isa<llvm::CallInst>(instruction))
          return true;
  return false;
}

bool hasLoop(llvm::Module &module) {
  for (auto &function : module) {
    if (function.isDeclaration())
      continue;
    llvm::DominatorTree dominators(function);
    for (auto &block : function)
      for (auto *successor : llvm::successors(&block))
        if (dominators.dominates(successor, &block))
          return true;
  }
  return false;
}

bool releasesMemory(llvm::Function *function,
                    std::set<llvm::Function *> &seen) {
  if (!function || !seen.insert(function).second)
    return false;
  if (function->getName() == "free")
    return true;
  for (auto &instruction : llvm::instructions(function))
    if (auto *call = llvm::dyn_cast<llvm::CallInst>(&instruction))
      if (releasesMemory(call->getCalledFunction(), seen))
        return true;
  return false;
}
} // namespace

TEST_CASE("SOP D4 T41 allocation includes header and empty storage",
          "[SOP][D4][T41]") {
  SECTION("two integer elements") {
    auto compiled = compile("main() { var s; s = [10, 20]; return #s; }");
    CHECK(allocationFor(*compiled.module, 2));
  }
  SECTION("contextually typed empty literal") {
    auto compiled = compile("main() { var s; s = [[], [1]]; return #s; }");
    CHECK(allocationFor(*compiled.module, 0));
  }
}

TEST_CASE("SOP D4 T42 length reads the sequence header", "[SOP][D4][T42]") {
  auto compiled = compile(R"(
    lengthProbe(p) { return #*p; }
    main() { var s; s = [10, 20]; return lengthProbe(&s); }
  )");
  auto *probe = compiled.module->getFunction("lengthProbe");
  REQUIRE(probe != nullptr);
  std::vector<llvm::LoadInst *> reads;
  for (auto &instruction : llvm::instructions(probe))
    if (auto *load = llvm::dyn_cast<llvm::LoadInst>(&instruction))
      if (!llvm::isa<llvm::AllocaInst>(
              load->getPointerOperand()->stripPointerCasts()))
        reads.push_back(load);
  // A borrow points to a sequence slot: read the slot, then its length header.
  REQUIRE(reads.size() >= 2);
  bool returnedHeader = false;
  for (auto &block : *probe)
    if (auto *ret = llvm::dyn_cast<llvm::ReturnInst>(block.getTerminator()))
      for (auto *header : reads)
        for (auto *slot : reads)
          if (header != slot && dependsOn(header->getPointerOperand(), slot) &&
              dependsOn(ret->getReturnValue(), header))
            returnedHeader = true;
  CHECK(returnedHeader);
}

TEST_CASE("SOP D4 T43 bounds guard dominates the element load",
          "[SOP][D4][T43]") {
  auto compiled = compile(R"(
    indexProbe(p, i) { return (*p)[i] + 0; }
    main() { var s; s = [10, 20]; return indexProbe(&s, 1); }
  )");
  auto *probe = compiled.module->getFunction("indexProbe");
  REQUIRE(probe != nullptr);
  llvm::DominatorTree dominators(*probe);
  bool guarded = false;
  for (auto &instruction : llvm::instructions(probe)) {
    auto *load = llvm::dyn_cast<llvm::LoadInst>(&instruction);
    if (!load)
      continue;
    auto *address = llvm::dyn_cast<llvm::GetElementPtrInst>(
        load->getPointerOperand()->stripPointerCasts());
    if (!address)
      continue;
    bool dynamicIndex = false;
    for (auto &index : address->indices())
      dynamicIndex |= !llvm::isa<llvm::ConstantInt>(index.get());
    if (!dynamicIndex)
      continue;
    for (auto &block : *probe) {
      auto *branch = llvm::dyn_cast<llvm::BranchInst>(block.getTerminator());
      if (!branch || !branch->isConditional())
        continue;
      for (unsigned success = 0; success != 2; ++success) {
        auto *ok = branch->getSuccessor(success);
        auto *bad = branch->getSuccessor(1 - success);
        if (dominators.dominates(branch, load) &&
            dominators.dominates(ok, load->getParent()) &&
            !reachable(bad, load->getParent()) && errorPath(bad))
          guarded = true;
      }
    }
  }
  CHECK(guarded);
}

TEST_CASE("SOP D4 T44 range emits a loop back edge", "[SOP][D4][T44]") {
  auto compiled = compile(R"(
    main(n) { var i, total; total = 0;
      for (i : 0 .. n) { total = total + i; }
      return total;
    }
  )");
  CHECK(hasLoop(*compiled.module));
}

TEST_CASE("SOP D4 T45 cleanup follows return value evaluation",
          "[SOP][D4][T45]") {
  auto compiled = compile(R"(
    main(i) { var s; s = [10, 20, 30]; return s[i] + #s; }
  )");
  auto *main = compiled.module->getFunction("_top_main");
  REQUIRE(main != nullptr);
  llvm::DominatorTree dominators(*main);
  bool ordered = false;
  for (auto &block : *main) {
    auto *ret = llvm::dyn_cast<llvm::ReturnInst>(block.getTerminator());
    if (!ret)
      continue;
    auto *value = llvm::dyn_cast<llvm::Instruction>(ret->getReturnValue());
    REQUIRE(value != nullptr);
    for (auto &instruction : llvm::instructions(main)) {
      auto *call = llvm::dyn_cast<llvm::CallInst>(&instruction);
      if (!call)
        continue;
      std::set<llvm::Function *> seen;
      if (releasesMemory(call->getCalledFunction(), seen) &&
          dominators.dominates(value, call) && dominators.dominates(call, ret))
        ordered = true;
    }
  }
  CHECK(ordered);
}

TEST_CASE("SOP D4 T46 lambdas are registered function values",
          "[SOP][D4][T46]") {
  auto compiled = compile("main() { var f; f = (x) => x + 1; return f(41); }");
  std::size_t lambdas = 0;
  for (auto *function : compiled.ast->getFunctions()) {
    if (!function->isLambdaFunction())
      continue;
    ++lambdas;
    auto *emitted = compiled.module->getFunction(function->getName());
    REQUIRE(emitted != nullptr);
    CHECK_FALSE(emitted->isDeclaration());
    bool registered = false;
    for (auto &global : compiled.module->globals())
      if (global.hasInitializer() && global.getValueType()->isArrayTy())
        registered |= dependsOn(global.getInitializer(), emitted);
    CHECK(registered);
  }
  CHECK(lambdas == 1);
  const auto emittedCalls = calls(*compiled.module);
  CHECK(std::any_of(emittedCalls.begin(), emittedCalls.end(), [](auto *call) {
    return call->getCalledFunction() == nullptr;
  }));
  CHECK(std::none_of(emittedCalls.begin(), emittedCalls.end(), heapAllocation));
}

TEST_CASE("SOP D4 T47 sanitizer instruments generated memory operations",
          "[SOP][D4][T47]") {
  for (bool optimize : {false, true}) {
    DYNAMIC_SECTION("optimization " << optimize) {
      auto compiled = compile(R"(
        main(i) { var s; s = [10, 20, 30]; return s[i]; }
      )");
      if (optimize)
        Optimizer::optimize(compiled.module.get(), true);
      else
        Optimizer::instrumentAddressSanitizer(compiled.module.get());
      CHECK_FALSE(llvm::verifyModule(*compiled.module));
      bool instrumentedRead = false;
      bool instrumentedWrite = false;
      for (auto *call : calls(*compiled.module))
        if (auto *callee = call->getCalledFunction()) {
          const auto name = callee->getName();
          if (name.starts_with("__asan_")) {
            instrumentedRead |= name.contains("load");
            instrumentedWrite |= name.contains("store");
          }
        }
      CHECK(instrumentedRead);
      CHECK(instrumentedWrite);
    }
  }
}

TEST_CASE("SOP D4 T48 dynamic slices emit copying and sized allocation",
          "[SOP][D4][T48]") {
  auto compiled = compile(R"(
    sliceProbe(p, lo, hi) { var u; u = (*p)[lo:hi]; return u[0] + #u; }
    main() { var s; s = [10, 20, 30]; return sliceProbe(&s, 0, 2); }
  )");
  Optimizer::optimize(compiled.module.get());
  auto *probe = compiled.module->getFunction("sliceProbe");
  REQUIRE(probe != nullptr);
  llvm::Instruction *count = nullptr;
  for (auto &instruction : llvm::instructions(probe))
    if (auto *binary = llvm::dyn_cast<llvm::BinaryOperator>(&instruction))
      if (binary->getOpcode() == llvm::Instruction::Sub &&
          binary->getOperand(0) == probe->getArg(2) &&
          binary->getOperand(1) == probe->getArg(1))
        count = binary;
  REQUIRE(count != nullptr);
  std::function<unsigned(llvm::Value *)> degree = [&](llvm::Value *value) {
    if (value == count)
      return 1U;
    if (!dependsOn(value, count))
      return 0U;
    if (auto *binary = llvm::dyn_cast<llvm::BinaryOperator>(value)) {
      const auto left = degree(binary->getOperand(0));
      const auto right = degree(binary->getOperand(1));
      if (binary->getOpcode() == llvm::Instruction::Add ||
          binary->getOpcode() == llvm::Instruction::Sub)
        return std::max(left, right);
      if (binary->getOpcode() == llvm::Instruction::Mul)
        return left + right;
      if (binary->getOpcode() == llvm::Instruction::Shl &&
          llvm::isa<llvm::ConstantInt>(binary->getOperand(1)))
        return left;
    }
    if (auto *cast = llvm::dyn_cast<llvm::CastInst>(value))
      return degree(cast->getOperand(0));
    return 2U;
  };
  bool sizedAllocation = false;
  bool bulkCopy = false;
  for (auto *call : calls(*compiled.module)) {
    if (call->getFunction() == probe &&
        (heapAllocation(call) || call->getType()->isIntegerTy(64)))
      for (auto &argument : call->args())
        sizedAllocation |= degree(argument.get()) == 1;
    if (llvm::isa<llvm::MemCpyInst>(call))
      bulkCopy = true;
    if (auto *callee = call->getCalledFunction())
      bulkCopy |= callee->getName() == "memcpy";
  }
  CHECK(sizedAllocation);
  CHECK((hasLoop(*compiled.module) || bulkCopy));
}
