#include "ASTForStmt.h"
#include "ASTIndexExpr.h"
#include "ASTLambdaExpr.h"
#include "ASTLengthExpr.h"
#include "ASTRange.h"
#include "ASTSequenceExpr.h"
#include "ASTSliceExpr.h"
#include "CodeGenVisitor.h"
#include "InternalError.h"
#include "OwnershipClassifier.h"
#include "SemanticAnalysis.h"
#include "TopBorrowRef.h"
#include "TopFunction.h"
#include "TopMu.h"
#include "TopSequence.h"
#include "TopVar.h"
#include "llvm/IR/Intrinsics.h"

// D3 determines the owner and lifetime; code generation only materializes the
// recorded cleanup. In particular, borrowing a temporary keeps it alive until
// the receiving call/loop has finished, rather than freeing its indexed slot.
void CodeGenVisitor::emitTemporaryCleanup(ASTNode *point, llvm::Value *result) {
  if (!semanticAnalysis_ || !semanticAnalysis_->getDestructionPlan())
    return;
  const auto &actions = semanticAnalysis_->getDestructionPlan()->getActions();
  for (const auto &action : actions) {
    if (action->kind != CleanupAction::Kind::Destroy || !action->temporary ||
        !dynamic_cast<TopSequence *>(action->type.get()))
      continue;
    if (action->temporary == point)
      temporarySequences_[action->temporary] = result;
    if (action->point == point) {
      auto found = temporarySequences_.find(action->temporary);
      if (found != temporarySequences_.end()) {
        emitDestroyValue(found->second, action->type.get(), *ctx_);
        temporarySequences_.erase(found);
      }
    }
  }
}

void CodeGenVisitor::emitGuard(llvm::Value *valid, int category, int line) {
  auto &ctx = *ctx_;
  auto &b = ctx.irBuilder;
  auto *fn = b.GetInsertBlock()->getParent();
  auto *ok = llvm::BasicBlock::Create(ctx.llvmContext, "sop.ok", fn);
  auto *error = llvm::BasicBlock::Create(ctx.llvmContext, "sop.error", fn);
  b.CreateCondBr(valid, ok, error);
  b.SetInsertPoint(error);
  auto runtime = ctx.module->getOrInsertFunction(
      "_top_sop_error",
      llvm::FunctionType::get(b.getVoidTy(), {b.getInt64Ty(), b.getInt64Ty()},
                              false));
  if (auto *decl = llvm::dyn_cast<llvm::Function>(runtime.getCallee()))
    decl->addFnAttr(llvm::Attribute::NoReturn);
  b.CreateCall(runtime, {b.getInt64(category), b.getInt64(line)});
  b.CreateUnreachable();
  b.SetInsertPoint(ok);
}

llvm::Value *CodeGenVisitor::allocateSequence(llvm::Value *length) {
  auto &b = ctx_->irBuilder;
  auto *words = b.CreateAdd(length, b.getInt64(1), "sequence.words");
  auto *storage =
      b.CreateCall(ctx_->callocFun, {words, b.getInt64(8)}, "sequence.storage");
  b.CreateStore(length, storage);
  return storage;
}

llvm::Value *CodeGenVisitor::generate(ASTSequenceExpr *node) {
  auto &b = ctx_->irBuilder;
  // Complete each element before starting the next one. Allocation itself has
  // no source-language side effect; every evaluation gets fresh storage.
  std::vector<llvm::Value *> elements;
  for (auto *element : node->getElements())
    elements.push_back(dispatch(element));
  auto *storage = allocateSequence(b.getInt64(elements.size()));
  for (std::size_t i = 0; i < elements.size(); ++i) {
    auto *slot =
        b.CreateInBoundsGEP(b.getInt64Ty(), storage, b.getInt64(i + 1));
    b.CreateStore(elements[i], slot);
  }
  return b.CreatePtrToInt(storage, b.getInt64Ty(), "sequence.value");
}

llvm::Value *CodeGenVisitor::generate(ASTLengthExpr *node) {
  auto &b = ctx_->irBuilder;
  auto *source = dispatch(node->getSequence());
  auto *storage = b.CreateIntToPtr(source, b.getPtrTy(), "sequence.header");
  return b.CreateLoad(b.getInt64Ty(), storage, "sequence.length");
}

llvm::Value *CodeGenVisitor::generate(ASTIndexExpr *node) {
  auto &ctx = *ctx_;
  auto &b = ctx.irBuilder;
  const bool addressRequested = ctx.lValueGen;
  ctx.lValueGen = false;
  auto *source = dispatch(node->getSequence());
  auto *index = dispatch(node->getIndex());
  auto *storage = b.CreateIntToPtr(source, b.getPtrTy(), "sequence.header");
  auto *length = b.CreateLoad(b.getInt64Ty(), storage, "sequence.length");
  auto *valid = b.CreateAnd(b.CreateICmpSGE(index, b.getInt64(0)),
                            b.CreateICmpSLT(index, length), "index.valid");
  emitGuard(valid, 1, node->getLine());
  auto *offset = b.CreateAdd(index, b.getInt64(1), "element.offset");
  auto *slot =
      b.CreateInBoundsGEP(b.getInt64Ty(), storage, offset, "element.slot");
  ctx.lValueGen = addressRequested;
  return addressRequested ? slot
                          : b.CreateLoad(b.getInt64Ty(), slot, "element.value");
}

llvm::Value *CodeGenVisitor::generate(ASTSliceExpr *node) {
  auto &b = ctx_->irBuilder;
  auto *source = dispatch(node->getSequence());
  auto *storage = b.CreateIntToPtr(source, b.getPtrTy(), "slice.source");
  auto *lo =
      node->getLowerBound() ? dispatch(node->getLowerBound()) : b.getInt64(0);
  auto *hi = node->getUpperBound() ? dispatch(node->getUpperBound()) : nullptr;
  auto *length = b.CreateLoad(b.getInt64Ty(), storage, "sequence.length");
  if (!hi)
    hi = length;
  auto *valid = b.CreateAnd(
      b.CreateICmpSGE(lo, b.getInt64(0)),
      b.CreateAnd(b.CreateICmpSLE(lo, hi), b.CreateICmpSLE(hi, length)),
      "slice.valid");
  emitGuard(valid, 1, node->getLine());
  auto *count = b.CreateSub(hi, lo, "slice.length");
  auto *copy = allocateSequence(count);
  auto *from = b.CreateInBoundsGEP(b.getInt64Ty(), storage,
                                   b.CreateAdd(lo, b.getInt64(1)));
  auto *to = b.CreateInBoundsGEP(b.getInt64Ty(), copy, b.getInt64(1));
  b.CreateMemCpy(to, llvm::Align(8), from, llvm::Align(8),
                 b.CreateMul(count, b.getInt64(8), "slice.bytes"));
  return b.CreatePtrToInt(copy, b.getInt64Ty(), "slice.value");
}

llvm::Value *CodeGenVisitor::generate(ASTLambdaExpr *node) {
  if (!node->getFunction())
    throw InternalError("lambda not materialized before code generation");
  auto type = std::dynamic_pointer_cast<TopFunction>(expressionType(node));
  return functionValue(node->getFunction(), type->getParamTypes(),
                       type->getReturnType());
}

llvm::Value *CodeGenVisitor::generate(ASTForStmt *node) {
  auto &ctx = *ctx_;
  auto &b = ctx.irBuilder;
  auto *fn = b.GetInsertBlock()->getParent();
  auto *range = dynamic_cast<ASTRange *>(node->getInput());
  llvm::Value *start;
  llvm::Value *end;
  llvm::Value *step;
  llvm::Value *storage = nullptr;
  if (range) {
    start = dispatch(range->getStart());
    end = dispatch(range->getEnd());
    step = node->getStep() ? dispatch(node->getStep()) : b.getInt64(1);
    emitGuard(b.CreateICmpNE(step, b.getInt64(0)), 2, node->getLine());
  } else {
    // A borrow points at the variable/element slot containing the sequence
    // address, not directly at the sequence header.
    auto *borrow = dispatch(dynamic_cast<ASTExpr *>(node->getInput()));
    auto *slot = b.CreateIntToPtr(borrow, b.getPtrTy(), "iterable.slot");
    auto *value = b.CreateLoad(b.getInt64Ty(), slot, "iterable.value");
    storage = b.CreateIntToPtr(value, b.getPtrTy(), "iterable.storage");
    start = b.getInt64(0);
    end = b.CreateLoad(b.getInt64Ty(), storage, "iterable.length");
    step = b.getInt64(1);
  }
  auto *binder = ctx.namedValues[node->getBinder()->getName()];
  if (!binder) {
    llvm::IRBuilder<> entry(&fn->getEntryBlock(), fn->getEntryBlock().begin());
    binder = entry.CreateAlloca(b.getInt64Ty(), nullptr,
                                node->getBinder()->getName());
    ctx.namedValues[node->getBinder()->getName()] = binder;
  }
  auto *preheader = b.GetInsertBlock();
  auto *header = llvm::BasicBlock::Create(ctx.llvmContext, "for.header", fn);
  auto *body = llvm::BasicBlock::Create(ctx.llvmContext, "for.body", fn);
  auto *exit = llvm::BasicBlock::Create(ctx.llvmContext, "for.exit", fn);
  b.CreateBr(header);
  b.SetInsertPoint(header);
  auto *counter = b.CreatePHI(b.getInt64Ty(), 2, "for.counter");
  counter->addIncoming(start, preheader);
  auto *condition = range ? b.CreateSelect(b.CreateICmpSGT(step, b.getInt64(0)),
                                           b.CreateICmpSLT(counter, end),
                                           b.CreateICmpSGT(counter, end))
                          : b.CreateICmpSLT(counter, end);
  b.CreateCondBr(condition, body, exit);
  b.SetInsertPoint(body);
  llvm::Value *element = counter;
  if (!range) {
    auto *slot = b.CreateInBoundsGEP(b.getInt64Ty(), storage,
                                     b.CreateAdd(counter, b.getInt64(1)),
                                     "iterable.element");
    auto inputType = expressionType(dynamic_cast<ASTExpr *>(node->getInput()));
    auto sequenceType = dynamic_cast<TopSequence *>(
        dynamic_cast<ReferenceType *>(inputType.get())
            ->getReferencedType()
            .get());
    // A Copy element can itself be a borrow. Decide from the element's
    // ownership, rather than from the binder's outer reference constructor.
    element = OwnershipClassifier::classifyType(
                  sequenceType->getElementType().get()) == OwnershipClass::Own
                  ? b.CreatePtrToInt(slot, b.getInt64Ty(), "element.borrow")
                  : b.CreateLoad(b.getInt64Ty(), slot, "element.copy");
  }
  b.CreateStore(element, binder);
  dispatch(node->getBody());
  auto *latch = b.GetInsertBlock();
  if (range) {
    auto *add = llvm::Intrinsic::getOrInsertDeclaration(
        ctx.module.get(), llvm::Intrinsic::sadd_with_overflow,
        {b.getInt64Ty()});
    auto *pair = b.CreateCall(add, {counter, step}, "range.advance");
    auto *next = b.CreateExtractValue(pair, 0, "range.next");
    auto *overflow = b.CreateExtractValue(pair, 1, "range.overflow");
    b.CreateCondBr(overflow, exit, header);
    counter->addIncoming(next, latch);
  } else {
    auto *next = b.CreateAdd(counter, step, "for.next");
    b.CreateBr(header);
    counter->addIncoming(next, latch);
  }
  b.SetInsertPoint(exit);
  return b.CreateCall(ctx.nop);
}

llvm::Function *
CodeGenVisitor::getOrCreateSequenceDestroyFn(TopType *type,
                                             CodeGenContext &ctx) {
  const std::string key = type->toString();
  auto found = ctx.sequenceDestroyFns.find(key);
  if (found != ctx.sequenceDestroyFns.end())
    return found->second;
  auto &b = ctx.irBuilder;
  auto *fn = llvm::Function::Create(
      llvm::FunctionType::get(b.getVoidTy(), {b.getInt64Ty()}, false),
      llvm::Function::InternalLinkage,
      "_top_destroy_sequence." + std::to_string(ctx.sequenceDestroyFns.size()),
      ctx.module.get());
  ctx.sequenceDestroyFns[key] = fn;
  auto saved = b.saveIP();
  ensureFreeDecl(ctx);
  auto *entry = llvm::BasicBlock::Create(ctx.llvmContext, "entry", fn);
  b.SetInsertPoint(entry);
  auto *storage =
      b.CreateIntToPtr(fn->getArg(0), b.getPtrTy(), "sequence.storage");
  auto elementType = dynamic_cast<TopSequence *>(type)->getElementType();
  if (OwnershipClassifier::classifyType(elementType.get()) ==
      OwnershipClass::Own) {
    auto *length = b.CreateLoad(b.getInt64Ty(), storage, "sequence.length");
    auto *header =
        llvm::BasicBlock::Create(ctx.llvmContext, "destroy.header", fn);
    auto *body =
        llvm::BasicBlock::Create(ctx.llvmContext, "destroy.element", fn);
    auto *done =
        llvm::BasicBlock::Create(ctx.llvmContext, "destroy.storage", fn);
    b.CreateBr(header);
    b.SetInsertPoint(header);
    auto *index = b.CreatePHI(b.getInt64Ty(), 2, "destroy.index");
    index->addIncoming(b.getInt64(0), entry);
    b.CreateCondBr(b.CreateICmpSLT(index, length), body, done);
    b.SetInsertPoint(body);
    auto *slot = b.CreateInBoundsGEP(b.getInt64Ty(), storage,
                                     b.CreateAdd(index, b.getInt64(1)));
    emitDestroyValue(b.CreateLoad(b.getInt64Ty(), slot), elementType.get(),
                     ctx);
    auto *next = b.CreateAdd(index, b.getInt64(1));
    index->addIncoming(next, b.GetInsertBlock());
    b.CreateBr(header);
    b.SetInsertPoint(done);
  }
  b.CreateCall(ctx.freeFun, {storage});
  b.CreateRetVoid();
  b.restoreIP(saved);
  return fn;
}

namespace {
using CodegenTypeBindings = std::map<std::string, std::shared_ptr<TopType>>;

std::shared_ptr<TopType>
substituteCodegenType(std::shared_ptr<TopType> type,
                      const CodegenTypeBindings &bindings) {
  if (dynamic_cast<TopVar *>(type.get())) {
    auto found = bindings.find(type->toString());
    return found == bindings.end() ? type : found->second;
  }
  // Recursive types are closed and their bound variables must stay bound.
  if (dynamic_cast<TopMu *>(type.get()))
    return type;
  auto children = type->getChildTypes();
  if (children.empty())
    return type;
  for (auto &child : children)
    child = substituteCodegenType(child, bindings);
  return type->withChildTypes(std::move(children));
}

void matchCodegenType(std::shared_ptr<TopType> pattern,
                      std::shared_ptr<TopType> actual,
                      CodegenTypeBindings &bindings) {
  if (dynamic_cast<TopVar *>(pattern.get())) {
    // An unresolved caller type supplies no representation information yet.
    if (!dynamic_cast<TopVar *>(actual.get()))
      bindings[pattern->toString()] = actual;
    return;
  }
  if (dynamic_cast<TopMu *>(pattern.get()) ||
      pattern->getFunctor() != actual->getFunctor())
    return;
  auto left = pattern->getChildTypes();
  auto right = actual->getChildTypes();
  if (left.size() == right.size())
    for (std::size_t i = 0; i < left.size(); ++i)
      matchCodegenType(left[i], right[i], bindings);
}
} // namespace

std::shared_ptr<TopType>
CodeGenVisitor::substituteType(std::shared_ptr<TopType> type) const {
  return substituteCodegenType(std::move(type), typeBindings_);
}

std::shared_ptr<TopType> CodeGenVisitor::expressionType(ASTExpr *expr) const {
  return substituteType(semanticAnalysis_->getTypeResults()->getInferredType(
      expr, currentFunction_->getDecl()));
}

llvm::Value *CodeGenVisitor::functionValue(
    ASTFunction *source, const std::vector<std::shared_ptr<TopType>> &params,
    std::shared_ptr<TopType> result) {
  auto &ctx = *ctx_;
  if (!specializeIterations_ || source->getName() == "main")
    return ctx.irBuilder.getInt64(ctx.functionIndex.at(source->getName()));
  auto *types = semanticAnalysis_->getTypeResults();
  auto scheme = std::dynamic_pointer_cast<TopFunction>(
      types->getInferredType(source->getDecl()));
  TypeBindings bindings;
  auto formals = scheme->getParamTypes();
  for (std::size_t i = 0; i < formals.size(); ++i)
    matchCodegenType(formals[i], params[i], bindings);
  matchCodegenType(scheme->getReturnType(), result, bindings);
  for (const auto &iteration :
       types->getIterationConstraints(source->getDecl())) {
    auto element = substituteCodegenType(iteration.element, bindings);
    if (!dynamic_cast<TopVar *>(element.get()) &&
        dynamic_cast<TopVar *>(iteration.binder.get())) {
      bindings[iteration.binder->toString()] =
          OwnershipClassifier::classifyType(element.get()) ==
                  OwnershipClass::Own
              ? std::make_shared<TopBorrowRef>(element)
              : element;
    }
  }
  if (bindings.empty())
    return ctx.irBuilder.getInt64(ctx.functionIndex.at(source->getName()));
  std::string key = source->getName();
  for (const auto &[name, type] : bindings)
    key += "|" + name + "=" + type->toString();
  auto found = specializationIndices_.find(key);
  if (found != specializationIndices_.end())
    return ctx.irBuilder.getInt64(found->second);
  const int index = functionEntries_.size();
  specializationIndices_[key] = index;
  auto *target = llvm::Function::Create(
      ctx.module->getFunction(source->getName())->getFunctionType(),
      llvm::Function::InternalLinkage,
      source->getName() + ".sop." + std::to_string(index), ctx.module.get());
  unsigned i = 0;
  for (auto &arg : target->args())
    arg.setName(source->getFormals()[i++]->getName());
  functionEntries_.push_back(target);
  specializations_.push_back({source, target, std::move(bindings)});
  return ctx.irBuilder.getInt64(index);
}
