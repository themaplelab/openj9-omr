#include "IDT.hpp"

#include "compiler/env/j9method.h"
#include "compiler/optimizer/AbsOpStack.hpp"
#include "compiler/optimizer/AbsVarArrayStatic.hpp"
#include "compiler/optimizer/AbsEnvStatic.hpp"
#include "compiler/infra/ILWalk.hpp"
#include "compiler/infra/OMRCfg.hpp"
#include "compiler/il/OMRBlock.hpp"
#include "il/Block.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"

#define SINGLE_CHILD_BIT 1


IDT::Node::Node(IDT* idt, int idx, int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, IDT::Node *parent):
  _idx(idx),
  _benefit(1), // must be positive
  _callsite_bci(callsite_bci),
  _children(nullptr),
  _rms(rms),
  _parent(parent),
  _head(idt)
  {}

IDT::Node::Node(IDT* idt, int idx, int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, IDT::Node *parent, unsigned int benefit):
  _idx(idx),
  _benefit(benefit),
  _callsite_bci(callsite_bci),
  _children(nullptr),
  _rms(rms),
  _parent(parent),
  _head(idt)
  {}

IDT::IDT(TR_InlinerBase* inliner, TR::Region &mem, TR::ResolvedMethodSymbol* rms):
  _mem(mem),
  _vp(nullptr),
  _inliner(inliner),
  _max_idx(-1),
  _root(new (_mem) IDT::Node(this, nextIdx(), -1, rms, nullptr, 1)),
  _current(_root)
  {
  }

void
IDT::printTrace() const
  {
  if (comp()->trace(OMR::benefitInliner)) {
    const int candidates = howManyNodes() - 1;
    TR_VerboseLog::writeLineLocked(TR_Vlog_SIP, "#IDT: %d candidate methods to inline into %s",
      candidates,
      getRoot()->getName(this)
    );
    if (candidates <= 0) {
      return;
    }
    getRoot()->printNodeThenChildren(this, -2);
  }
  }

void
IDT::Node::printNodeThenChildren(const IDT* idt, int callerIndex) const
  {
  if (this != idt->getRoot()) {
    const char *nodeName = getName(idt);
    TR_VerboseLog::writeLineLocked(TR_Vlog_SIP, "#IDT: %d: %d inlinable @%d -> bcsz=%d %s, benefit = %u", 
      _idx,
      callerIndex,
      _callsite_bci,
      getBcSz(),
      nodeName,
      this->getBenefit()
    );
  }
  
  if (_children == nullptr) {
    return;
  }

  // Print children
  IDT::Node* child = getOnlyChild();

  if (child != nullptr)
    {
    child->printNodeThenChildren(idt, _idx);
    } 
  else
    {
    for (auto curr = _children->begin(); curr != _children->end(); ++curr)
      {
      curr->printNodeThenChildren(idt, _idx);
      }
    }
  }

uint32_t
IDT::Node::getBcSz() const 
  {
  return _rms->getResolvedMethod()->maxBytecodeIndex();
  }

IDT::Node*
IDT::Node::getOnlyChild() const
  {
  if (((uintptr_t)_children) & SINGLE_CHILD_BIT)
    {
    return (IDT::Node *)((uintptr_t)(_children) & ~SINGLE_CHILD_BIT);
    }
  return nullptr;
  }

void
IDT::Node::setOnlyChild(IDT::Node* child)
  {
  TR_ASSERT_FATAL(!((uintptr_t)child & SINGLE_CHILD_BIT), "Maligned memory address.\n");
  _children = (IDT::Node::Children*)((uintptr_t)child | SINGLE_CHILD_BIT);
  }

IDT::Node*
IDT::getRoot() const
  {
  return _root;
  }


void
IDT::buildIndices() 
  {
    _indices = new (_mem) IDT::Indices(howManyNodes(), nullptr, _mem);
    getRoot()->buildIndices(*_indices);
  }

void
IDT::Node::buildIndices(IDT::Indices &indices)
  {
    TR_ASSERT(indices[getCalleeIndex()] == nullptr, "callee index not unique");
    indices[getCalleeIndex()] = this;
    for (auto curr = _children->begin(); curr != _children->end(); ++curr)
      {
      curr->buildIndices(indices);
      }
  }

int
IDT::nextIdx()
  {
  return _max_idx++;
  }


int
IDT::Node::getCalleeIndex() const
  {
    return _idx;
  }

unsigned int
IDT::Node::getCost() const
  {
    return this->getBcSz();
  }

unsigned int
IDT::Node::getBenefit() const
  {
    return this->_benefit;
  }

void
IDT::Node::setBenefit(unsigned int benefit)
  {
  this->_benefit = benefit;
  }

IDT::Node*
IDT::Node::addChildIfNotExists(IDT* idt,
                         int32_t callsite_bci,
                         TR::ResolvedMethodSymbol* rms,
                         int benefit)
  {
  // 0 Children
  if (_children == nullptr)
    {
    IDT::Node* created = new (idt->_mem) IDT::Node(idt, idt->nextIdx(), callsite_bci, rms, this, benefit);
    setOnlyChild(created);
    return created;
    }
  // 1 Child
  IDT::Node* onlyChild = getOnlyChild();

  if (onlyChild && onlyChild->nodeSimilar(callsite_bci, rms)) return nullptr;

  if (onlyChild)
    {
    _children = new (idt->_mem) IDT::Node::Children(idt->_mem);
    TR_ASSERT_FATAL(!((uintptr_t)_children & SINGLE_CHILD_BIT), "Maligned memory address.\n");
    _children->push_back(*onlyChild);
    // Fall through to two child case.
    }

  // 2+ children
  //IDT::Node::Children* children = _children;

  for (auto curr = _children->begin(); curr != _children->end(); ++curr)
    {
    if (curr->nodeSimilar(callsite_bci, rms))
      {
      return nullptr;
      }
    }

  _children->push_back(IDT::Node(idt, idt->nextIdx(), callsite_bci, rms, this, benefit));
  return &_children->back();
  }

bool
IDT::Node::nodeSimilar(int32_t callsite_bci, TR::ResolvedMethodSymbol* rms) const
  {
  auto a = _rms->getResolvedMethod()->maxBytecodeIndex();
  auto b = rms->getResolvedMethod()->maxBytecodeIndex();

  return a == b && _callsite_bci == callsite_bci;
  }

TR::Compilation*
IDT::comp() const
  {
  return _inliner->comp();
  }

TR::Compilation*
IDT::Node::comp() const
  {
  return this->_head->comp();
  }

unsigned int
IDT::howManyNodes() const
  {
    return getRoot()->howManyDescendantsIncludingMe();
  }

unsigned int
IDT::Node::getNumChildren() const
   {
   if (_children == nullptr) return 0;
   if (getOnlyChild() != nullptr) return 1;
   return _children->size();
   }

unsigned int
IDT::Node::howManyDescendantsIncludingMe() const
  {
   return 1 + this->howManyDescendants();
    if (_children == nullptr) {
      return 1;
    }
    Node *child = this->getOnlyChild();
    if (getOnlyChild() != nullptr) {
      return 1 + child->howManyDescendantsIncludingMe();
    }
    int sum = 1;
    for (auto child = _children->begin(); child != _children->end(); ++child) {
      sum += child->howManyDescendantsIncludingMe();
    }
    return sum;
  }

unsigned int
IDT::Node::howManyDescendants() const
  {
  if (!_children) return 0;
  IDT::Node *onlyChild = this->getOnlyChild();
  if (onlyChild) return 1 + onlyChild->howManyDescendants();

  int sum = 0;
  for (auto child = _children->begin(); child != _children->end(); ++child)
     {
     sum += 1 + child->howManyDescendants();
     }
  return sum;
  }

const char *
IDT::Node::getName(const IDT *idt) const 
  {
  return _rms->getResolvedMethod()->signature(idt->comp()->trMemory());
  }

IDT::Node *
IDT::Node::getParent() const
  {
    return _parent;
  }

bool IDT::addToCurrentChild(int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, float callRatio)
  {
    IDT::Node *node = _current->addChildIfNotExists(this, callsite_bci, rms, (int)(100 * callRatio));
    if (node == nullptr) {
      return false;
    }
    _current = node;
    _indices = nullptr;
    return true;
  }

void IDT::popCurrent()
  {
    _current = _current->getParent();
    TR_ASSERT_FATAL(_current != nullptr, "Invalid IDT pop");
  }

IDT::Node *IDT::getNodeByCalleeIndex(int calleeIndex)
  {
    if (!_indices) return NULL;
    return (*_indices)[calleeIndex];
  }

void
IDT::Node::enqueue_subordinates(IDT::NodePtrPriorityQueue *q) const
   {
      int count = this->getNumChildren();
      if (count == 1) {
         IDT::Node* child = this->getOnlyChild();
         q->push(child);
         return;
      }
      for (int i = 0; i < count; i++)
         {
            IDT::Node *child = &(_children->at(i));
            q->push(child);
         }
   }

int
IDT::Node::getCallerIndex() const
   {
      if (this->isRoot()) return -2;
      const IDT::Node* const parent = this->getParent();
      return parent->getCalleeIndex();
   }

bool
IDT::Node::isRoot() const
  {
  return this->getParent() == nullptr;
  }

TR::ResolvedMethodSymbol*
IDT::Node::getResolvedMethodSymbol() const
  {
      return this->_rms;
  }

int
IDT::Node::numberOfParameters()
  {
      TR_ASSERT_FATAL(false, "unimplemented");
      return 0;
  }

bool
IDT::Node::isSameMethod(TR::ResolvedMethodSymbol *rms) const
  {
    TR::ResolvedMethodSymbol *anRMS = this->getResolvedMethodSymbol();
    return anRMS->getResolvedMethod()->isSameMethod(rms->getResolvedMethod());
  }

bool
IDT::Node::isSameMethod(IDT::Node* aNode) const
  {
     return this->isSameMethod(aNode->getResolvedMethodSymbol());
  }

IDT::Node*
IDT::Node::findChildWithBytecodeIndex(int bcIndex)
  {
     TR_ASSERT_FATAL(false, "unimplemented");
     return NULL;
  }

const char *
IDT::Node::getName() const 
  {
  //slow implementation TR::comp() is expensive but here we are in a context where we don't have access to IDT
  return _rms->getResolvedMethod()->signature(TR::comp()->trMemory());
  }

UDATA
IDT::Node::maxStack() const
  {
   TR_ResolvedJ9Method *method = (TR_ResolvedJ9Method *)this->getResolvedMethodSymbol()->getResolvedMethod();
  J9ROMMethod *romMethod = method->romMethod();
  return J9_MAX_STACK_FROM_ROM_METHOD(romMethod);
  }

IDATA
IDT::Node::maxLocals() const
  {
   TR_ResolvedJ9Method *method = (TR_ResolvedJ9Method *)this->getResolvedMethodSymbol()->getResolvedMethod();
  J9ROMMethod *romMethod = method->romMethod();
  return J9_ARG_COUNT_FROM_ROM_METHOD(romMethod) + J9_TEMP_COUNT_FROM_ROM_METHOD(romMethod);
  }

TR::Region&
IDT::Node::getAbsOpStackMemoryRegion() const
  {
  return this->_head->getMemoryRegion();
  }

TR::Region&
IDT::Node::getAbsVarArrayMemoryRegion() const
  {
  return this->_head->getMemoryRegion();
  }

TR::Region&
IDT::Node::getAbsEnvMemoryRegion() const
  {
  return this->_head->getMemoryRegion();
  }

TR::ValuePropagation*
IDT::Node::getValuePropagation()
  {
  return this->_head->getValuePropagation();
  }

TR::ValuePropagation*
IDT::getValuePropagation()
  {
  if (this->_vp != nullptr) return this->_vp;

  TR::OptimizationManager* manager = this->_inliner->comp()->getOptimizer()->getOptimization(OMR::globalValuePropagation);
  this->_vp = (TR::ValuePropagation*) manager->factory()(manager);
  this->_vp->initialize();
  return this->_vp;
  }

TR::Region&
IDT::getMemoryRegion() const
  {
  return this->_mem;
  }

AbsOpStackStatic*
IDT::Node::createAbsOpStack()
  {
  TR::Region &region = this->getAbsOpStackMemoryRegion();
  const UDATA maxStack = this->maxStack();
  return new (region) AbsOpStackStatic(region, maxStack);
  }

AbsVarArrayStatic*
IDT::Node::createAbsVarArray()
  {
  TR::Region &region = this->getAbsVarArrayMemoryRegion();
  const IDATA maxLocals = this->maxLocals();
  return new (region) AbsVarArrayStatic(region, maxLocals);
  }

AbsEnvStatic*
IDT::Node::createAbsEnv()
  {
  TR::Region &region = this->getAbsEnvMemoryRegion();
  const UDATA maxStack = this->maxStack();
  const IDATA maxLocals = this->maxLocals();
  TR::ResolvedMethodSymbol *rms = this->getResolvedMethodSymbol();
  return new (region) AbsEnvStatic(region, maxStack, maxLocals, this->getValuePropagation(), rms);
  }


AbsEnvStatic*
IDT::Node::enterMethod()
  {
  AbsEnvStatic *absEnv = this->createAbsEnv();
  absEnv->enterMethod(this->getResolvedMethodSymbol());
  if (comp()->trace(OMR::benefitInliner))
    absEnv->trace(this->getName());
  return absEnv;
  }


void
IDT::Node::getMethodSummary()
  {
  TR::ResolvedMethodSymbol *resolvedMethodSymbol = this->getResolvedMethodSymbol();
  TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
  TR_ResolvedJ9Method *resolvedJ9Method = static_cast<TR_ResolvedJ9Method*>(resolvedMethod);
  TR_J9VMBase *vm = static_cast<TR_J9VMBase*>(this->comp()->fe());
  TR_J9ByteCodeIterator bci(resolvedMethodSymbol, resolvedJ9Method, vm, this->comp());

  TR::CFG* cfg = resolvedMethodSymbol->getFlowGraph();
  TR::CFGNode *cfgNode = cfg->getStartForReverseSnapshot();
  TR::Block *startBlock = cfgNode->asBlock();
  TR::Compilation *comp = this->comp();
  AbsEnvStatic *absEnv = this->enterMethod();


  for (TR::ReversePostorderSnapshotBlockIterator blockIt (startBlock, comp); blockIt.currentBlock(); ++blockIt)
     {
        OMR::Block *block = blockIt.currentBlock();
        analyzeBasicBlock(block, absEnv, bci);
     }

  }

AbsEnvStatic*
IDT::Node::analyzeBasicBlock(OMR::Block *block, AbsEnvStatic* absEnv, TR_J9ByteCodeIterator &bci)
  {
  int start = block->getBlockBCIndex();
  int end = start + block->getBlockSize() - 1;
  bci.setIndex(start);
  if (this->comp()->trace(OMR::benefitInliner))
     {
     traceMsg(this->comp(), "basic block start = %d end = %d\n", start, end);
     }
  for (TR_J9ByteCode bc = bci.current(); bc != J9BCunknown && bci.currentByteCodeIndex() < end; bc = bci.next())
     {
     absEnv->interpret(bc, bci);
     }
  }

AbsEnvStatic*
IDT::Node::analyzeBasicBlock(OMR::Block *block, AbsEnvStatic* absEnv, unsigned int start, unsigned int end)
  {
  return NULL;
  }
