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
#include "optimizer/InlinerPacking.hpp"
#include "optimizer/InliningProposal.hpp"
#include "optimizer/IDTBuilder.hpp"
#include "optimizer/IDT.hpp"


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
      
   
    int recursiveCost = inliner._idt->getRoot()->getRecursiveCost();
    bool canSkipAbstractInterpretation = recursiveCost < budget;

   

    if (!canSkipAbstractInterpretation) {
     
       inliner.inlinerPacking();
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
   _inliningProposal->addNode(node);
   int children = node->getNumChildren();
   for (int i = 0; i < children; i++)
      {
      IDTNode *child = node->getChild(i);
      this->addEverythingRecursively(child);
      }
   }

void OMR::BenefitInliner::buildIDT()
   {
   IDTBuilder idtBuilder(comp()->getMethodSymbol(), _budget, _cfgRegion, comp(), this);
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
   TR_InlinerBase::getSymbolAndFindInlineTargets(callStack,callsite);

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

void OMR::BenefitInliner::inlinerPacking()
   {
   _idt->buildIndices();

   unsigned int idtSize = _idt->getNumNodes();
   unsigned int budget = _budget;

   //initialize InliningProposal Table (idtSize x budget)
   InliningProposalTable table(idtSize, budget + 1, comp()->trMemory()->currentStackRegion());

   IDTPreorderPriorityQueue preorderPQueue(_idt, comp()->trMemory()->currentStackRegion()); 
   for (unsigned int row = 0; row < idtSize; row ++)
      {
      for (unsigned int col = 1; col < budget + 1; col ++)
         {
         InliningProposal currentSet(comp()->trMemory()->currentStackRegion(), _idt); // []
         IDTNode* currentNode = preorderPQueue.get(row); 
         
         currentSet.addNode(currentNode); //[ currentNode ]
         
         int offsetRow = row - 1;

         while (!currentNode->isRoot() 
            && !table.get(offsetRow, col-currentSet.getCost())->inSet(currentNode->getParent()))
            {
            currentSet.addNode(currentNode->getParent());
            currentNode = currentNode->getParent();
            }

         while ( currentSet.intersects(table.get(offsetRow, col - currentSet.getCost()))
            || ( !(currentNode->getParent() && table.get(offsetRow, col - currentSet.getCost())->inSet(currentNode->getParent()) )
                  && !table.get(offsetRow, col - currentSet.getCost())->isEmpty() 
                  ))
            {
            offsetRow--;
            }

         InliningProposal* newProposal = new (comp()->trMemory()->currentStackRegion()) InliningProposal(comp()->trMemory()->currentStackRegion(), _idt);
         newProposal->unionInPlace(*table.get(offsetRow, col - currentSet.getCost()), currentSet);

         if (newProposal->getCost() <= col && newProposal->getBenefit() > table.get(row-1, col)->getBenefit()) //only set the new proposal if it fits the budget and has more benefits
            table.set(row, col, newProposal);
         else
            table.set(row, col, table.get(row-1, col));
         }
      }
   
   InliningProposal* result = new (_cfgRegion) InliningProposal(_cfgRegion, _idt);
   result->unionInPlace(*result, *table.get(idtSize-1, budget));
   traceMsg(comp(), "inliner packing\n");
   result->print(comp());

   _inliningProposal = result;
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

      if (!calltarget->_calleeSymbol)
            {
            if (comp()->getOption(TR_TraceBIIDTGen))
               {
               traceMsg(comp(), "unresolved callee\n");
               }
            callsite->removecalltarget(i,tracer(),Unresolved_Callee);
            i--;
            continue;
            }

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

         //if we have the block and callerCFG
         if (!callerCFG || !callblock)
            continue;

         bool allowInliningColdCallSites = false;
         bool allowInliningColdTargets = false;
        
         int frequency = comp()->convertNonDeterministicInput(callblock->getFrequency(), MAX_BLOCK_COUNT + MAX_COLD_BLOCK_COUNT, randomGenerator(), 0);
         bool isColdCall = callerCFG->isColdCall(callsite->_bcInfo, this);

         bool isCold = (isColdCall &&  (frequency < MAX_COLD_BLOCK_COUNT));
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

         
         if (!allowInliningColdTargets && callblock->getFrequency() < 6)
            {
            if (comp()->getOption(TR_TraceBIIDTGen))
                {
                traceMsg(TR::comp(), "not considering %s cold target\n", calltarget->_calleeMethod->signature(this->comp()->trMemory()));
                }
            callsite->removecalltarget(i,tracer(),DontInline_Callee);
            i--;
            continue;
            }
      }
   return;
   }

OMR::BenefitInlinerUtil::BenefitInlinerUtil(TR::Compilation *c) : TR_J9InlinerUtil(c) {}


//Is this correct?
void OMR::BenefitInlinerUtil::computeMethodBranchProfileInfo2(TR_CallTarget * calltarget, TR::ResolvedMethodSymbol* callerSymbol, int callsiteIndex, TR::Block *callBlock, TR::CFG * callerCFG)
   {
   TR::ResolvedMethodSymbol * calleeSymbol = calltarget->_calleeSymbol;

   TR_MethodBranchProfileInfo *mbpInfo = TR_MethodBranchProfileInfo::getMethodBranchProfileInfo(callsiteIndex, comp());
   if (!mbpInfo)
      {
      mbpInfo = TR_MethodBranchProfileInfo::addMethodBranchProfileInfo (callsiteIndex, comp());
      calleeSymbol->getFlowGraph()->computeInitialBlockFrequencyBasedOnExternalProfiler(comp());
      uint32_t firstBlockFreq = calleeSymbol->getFlowGraph()->getInitialBlockFrequency();

      int32_t blockFreq = callBlock->getFrequency();
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
