#include "IDT.hpp"

#include "compiler/env/j9method.h"
#include "compiler/infra/ILWalk.hpp"
#include "compiler/infra/OMRCfg.hpp"
#include "compiler/il/OMRBlock.hpp"
#include "il/Block.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "compiler/optimizer/InliningProposal.hpp"

#define SINGLE_CHILD_BIT 1



IDT::Node::Node(IDT* idt, int idx, int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, IDT::Node *parent, unsigned int benefit, int budget, TR_CallSite *callsite):
  _head(idt),
  _idx(idx),
  _benefit(benefit),
  _callsite_bci(callsite_bci),
  _children(nullptr),
  _rms(rms),
  _parent(parent),
  _budget(budget),
  _callSite(callsite),
  _calltarget(nullptr),
  _callStack(nullptr)
  {}

/*
IDT::Node::Node(const Node& node) :
  _head(node._head),
  _idx(node._idx),
  _benefit(node.getBenefit()),
  _callsite_bci(node._callsite_bci),
  _children(node._children),
  _rms(node._rms),
  _parent(node._parent),
  _budget(node._budget),
  _callSite(node._callSite) // this is bad...
  {}
*/
  

IDT::IDT(TR_InlinerBase* inliner, TR::Region &mem, TR::ResolvedMethodSymbol* rms, int budget):
  _mem(mem),
  _vp(nullptr),
  _inliner(inliner),
  _max_idx(-1),
  _root(new (_mem) IDT::Node(this, nextIdx(), -1, rms, nullptr, 1, budget, nullptr)),
  _current(_root)
  {
  }

bool
IDT::Node::isInProposal(InliningProposal *inliningProposal)
{
  TR_ASSERT_FATAL(inliningProposal, "inliningProposal is null");
  return inliningProposal->inSet(this);
}


int
IDT::Node::budget() const
  {
  return this->_budget;
  }

void
IDT::printTrace() const
  {
  // TODO: fix this flag. We need to print to Verbose when Verbose is set not Trace.
  if (TR::comp()->getOption(TR_TraceBIIDTGen))
    {
    const int candidates = howManyNodes() - 1;
    TR_VerboseLog::writeLineLocked(TR_Vlog_SIP, "#IDT: %d candidate methods to inline into %s with a budget %d",
      candidates,
      getRoot()->getName(this),
      getRoot()->budget()
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
    TR_VerboseLog::writeLineLocked(TR_Vlog_SIP, "#IDT: %d: %d inlinable @%d -> bcsz=%d %s target %s, benefit = %d, budget = %d", 
      _idx,
      callerIndex,
      _callsite_bci,
      getBcSz(),
      this->getCallTarget()->_calleeSymbol ? this->getCallTarget()->_calleeSymbol->signature(comp()->trMemory()) : "no callee symbol???",
      nodeName,
      this->getBenefit(),
      this->budget()
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
      (*curr)->printNodeThenChildren(idt, _idx);
      }
    }
  }

uint32_t
IDT::Node::getBcSz() const 
  {
  return this->getCallTarget()->_calleeMethod->maxBytecodeIndex();
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
    int howManyNodes = this->howManyNodes() + 1;
    _indices = new (_mem) IDT::Node *[howManyNodes];
    memset(_indices, 0, sizeof(IDT::Node*) * (howManyNodes));
    getRoot()->buildIndices(_indices);
  }

void
IDT::Node::buildIndices(IDT::Node **indices)
  {
    TR_ASSERT(indices[getCalleeIndex() + 1] == 0, "callee index not unique");
    indices[getCalleeIndex() + 1] = this;
    if (0 == this->getNumChildren()) return;

    if (1 == this->getNumChildren())
    {
       IDT::Node *onlyChild = this->getOnlyChild();
       onlyChild->buildIndices(indices);
       return;
    }

    // this only works for if you have more than 1 children..
    for (auto curr = _children->begin(); curr != _children->end(); ++curr)
      {
      (*curr)->buildIndices(indices);
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
    return this->isRoot() ? 1 : this->getBcSz();
  }

unsigned int
IDT::Node::getRecursiveCost() const
  {
  int children = this->getNumChildren();
  int cost = this->getCost();
  for (int i = 0; i < children; i++)
     {
     IDT::Node *child = this->getChild(i);
     cost += child->getRecursiveCost();
     }
  return cost;
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
                         unsigned int benefit, TR_CallSite *callsite)
  {
  // 0 Children
  if (_children == nullptr)
    {
    IDT::Node* created = new (idt->_mem) IDT::Node(idt, idt->nextIdx(), callsite_bci, rms, this, benefit, this->budget() - rms->getResolvedMethod()->maxBytecodeIndex(), callsite);
    setOnlyChild(created);
    this->_head->_indices = nullptr;
    return created;
    }
  // 1 Child
  IDT::Node* onlyChild = getOnlyChild();

  if (onlyChild && onlyChild->nodeSimilar(callsite_bci, rms)) return nullptr;

  if (onlyChild)
    {
    _children = new (idt->_mem) IDT::Node::Children(idt->_mem);
    TR_ASSERT_FATAL(!((uintptr_t)_children & SINGLE_CHILD_BIT), "Maligned memory address.\n");
    TR_ASSERT_FATAL(onlyChild, "storing a null child");
    _children->push_back(onlyChild);
    // Fall through to two child case.
    TR_ASSERT(_children->size() == 1, "something wrong");
    }

  // 2+ children
  //IDT::Node::Children* children = _children;

  for (auto curr = _children->begin(); curr != _children->end(); ++curr)
    {
    TR_ASSERT(*curr, "no child can be null");
    if ((*curr)->nodeSimilar(callsite_bci, rms))
      {
      return nullptr;
      }
    }

  IDT::Node *newChild = new (this->_head->getMemoryRegion()) IDT::Node(idt, idt->nextIdx(), callsite_bci, rms, this, benefit, this->budget() - rms->getResolvedMethod()->maxBytecodeIndex(), callsite);
  TR_ASSERT_FATAL(newChild, "storing a null child");
  this->_children->push_back(newChild);
  TR_ASSERT(_children->size() > 1, "something wrong");
  this->_head->_indices = nullptr;
  return _children->back();
  }

bool
IDT::Node::nodeSimilar(int32_t callsite_bci, TR::ResolvedMethodSymbol* rms) const
  {
  auto a = _rms->getResolvedMethod()->maxBytecodeIndex();
  auto b = rms->getResolvedMethod()->maxBytecodeIndex();

  bool retval = a == b && _callsite_bci == callsite_bci;
  return retval;
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
   int retval = _children->size();
   //TODO... why is this assertion being triggered?
   TR_ASSERT(retval > 1, "retval can't be 1 nor 0");
   return retval;
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
      sum += (*child)->howManyDescendantsIncludingMe();
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
     sum += 1 + (*child)->howManyDescendants();
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

IDT::Node *IDT::getNodeByCalleeIndex(int calleeIndex)
  {
    if (!_indices) return NULL;
    return (_indices)[calleeIndex + 1];
  }

void
IDT::Node::copyChildrenFrom(const IDT::Node * other, Indices& someIndex)
   {
   TR_ASSERT(other->getResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier() == this->getResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier(), "copying different nodes not allowed");
   //FIXME: fix the callsites (they have a call stack that needs some modification, but other than that, the call site index should be the same)
   int count = other->getNumChildren();
   if (count == 0) return;
 
   IDT::Node *childCopy = other->getOnlyChild();
   if (childCopy)
      {
      if (TR::comp()->getOption(TR_TraceBIIDTGen))
         {
         traceMsg(TR::comp(), "copying %d into %d\n", childCopy->getCalleeIndex(), this->getCalleeIndex());
         }
      IDT::Node *newChild = this->addChildIfNotExists(this->_head,
                         childCopy->_callsite_bci,
                         childCopy->_rms,
                         childCopy->_benefit, childCopy->_callSite);
      newChild->setCallTarget(childCopy->getCallTarget());
      return;
      }

   for (int i = 0; i < count; i++)
      {
      IDT::Node *childCopy = other->_children->at(i);
      if (TR::comp()->getOption(TR_TraceBIIDTGen))
         {
         traceMsg(TR::comp(), "copying %d into %d\n", childCopy->getCalleeIndex(), this->getCalleeIndex());
         }
      IDT::Node *newChild = this->addChildIfNotExists(this->_head,
                         childCopy->_callsite_bci,
                         childCopy->_rms,
                         childCopy->_benefit, childCopy->_callSite);
      newChild->setCallTarget(childCopy->getCallTarget());
      return;
      }
   }

void
IDT::Node::enqueue_subordinates(IDT::NodePtrPriorityQueue *q) const
   {
      TR_ASSERT(q, "priority queue can't be nullptr");
      int count = this->getNumChildren();
      IDT::Node* child = this->getOnlyChild();
      if (child) {
         TR_ASSERT(child, "child can't be null");
         q->push(child);
         return;
      }
      for (int i = 0; i < count; i++)
         {
            IDT::Node *child = _children->at(i);
            TR_ASSERT(child, "child can't be null");
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
      return !this->getCallTarget() ? this->_rms : this->getCallTarget()->_calleeSymbol;
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

unsigned int
IDT::Node::getByteCodeIndex()
{
   return this->_callsite_bci;
}

IDT::Node*
IDT::Node::findChildWithBytecodeIndex(int bcIndex)
  {
    //TODO: not linear search
    int size = this->getNumChildren();
    if (size == 0) return nullptr;
    if (size == 1) {
        IDT::Node *child = this->getOnlyChild();
        return (child->_callsite_bci == bcIndex) ? child : nullptr;
    }
    IDT::Node *child = nullptr;
    for (int i = 0; i < size; i++) {
       if (child && _children->at(i)->_callsite_bci == bcIndex)
          TR_ASSERT(false, "we have more than two, how to disambiguate?");
       if (_children->at(i)->_callsite_bci == bcIndex)
          child = ((_children->at(i)));
    }
  //TR_ASSERT_FATAL(false, "we shouldn't be here");
  return child;
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

/*
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
        break;
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
*/

void
IDT::Node::print()
{
  if (!TR::comp()->getOption(TR_TraceBIIDTGen)) return;
  traceMsg(TR::comp(), "IDT: name = %s\n", this->getName());
}

IDT::Node*
IDT::Node::getChild(unsigned int childIndex) const
{
   int children = this->getNumChildren();
   if (0 == children) return nullptr;

   bool correct = childIndex < children;
   if (!correct) {
      TR_ASSERT_FATAL(false, "we shouldn't call this");
      return nullptr;
   }

   bool onlyOneChild = (childIndex == 0 && children == 1);
   if (onlyOneChild)
      {
      return this->getOnlyChild();
      }


   return _children->at(childIndex);
}

TR_CallTarget *
IDT::Node::getCallTarget() const
{
  //this->_calltarget->_calleeSymbol->setFlowGraph(this->_calltarget->_cfg);
  return this->_calltarget;
}

void
IDT::Node::setCallTarget(TR_CallTarget *calltarget)
{
   this->_calltarget = calltarget;
}

TR_CallStack*
IDT::Node::getCallStack()
{
  this->_callStack->_methodSymbol->setFlowGraph(this->getCallTarget()->_cfg);
  return this->_callStack;
}

void
IDT::Node::setCallStack(TR_CallStack* callStack)
{
  this->_callStack = callStack;
}

void
IDT::Node::printByteCode()
{
   TR::ResolvedMethodSymbol *resolvedMethodSymbol = this->_rms;
   TR_ResolvedJ9Method* resolvedMethod = static_cast<TR_ResolvedJ9Method*>(resolvedMethodSymbol->getResolvedMethod());
   TR_J9VMBase *fe = static_cast<TR_J9VMBase*>(TR::comp()->fe());
   TR_J9ByteCodeIterator bci(resolvedMethodSymbol, resolvedMethod, fe, TR::comp());
   const char* signature1 = bci.methodSymbol()->signature(TR::comp()->trMemory());
   const char* signature2 = bci.method()->signature(TR::comp()->trMemory());
   int end = resolvedMethod->maxBytecodeIndex();
   for (TR_J9ByteCode bc = bci.current(); bc != J9BCunknown && bci.currentByteCodeIndex() < end; bc = bci.next())
   {
     bci.printByteCode();
     IDT::Node *child = nullptr;
     switch(bc)
     {
        case J9BCinvokestatic:
        case J9BCinvokevirtual:
        case J9BCinvokedynamic:
        case J9BCinvokespecial:
        case J9BCinvokeinterface:
        // print name of target...
           child = this->findChildWithBytecodeIndex(bci.currentByteCodeIndex());
           if (!child) {
              traceMsg(TR::comp(), "No child for bytecode index %d\n", bci.currentByteCodeIndex());
              break;
           }
           traceMsg(TR::comp(), "CalleeSymbol signature %s\n", child->getCallTarget()->_calleeSymbol->signature(TR::comp()->trMemory()));
           traceMsg(TR::comp(), "calleeMethod signature %s\n", child->getCallTarget()->_calleeMethod->signature(TR::comp()->trMemory()));
        break;
        default:
        break;
     }
   }
   traceMsg(TR::comp(), "\n");
}
