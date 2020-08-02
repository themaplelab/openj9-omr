#include "optimizer/IDTBuilder.hpp"
#include "optimizer/AbsInterpreter.hpp"

IDTBuilder::IDTBuilder(TR::ResolvedMethodSymbol* symbol, int32_t budget, TR::Region& region, TR::Compilation* comp, OMR::BenefitInliner* inliner) :
      _rootSymbol(symbol),
      _rootBudget(budget),
      _region(region),
      _comp(comp),
      _inliner(inliner),
      _idt(NULL),
      _interpretedMethodMap(InterpretedMethodMapComparator(), InterpretedMethodMapAllocator(region))
   {
   }

//The CFG is generated from EstimateCodeSize. It is different from a normal CFG. 
TR::CFG* IDTBuilder::generateFlowGraph(TR_CallTarget* callTarget, TR::Region& region, TR_CallStack* callStack)
   {
   TR_J9EstimateCodeSize* cfgGen = (TR_J9EstimateCodeSize *)TR_EstimateCodeSize::get(getInliner(), getInliner()->tracer(), 0);   
   TR::CFG* cfg = cfgGen->generateCFG(callTarget, callStack, region);
   callTarget->_calleeSymbol->setFlowGraph(cfg);
   return cfg;
   }


IDT* IDTBuilder::buildIDT()
   {
   bool traceBIIDTGen = comp()->getOption(TR_TraceBIIDTGen);

   if (traceBIIDTGen)
      traceMsg(comp(), "\n+ IDTBuilder: Start building IDT |\n\n");
   
   //root call target
   TR_ResolvedMethod* rootMethod = _rootSymbol->getResolvedMethod();

   TR_CallTarget *rootCallTarget = new (region()) TR_CallTarget(
                                    NULL,
                                    _rootSymbol, 
                                    rootMethod, 
                                    NULL, 
                                    rootMethod->containingClass(), 
                                    NULL);
   
   //Initialize IDT
   _idt = new (region()) IDT(region(), _rootSymbol, rootCallTarget, _rootBudget, comp());
   
   IDTNode* root = _idt->getRoot();

   //generate the CFG for root call target 
   TR::CFG* cfg = generateFlowGraph(rootCallTarget, region());

   if (!cfg) //Fail to generate a CFG
      return _idt;

   cfg->setFrequencies();
   cfg->setStartBlockFrequency();
   //setCFGBlockFrequency(rootCallTarget, true);
   
   //add the decendants
   buildIDTHelper(root, NULL, -1, _rootBudget, NULL);

   if (traceBIIDTGen)
      traceMsg(comp(), "\n+ IDTBuilder: Finish building IDT |\n");

   return _idt;
   }

void IDTBuilder::buildIDTHelper(IDTNode* node, AbsParameterArray* parameterArray, int callerIndex, int32_t budget, TR_CallStack* callStack)
   {
   TR::ResolvedMethodSymbol* symbol = node->getResolvedMethodSymbol();
   TR_ResolvedMethod* method = symbol->getResolvedMethod();
   
   bool traceBIIDTGen = comp()->getOption(TR_TraceBIIDTGen);

   TR_CallStack* nextCallStack = new (region()) TR_CallStack(comp(), symbol, method, callStack, budget, true);

   IDTNode* interpretedMethodIDTNode = getInterpretedMethod(node->getResolvedMethodSymbol());
   
   if (interpretedMethodIDTNode)//If this method has been already interpreted
      {
      node->setMethodSummary(interpretedMethodIDTNode->getMethodSummary()); //since they are the same method, they should have the same method summary. 
      int staticBenefit = computeStaticBenefitWithMethodSummary(node->getMethodSummary(), parameterArray);

      node->setStaticBenefit(staticBenefit);
      _idt->copyDescendants(interpretedMethodIDTNode, node); //IDT is built in DFS order, at this time, we have all the descendants so it is safe to copy the descendants
      return;
      }
   else
      {
      performAbstractInterpretation(node, callerIndex, nextCallStack);

      //At this point we have the method summary generated by abstract interpretation
      if (!node->isRoot())
         {
         int staticBenefit = computeStaticBenefitWithMethodSummary(node->getMethodSummary(), parameterArray);
         node->setStaticBenefit(staticBenefit); 
         }
      
      addInterpretedMethod(node->getResolvedMethodSymbol(), node); //store this method to already interpreted method map
      }
   
   return;
   }

//Abstract interpetation = Walk the bytecode + identifying invoke bytecode + update AbsState + generate method summary
void IDTBuilder::performAbstractInterpretation(IDTNode* node, int callerIndex, TR_CallStack* callStack)
   {
   AbsInterpreter interpreter(node, callerIndex, this, callStack, region(), comp());
   bool success = interpreter.interpret();
   if (!success)
      {
      if (comp()->getOption(TR_TraceAbstractInterpretation))
         traceMsg(comp(), "Fail to interpret method: %s\n", node->getName());
      }
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

void IDTBuilder::addChild(IDTNode*node, int callerIndex, TR_CallSite* callSite, AbsParameterArray* parameterArray, TR_CallStack* callStack, TR::Block* block)
   {
   bool traceBIIDTGen = comp()->getOption(TR_TraceBIIDTGen);

   if (callSite == NULL)
      {
      if (traceBIIDTGen)
         traceMsg(comp(), "Do not have a callsite. Don't add\n");
      return;
      }
   
   if (block->getFrequency() <= 0) 
      {
      if (traceBIIDTGen)
         traceMsg(comp(), "Block frequency error. Don't add\n");
      return;
      }

   getInliner()->applyPolicyToTargets(callStack, callSite); // eliminate call targets that are not inlinable thus they won't be added to IDT 

   if (callSite->numTargets() == 0) 
      {
      if (traceBIIDTGen)
         traceMsg(comp(), "Do not have a call target. Don't add\n");
      return;
      }
      
      
   for (int i = 0 ; i < callSite->numTargets(); i++)
      {
      TR_CallTarget* callTarget = callSite->getTarget(0);
      
      int remainingBudget = node->getBudget() - callTarget->_calleeMethod->maxBytecodeIndex();
      if (remainingBudget < 0 ) // no budget remains, stop building IDT.
         {
         if (traceBIIDTGen)
            traceMsg(comp(), "No budget left. Don't add\n");
         continue;
         }

      bool isRecursiveCall = callStack->isAnywhereOnTheStack(callTarget->_calleeMethod, 1);
      if (isRecursiveCall) //Stop for recursive call
         {
         if (traceBIIDTGen)
            traceMsg(comp(), "Recursive call. Don't add\n");  
         continue;
         }
         
      
      TR::ResolvedMethodSymbol * calleeMethodSymbol = TR::ResolvedMethodSymbol::create(comp()->trHeapMemory(), callTarget->_calleeMethod, comp());

      //generate the CFG of this call target and set the block frequencies. 
      TR::CFG* cfg = generateFlowGraph(callTarget, region(), callStack);

      if (!cfg)
         {
         if (traceBIIDTGen)
            traceMsg(comp(), "Fail to generate a CFG. Don't add\n");  
         continue;
         }
         
      cfg->setFrequencies();
      cfg->setStartBlockFrequency();

      //setCFGBlockFrequency(callTarget, false, callStack, block, node->getCallTarget()->_cfg);
      //printf("compute %s\n",callTarget->signature(comp()->trMemory()));
      float callRatio = computeCallRatio(block, node->getCallTarget()->_cfg);

      if (traceBIIDTGen)
         traceMsg(comp(), "+ IDTBuilder: Adding Child %s for IDTNode: %s\n",calleeMethodSymbol->signature(comp()->trMemory()), node->getName(comp()->trMemory()));

      IDTNode* child = node->addChild(
                              _idt->getNextGlobalIDTNodeIndex(),
                              callTarget,
                              callSite->_byteCodeIndex,
                              calleeMethodSymbol,
                              callRatio,
                              _idt->getMemoryRegion()
                              );
      if (traceBIIDTGen)
         traceMsg(comp(), child != NULL ? "success\n" : "fail\n");
      
      if (child == NULL) // Fail to add the Node to IDT
         continue;
      
      //If successfully add this child to the IDT
      _idt->increaseGlobalIDTNodeIndex();

      if (!comp()->incInlineDepth(calleeMethodSymbol, callSite->_bcInfo, callSite->_cpIndex, NULL, !callSite->isIndirectCall(), 0))
         continue;
         
      // Build the IDT recursively
      buildIDTHelper(child, parameterArray, callerIndex + 1, child->getBudget(), callStack);

      comp()->decInlineDepth(true); 
         
      }
   }
   

float IDTBuilder::computeCallRatio(TR::Block* block, TR::CFG* callerCfg)
   {
   return ((float)block->getFrequency() / (float) callerCfg->getStartBlockFrequency());  
   }

int IDTBuilder::computeStaticBenefitWithMethodSummary(MethodSummary* methodSummary, AbsParameterArray* parameterArray)
   {
   TR_ASSERT_FATAL(parameterArray, "parameter array is NULL");

   if (methodSummary == NULL) //if we do not have method summary
      return 0;

   int benefit = 0;

   for (size_t i = 0; i < parameterArray->size(); i ++)
      {
      AbsValue* param = parameterArray->at(i);
      benefit += methodSummary->predicates(param->getConstraint(), i);
      }

   return benefit;
   }

