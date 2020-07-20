#include "compile/SymbolReferenceTable.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/OMRBlock.hpp"
#include "il/Block.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "infra/Cfg.hpp"
#include "infra/CfgNode.hpp"
#include "infra/ILWalk.hpp"
#include "compiler/env/PersistentCHTable.hpp"
#include "infra/SimpleRegex.hpp"
#include "optimizer/BenefitInliner.hpp"
#include "optimizer/MethodSummary.hpp"
#include "optimizer/J9EstimateCodeSize.hpp"
#include "optimizer/PriorityPreorder.hpp"
#include "optimizer/InlinerPacking.hpp"
#include "optimizer/InliningProposal.hpp"
#include "optimizer/IDTBuilder.hpp"



int32_t OMR::BenefitInlinerWrapper::perform()
   {
  
   TR::ResolvedMethodSymbol * sym = comp()->getMethodSymbol();
   //printf("======= %s\n",sym->signature(comp()->trMemory()));
   TR::CFG *prevCFG = sym->getFlowGraph();
   int32_t budget = getBudget(sym);
   if (budget < 0)
      return 1;

   OMR::BenefitInliner inliner(optimizer(), this, budget);

   inliner.buildIDT();

  
   if (inliner._idt->getNumNodes() == 1)
      {
      sym->setFlowGraph(prevCFG);
      return 1;
      }
      
   
//    if (budget < 0) return 1;
//    OMR::BenefitInliner inliner(optimizer(), this, budget);
//    inliner._rootRms = prevCFG;
//    inliner.initIDT(sym, budget);
//    if (comp()->trace(OMR::benefitInliner))
//      {
//      traceMsg(TR::comp(), "starting benefit inliner for %s, budget = %d, hotness = %s\n", sym->signature(this->comp()->trMemory()), budget, comp()->getHotnessName(comp()->getMethodHotness()));
//      }
   
//    inliner.obtainIDT(inliner._idt->getRoot(), budget);
//    inliner.traceIDT();
//    if (inliner._idt->getNumNodes() == 1) 
//       {
//       inliner._idt->getRoot()->getResolvedMethodSymbol()->setFlowGraph(inliner._rootRms);
//       return 1;
//       }
//    inliner._idt->getRoot()->getResolvedMethodSymbol()->setFlowGraph(inliner._rootRms);
    int recursiveCost = inliner._idt->getRoot()->getRecursiveCost();
    bool canSkipAbstractInterpretation = recursiveCost < budget;
    if (!canSkipAbstractInterpretation) {
      //  if (TR::comp()->getOption(TR_TraceAbstractInterpretation))
      //  {
      //  traceMsg(TR::comp(), "STARTXXX: about to start abstract interpretation\n");
      //  }
      // inliner.abstractInterpreter();
      //  if (TR::comp()->getOption(TR_TraceAbstractInterpretation))
      //  {
      //  traceMsg(TR::comp(), "ENDXXX: about to end abstract interpretation\n");
      //  }
      //inliner.updateIDT();
      inliner.analyzeIDT();
   } else {
      inliner.addEverything();
   }
    //restore the 'real' CFG since we changed it. 
   sym->setFlowGraph(prevCFG);
/*
   inliner._idt->getRoot()->getResolvedMethodSymbol()->setFlowGraph(inliner._rootRms);
*/ 
   inliner._idt->printTrace();
   inliner._currentNode = inliner._idt->getRoot();
   inliner.performInlining(sym);

   // while (inliner.inlinedNodes.empty() == false)
   //    {
   //    printf(" %d  ,", inliner.inlinedNodes.front());
   //    inliner.inlinedNodes.pop_front();
   //    }
   // printf("\n");

   return 1;
   }

void
OMR::BenefitInliner::addEverything() {
  _inliningProposal = new (this->_cfgRegion) InliningProposal(this->comp()->trMemory()->currentStackRegion(), this->_idt);
  this->addEverythingRecursively(this->_idt->getRoot());
  this->_inliningProposal->print(comp());
}

void
OMR::BenefitInliner::addEverythingRecursively(IDTNode *node)
   {
   TR_ASSERT(_inliningProposal, "we don't have an inlining proposal\n");
   _inliningProposal->pushBack(node);
   int children = node->getNumChildren();
   for (int i = 0; i < children; i++)
      {
      IDTNode *child = node->getChild(i);
      this->addEverythingRecursively(child);
      }
   }

void OMR::BenefitInliner::buildIDT()
   {
   IDTBuilder idtBuilder(comp()->getMethodSymbol(), _budget, comp()->trMemory()->currentStackRegion(), comp(), this);
   _idt = idtBuilder.buildIDT();
   }

bool
OMR::BenefitInlinerBase::inlineCallTargets(TR::ResolvedMethodSymbol *rms, TR_CallStack *prevCallStack, TR_InnerPreexistenceInfo *info)
{
   // inputs : IDT = all possible inlining candidates
   // TR::InliningProposal results = candidates selected for inlining
   // IDTNode = _currentNode being analyzed

   // if current node is in inlining proposal
   // at the beginning, current node is root, so it can't be inlined. So we need to iterate over its children to see if they are inlined...
   if (!this->_currentNode) {
      return false;
   }
   //printf("inlining into %s\n", this->_currentNode->getName());
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
OMR::BenefitInlinerBase::usedSavedInformation(TR::ResolvedMethodSymbol *rms, TR_CallStack *callStack, TR_InnerPreexistenceInfo *info, IDTNode *idtNode)
{     
      //level ++;
      //printf("level %d\n",level);
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
         IDTNode *child = idtNode->findChildWithBytecodeIndex(bcInfo.getByteCodeIndex());
         if (!child) continue;

         // is this IDT node in the proposal to inline?
         TR_ASSERT(_inliningProposal, "Inlining Proposal is NULL!\n");
         bool shouldBeInlined = _inliningProposal->inSet(child);
         if (!shouldBeInlined) continue;

         if (TR::comp()->trace(OMR::benefitInliner))
             {
             traceMsg(TR::comp(), "inlining node %d %s\n", child->getGlobalIndex(), child->getName());
             }

         IDTNode *prevChild = this->_currentChild;
         this->_currentChild = child;
         bool success = analyzeCallSite(callStack, tt, parent, node, child->getCallTarget());
         if (success)
             {
            //inlinedNodes.push_back(child->getGlobalIndex());
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
OMR::BenefitInliner::analyzeIDT()
   {
      if (this->_idt->getNumNodes() == 1) return; // No Need to analyze, since there is nothing to inline.
      _idt->buildIndices();
      PriorityPreorder items(this->_idt, this->comp());
      // traceMsg(TR::comp(), "Inlining Proposal: \n");
      // traceMsg(TR::comp(), "size = %d, budget = %d\n", items.size(), this->budget());
      Growable2dArray results(this->comp(), items.size(), this->budget() + 1, this);
      TR_ASSERT_FATAL(_idt, "IDT NULL");
      _inliningProposal = forwards_BitVectorImpl(this->budget(), items, &results, this->comp(), this, this->_idt);
      _inliningProposal->print(comp());
   }

int32_t
OMR::BenefitInlinerWrapper::getBudget(TR::ResolvedMethodSymbol *resolvedMethodSymbol)
   {
      int32_t size = resolvedMethodSymbol->getResolvedMethod()->maxBytecodeIndex();
      
      if (this->comp()->getMethodHotness() >= scorching)
      {
         //printf("size: %d\n",size);
         return std::max(1500, size * 2);
      }

      if (this->comp()->getMethodHotness() >= hot)
      {
         //printf("size: %d\n",size);
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
         
         TR_ASSERT_FATAL(callerCFG, "cfg is null");
         TR_ASSERT_FATAL(callblock, "block is null");
        
         int frequency = comp()->convertNonDeterministicInput(callblock->getFrequency(), MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT, randomGenerator(), 0);
         bool isColdCall = callerCFG->isColdCall(callsite->_bcInfo, this);

         bool isCold = (isColdCall &&  (frequency <= MAX_COLD_BLOCK_COUNT));
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
      }
   return;
   }

OMR::BenefitInlinerUtil::BenefitInlinerUtil(TR::Compilation *c) : TR_J9InlinerUtil(c) {}

void
OMR::BenefitInlinerUtil::computeMethodBranchProfileInfo2(TR::Block *cfgBlock, TR_CallTarget * calltarget, TR::ResolvedMethodSymbol* callerSymbol, int callerIndex, TR::Block *callblock, TR::CFG * callerCFG)
   {
   if (cfgBlock) //isn't this equal to genILSucceeded?? Nope. cfgBlock comes from ecs
      {

      TR::ResolvedMethodSymbol * calleeSymbol = calltarget->_calleeSymbol;
      calleeSymbol->setFlowGraph(calltarget->_cfg);

      TR_MethodBranchProfileInfo *mbpInfo = TR_MethodBranchProfileInfo::getMethodBranchProfileInfo(callerIndex, comp());
      if (!mbpInfo)
         {

         mbpInfo = TR_MethodBranchProfileInfo::addMethodBranchProfileInfo (callerIndex, comp());
         calleeSymbol->getFlowGraph()->computeInitialBlockFrequencyBasedOnExternalProfiler(comp());
         uint32_t firstBlockFreq = calleeSymbol->getFlowGraph()->getInitialBlockFrequency();
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
      BenefitInlinerUtil *absEnvUtil = new (comp()->allocator()) BenefitInlinerUtil(this->comp());
      this->setAbsEnvUtil(absEnvUtil);
   }

OMR::BenefitInliner::BenefitInliner(TR::Optimizer *optimizer, TR::Optimization *optimization, uint32_t budget) : 
         BenefitInlinerBase(optimizer, optimization),
         _holdingProposalRegion(comp()->trMemory()->heapMemoryRegion()),
         _idtRegion(comp()->trMemory()->heapMemoryRegion()),
         _rootRms(NULL),
         _budget(budget)
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
