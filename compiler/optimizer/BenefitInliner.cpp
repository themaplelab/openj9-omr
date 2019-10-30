#include "compile/SymbolReferenceTable.hpp"
#include "il/SymbolReference.hpp"
#include "il/symbol/ResolvedMethodSymbol.hpp"
#include "il/OMRBlock.hpp"
#include "il/Block.hpp"
#include "ilgen/J9ByteCodeIterator.hpp"
#include "infra/Cfg.hpp"
#include "infra/CfgNode.hpp" // for CFGNode
#include "infra/ILWalk.hpp"
#include "optimizer/BenefitInliner.hpp"
#include "optimizer/J9CallGraph.hpp"
#include "optimizer/J9EstimateCodeSize.hpp"


int32_t OMR::BenefitInlinerWrapper::perform()
   {
   TR::ResolvedMethodSymbol * sym = comp()->getMethodSymbol();
   int32_t budget = this->getBudget(sym);
   if (budget < 0) return -1;

   OMR::BenefitInliner inliner(optimizer(), this, budget);
   inliner.initIDT(sym);
   inliner.obtainIDT(sym, budget);
   inliner.performInlining(sym);
   inliner.traceIDT();
   return 1;
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

void OMR::BenefitInliner::initIDT(TR::ResolvedMethodSymbol *root)
   {
   _idt = new (comp()->trMemory()->currentStackRegion()) IDT(this, &comp()->trMemory()->currentStackRegion(), root);
   }

void OMR::BenefitInliner::traceIDT()
   {
   _idt->printTrace();
   }

void
OMR::BenefitInliner::obtainIDT(TR_CallSite *callsite, int32_t budget, TR_ByteCodeInfo &info, int cpIndex)
   {
      if (!callsite) return;
      this->_callerIndex++;
      for (int i = 0; i < callsite->numTargets(); i++)
         {
         TR_CallTarget *callTarget = callsite->getTarget(i);
         TR::ResolvedMethodSymbol *resolvedMethodSymbol = callTarget->_calleeSymbol ? callTarget->_calleeSymbol : callTarget->_calleeMethod->findOrCreateJittedMethodSymbol(this->comp());
         int oldBudget = budget;
         budget = budget - resolvedMethodSymbol->getResolvedMethod()->maxBytecodeIndex();
         // Ensures that there is a decrease in the budget
         if (oldBudget == budget) {
            budget--;
         }
         callTarget->_calleeSymbol = resolvedMethodSymbol;
         callTarget->_myCallSite = callsite;
         if (!comp()->incInlineDepth(resolvedMethodSymbol, callsite->_bcInfo, callsite->_cpIndex, NULL, !callTarget->_myCallSite->isIndirectCall(), 0)) continue;

         bool added = _idt->addToCurrentChild(callsite->_byteCodeIndex, resolvedMethodSymbol);
         if (added) 
            {
            this->obtainIDT(resolvedMethodSymbol, budget);
            _idt->popCurrent();
            }
            comp()->decInlineDepth(true);
            budget = oldBudget;
         }
      this->_callerIndex--;
   }

void
OMR::BenefitInliner::obtainIDT(TR::ResolvedMethodSymbol *resolvedMethodSymbol, int32_t budget)
   {
      if (budget < 0) return;


      TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
      TR_CallStack *prevCallStack = this->_inliningCallStack;
      TR::CFG *cfg = NULL;
      TR::CFG *prevCFG = resolvedMethodSymbol->getFlowGraph();
      int32_t maxBytecodeIndex = resolvedMethodSymbol->getResolvedMethod()->maxBytecodeIndex();
      if (!prevCallStack)
         {
         TR_CallTarget *calltarget = new (this->_callStacksRegion) TR_CallTarget(NULL, resolvedMethodSymbol, resolvedMethod, NULL, resolvedMethod->containingClass(), NULL);
         /* TODO: use new interface
         CFGgenerator *cfgGen = new (comp()->allocator()) CFGgenerator();
         cfgGen = (CFGgenerator*)TR_EstimateCodeSize::get(this, this->tracer(), 0);
         cfg = cfgGen->generateCFG(calltarget, NULL);
         resolvedMethodSymbol->setFlowGraph(prevCFG);
         */
         } 
      else 
         {
         cfg = resolvedMethodSymbol->getFlowGraph();
         if (this->_inliningCallStack->isAnywhereOnTheStack(resolvedMethod, 1)) 
            {
            return;
            }
         }


      this->_inliningCallStack = new (this->_callStacksRegion) TR_CallStack(this->comp(), resolvedMethodSymbol, resolvedMethod, prevCallStack, budget);

      for (TR::ReversePostorderSnapshotBlockIterator blockIt (cfg->getStart()->asBlock(), comp()); blockIt.currentBlock(); ++blockIt)
         {
         TR::Block *block = blockIt.currentBlock();
         this->obtainIDT(resolvedMethodSymbol, block, budget);
         }
      
       this->_inliningCallStack = prevCallStack;
   }

void
OMR::BenefitInliner::obtainIDT(TR::ResolvedMethodSymbol *resolvedMethodSymbol, TR::Block *block, int budget)
   {

      TR_ResolvedMethod *resolvedMethod = resolvedMethodSymbol->getResolvedMethod();
      TR_ResolvedJ9Method *resolvedJ9Method = static_cast<TR_ResolvedJ9Method*>(resolvedMethod);
      TR_J9VMBase *vm = static_cast<TR_J9VMBase*>(this->comp()->fe());
      TR_J9ByteCodeIterator bci(resolvedMethodSymbol, resolvedJ9Method, vm, this->comp());
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
            TR_ByteCodeInfo bcInfo;
            bool isInterface = kind == TR::MethodSymbol::Kinds::Interface;
            TR_CallSite *callsite = this->findCallSiteTarget(resolvedMethodSymbol, bci.currentByteCodeIndex(), bci.next2Bytes(), kind, bcInfo, block);
            this->obtainIDT(callsite, budget, bcInfo, bci.next2Bytes());
            }
         }
   }

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
OMR::BenefitInliner::findCallSiteTarget(TR::ResolvedMethodSymbol *callerSymbol, int bcIndex, int cpIndex, TR::MethodSymbol::Kinds kind, TR_ByteCodeInfo &info, TR::Block *block)
   {
      TR_ResolvedMethod *caller = callerSymbol->getResolvedMethod();
      uint32_t methodSize = TR::Compiler->mtd.bytecodeSize(caller->getPersistentIdentifier());
      TR::SymbolReference *symRef = this->getSymbolReference(callerSymbol, cpIndex, kind);
      TR::Symbol *sym = symRef->getSymbol();
      bool isInterface = kind == TR::MethodSymbol::Kinds::Interface;
      if (symRef->isUnresolved() && !isInterface) return NULL;
      
      TR::ResolvedMethodSymbol *calleeSymbol = !isInterface ? sym->castToResolvedMethodSymbol() : NULL;
      TR_ResolvedMethod *callee = !isInterface ? calleeSymbol->getResolvedMethod() : NULL;
      TR_Method *calleeMethod = !isInterface ? calleeSymbol->getMethod() : this->comp()->fej9()->createMethod(this->comp()->trMemory(), caller->containingClass(), cpIndex);

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

      applyPolicyToTargets(this->_inliningCallStack, callsite, block);
      return callsite;
   }

TR_CallSite *
OMR::BenefitInliner::getCallSite(TR::MethodSymbol::Kinds kind,
                                    TR_ResolvedMethod *callerResolvedMethod,
                                    TR::TreeTop *callNodeTreeTop,
                                    TR::Node *parent,
                                    TR::Node *callNode,
                                    TR_Method * interfaceMethod,
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

inline const uint32_t
OMR::BenefitInliner::budget() const
   {
      return this->_budget;
   }

void
OMR::BenefitInlinerBase::applyPolicyToTargets(TR_CallStack *callStack, TR_CallSite *callsite, TR::Block *callblock)
   {
   for (int32_t i=0; i<callsite->numTargets(); i++)
      {
      TR_CallTarget *calltarget = callsite->getTarget(i);

      if (!supportsMultipleTargetInlining () && i > 0)
         {
         callsite->removecalltarget(i,tracer(),Exceeds_ByteCode_Threshold);
         i--;
         continue;
         }

      TR_ASSERT(calltarget->_guard, "assertion failure");
      if (!getPolicy()->canInlineMethodWhileInstrumenting(calltarget->_calleeMethod))
         {
         callsite->removecalltarget(i,tracer(),Needs_Method_Tracing);
         i--;
         continue;
         }

      bool toInline = getPolicy()->tryToInline(calltarget, callStack, true);
      if (toInline)
         {
         if (comp()->trace(OMR::inlining))
            traceMsg(comp(), "tryToInline pattern matched.  Skipping size check for %s\n", calltarget->_calleeMethod->signature(comp()->trMemory()));
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
         debugTrace(tracer(),"Don't inline recursive call %p %s\n", calltarget, tracer()->traceSignature(calltarget));
         tracer()->insertCounter(Recursive_Callee,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),Recursive_Callee);
         i--;
         continue;
         }

      TR_InlinerFailureReason checkInlineableTarget = getPolicy()->checkIfTargetInlineable(calltarget, callsite, comp());

      if (checkInlineableTarget != InlineableTarget)
         {
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
         tracer()->insertCounter(Virtual_Inlining_Disabled, callsite->_callNodeTreeTop);
         callsite->removecalltarget(i, tracer(), Virtual_Inlining_Disabled);
         i--;
         continue;
         }

      static const char * onlyVirtualInlining = feGetEnv("TR_OnlyVirtualInlining");
      if (comp()->getOption(TR_DisableNonvirtualInlining) && !realGuard)
         {
         tracer()->insertCounter(NonVirtual_Inlining_Disabled,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),NonVirtual_Inlining_Disabled);
         i--;
         continue;
         }

      static const char * dontInlineSyncMethods = feGetEnv("TR_DontInlineSyncMethods");
      if (calltarget->_calleeMethod->isSynchronized() && (!inlineSynchronized() || comp()->getOption(TR_DisableSyncMethodInlining)))
         {
         tracer()->insertCounter(Sync_Method_Inlining_Disabled,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),Sync_Method_Inlining_Disabled);
         i--;
         continue;
         }

      if (debug("dontInlineEHAware") && calltarget->_calleeMethod->numberOfExceptionHandlers() > 0)
         {
         tracer()->insertCounter(EH_Aware_Callee,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),EH_Aware_Callee);
         i--;
         continue;
         }

      if (!callsite->_callerResolvedMethod->isStrictFP() && calltarget->_calleeMethod->isStrictFP())
         {
         tracer()->insertCounter(StrictFP_Callee,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),StrictFP_Callee);

         i--;
         continue;
         }

      if (getPolicy()->tryToInline(calltarget, callStack, false))
         {
         tracer()->insertCounter(DontInline_Callee,callsite->_callNodeTreeTop);
         callsite->removecalltarget(i,tracer(),DontInline_Callee);
         i--;
         continue;
         }

         {
         TR::SimpleRegex * regex = comp()->getOptions()->getOnlyInline();
         if (regex && !TR::SimpleRegex::match(regex, calltarget->_calleeMethod))
            {
            tracer()->insertCounter(Not_InlineOnly_Callee,callsite->_callNodeTreeTop);
            callsite->removecalltarget(i,tracer(),Not_InlineOnly_Callee);
            i--;
            continue;
            }
         }


         bool allowInliningColdCallSites = false;
         TR::ResolvedMethodSymbol *caller = callStack->_methodSymbol;
         TR::CFG *cfg = caller->getFlowGraph();
         if (!allowInliningColdCallSites && cfg->isColdCall(callsite->_bcInfo, this))
            {
            tracer()->insertCounter(DontInline_Callee,callsite->_callNodeTreeTop);
            callsite->removecalltarget(i,tracer(),DontInline_Callee);
            i--;
            continue;
            }


         bool allowInliningColdTargets = false;
         if (!allowInliningColdTargets && cfg->isColdTarget(callsite->_bcInfo, calltarget, this))
            {
            tracer()->insertCounter(DontInline_Callee,callsite->_callNodeTreeTop);
            callsite->removecalltarget(i,tracer(),DontInline_Callee);
            i--;
            continue;
            }


         /* TODO: Dependent on interface to generate CFG */
         /*
         CFGgenerator *cfgGen = new (comp()->allocator()) CFGgenerator();
         cfgGen = (CFGgenerator*)TR_EstimateCodeSize::get(this, this->tracer(), 0);
         TR::CFG *cfg2 = cfgGen->generateCFG(calltarget, callStack);
         //Not yet completely functionsl. Need some basic blocks...
         // now we have the call block
         cfg2->computeMethodBranchProfileInfo(this->getAbsEnvUtil(), calltarget, caller, this->_nodes, callblock);
         this->_nodes++;
         //Now the frequencies should have been set...
         */
      }

   return;
   }

OMR::AbsEnvInlinerUtil::AbsEnvInlinerUtil(TR::Compilation *c) : TR_J9InlinerUtil(c) {}

OMR::BenefitInlinerBase::BenefitInlinerBase(TR::Optimizer *optimizer, TR::Optimization *optimization) :
   TR_InlinerBase(optimizer, optimization),
   _callerIndex(-1),
   _nodes(0),
   _util2(NULL)
   {
      AbsEnvInlinerUtil *absEnvUtil = new (comp()->allocator()) AbsEnvInlinerUtil(this->comp());
      this->setAbsEnvUtil(absEnvUtil);
   }

OMR::BenefitInliner::BenefitInliner(TR::Optimizer *optimizer, TR::Optimization *optimization, uint32_t budget) : 
         BenefitInlinerBase(optimizer, optimization),
         _callSitesRegion(optimizer->comp()->region()),
         _callStacksRegion(optimizer->comp()->region()),
         _inliningCallStack(NULL),
         _budget(budget)
         {
         }
