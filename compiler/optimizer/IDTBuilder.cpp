#include "optimizer/IDTBuilder.hpp"
#include "optimizer/AbsInterpreter.hpp"

IDTBuilder::IDTBuilder(TR::ResolvedMethodSymbol* symbol, int32_t budget, TR::Region& region, TR::Compilation* comp, OMR::BenefitInliner* inliner) :
      _rootSymbol(symbol),
      _rootBudget(budget),
      _region(region),
      _comp(comp),
      _inliner(inliner),
      _cfgGen(NULL),
      _idt(NULL),
      _valuePropagation(NULL),
      _util(NULL),
      _callSiteIndex(0),
      _interpretedMethodMap(InterpretedMethodMapComparator(), InterpretedMethodMapAllocator(region))
   {
   }

OMR::BenefitInlinerUtil* IDTBuilder::getUtil()
   {
   if (!_util)
      _util = new (comp()->allocator()) OMR::BenefitInlinerUtil(comp());
   return _util;
   }

TR::CFG* IDTBuilder::generateCFG(TR_CallTarget* callTarget, TR_CallStack* callStack)
   {
   if (!_cfgGen)
      {
      _cfgGen = (TR_J9EstimateCodeSize *)TR_EstimateCodeSize::get(getInliner(), getInliner()->tracer(), 0);   
      }

   TR::CFG* cfg = _cfgGen->generateCFG(callTarget, callStack, _region);
   return cfg;
   }

TR::ValuePropagation* IDTBuilder::getValuePropagation()
   {
   if (!_valuePropagation)
      {
      TR::OptimizationManager* manager = comp()->getOptimizer()->getOptimization(OMR::globalValuePropagation);
      _valuePropagation = (TR::ValuePropagation*) manager->factory()(manager);
      _valuePropagation->initialize();
      }
   return _valuePropagation;
   }
   
IDT* IDTBuilder::buildIDT()
   {
   bool traceBIIDTGen = comp()->getOption(TR_TraceBIIDTGen);

   if (traceBIIDTGen)
      traceMsg(comp(), "\n+ IDTBuilder: Start building IDT |\n\n");
   
   
   //root call target
   TR_ResolvedMethod* rootMethod = _rootSymbol->getResolvedMethod();
   TR_CallTarget *rootCallTarget = new (getRegion()) TR_CallTarget(
                                    NULL,
                                    _rootSymbol, 
                                    rootMethod, 
                                    NULL, 
                                    rootMethod->containingClass(), 
                                    NULL);
   
   //Initialize IDT
   _idt = new (getRegion()) IDT(getRegion(), _rootSymbol, rootCallTarget, _rootBudget, comp());
   IDTNode* root = _idt->getRoot();

   //add the decendants
   buildIDTHelper(root, NULL, -1, _rootBudget, NULL);

   if (traceBIIDTGen)
      traceMsg(comp(), "\n+ IDTBuilder: Finish building IDT |\n");

   return _idt;
   }

// The return value means if this node is going to be abstract interpreted.
bool IDTBuilder::buildIDTHelper(IDTNode* node, AbsState* invokeState, int callerIndex, int32_t budget, TR_CallStack* callStack)
   {
   // stop building IDT
   if (budget < 0)
      return false;

   TR::ResolvedMethodSymbol* symbol = node->getResolvedMethodSymbol();
   TR_ResolvedMethod* method = symbol->getResolvedMethod();
   
   bool recursiveCall = callStack && callStack->isAnywhereOnTheStack(method, 1);

   // stop for recursive call
   if (recursiveCall)
      return false;

   bool traceBIIDTGen = comp()->getOption(TR_TraceBIIDTGen);
   if (traceBIIDTGen)
      traceMsg(comp(), "+ IDTBuilder: Adding children for IDTNode: %s\n",node->getName(comp()->trMemory()));
   
   if (callStack == NULL) //This is the root method
      {
      TR::CFG* cfg = generateCFG(node->getCallTarget());
      cfg->computeInitialBlockFrequencyBasedOnExternalProfiler(comp());
      cfg->setFrequencies();
      symbol->setFlowGraph(cfg);
      cfg->getStartForReverseSnapshot()->setFrequency(cfg->getStartBlockFrequency());
      }
   else //cfg has already been set in computeCallRatio()
      {
      TR::CFG* cfg = node->getCallTarget()->_cfg;
      cfg->getStartForReverseSnapshot()->setFrequency(cfg->getStartBlockFrequency());
      }
   

   TR_CallStack* nextCallStack = new (getRegion()) TR_CallStack(comp(), symbol, method, callStack, budget, true);
   IDTNodeDeque idtNodeChildren(getRegion());

   IDTNode* interpretedMethodIDTNode = getInterpretedMethod(node->getResolvedMethodSymbol());
   
   if (interpretedMethodIDTNode)
      {
      //IDT is built in DFS, at this time, we have all the descendants so it is safe to copy the descendants
      _idt->copyDescendants(interpretedMethodIDTNode, node);
      return false;
      }
   else
      {
      performAbstractInterpretation(node, invokeState, callerIndex, nextCallStack, idtNodeChildren);
      addInterpretedMethod(node->getResolvedMethodSymbol(), node);
      }
   
   return true;
   }

//Perform abstract interpretation while building the IDT (adding children)
void IDTBuilder::performAbstractInterpretation(IDTNode* node, AbsState* invokeState, int callerIndex, TR_CallStack* callStack, IDTNodeDeque& idtNodeChildren)
   {
   AbsInterpreter interpreter(node, callerIndex, this, getValuePropagation(), callStack, &idtNodeChildren, getRegion(), comp());
   interpreter.interpret(invokeState);
   }

IDTNode* IDTBuilder::getInterpretedMethod(TR::ResolvedMethodSymbol* symbol)
   {
   TR_ResolvedMethod* method = symbol->getResolvedMethod();
   TR_OpaqueMethodBlock* persistentIdentifier = method->getPersistentIdentifier();

   auto iter = _interpretedMethodMap.find(persistentIdentifier);
   if (iter == _interpretedMethodMap.end()) //Not interpreted yet
      return NULL;
   
   return iter->second;
   }

void IDTBuilder::addInterpretedMethod(TR::ResolvedMethodSymbol* symbol, IDTNode* node)
   {
   TR_ResolvedMethod* method = symbol->getResolvedMethod();
   TR_OpaqueMethodBlock* persistentIdentifier = method->getPersistentIdentifier();

   _interpretedMethodMap.insert(std::pair<TR_OpaqueMethodBlock *, IDTNode *>(persistentIdentifier,node));
   }

void IDTBuilder::addChild(IDTNode*node, int callerIndex, TR_ResolvedMethod* containingMethod, AbsState* invokeState, int bcIndex, int cpIndex, TR::MethodSymbol::Kinds kind, TR_CallStack* callStack, IDTNodeDeque* idtNodeChildren, TR::Block* block, TR::CFG* callerCfg)
   {
   bool traceBIIDTGen = comp()->getOption(TR_TraceBIIDTGen);

   TR::ResolvedMethodSymbol* containingMethodSymbol = TR::ResolvedMethodSymbol::create(comp()->trHeapMemory(), containingMethod, comp());

   TR_CallSite* callsite = findCallSiteTargets(containingMethodSymbol, bcIndex, cpIndex, kind, callerIndex, callStack, block, callerCfg);
   
   if (!callsite || callsite->numTargets() == 0) 
      {
      AbsInterpreter::cleanInvokeState(containingMethod, cpIndex, invokeState, kind, getRegion(), comp());
      return;
      }
      
   
   //There should be only one call Target in callsite (disable multiTargetInlining)
   TR_CallTarget* callTarget = callsite->getTarget(0);

   TR::ResolvedMethodSymbol * resolvedMethodSymbol = TR::ResolvedMethodSymbol::create(comp()->trHeapMemory(), callTarget->_calleeMethod, comp());

   int remainingBudget = node->getBudget() - callTarget->_calleeMethod->maxBytecodeIndex();
   if (remainingBudget < 0 )
      {
      AbsInterpreter::cleanInvokeState(containingMethod, cpIndex, invokeState, kind, getRegion(), comp());
      return;
      }
      
   //At this point we have the callsite, next thing is to compute the call ratio of the call target.
   float callRatio = computeCallRatio(callTarget, callStack, block, callerCfg);

   IDTNode* child = node->addChild(
                           _idt->getNextGlobalIDTNodeIndex(),
                           kind,
                           callTarget,
                           callsite->_byteCodeIndex,
                           resolvedMethodSymbol,
                           callRatio,
                           _idt->getMemoryRegion()
                           );

   if (child == NULL) // Fail to add the Node to IDT
      {
      AbsInterpreter::cleanInvokeState(containingMethod, cpIndex, invokeState, kind, getRegion(),comp());   
      return;
      }
   
   //If successfully add this child to the IDT
   _idt->increaseGlobalIDTNodeIndex();

   if (!comp()->incInlineDepth(resolvedMethodSymbol, callsite->_bcInfo, callsite->_cpIndex,NULL, !callsite->isIndirectCall(),0))
      {
      AbsInterpreter::cleanInvokeState(containingMethod, cpIndex, invokeState, kind, getRegion(), comp());
      return;
      }

   bool toBeAbstractInterpreted = buildIDTHelper(child, invokeState, callerIndex + 1, child->getBudget(), callStack);

   if (!toBeAbstractInterpreted)
      AbsInterpreter::cleanInvokeState(containingMethod, cpIndex, invokeState, kind, getRegion(), comp());

   comp()->decInlineDepth(true); 

   if (traceBIIDTGen)
      traceMsg(comp(), "+ IDTBuilder: add child: %s to parent: %s\n",child->getName(comp()->trMemory()),node->getName(comp()->trMemory()));
      
   }

float IDTBuilder::computeCallRatio(TR_CallTarget* callTarget, TR_CallStack* callStack, TR::Block* block, TR::CFG* callerCfg)
   {
   TR_ASSERT_FATAL(callTarget, "Call Target is NULL!");

   TR::CFG* cfg = generateCFG(callTarget, callStack); //Now the callTarget has its own CFG
   TR::ResolvedMethodSymbol *caller = callStack->_methodSymbol;

   cfg->computeMethodBranchProfileInfo(getUtil(), callTarget, caller, _callSiteIndex++, block, callerCfg);

   return ((float)block->getFrequency() / (float) callerCfg->getStartBlockFrequency());  
   }

TR_CallSite* IDTBuilder::findCallSiteTargets(
      TR::ResolvedMethodSymbol *callerSymbol, 
      int bcIndex, int cpIndex,
      TR::MethodSymbol::Kinds kind, 
      int callerIndex, 
      TR_CallStack* callStack,
      TR::Block* block,
      TR::CFG* cfg
      )
   {
   TR_ByteCodeInfo *infoMem = new (getRegion()) TR_ByteCodeInfo();
   TR_ByteCodeInfo &info = *infoMem;

   TR_ResolvedMethod *caller = callerSymbol->getResolvedMethod();
   uint32_t methodSize = TR::Compiler->mtd.bytecodeSize(caller->getPersistentIdentifier());
   TR::SymbolReference *symRef = getSymbolReference(callerSymbol, cpIndex, kind);
   TR::Symbol *sym = symRef->getSymbol();
   bool isInterface = kind == TR::MethodSymbol::Kinds::Interface;

   if (symRef->isUnresolved() && !isInterface) 
      {
      //TODO: remove TR::comp()
      if (TR::comp()->getOption(TR_TraceBIIDTGen))
         {
         traceMsg(TR::comp(), "not considering: method is unresolved and is not interface\n");
         }
      return NULL;
      }

   TR::ResolvedMethodSymbol *calleeSymbol = !isInterface ? sym->castToResolvedMethodSymbol() : NULL;
   TR_ResolvedMethod *callee = !isInterface ? calleeSymbol->getResolvedMethod() : NULL;

   TR::Method *calleeMethod = !isInterface ? calleeSymbol->getMethod() : comp()->fej9()->createMethod(comp()->trMemory(), caller->containingClass(), cpIndex);
   info.setByteCodeIndex(bcIndex);
   info.setDoNotProfile(false);
   
   info.setCallerIndex(callerIndex);
   TR_OpaqueClassBlock *callerClass = caller->classOfMethod();
   TR_OpaqueClassBlock *calleeClass = callee ? callee->classOfMethod() : NULL;
   info.setIsSameReceiver(callerClass == calleeClass);
   
   bool isIndirect = kind == TR::MethodSymbol::Kinds::Static || TR::MethodSymbol::Kinds::Special;
   int32_t offset = kind == TR::MethodSymbol::Virtual ? symRef->getOffset() : -1;

   TR_CallSite *callsite = getCallSite
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
         comp(),
         -1,
         false,
         symRef
      );
   
   //TODO: Sometimes these were not set, why?
   callsite->_byteCodeIndex = bcIndex;
   callsite->_bcInfo = info; //info has to be a reference, so it is being deleted after node exits.
   callsite->_cpIndex= cpIndex;
   callsite->findCallSiteTarget(callStack, getInliner());

   //TODO: Sometimes these were not set, why?
   callStack->_methodSymbol = callStack->_methodSymbol ? callStack->_methodSymbol : callerSymbol;
   
   //This elimiates all the call targets that do not satisfy the inlining policy
   getInliner()->applyPolicyToTargets(callStack, callsite, block, cfg);
   return callsite;   
   }

TR_CallSite* IDTBuilder::getCallSite(TR::MethodSymbol::Kinds kind,
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
                                       bool allConsts,
                                       TR::SymbolReference *symRef)
   {
   TR::ResolvedMethodSymbol *rms = TR::ResolvedMethodSymbol::create(TR::comp()->trHeapMemory(), callerResolvedMethod, TR::comp());
   auto owningMethod = (TR_ResolvedJ9Method*) callerResolvedMethod;
   bool unresolvedInCP;
   if (kind == TR::MethodSymbol::Kinds::Virtual)
      {
      TR_ResolvedMethod *method = owningMethod->getResolvedPossiblyPrivateVirtualMethod(
         comp,
         cpIndex,
         /* ignoreRtResolve = */ false,
         &unresolvedInCP);

      bool opposite = !(method != NULL && method->isPrivate());
      TR::SymbolReference * somesymref = NULL;
      if (!opposite) 
         {
         somesymref = comp->getSymRefTab()->findOrCreateMethodSymbol(
         rms->getResolvedMethodIndex(),
         cpIndex,
         method,
         TR::MethodSymbol::Special,
         /* isUnresolvedInCP = */ false);
         }
      else 
         {
         somesymref = comp->getSymRefTab()->findOrCreateVirtualMethodSymbol(rms, cpIndex);
         if (!somesymref->isUnresolved())
            {
            method = somesymref->getSymbol()->castToResolvedMethodSymbol()->getResolvedMethod();
            }
         }
      if (method) 
         {
         kind = somesymref->getSymbol()->isFinal() ||
         method->isPrivate() ||
         (debug("omitVirtualGuard") && !method->virtualMethodIsOverridden()) ? TR::MethodSymbol::Kinds::Static : kind;
         }
      }

   
   if (initialCalleeMethod)
      {
      kind = symRef->getSymbol()->isFinal() ||
      initialCalleeMethod->isPrivate() ||
      (debug("omitVirtualGuard") && !initialCalleeMethod->virtualMethodIsOverridden()) ? TR::MethodSymbol::Kinds::Static : kind;
      }

   TR_CallSite *callsite = NULL;
   switch (kind) 
      {
      case TR::MethodSymbol::Kinds::Virtual:
         callsite = new (getRegion()) TR_J9VirtualCallSite(callerResolvedMethod, callNodeTreeTop, parent, callNode, interfaceMethod, receiverClass, vftSlot, cpIndex, initialCalleeMethod, initialCalleeSymbol, isIndirectCall, isInterface, bcInfo, comp, depth, allConsts);
         break;
      case TR::MethodSymbol::Kinds::Static:
      case TR::MethodSymbol::Kinds::Special:
         callsite = new (getRegion()) TR_DirectCallSite(callerResolvedMethod, callNodeTreeTop, parent, callNode, interfaceMethod, receiverClass, vftSlot, cpIndex, initialCalleeMethod, initialCalleeSymbol, isIndirectCall, isInterface, bcInfo, comp, depth, allConsts);
         break;
      case TR::MethodSymbol::Kinds::Interface:
         callsite = new (getRegion()) TR_J9InterfaceCallSite(callerResolvedMethod, callNodeTreeTop, parent, callNode, interfaceMethod, receiverClass, vftSlot, cpIndex, initialCalleeMethod, initialCalleeSymbol, isIndirectCall, isInterface, bcInfo, comp, depth, allConsts);
         break;
      }
   return callsite;
   }

TR::SymbolReference* IDTBuilder::getSymbolReference(TR::ResolvedMethodSymbol *callerSymbol, int cpIndex, TR::MethodSymbol::Kinds kind)
   {
   TR::SymbolReference *symRef = NULL;
   switch(kind)
      {
      case (TR::MethodSymbol::Kinds::Virtual):
         symRef = comp()->getSymRefTab()->findOrCreateVirtualMethodSymbol(callerSymbol, cpIndex);
         break;
      case (TR::MethodSymbol::Kinds::Static):
         symRef = comp()->getSymRefTab()->findOrCreateStaticMethodSymbol(callerSymbol, cpIndex);
         break;
      case (TR::MethodSymbol::Kinds::Interface):
         symRef = comp()->getSymRefTab()->findOrCreateInterfaceMethodSymbol(callerSymbol, cpIndex);
         break;
      case (TR::MethodSymbol::Kinds::Special):
         symRef = comp()->getSymRefTab()->findOrCreateSpecialMethodSymbol(callerSymbol, cpIndex);
         break;
      }
   return symRef;
   }