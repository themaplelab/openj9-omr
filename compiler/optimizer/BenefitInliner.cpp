#include "compile/SymbolReferenceTable.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/OMRBlock.hpp"
#include "il/Block.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "infra/Cfg.hpp"
#include "infra/CfgNode.hpp" // for CFGNode
#include "infra/ILWalk.hpp"
#include "compiler/env/PersistentCHTable.hpp"
#include "infra/SimpleRegex.hpp"
#include "optimizer/BenefitInliner.hpp"
#include "optimizer/MethodSummary.hpp"
#include "optimizer/J9CallGraph.hpp"
#include "optimizer/J9EstimateCodeSize.hpp"
#include "optimizer/PriorityPreorder.hpp"
#include "optimizer/Growable_2d_array.hpp"
#include "optimizer/InlinerPacking.hpp"
#include "optimizer/AbsEnvStatic.hpp"
#include "optimizer/IDTConstructor.hpp"
#include "optimizer/InliningProposal.hpp"


int32_t OMR::BenefitInlinerWrapper::perform()
   {
   TR::ResolvedMethodSymbol * sym = comp()->getMethodSymbol();
   TR::CFG *prevCFG = sym->getFlowGraph();
   int32_t budget = this->getBudget(sym);
   if (budget < 0) return 1;
   OMR::BenefitInliner inliner(optimizer(), this, budget);
   inliner._rootRms = prevCFG;
   inliner.initIDT(sym, budget);
   if (comp()->trace(OMR::benefitInliner))
     {
     traceMsg(TR::comp(), "starting benefit inliner for %s, budget = %d, hotness = %s\n", sym->signature(this->comp()->trMemory()), budget, comp()->getHotnessName(comp()->getMethodHotness()));
     }
   
   inliner.obtainIDT(inliner._idt->getRoot(), budget);
   inliner.traceIDT();
   if (inliner._idt->howManyNodes() == 1) 
      {
      inliner._idt->getRoot()->getResolvedMethodSymbol()->setFlowGraph(inliner._rootRms);
      return 1;
      }
   inliner._idt->getRoot()->getResolvedMethodSymbol()->setFlowGraph(inliner._rootRms);
   int recursiveCost = inliner._idt->getRoot()->getRecursiveCost();
   bool canSkipAbstractInterpretation = recursiveCost < budget;
   if (!canSkipAbstractInterpretation) {
       if (TR::comp()->getOption(TR_TraceAbstractInterpretation))
       {
       traceMsg(TR::comp(), "STARTXXX: about to start abstract interpretation\n");
       }
      inliner.abstractInterpreter();
       if (TR::comp()->getOption(TR_TraceAbstractInterpretation))
       {
       traceMsg(TR::comp(), "ENDXXX: about to end abstract interpretation\n");
       }
      inliner.analyzeIDT();
   } else {
      inliner.addEverything();
   }

/*
   inliner._idt->getRoot()->getResolvedMethodSymbol()->setFlowGraph(inliner._rootRms);
*/
   inliner._currentNode = inliner._idt->getRoot();
   inliner.performInlining(sym);
   return 1;
   }

void
OMR::BenefitInliner::addEverything() {
  _inliningProposal = new (this->_cfgRegion) InliningProposal(this->comp()->trMemory()->currentStackRegion(), this->_idt);
  this->addEverythingRecursively(this->_idt->getRoot());
  this->_inliningProposal->print(comp());
}

void
OMR::BenefitInliner::addEverythingRecursively(IDT::Node *node)
   {
   TR_ASSERT(_inliningProposal, "we don't have an inlining proposal\n");
   _inliningProposal->pushBack(node);
   int children = node->getNumChildren();
   for (int i = 0; i < children; i++)
      {
      IDT::Node *child = node->getChild(i);
      this->addEverythingRecursively(child);
      }
   }

void
OMR::BenefitInlinerBase::loopThroughIDT(IDT::Node* node) {
  traceMsg(TR::comp(), "looping through %s\n", node->getName());
  // We don't really need to have a flow graph for _calleeSymbol since the _calleeSymbol might not be the target _calleeMethod
  //TR_ASSERT(node->getCallTarget()->_calleeSymbol->getFlowGraph(), "we have a flow graph");
  node->printByteCode();
  for (int i = 0; i < node->getNumChildren(); i++)
  {
    IDT::Node *child = node->getChild(i);
    loopThroughIDT(child);
  }
}

void
OMR::BenefitInlinerBase::debugTrees(TR::ResolvedMethodSymbol *rms)
{
      for (TR::TreeTop * tt = rms->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
         {
         TR::Node * parent = tt->getNode();
         if (!parent->getNumChildren()) continue;

         TR::Node * node = parent->getChild(0);
         if (!node->getOpCode().isCall()) continue;
         traceMsg(TR::comp(), "name of node ? %s\n", node->getOpCode().getName());
         TR_ByteCodeInfo &bcInfo = node->getByteCodeInfo();
         traceMsg(TR::comp(), "bytecodeIndex %d\n", bcInfo.getByteCodeIndex());
         traceMsg(comp(), "is call indirect? %s\n", node->getOpCode().isCallIndirect() ? "true" : "false");
         }
}


bool
OMR::BenefitInlinerBase::inlineCallTargets(TR::ResolvedMethodSymbol *rms, TR_CallStack *prevCallStack, TR_InnerPreexistenceInfo *info)
{
   // inputs : IDT = all possible inlining candidates
   // TR::InliningProposal results = candidates selected for inlining
   // IDT::Node = _currentNode being analyzed

   // if current node is in inlining proposal
   // at the beginning, current node is root, so it can't be inlined. So we need to iterate over its children to see if they are inlined...
   if (!this->_currentNode) {
      return false;
   }

   traceMsg(TR::comp(), "inlining into %s\n", this->_currentNode->getName());
   TR_ASSERT(!prevCallStack || prevCallStack->_methodSymbol->getFlowGraph(), "we have a null call graph");

   TR_CallStack callStack(comp(), rms, rms->getResolvedMethod(), prevCallStack, 1500, true);
   if (info)
      callStack._innerPrexInfo = info;

   bool inlined = this->usedSavedInformation(rms, &callStack, info, this->_currentNode);
   return inlined;
}

bool
OMR::BenefitInlinerBase::analyzeCallSite(TR_CallStack * callStack, TR::TreeTop * callNodeTreeTop, TR::Node * parent, TR::Node * callNode, TR_CallTarget *calltargetToInline)
   {
   TR_InlinerDelimiter delimiter(tracer(),"TR_DumbInliner::analyzeCallSite");

   TR::SymbolReference *symRef = callNode->getSymbolReference();
   TR::MethodSymbol *calleeSymbol = symRef->getSymbol()->castToMethodSymbol();

   TR_CallSite *callsite = TR_CallSite::create(callNodeTreeTop, parent, callNode,
                                               (TR_OpaqueClassBlock*) 0, symRef, (TR_ResolvedMethod*) 0,
                                               comp(), trMemory() , stackAlloc);
   getSymbolAndFindInlineTargets(callStack,callsite);

   if (!callsite->numTargets())
      return false;

   bool success = false;
   for(int32_t i=0;i<callsite->numTargets();i++)
      {
      TR_CallTarget *calltarget = callsite->getTarget(i);
      if (!calltarget->_calleeMethod->isSameMethod(calltargetToInline->_calleeMethod)) continue;
      if (calltarget->_alreadyInlined) continue;

      success |= inlineCallTarget(callStack, calltarget, false);
      }
   return success;
   }

bool
OMR::BenefitInlinerBase::usedSavedInformation(TR::ResolvedMethodSymbol *rms, TR_CallStack *callStack, TR_InnerPreexistenceInfo *info, IDT::Node *idtNode)
{
      bool inlined = false;
      uint32_t inlineCount = 0;
      for (TR::TreeTop * tt = rms->getFirstTreeTop(); tt; tt = tt->getNextTreeTop())
         {
         _inliningAsWeWalk = false;
         TR::Node * parent = tt->getNode();
         if (!parent->getNumChildren()) continue;

         TR::Node * node = parent->getChild(0);
         if (!node->getOpCode().isCall()) continue;

         if (node->getVisitCount() == _visitCount) continue;

         // is this call site in the IDT?
         TR_ByteCodeInfo &bcInfo = node->getByteCodeInfo();
         IDT::Node *child = idtNode->findChildWithBytecodeIndex(bcInfo.getByteCodeIndex());
         if (!child) continue;

         // is this IDT node in the proposal to inline?
         bool shouldBeInlined = child->isInProposal(this->_inliningProposal);
         if (!shouldBeInlined) continue;

         if (TR::comp()->trace(OMR::benefitInliner))
             {
             traceMsg(TR::comp(), "inlining node %d %s\n", child->getCalleeIndex(), child->getName());
             }

         IDT::Node *prevChild = this->_currentChild;
         this->_currentChild = child;
         bool success = analyzeCallSite(callStack, tt, parent, node, child->getCallTarget());
         if (success)
             {
             inlineCount++;
#define MAX_INLINE_COUNT 1000
               if (inlineCount >= MAX_INLINE_COUNT)
                  {
                  if (comp()->trace(OMR::inlining))
                     traceMsg(comp(), "inliner: stopping inlining as max inline count of %d reached\n", MAX_INLINE_COUNT);
                  break;
                  }
#undef MAX_INLINE_COUNT
             node->setVisitCount(_visitCount);
             }
         if (TR::comp()->trace(OMR::benefitInliner))
             {
             traceMsg(TR::comp(), "success ? %s\n", success ? "true" : "false");
             }

         this->_currentChild = prevChild;

         }
    callStack->commit();
    return inlined;
}

void
OMR::BenefitInlinerBase::getSymbolAndFindInlineTargets(TR_CallStack *callStack, TR_CallSite *callsite, bool findNewTargets)
   {
   TR_InlinerDelimiter delimiter(tracer(),"getSymbolAndFindInlineTargets");

   TR::Node *callNode = callsite->_callNode;
   TR::TreeTop *callNodeTreeTop  = callsite->_callNodeTreeTop;

   TR::SymbolReference * symRef = callNode->getSymbolReference();
   TR_InlinerFailureReason isInlineable = InlineableTarget;


   if (callsite->_initialCalleeSymbol)
      {
      callsite->_initialCalleeMethod = callsite->_initialCalleeSymbol->getResolvedMethod();
      //this heuristic is not available during IDT construction so, it shouldn't be available here either...
      //if (getPolicy()->supressInliningRecognizedInitialCallee(callsite, comp()))
      //   isInlineable = Recognized_Callee;

      if (isInlineable != InlineableTarget)
         {
         tracer()->insertCounter(isInlineable, callNodeTreeTop);
         callsite->_failureReason=isInlineable;
         callsite->removeAllTargets(tracer(),isInlineable);
         traceMsg(comp(), "not inlineable target 1\n");
         return;
         }

      if (comp()->fe()->isInlineableNativeMethod(comp(), callsite->_initialCalleeSymbol))
         {
         TR_VirtualGuardSelection *guard = new (trHeapMemory()) TR_VirtualGuardSelection(TR_NoGuard);
         callsite->addTarget(trMemory(),this ,guard ,callsite->_initialCalleeSymbol->getResolvedMethod(),callsite->_receiverClass, heapAlloc);
         }
      }
   else if ((isInlineable = static_cast<TR_InlinerFailureReason> (checkInlineableWithoutInitialCalleeSymbol (callsite, comp()))) != InlineableTarget)
      {
      tracer()->insertCounter(isInlineable, callNodeTreeTop);
      callsite->_failureReason=isInlineable;
      callsite->removeAllTargets(tracer(),isInlineable);
      traceMsg(comp(), "not inlineable target\n");
      return;
      }

   if (callsite->_initialCalleeSymbol && callsite->_initialCalleeSymbol->firstArgumentIsReceiver())
      {
      if (callsite->_isIndirectCall && !callsite->_receiverClass)
         {
         if (callsite->_initialCalleeMethod)
            callsite->_receiverClass = callsite->_initialCalleeMethod->classOfMethod();

         int32_t len;
         const char * s = callNode->getChild(callNode->getFirstArgumentIndex())->getTypeSignature(len);
         TR_OpaqueClassBlock * type = s ? fe()->getClassFromSignature(s, len, callsite->_callerResolvedMethod, true) : 0;
         if (type && (!callsite->_receiverClass || (type != callsite->_receiverClass && fe()->isInstanceOf(type, callsite->_receiverClass, true, true) == TR_yes)))
            callsite->_receiverClass = type;
         }
      }

   if(tracer()->debugLevel())
      tracer()->dumpCallSite(callsite, "CallSite Before finding call Targets");

   //////////

   if (findNewTargets)
      {
      callsite->findCallSiteTarget(callStack, this);
      TR_InlinerBase::applyPolicyToTargets(callStack, callsite);
      }

   if(tracer()->debugLevel())
      tracer()->dumpCallSite(callsite, "CallSite after finding call Targets");

   for (int32_t i=0; i<callsite->numTargets(); i++)
      {
      TR_CallTarget *target = callsite->getTarget(i);
      if (!target->_calleeSymbol || !target->_calleeMethod->isSameMethod(target->_calleeSymbol->getResolvedMethod()))
         {
         target->_calleeMethod->setOwningMethod(callsite->_callNode->getSymbolReference()->getOwningMethodSymbol(comp())->getResolvedMethod()); //need to point to the owning method to the same one that got ilgen'd d157939

         target->_calleeSymbol = comp()->getSymRefTab()->findOrCreateMethodSymbol(
               symRef->getOwningMethodIndex(), -1, target->_calleeMethod, TR::MethodSymbol::Virtual /* will be target->_calleeMethodKind */)->getSymbol()->castToResolvedMethodSymbol();
         }

      TR_InlinerFailureReason checkInlineableTarget = getPolicy()->checkIfTargetInlineable(target, callsite, comp());

      if (checkInlineableTarget != InlineableTarget)
         {
         tracer()->insertCounter(checkInlineableTarget,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),checkInlineableTarget);
         i--;
         continue;
         }

      TR::RecognizedMethod rm = target->_calleeSymbol ? target->_calleeSymbol->getRecognizedMethod() : target->_calleeMethod->getRecognizedMethod();
      if (rm != TR::unknownMethod && !inlineRecognizedMethod(rm))
         {
         tracer()->insertCounter(Recognized_Callee,callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),Recognized_Callee);
         i--;
         continue;
         }

      getUtil()->collectCalleeMethodClassInfo(target->_calleeMethod);
      }

   if(callsite->numTargets())    //if we still have targets at this point we can return true as we would have in the loop
      return;

/*
   if ( getUtil()->getCallCount(callNode) > 0)
      {
      if (callsite->isInterface())
         TR::Options::INLINE_failedToDevirtualize ++;
      else
         TR::Options::INLINE_failedToDevirtualizeInterface ++;
      }
*/

   if (callsite->numTargets()>0 && callsite->getTarget(0) && !callsite->getTarget(0)->_calleeMethod && comp()->trace(OMR::inlining))
      {
      traceMsg(comp(), "inliner: method is unresolved: %s into %s\n", callsite->_interfaceMethod->signature(trMemory()), tracer()->traceSignature(callStack->_methodSymbol));
      callsite->_failureReason=Unresolved_Callee;
      }

   callsite->removeAllTargets(tracer(),Unresolved_Callee);
   traceMsg(comp(), "unresolved callee\n");
   callsite->_failureReason=No_Inlineable_Targets;
   return;
   }
/*
bool
OMR::BenefitInlinerBase::inlineCallTarget(TR_CallStack *callStack, TR_CallTarget *calltarget, bool inlinefromgraph, TR_PrexArgInfo *argInfo, TR::TreeTop** cursorTreeTop)
{
   return false;
}
*/




void
OMR::BenefitInliner::abstractInterpreter()
   {
   AbsFrame absFrame(_absEnvRegion, this->_idt->getRoot());
   AbsEnvStatic absEnvStatic(_absEnvRegion, this->_idt->getRoot(), &absFrame);
   absFrame.interpret();
   this->_idt->getRoot()->getResolvedMethodSymbol()->setFlowGraph(this->_rootRms);
   }

void
OMR::BenefitInliner::analyzeIDT()
   {
      if (this->_idt->howManyNodes() == 1) return; // No Need to analyze, since there is nothing to inline.
      this->_idt->buildIndices();
      PriorityPreorder items(this->_idt, this->comp());
      traceMsg(TR::comp(), "size = %d, budget = %d\n", items.size(), this->budget());
      Growable_2d_array_BitVectorImpl results(this->comp(), items.size(), this->budget() + 1, this);
      _inliningProposal = forwards_BitVectorImpl(this->budget(), items, &results, this->comp(), this, this->_idt);
      traceMsg(TR::comp(), "budget: %d result: \n", this->budget());
      _inliningProposal->print(comp());
   }

int32_t
OMR::BenefitInlinerWrapper::getBudget(TR::ResolvedMethodSymbol *resolvedMethodSymbol)
   {
      int32_t size = resolvedMethodSymbol->getResolvedMethod()->maxBytecodeIndex();
      if (this->comp()->getMethodHotness() >= scorching)
      {
         return std::max(1500, size * 2);
      }

      if (this->comp()->getMethodHotness() >= hot)
      {
         return std::max(1500, size + (size >> 2));
      }

      if (this->comp()->getMethodHotness() >= warm && size < 250)
      {
         return 250;
      }

      if (this->comp()->getMethodHotness() >= warm && size < 700)
      {
         return std::max(700, size + (size >> 2));
      }

      if (this->comp()->getMethodHotness() >= warm)
      {
         return size + (size >> 3);
      }

      return 25;
   }

void OMR::BenefitInliner::initIDT(TR::ResolvedMethodSymbol *root, int budget)
   {
   //TODO: can we do this in the initialization list?
   _idt = new (comp()->trMemory()->currentStackRegion()) IDT(this, comp()->trMemory()->currentStackRegion(), root, budget);
   }

void OMR::BenefitInliner::traceIDT()
   {
  if (!TR::comp()->getOption(TR_TraceBIIDTGen)) return;
   _idt->printTrace();
   }

void
OMR::BenefitInliner::obtainIDT(IDT::Indices *Deque, IDT::Node *currentNode, TR_CallSite *callsite, int32_t budget, int cpIndex)
   {
   if (!callsite) return;
   for (int i = 0; i < callsite->numTargets(); i++)
      {
      TR_CallTarget *callTarget = callsite->getTarget(i);
      TR::ResolvedMethodSymbol * resolvedMethodSymbol = TR::ResolvedMethodSymbol::create(comp()->trHeapMemory(), callTarget->_calleeMethod, comp());

      int budgetForChild = currentNode->budget() - callTarget->_calleeMethod->maxBytecodeIndex();
      
      bool hasEnoughBudget = 0 < budgetForChild;
      if (!hasEnoughBudget) continue;

      IDT::Node *node = currentNode->addChildIfNotExists(this->_idt, callsite->_byteCodeIndex, resolvedMethodSymbol, 100, callsite, callTarget->_callRatioCallerCallee);
      if (node) {
         node->setCallStack(this->_inliningCallStack);
         node->setCallTarget(callTarget);
         TR::CFGNode *cfgNode = callTarget->_calleeSymbol->getFlowGraph()->getStartForReverseSnapshot();
         TR::Block *startBlock = cfgNode->asBlock();
         Deque->push_back(node);
      }
      if (node && comp()->trace(OMR::benefitInliner))
         {
         traceMsg(TR::comp(), "adding %d %s into %d %s budget for child %d\n", node->getCalleeIndex(), node->getName(), currentNode->getCalleeIndex(), currentNode->getName(), budgetForChild);
         trfflush(comp()->getOutFile());
         callTarget = node->getCallTarget();
         traceMsg(comp(), "with guard kind=%s and type=%s\n",
                              tracer()->getGuardKindString(callTarget->_guard),
                              tracer()->getGuardTypeString(callTarget->_guard));
         }
      }

   }

void
OMR::BenefitInliner::obtainIDT(IDT::Node *node, int32_t budget)
   {
      if (budget < 0) return;

      TR::ResolvedMethodSymbol *resolvedMethodSymbol = node->getResolvedMethodSymbol();
      TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();

      TR_CallStack *prevCallStack = this->_inliningCallStack;
      bool isRecursiveCall = prevCallStack && this->_inliningCallStack->isAnywhereOnTheStack(resolvedMethod, 1);
      if (isRecursiveCall) { return; }

      TR::CFG *cfg = NULL;
      TR::CFG *prevCFG = resolvedMethodSymbol->getFlowGraph();
      int32_t maxBytecodeIndex = resolvedMethodSymbol->getResolvedMethod()->maxBytecodeIndex();
      if (!prevCallStack)
         {
         TR_CallTarget *calltarget = new (this->comp()->trHeapMemory()) TR_CallTarget(NULL, resolvedMethodSymbol, resolvedMethod, NULL, resolvedMethod->containingClass(), NULL);
         TR_J9EstimateCodeSize *cfgGen = (TR_J9EstimateCodeSize *)TR_EstimateCodeSize::get(this, this->tracer(), 0);
         cfg = cfgGen->generateCFG(calltarget, NULL, this->_cfgRegion);
         cfg->computeInitialBlockFrequencyBasedOnExternalProfiler(comp());
         //cfg->setBlockFrequenciesBasedOnInterpreterProfiler();
         //uint32_t firstBlockFreq = cfg->getInitialBlockFrequency();
         //traceMsg(TR::comp(), "initialBlockFrequency = %d\n", firstBlockFreq);
         cfg->setFrequencies();
         traceMsg(TR::comp(), "cfg->getStartBlockFrequency() = %d\n", cfg->getStartBlockFrequency());
         resolvedMethodSymbol->setFlowGraph(cfg);
         cfg->getStartForReverseSnapshot()->setFrequency(cfg->getStartBlockFrequency());
         node->setCallTarget(calltarget);
         } 
      else 
         {
         cfg = resolvedMethodSymbol->getFlowGraph();
         cfg->getStartForReverseSnapshot()->setFrequency(cfg->getStartBlockFrequency());
         }

   IDT::Indices Deque(0, nullptr, this->comp()->trMemory()->currentStackRegion());
   bool shouldInterpret = this->obtainIDT(Deque, node, budget);
   this->_inliningCallStack = new (this->_callSitesRegion) TR_CallStack(this->comp(), resolvedMethodSymbol, resolvedMethod, prevCallStack, budget, true);
   AbsFrameIDTConstructor constructor(this->_callSitesRegion, node, this->_callerIndex, this->_inliningCallStack, this);
   //TODO: remove this interface
   constructor.setDeque(&Deque);

   if (shouldInterpret)
      {
      constructor.interpret();
      }

   if (TR::comp()->getOption(TR_TraceAbstractInterpretation))
      {
      constructor.traceMethodSummary();
      }


   node->_summary = constructor._summary;

   while (!Deque.empty())
      {
      IDT::Node *node2 = Deque.front();
      Deque.pop_front();
      TR_ASSERT(node2 && node2->_callSite, "call site is null");
      TR::ResolvedMethodSymbol * resolvedMethodSymbol = TR::ResolvedMethodSymbol::create(comp()->trHeapMemory(), node2->getCallTarget()->_calleeMethod, comp());
      if (!comp()->incInlineDepth(resolvedMethodSymbol, node2->_callSite->_bcInfo, node2->_callSite->_cpIndex, NULL, !node2->_callSite->isIndirectCall(), 0)) continue;
      this->_callerIndex++;
      this->obtainIDT(node2, node2->budget());
      this->_callerIndex--;
      comp()->decInlineDepth(true);
      }
   this->_inliningCallStack = prevCallStack;
   
   }

bool
OMR::BenefitInliner::obtainIDT(IDT::Indices &Deque, IDT::Node *node, int32_t budget)
   {
      // Here is where I should make a method summary and edit it in the next obtainIDT...
      TR::ResolvedMethodSymbol *resolvedMethodSymbol = node->getResolvedMethodSymbol();
      TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
      TR_OpaqueMethodBlock *persistentIdentifier = resolvedMethod->getPersistentIdentifier();
      // if it is found, keep it as a nullptr, as we don't want to write new data?
      auto iter = this->_methodSummaryMap.find(persistentIdentifier);
      IDT::Node *ms = iter == this->_methodSummaryMap.end() ? nullptr : iter->second;

      if (ms != nullptr)
         {
         // At this moment, the method visited by node has already been visited by the method in ms.
         // So, ideally we would like to create avoid all the iteration and just create nodes.
         // We add the children from ms to Deque and Children in Node.
         if ((node->budget() - node->getCost()) < 0) return false;
         node->copyChildrenFrom(ms, Deque);
         return false;
         }
      if (ms == nullptr)
         {
         this->_methodSummaryMap.insert(std::pair<TR_OpaqueMethodBlock *, IDT::Node *>(persistentIdentifier,node));
         }

      return true;
   }


// This is basically very limited abstract interpreter, we should be able to 
// re-use the class.
void
OMR::BenefitInliner::obtainIDT(IDT::Indices &Deque, IDT::Node *node, TR_J9ByteCodeIterator &bci, TR::Block *block, int budget)
   {
      int start = block->getBlockBCIndex();
      int end = block->getBlockBCIndex() + block->getBlockSize();
      bci.setIndex(start);
      for(TR_J9ByteCode bc = bci.current(); bc != J9BCunknown && bci.currentByteCodeIndex() < end; bc = bci.next())
         {
         TR::MethodSymbol::Kinds kind = TR::MethodSymbol::Kinds::Helper;
         switch(bc)
            {
               
               case J9BCinvokestatic:
               kind = TR::MethodSymbol::Kinds::Static;
               break;
               case J9BCinvokespecial:
               kind = TR::MethodSymbol::Kinds::Special;
               break;
               case J9BCinvokevirtual:
               kind = TR::MethodSymbol::Kinds::Virtual;
               break;
               case J9BCinvokeinterface:
               kind = TR::MethodSymbol::Kinds::Interface;
               default:
               break;
            }


         if (kind != TR::MethodSymbol::Kinds::Helper)
            {
            TR_CallSite *callsite = this->findCallSiteTarget(bci.methodSymbol(), bci.currentByteCodeIndex(), bci.next2Bytes(), kind, block, node->getCallTarget()->_cfg);
            this->obtainIDT(&Deque, node, callsite, budget, bci.next2Bytes());
            }
         }
   }

//TODO: delete me
TR::SymbolReference*
OMR::BenefitInliner::getSymbolReference(TR::ResolvedMethodSymbol *callerSymbol, int cpIndex, TR::MethodSymbol::Kinds kind)
   {
      TR::SymbolReference *symRef = NULL;
      switch(kind) {
         case (TR::MethodSymbol::Kinds::Virtual):
            symRef = this->comp()->getSymRefTab()->findOrCreateVirtualMethodSymbol(callerSymbol, cpIndex);
         break;
         case (TR::MethodSymbol::Kinds::Static):
            symRef = this->comp()->getSymRefTab()->findOrCreateStaticMethodSymbol(callerSymbol, cpIndex);
         break;
         case (TR::MethodSymbol::Kinds::Interface):
            symRef = this->comp()->getSymRefTab()->findOrCreateInterfaceMethodSymbol(callerSymbol, cpIndex);
         break;
         case (TR::MethodSymbol::Kinds::Special):
            symRef = this->comp()->getSymRefTab()->findOrCreateSpecialMethodSymbol(callerSymbol, cpIndex);
         break;
      }
      return symRef;
   }

TR_CallSite*
OMR::BenefitInliner::findCallSiteTarget(TR::ResolvedMethodSymbol *callerSymbol, int bcIndex, int cpIndex, TR::MethodSymbol::Kinds kind, TR::Block *block, TR::CFG* cfg)
   {
      TR_ByteCodeInfo info;
      TR_ResolvedMethod *caller = callerSymbol->getResolvedMethod();
      uint32_t methodSize = TR::Compiler->mtd.bytecodeSize(caller->getPersistentIdentifier());
      TR::SymbolReference *symRef = this->getSymbolReference(callerSymbol, cpIndex, kind);
      TR::Symbol *sym = symRef->getSymbol();
      bool isInterface = kind == TR::MethodSymbol::Kinds::Interface;
      if (symRef->isUnresolved() && !isInterface) return NULL;
      
      TR::ResolvedMethodSymbol *calleeSymbol = !isInterface ? sym->castToResolvedMethodSymbol() : NULL;
      TR_ResolvedMethod *callee = !isInterface ? calleeSymbol->getResolvedMethod() : NULL;
      TR::Method *calleeMethod = !isInterface ? calleeSymbol->getMethod() : this->comp()->fej9()->createMethod(this->comp()->trMemory(), caller->containingClass(), cpIndex);

      info.setByteCodeIndex(bcIndex);
      info.setDoNotProfile(false);
      info.setCallerIndex(this->_callerIndex);
      TR_OpaqueClassBlock *callerClass = caller->classOfMethod();
      TR_OpaqueClassBlock *calleeClass = callee ? callee->classOfMethod() : NULL;
      info.setIsSameReceiver(callerClass == calleeClass);

      bool isIndirect = kind == TR::MethodSymbol::Kinds::Static || TR::MethodSymbol::Kinds::Special;
      int32_t offset = kind == TR::MethodSymbol::Virtual ? symRef->getOffset() : -1;
      
      
      TR_CallSite *callsite = this->getCallSite
         (
            kind,
            caller,
            NULL,
            NULL,
            NULL,
            calleeMethod,
            calleeClass,
            offset,
            cpIndex,
            callee,
            calleeSymbol,
            isIndirect,
            isInterface,
            info,
            this->comp()
         );
      //TODO: Sometimes these were not set, why?
      callsite->_byteCodeIndex = bcIndex;
      callsite->_bcInfo = info;
      callsite->_cpIndex= cpIndex;

      callsite->findCallSiteTarget(this->_inliningCallStack, this);
      //TODO: Sometimes these were not set, why?
      this->_inliningCallStack->_methodSymbol = this->_inliningCallStack->_methodSymbol ? this->_inliningCallStack->_methodSymbol : callerSymbol;

      applyPolicyToTargets(this->_inliningCallStack, callsite, block, cfg);
      return callsite;
   }

void
OMR::BenefitInliner::printTargets(TR::ResolvedMethodSymbol *callerSymbol, int bcIndex, int cpIndex, TR::MethodSymbol::Kinds kind)
   {
      TR_ResolvedMethod *caller = callerSymbol->getResolvedMethod();
      TR::SymbolReference *symRef = this->getSymbolReference(callerSymbol, cpIndex, kind);
      TR::Symbol *sym = symRef->getSymbol();
      bool isInterface = kind == TR::MethodSymbol::Kinds::Interface;
      if (symRef->isUnresolved() && !isInterface) return;
      
      TR::ResolvedMethodSymbol *calleeSymbol = !isInterface ? sym->castToResolvedMethodSymbol() : NULL;
      TR_ResolvedMethod *callee = !isInterface ? calleeSymbol->getResolvedMethod() : NULL;
      TR::Method *calleeMethod = !isInterface ? calleeSymbol->getMethod() : this->comp()->fej9()->createMethod(this->comp()->trMemory(), caller->containingClass(), cpIndex);

      TR_ByteCodeInfo info;
      info.setByteCodeIndex(bcIndex);
      info.setDoNotProfile(false);
      info.setCallerIndex(this->_callerIndex);
      TR_OpaqueClassBlock *callerClass = caller->classOfMethod();
      TR_OpaqueClassBlock *calleeClass = callee ? callee->classOfMethod() : NULL;
      info.setIsSameReceiver(callerClass == calleeClass);

      bool isIndirect = kind == TR::MethodSymbol::Kinds::Static || TR::MethodSymbol::Kinds::Special;
      int32_t offset = kind == TR::MethodSymbol::Virtual ? symRef->getOffset() : -1;
      
      
      TR_CallSite *callsite = this->getCallSite
         (
            kind,
            caller,
            NULL,
            NULL,
            NULL,
            calleeMethod,
            calleeClass,
            offset,
            cpIndex,
            callee,
            calleeSymbol,
            isIndirect,
            isInterface,
            info,
            this->comp()
         );
      callsite->findCallSiteTarget(this->_inliningCallStack, this);
      applyPolicyToTargets(this->_inliningCallStack, callsite);
      TR_VerboseLog::vlogAcquire();
      for (int i = 0; i < callsite->numTargets(); i++)
         {
         TR_CallTarget *callTarget = callsite->getTarget(i);
         TR_VerboseLog::writeLine(TR_Vlog_SIP, "virtual %s", callTarget->signature(this->comp()->trMemory()));
         }
      TR_VerboseLog::vlogRelease();
                                                
   }

TR_CallSite *
OMR::BenefitInliner::getCallSite(TR::MethodSymbol::Kinds kind,
                                    TR_ResolvedMethod *callerResolvedMethod,
                                    TR::TreeTop *callNodeTreeTop,
                                    TR::Node *parent,
                                    TR::Node *callNode,
                                    TR::Method * interfaceMethod,
                                    TR_OpaqueClassBlock *receiverClass,
                                    int32_t vftSlot,
                                    int32_t cpIndex,
                                    TR_ResolvedMethod *initialCalleeMethod,
                                    TR::ResolvedMethodSymbol * initialCalleeSymbol,
                                    bool isIndirectCall,
                                    bool isInterface,
                                    TR_ByteCodeInfo & bcInfo,
                                    TR::Compilation *comp,
                                    int32_t depth,
                                    bool allConsts)
   {
      TR_CallSite *callsite = NULL;
      switch (kind) {
         case TR::MethodSymbol::Kinds::Virtual:
            callsite = new (this->_callSitesRegion) TR_J9VirtualCallSite(callerResolvedMethod, callNodeTreeTop, parent, callNode, interfaceMethod, receiverClass, vftSlot, cpIndex, initialCalleeMethod, initialCalleeSymbol, isIndirectCall, isInterface, bcInfo, comp, depth, allConsts);
         break;
         case TR::MethodSymbol::Kinds::Static:
         case TR::MethodSymbol::Kinds::Special:
            callsite = new (this->_callSitesRegion) TR_DirectCallSite(callerResolvedMethod, callNodeTreeTop, parent, callNode, interfaceMethod, receiverClass, vftSlot, cpIndex, initialCalleeMethod, initialCalleeSymbol, isIndirectCall, isInterface, bcInfo, comp, depth, allConsts);
         break;
         case TR::MethodSymbol::Kinds::Interface:
            callsite = new (this->_callSitesRegion) TR_J9InterfaceCallSite(callerResolvedMethod, callNodeTreeTop, parent, callNode, interfaceMethod, receiverClass, vftSlot, cpIndex, initialCalleeMethod, initialCalleeSymbol, isIndirectCall, isInterface, bcInfo, comp, depth, allConsts);
         break;
      }
      return callsite;
   }

void
OMR::BenefitInliner::printVirtualTargets(TR::ResolvedMethodSymbol *callerSymbol, int bcIndex, int cpIndex)
   {
      TR_ResolvedMethod *caller = callerSymbol->getResolvedMethod();
      TR::SymbolReference *symRef = this->getSymbolReference(callerSymbol, cpIndex, TR::MethodSymbol::Kinds::Virtual);
      TR::Symbol *sym = symRef->getSymbol();
      if (symRef->isUnresolved()) return;

      TR::ResolvedMethodSymbol *calleeSymbol = sym->castToResolvedMethodSymbol();
      TR_ResolvedMethod *callee = calleeSymbol->getResolvedMethod();
      TR::Method *calleeMethod = calleeSymbol->getMethod();

      TR_ByteCodeInfo info;
      info.setByteCodeIndex(bcIndex);
      info.setDoNotProfile(false);
      info.setCallerIndex(this->_callerIndex);
      TR_OpaqueClassBlock *callerClass = caller->classOfMethod();
      TR_OpaqueClassBlock *calleeClass = callee->classOfMethod();
      info.setIsSameReceiver(callerClass == calleeClass);

      bool isIndirect = true;
      bool isInterface = false;

      TR_CallSite *callsite = new (this->_callSitesRegion) TR_J9VirtualCallSite
         (
            caller,
            NULL,
            NULL,
            NULL,
            calleeMethod,
            calleeClass,
            (int32_t)symRef->getOffset(),
            cpIndex,
            callee,
            calleeSymbol,
            isIndirect,
            isInterface,
            info,
            this->comp()
         );

      callsite->findCallSiteTarget(this->_inliningCallStack, this);
      applyPolicyToTargets(this->_inliningCallStack, callsite);
      TR_VerboseLog::vlogAcquire();
      for (int i = 0; i < callsite->numTargets(); i++)
         {
         TR_CallTarget *callTarget = callsite->getTarget(i);
         TR_VerboseLog::writeLine(TR_Vlog_SIP, "virtual %s", callTarget->signature(this->comp()->trMemory()));
         }
      TR_VerboseLog::vlogRelease();

   }

void
OMR::BenefitInliner::printInterfaceTargets(TR::ResolvedMethodSymbol *callerSymbol, int bcIndex, int cpIndex)
   {
      TR_ResolvedMethod *caller = callerSymbol->getResolvedMethod();
      TR::SymbolReference *symRef = this->getSymbolReference(callerSymbol, cpIndex, TR::MethodSymbol::Kinds::Interface);
      TR::Symbol *sym = symRef->getSymbol();
      TR::ResolvedMethodSymbol *calleeSymbol = NULL;
      TR_ResolvedMethod *callee = NULL;
      TR::Method *calleeMethod = this->comp()->fej9()->createMethod(this->comp()->trMemory(), callerSymbol->getResolvedMethod()->containingClass(), cpIndex);


      TR_ByteCodeInfo info;
      info.setByteCodeIndex(bcIndex);
      info.setDoNotProfile(false);
      info.setCallerIndex(this->_callerIndex);
      TR_OpaqueClassBlock *callerClass = caller->classOfMethod();
      TR_OpaqueClassBlock *calleeClass = NULL;
      info.setIsSameReceiver(callerClass == calleeClass);
      bool isIndirect = true;
      bool isInterface = true;

      TR_CallSite *callsite = new (this->_callSitesRegion) TR_J9InterfaceCallSite
         (
            caller,
            NULL,
            NULL,
            NULL,
            calleeMethod,
            calleeClass,
            -1,
            cpIndex,
            callee,
            calleeSymbol,
            isIndirect,
            isInterface,
            info,
            this->comp()
         );

      callsite->findCallSiteTarget(this->_inliningCallStack, this);
      applyPolicyToTargets(this->_inliningCallStack, callsite);
      TR_VerboseLog::vlogAcquire();
      for (int i = 0; i < callsite->numTargets(); i++)
         {
         TR_CallTarget *callTarget = callsite->getTarget(i);
         TR_VerboseLog::writeLine(TR_Vlog_SIP, "interface %s", callTarget->signature(this->comp()->trMemory()));
         }
      TR_VerboseLog::vlogRelease();

   }

void
OMR::BenefitInliner::printSpecialTargets(TR::ResolvedMethodSymbol *callerSymbol, int bcIndex, int cpIndex)
   {
      TR_ResolvedMethod *caller = callerSymbol->getResolvedMethod();
      TR::SymbolReference *symRef = this->getSymbolReference(callerSymbol, cpIndex, TR::MethodSymbol::Kinds::Special);
      TR::Symbol *sym = symRef->getSymbol();
      if (symRef->isUnresolved()) return;

      TR::ResolvedMethodSymbol *calleeSymbol = sym->castToResolvedMethodSymbol();
      TR_ResolvedMethod *callee = calleeSymbol->getResolvedMethod();

      TR_ByteCodeInfo info;
      info.setByteCodeIndex(bcIndex);
      info.setDoNotProfile(false);
      info.setCallerIndex(this->_callerIndex);
      TR_OpaqueClassBlock *callerClass = caller->classOfMethod();
      TR_OpaqueClassBlock *calleeClass = callee->classOfMethod();
      info.setIsSameReceiver(callerClass == calleeClass);

      TR_CallSite *callsite = new (this->_callSitesRegion) TR_DirectCallSite
         (
            caller,
            NULL,
            NULL,
            NULL,
            calleeSymbol->getMethod(),
            calleeClass,
            -1,
            cpIndex,
            callee,
            calleeSymbol,
            !(calleeSymbol->isStatic() || calleeSymbol->isSpecial()),
            calleeSymbol->isInterface(),
            info,
            this->comp()
         );

      callsite->findCallSiteTarget(this->_inliningCallStack, this);
      applyPolicyToTargets(this->_inliningCallStack, callsite);
      TR_VerboseLog::vlogAcquire();
      for (int i = 0; i < callsite->numTargets(); i++)
         {
         TR_CallTarget *callTarget = callsite->getTarget(i);
         TR_VerboseLog::writeLine(TR_Vlog_SIP, "special %s", callTarget->signature(this->comp()->trMemory()));
         }
      TR_VerboseLog::vlogRelease();

   }

void
OMR::BenefitInliner::printStaticTargets(TR::ResolvedMethodSymbol *callerSymbol, int bcIndex, int cpIndex)
   {
      TR_ResolvedMethod *caller = callerSymbol->getResolvedMethod();
      TR::SymbolReference *symRef = this->getSymbolReference(callerSymbol, cpIndex, TR::MethodSymbol::Kinds::Static);
      TR::Symbol *sym = symRef->getSymbol();
      if (symRef->isUnresolved()) return;

      TR::ResolvedMethodSymbol *calleeSymbol = sym->castToResolvedMethodSymbol();
      TR_ResolvedMethod *callee = calleeSymbol->getResolvedMethod();

      TR_ByteCodeInfo info;
      info.setByteCodeIndex(bcIndex);
      info.setDoNotProfile(false);
      info.setCallerIndex(this->_callerIndex);
      TR_OpaqueClassBlock *callerClass = caller->classOfMethod();
      TR_OpaqueClassBlock *calleeClass = callee->classOfMethod();
      info.setIsSameReceiver(callerClass == calleeClass);

      TR_CallSite *callsite = new (this->_callSitesRegion) TR_DirectCallSite
         (
            caller,
            NULL,
            NULL,
            NULL,
            calleeSymbol->getMethod(),
            calleeClass,
            -1,
            cpIndex,
            callee,
            calleeSymbol,
            !(calleeSymbol->isStatic() || calleeSymbol->isSpecial()),
            calleeSymbol->isInterface(),
            info,
            this->comp()
         );

      callsite->findCallSiteTarget(this->_inliningCallStack, this);
      applyPolicyToTargets(this->_inliningCallStack, callsite);
      TR_VerboseLog::vlogAcquire();
      for (int i = 0; i < callsite->numTargets(); i++)
         {
         TR_CallTarget *callTarget = callsite->getTarget(i);
         TR_VerboseLog::writeLine(TR_Vlog_SIP, "static %s", callTarget->signature(this->comp()->trMemory()));
         }
      TR_VerboseLog::vlogRelease();

   }


void
OMR::BenefitInlinerBase::applyPolicyToTargets(TR_CallStack *callStack, TR_CallSite *callsite, TR::Block *callblock, TR::CFG* callerCFG)
   {
   if (!callsite->_initialCalleeSymbol)
     {
     TR_InlinerFailureReason isInlineable = InlineableTarget;
     if ((isInlineable = static_cast<TR_InlinerFailureReason> (checkInlineableWithoutInitialCalleeSymbol (callsite, comp()))) != InlineableTarget)
        {
        callsite->_failureReason=isInlineable;
        callsite->removeAllTargets(tracer(),isInlineable);
        if (comp()->getOption(TR_TraceBIIDTGen))
           {
           traceMsg(comp(), "not considering inlineable target\n");
           }
        return;
        }
     }
   for (int32_t i=0; i<callsite->numTargets(); i++)
      {
      TR_CallTarget *calltarget = callsite->getTarget(i);
      TR::ResolvedMethodSymbol * hello1 = calltarget->_calleeSymbol;
      char nameBuffer[1024];
      const char *calleeName = NULL;
      calleeName = comp()->fej9()->sampleSignature(calltarget->_calleeMethod->getPersistentIdentifier(), nameBuffer, 1024, comp()->trMemory());
      if (!supportsMultipleTargetInlining () && i > 0)
         {
         if (comp()->getOption(TR_TraceBIIDTGen))
           {
           traceMsg(TR::comp(), "not considering %s because exceeds byte code threshold\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
           }
         callsite->removecalltarget(i,tracer(),Exceeds_ByteCode_Threshold);
         i--;
         continue;
         }
      TR_ASSERT(calltarget->_guard, "assertion failure");
      if (!getPolicy()->canInlineMethodWhileInstrumenting(calltarget->_calleeMethod))
         {
         if (comp()->getOption(TR_TraceBIIDTGen))
           {
           traceMsg(TR::comp(), "not considering %s cannot inline while instrumenting\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
           }
         callsite->removecalltarget(i,tracer(),Needs_Method_Tracing);
         i--;
         continue;
         }
      bool toInline = getPolicy()->tryToInline(calltarget, callStack, true);
      if (toInline)
         {
         if (comp()->getOption(TR_TraceBIIDTGen))
            {
            traceMsg(comp(), "tryToInline pattern matched.  Skipping size check for %s\n", calltarget->_calleeMethod->signature(comp()->trMemory()));
            }
         callsite->tagcalltarget(i, tracer(), OverrideInlineTarget);
         }

      // only inline recursive calls once
      //
      static char *selfInliningLimitStr = feGetEnv("TR_selfInliningLimit");
      int32_t selfInliningLimit =
           selfInliningLimitStr ? atoi(selfInliningLimitStr)
         : comp()->getMethodSymbol()->doJSR292PerfTweaks()  ? 1 // JSR292 methods already do plenty of inlining
         : 3;
      if (callStack && callStack->isAnywhereOnTheStack(calltarget->_calleeMethod, selfInliningLimit))
         {
         if (comp()->getOption(TR_TraceBIIDTGen))
           {
           traceMsg(TR::comp(), "not considering %s recursive\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
           }
         debugTrace(tracer(),"Don't inline recursive call %p %s\n", calltarget, tracer()->traceSignature(calltarget));
         tracer()->insertCounter(Recursive_Callee,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),Recursive_Callee);
         i--;
         continue;
         }
      TR_InlinerFailureReason checkInlineableTarget = getPolicy()->checkIfTargetInlineable(calltarget, callsite, comp());

      if (checkInlineableTarget != InlineableTarget)
         {
         if (comp()->getOption(TR_TraceBIIDTGen))
           {
           traceMsg(TR::comp(), "not considering %s not inlineable\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
           }
         tracer()->insertCounter(checkInlineableTarget,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),checkInlineableTarget);
         i--;
         continue;
         }

      bool realGuard = false;
      if (comp()->getHCRMode() != TR::none)
         {
         if (calltarget->_guard->_kind != TR_HCRGuard)
            {
            if ((calltarget->_guard->_kind != TR_NoGuard) && (calltarget->_guard->_kind != TR_InnerGuard))
               realGuard = true;
            }
         }
      else
         {
         if ((calltarget->_guard->_kind != TR_NoGuard) && (calltarget->_guard->_kind != TR_InnerGuard))
            realGuard = true;
         }
      if (realGuard && (!inlineVirtuals() || comp()->getOption(TR_DisableVirtualInlining)))
         {
         if (comp()->getOption(TR_TraceBIIDTGen))
           {
           traceMsg(TR::comp(), "not considering %s virtual inlining disabled\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
           }
         tracer()->insertCounter(Virtual_Inlining_Disabled, callsite->_callNodeTreeTop);
         callsite->removecalltarget(i, tracer(), Virtual_Inlining_Disabled);
         i--;
         continue;
         }

      static const char * onlyVirtualInlining = feGetEnv("TR_OnlyVirtualInlining");
      if (comp()->getOption(TR_DisableNonvirtualInlining) && !realGuard)
         {
         if (comp()->getOption(TR_TraceBIIDTGen))
           {
           traceMsg(TR::comp(), "not considering %s non virtual inlining disabled\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
           }
         tracer()->insertCounter(NonVirtual_Inlining_Disabled,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),NonVirtual_Inlining_Disabled);
         i--;
         continue;
         }

      static const char * dontInlineSyncMethods = feGetEnv("TR_DontInlineSyncMethods");
      if (calltarget->_calleeMethod->isSynchronized() && (!inlineSynchronized() || comp()->getOption(TR_DisableSyncMethodInlining)))
         {
         if (comp()->getOption(TR_TraceBIIDTGen))
           {
           traceMsg(TR::comp(), "not considering %s dont inline sync methods\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
           }
         tracer()->insertCounter(Sync_Method_Inlining_Disabled,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),Sync_Method_Inlining_Disabled);
         i--;
         continue;
         }

      if (debug("dontInlineEHAware") && calltarget->_calleeMethod->numberOfExceptionHandlers() > 0)
         {
         if (comp()->getOption(TR_TraceBIIDTGen))
           {
           traceMsg(TR::comp(), "not considering %s dont inline ehaware\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
           }
         tracer()->insertCounter(EH_Aware_Callee,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),EH_Aware_Callee);
         i--;
         continue;
         }

      if (!callsite->_callerResolvedMethod->isStrictFP() && calltarget->_calleeMethod->isStrictFP())
         {
         if (comp()->getOption(TR_TraceBIIDTGen))
           {
           traceMsg(TR::comp(), "not considering %s dont inline strictFP\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
           }
         tracer()->insertCounter(StrictFP_Callee,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),StrictFP_Callee);

         i--;
         continue;
         }

      if (getPolicy()->tryToInline(calltarget, callStack, false))
         {
         if (comp()->getOption(TR_TraceBIIDTGen))
           {
           traceMsg(TR::comp(), "not considering %s dont inline\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
           }
         tracer()->insertCounter(DontInline_Callee,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),DontInline_Callee);
         i--;
         continue;
         }

         {
         TR::SimpleRegex * regex = comp()->getOptions()->getOnlyInline();
         if (regex && !TR::SimpleRegex::match(regex, calltarget->_calleeMethod))
            {
            if (comp()->getOption(TR_TraceBIIDTGen))
               {
               traceMsg(TR::comp(), "not considering %s dont inline\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
               }
            tracer()->insertCounter(Not_InlineOnly_Callee,callsite->_callNodeTreeTop);
            callsite->removecalltarget(i,tracer(),Not_InlineOnly_Callee);
            i--;
            continue;
            }
         }


         bool allowInliningColdCallSites = false;
         bool allowInliningColdTargets = false;
         TR::ResolvedMethodSymbol *caller = callStack->_methodSymbol;
         TR::CFG *cfg = callerCFG;
         TR_ASSERT_FATAL(cfg, "cfg is null");
         //int frequency1 = comp()->convertNonDeterministicInput(comp()->fej9()->getIProfilerCallCount(callsite->_bcInfo, comp()), MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT, randomGenerator(), 0);
         int frequency2 = comp()->convertNonDeterministicInput(callblock->getFrequency(), MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT, randomGenerator(), 0);
         bool isColdCall = cfg->isColdCall(callsite->_bcInfo, this);
         bool isCold = (isColdCall &&  (frequency2 <= MAX_COLD_BLOCK_COUNT));
         if (!allowInliningColdCallSites && isCold)
            {
            if (comp()->getOption(TR_TraceBIIDTGen))
                {
                traceMsg(TR::comp(), "not considering %s cold call\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
                }
            tracer()->insertCounter(DontInline_Callee,callsite->_callNodeTreeTop);
            callsite->removecalltarget(i,tracer(),DontInline_Callee);
            i--;
            continue;
            }


/*
         if (!allowInliningColdTargets && cfg->isColdTarget(callsite->_bcInfo, calltarget, this))
            {
            if (comp()->getOption(TR_TraceBIIDTGen))
                {
                traceMsg(TR::comp(), "not considering %s cold target\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
                }
            tracer()->insertCounter(DontInline_Callee,callsite->_callNodeTreeTop);
            callsite->removecalltarget(i,tracer(),DontInline_Callee);
            i--;
            continue;
            }
*/
         // TODO: This was sometimes not set, why?
         //calltarget->_calleeSymbol = calltarget->_calleeSymbol ? calltarget->_calleeSymbol : calltarget->_calleeMethod->findOrCreateJittedMethodSymbol(this->comp());
         if (!calltarget->_calleeSymbol)
            {
            if (comp()->getOption(TR_TraceBIIDTGen))
               {
               traceMsg(comp(), "unresolved callee\n");
               }
            callsite->removeAllTargets(tracer(),Unresolved_Callee);
            callsite->_failureReason=No_Inlineable_Targets;
            return;
            }

         TR_J9EstimateCodeSize *cfgGen = (TR_J9EstimateCodeSize*)TR_EstimateCodeSize::get(this, this->tracer(), 0);
         // From this moment on, we are on the realm of the experimental.
         // Why is that you may ask?
         // Well... the way the cfg is generated by the generatedCFG function is apparently distinct from the way the cfg is generated for every other way.
         // The generateCFG is a modified function based on J9EstimateCodeSize::realEstimateCodeSize
         // that was used for partial inlining (?)
         // anyhow... it produces a control flow graph and I saved it.
         // however, now is the question, what about the profiling information that is supposedly linked with the blocks in the CFG?
         // is that accurate?
         // I am not sure!
         TR::CFG *cfg2 = cfgGen->generateCFG(calltarget, callStack, this->_cfgRegion);
         //Not yet completely functionsl. Need some basic blocks...
         // now we have the call block
         TR_ASSERT_FATAL(cfg2 != nullptr, "cfg2 is null\n");
         cfg2->computeMethodBranchProfileInfo(this->getAbsEnvUtil(), calltarget, caller, this->_nodes, callblock, callerCFG);
         this->_nodes++;
         //Now the frequencies should have been set...
         //bool allowInliningColdTargets = false;
         if (!allowInliningColdTargets && callblock->getFrequency() <= 6)
            {
            if (comp()->getOption(TR_TraceBIIDTGen))
                {
                traceMsg(TR::comp(), "not considering %s my cold target\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
                }
            callsite->removecalltarget(i,tracer(),DontInline_Callee);
            i--;
            continue;
            }
         // the + 1 is to make sure that this in non zero.
         calltarget->_callRatioCallerCallee = ((float)callblock->getFrequency() / (float) cfg->getStartBlockFrequency());
      }
   return;
   }

OMR::AbsEnvInlinerUtil::AbsEnvInlinerUtil(TR::Compilation *c) : TR_J9InlinerUtil(c) {}

void
OMR::AbsEnvInlinerUtil::computeMethodBranchProfileInfo2(TR::Block *cfgBlock, TR_CallTarget * calltarget, TR::ResolvedMethodSymbol* callerSymbol, int callerIndex, TR::Block *callblock, TR::CFG * callerCFG)
   {
   if (cfgBlock) //isn't this equal to genILSucceeded?? Nope. cfgBlock comes from ecs
      {

      TR::ResolvedMethodSymbol * calleeSymbol = calltarget->_calleeSymbol;
      calleeSymbol->setFlowGraph(calltarget->_cfg);

      TR_MethodBranchProfileInfo *mbpInfo = TR_MethodBranchProfileInfo::getMethodBranchProfileInfo(callerIndex, comp());
      if (!mbpInfo)
         {

         mbpInfo = TR_MethodBranchProfileInfo::addMethodBranchProfileInfo (callerIndex, comp());
         //calleeSymbol->setFlowGraph(calltarget->_cfg);
         calleeSymbol->getFlowGraph()->computeInitialBlockFrequencyBasedOnExternalProfiler(comp());
         uint32_t firstBlockFreq = calleeSymbol->getFlowGraph()->getInitialBlockFrequency();
         //uint32_t firstBlockFreq = calleeSymbol->getFlowGraph()->getStartBlockFrequency(); //?
         //TODO: What is the difference between initialBlockFrequency and callerSymbol->getFirstTreeTop()->getNode()->getBlock()->getFrequency()

         int32_t blockFreq = callblock->getFrequency();
         if (blockFreq < 0)
            blockFreq = 6;

         float freqScaleFactor = 0.0;
         if (callerCFG->getStartBlockFrequency() > 0)
            {
            freqScaleFactor = (float)(blockFreq)/callerCFG->getStartBlockFrequency();
            if (callerCFG->getInitialBlockFrequency() > 0)
               freqScaleFactor *= (float)(callerCFG->getInitialBlockFrequency())/(float)firstBlockFreq;
              // freqScaleFactor *= (float)(callerSymbol->getFlowGraph()->getStartBlockFrequency())/(float)firstBlockFreq;
            }
         mbpInfo->setInitialBlockFrequency(firstBlockFreq);
         mbpInfo->setCallFactor(freqScaleFactor);

         calleeSymbol->getFlowGraph()->setFrequencies();

         if (comp()->getOption(TR_TraceBFGeneration))
            {
            traceMsg(comp(), "Setting initial block count for a call with index %d to be %d, call factor %f where block %d (%p) and blockFreq = %d\n", cfgBlock->getEntry()->getNode()->getInlinedSiteIndex(), firstBlockFreq, freqScaleFactor, callblock->getNumber(), callblock, blockFreq);
            }
         }
      }
   }

OMR::BenefitInlinerBase::BenefitInlinerBase(TR::Optimizer *optimizer, TR::Optimization *optimization) :
   TR_InlinerBase(optimizer, optimization),
   _cfgRegion(optimizer->comp()->region()),
   _callerIndex(-1),
   _nodes(0),
   _util2(NULL),
   _idt(NULL),
   _currentNode(NULL),
   _previousNode(NULL),
   _currentChild(NULL),
   _inliningProposal(NULL)
   {
      AbsEnvInlinerUtil *absEnvUtil = new (comp()->allocator()) AbsEnvInlinerUtil(this->comp());
      this->setAbsEnvUtil(absEnvUtil);
   }

OMR::BenefitInliner::BenefitInliner(TR::Optimizer *optimizer, TR::Optimization *optimization, uint32_t budget) : 
         BenefitInlinerBase(optimizer, optimization),
         _absEnvRegion(optimizer->comp()->region()),
         _callSitesRegion(optimizer->comp()->region()),
         _callStacksRegion(optimizer->comp()->region()),
         _holdingProposalRegion(optimizer->comp()->region()),
         _mapRegion(optimizer->comp()->region()),
         _IDTConstructorRegion(optimizer->comp()->region()),
         _inliningCallStack(NULL),
         _rootRms(NULL),
         _budget(budget),
         _methodSummaryMap(MethodSummaryMapComparator(), MethodSummaryMapAllocator(_mapRegion))
         {
         }

void
OMR::BenefitInlinerBase::updateBenefitInliner()
{
   this->_currentNode = this->_currentChild;
}

void
OMR::BenefitInlinerBase::popBenefitInlinerInformation()
{
  this->_currentNode = this->_currentNode->getParent();
}
