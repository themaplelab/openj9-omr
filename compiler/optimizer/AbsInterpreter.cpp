#include "optimizer/AbsInterpreter.hpp"
#include "optimizer/CallInfo.hpp"

AbsInterpreter::AbsInterpreter(
   IDTNode* node, 
   IDTBuilder* idtBuilder, 
   TR::ValuePropagation* valuePropagation,
   TR::Region& region,
   TR::Compilation* comp
   ):
      _idtNode(node),
      _idtBuilder(idtBuilder),
      _region(region),
      _comp(comp),
      _valuePropagation(valuePropagation),
      _bcIterator(node->getResolvedMethodSymbol(),static_cast<TR_ResolvedJ9Method*>(node->getCallTarget()->_calleeMethod),static_cast<TR_J9VMBase*>(this->comp()->fe()), this->comp())
   {
   _methodSummary = new (_region) MethodSummaryExtension(_region, valuePropagation);
   TR_ASSERT_FATAL(idtBuilder, "Cannot pass a NULL IDTBuilder!");
   }

AbsInterpreter::AbsInterpreter(IDTNode* node, TR::ValuePropagation* valuePropagation, TR::Region& region, TR::Compilation* comp):
      _idtNode(node),
      _region(region),
      _comp(comp),
      _valuePropagation(valuePropagation),
      _idtBuilder(NULL),
      _methodSummary(NULL),
      _bcIterator(node->getResolvedMethodSymbol(),static_cast<TR_ResolvedJ9Method*>(node->getCallTarget()->_calleeMethod),static_cast<TR_J9VMBase*>(this->comp()->fe()), this->comp())
   {
   }

TR::Compilation* AbsInterpreter::comp()
   {
   return _comp;
   }

TR::Region& AbsInterpreter::region()
   {
   return _region;
   }

void AbsInterpreter::interpret()
   {
   TR_CallTarget* callTarget = _idtNode->getCallTarget();
   TR_ASSERT_FATAL(callTarget,"Call Target is NULL!");

   TR::CFG* cfg = callTarget->_cfg;
   TR_ASSERT_FATAL(cfg, "CFG is NULL!");

   traverseBasicBlocks(cfg);
   }

AbsValue* AbsInterpreter::getClassConstraint(TR_OpaqueClassBlock* opaqueClass, TR::VPClassPresence *presence, TR::VPArrayInfo *info)
   {
   if (!opaqueClass)
      return new (region()) AbsValue(NULL,TR::Address);

   TR::VPClassType *fixedClass = TR::VPFixedClass::create(_valuePropagation, opaqueClass);
   TR::VPConstraint *classConstraint = TR::VPClass::create(_valuePropagation, fixedClass, presence, NULL, info, NULL);
   return new (region()) AbsValue (classConstraint, TR::Address);
   }

//get the abstract state of the start block of CFG
AbsState* AbsInterpreter::enterMethod(TR::ResolvedMethodSymbol* symbol)
   {
   if (comp()->getOption(TR_TraceAbstractInterpretation)) 
      traceMsg(comp(), "AbsInterpreter: Enter Method: %s\n", symbol->signature(comp()->trMemory()));

   AbsState* absState = new (region()) AbsState(region(), symbol);

   TR_ResolvedMethod *callerResolvedMethod = _bcIterator.method();
   TR::ResolvedMethodSymbol* callerResolvedMethodSymbol = TR::ResolvedMethodSymbol::create(comp()->trHeapMemory(), callerResolvedMethod, comp());
   TR_ResolvedMethod *resolvedMethod = callerResolvedMethodSymbol->getResolvedMethod();

   int32_t numberOfParameters = resolvedMethod->numberOfParameters();
   int32_t numberOfExplicitParameters = resolvedMethod->numberOfExplicitParameters();
   int32_t numberOfImplicitParameters = numberOfParameters - numberOfExplicitParameters;
   int32_t hasImplicitParameter = numberOfImplicitParameters > 0;

   //set the implicit parameter
   if (hasImplicitParameter)
      {
      TR_OpaqueClassBlock *implicitParameterClass = resolvedMethod->containingClass();
      AbsValue* value = getClassConstraint(implicitParameterClass);
      absState->set(0, value);
      }
    
    TR_MethodParameterIterator *parameterIterator = resolvedMethod->getParameterIterator(*comp());

   //setting the rest explicit parameters
   for (int32_t i = numberOfImplicitParameters; !parameterIterator->atEnd(); parameterIterator->advanceCursor(), i++)
      {
      TR::DataType dataType = parameterIterator->getDataType();
      AbsValue* temp;

      //primitive types
      switch (dataType)
         {
         case TR::Int8:
            temp = new (region()) AbsValue(NULL, TR::Int32);
            temp->setParamPosition(i);
            absState->set(i,temp);
            continue;
            break;

         case TR::Int16:
            temp = new (region()) AbsValue(NULL, TR::Int32);
            temp->setParamPosition(i);
            absState->set(i, temp);
            continue;
            break;

         case TR::Int32:
            temp = new (region()) AbsValue(NULL, dataType);
            temp->setParamPosition(i);
            absState->set(i, temp);
            continue;
            break;

         case TR::Int64:
            temp = new (region()) AbsValue(NULL, dataType);
            temp->setParamPosition(i);
            absState->set(i, temp);
            i = i+1;
            absState->set(i, new (region()) AbsValue(NULL, TR::NoType));
            continue;
            break;

         case TR::Float:
            temp = new (region()) AbsValue(NULL, dataType);
            temp->setParamPosition(i);
            absState->set(i, temp);
            continue;
            break;

         case TR::Double:
            temp = new (region()) AbsValue(NULL, dataType);
            temp->setParamPosition(i);
            absState->set(i, temp);
            i = i+1;
            absState->set(i, new (region()) AbsValue(NULL, TR::NoType));
            continue;
            break;

         default:
            //TODO: what about vectors and aggregates?
            break;
         }

      //Reference types
      bool isClass = parameterIterator->isClass();
      if (!isClass) // not a class
         {
         temp = new (region()) AbsValue(NULL, TR::Address);
         temp->setParamPosition(i);
         absState->set(i,temp);
         continue;
         }
      else // a class
         {
         TR_OpaqueClassBlock *parameterClass = parameterIterator->getOpaqueClass();
         if (!parameterClass)
            {
            temp = new (region()) AbsValue(NULL, TR::Address);
            temp->setParamPosition(i);
            absState->set(i,temp);
            continue;
            }
         else 
            {
            temp = getClassConstraint(parameterClass);
            temp->setParamPosition(i);
            absState->set(i,temp);
            continue;
            }   
         }
      

      }
   return absState;
   }

AbsState* AbsInterpreter::mergeAllPredecessors(TR::Block* block)
   {
   TR::CFGEdgeList &predecessors = block->getPredecessors();
   AbsState* absState = NULL;

   if (comp()->trace(OMR::benefitInliner))
      traceMsg(comp(), "Merge all predecessors for basic block %d\n", block->getNumber());

   bool first = true;

   for (auto i = predecessors.begin(), e = predecessors.end(); i != e; ++i)
      {
      auto *edge = *i;
      TR::Block *aBlock = edge->getFrom()->asBlock();
      TR::Block *check = edge->getTo()->asBlock();
      if (check != block) {
         continue;
      }
      //TODO: can aBlock->_absState be null?
      TR_ASSERT_FATAL(aBlock->_absState, "absState is NULL, i don't think this is possible");
      if (first) 
         {
         first = false;
         absState = new (region()) AbsState(aBlock->_absState);

         if (comp()->trace(OMR::benefitInliner))
            absState->trace(_valuePropagation);
         continue;
         }

      // merge with the rest;
      if (comp()->trace(OMR::benefitInliner))
         traceMsg(comp(), "Merge with Basic Block %d\n", aBlock->getNumber());
         
      absState->merge(aBlock->_absState,_valuePropagation);
      
      if (comp()->trace(OMR::benefitInliner))
         absState->trace(_valuePropagation);
      }

      return absState;
   }

void AbsInterpreter::transferAbsStates(TR::Block* block)
   {
   bool traceAbstractInterpretion = comp()->getOption(TR_TraceAbstractInterpretation);

   if (traceAbstractInterpretion) 
      traceMsg(comp(), "%s:%d:%s\n", __FILE__, __LINE__, __func__);

   if (block->getPredecessors().size() == 0) //Has no predecessors
      return;

   //Case 1:
   // this can happen I think if we have a loop that has some CFG inside. So its better to just return without assigning anything
   // as we should only visit if we actually have abs state to propagate
   if (traceAbstractInterpretion) 
      traceMsg(comp(), "There is a loop. Stop\n");

   if (block->hasOnlyOnePredecessor() && !block->getPredecessors().front()->getFrom()->asBlock()->_absState)
      //TODO: put it at the end of the queue?
      return;

   //Case: 2
   // If we only have one predecessor and there are no loops.
   if (traceAbstractInterpretion) 
      traceMsg(comp(), "There is only one predecessor.\n");
   if (block->hasOnlyOnePredecessor() && block->getPredecessors().front()->getFrom()->asBlock()->_absState) 
      {
      AbsState *absState = new (this->getRegion()) AbsEnvStatic(*block->getPredecessors().front()->getFrom()->asBlock()->_absState);
      block->_absState = absState;
      return;
      }

   //Case: 3
   // multiple predecessors...all interpreted
   if (traceAbstractInterpretion) 
      traceMsg(comp(), "There are multiple predecessors. All interpreted.\n");
   if (block->hasAbstractInterpretedAllPredecessors()) 
      {
      block->_absState = mergeAllPredecessors(block);
      return;
      }

   //Case: 4
   // we have not interpreted all predecessors...
   // look for a predecessor that has been interpreted
   if (traceAbstractInterpretion) 
      traceMsg(comp(), "Not all predecessors are interpreted. Finding one interpretd...\n");
   TR::CFGEdgeList &predecessors = block->getPredecessors();
   for (auto i = predecessors.begin(), e = predecessors.end(); i != e; ++i)
      {
      auto *edge = *i;
      TR::Block *aBlock = edge->getFrom()->asBlock();
      TR::Block *check = edge->getTo()->asBlock();
      if (check != block)
         {
         if (traceAbstractInterpretion)
            traceMsg(comp(), "fail check\n");
         continue;
         }

      if (!aBlock->_absState)
         {
         if (traceAbstractInterpretion)
            traceMsg(comp(), "does not have absState\n");
         continue;
         }

      if (traceAbstractInterpretion)
         traceMsg(comp(), "Find a predecessor interpreted\n");

      AbsState *oldState = aBlock->_absState;

      // how many 
      oldState->trace(_valuePropagation);
      AbsState *absState = new (region()) AbsState(*oldState);
    
      aBlock->_absState->zeroOut(absState);
      block->_absState = absState;
      return;
      }
      
   if (traceAbstractInterpretion)
      traceMsg(comp(), "No predecessor is interpreted.");
   }

void AbsInterpreter::traverseBasicBlocks(TR::CFG* cfg)
   {
   if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) 
      traceMsg(TR::comp(), "%s:%d:%s\n", __FILE__, __LINE__, __func__);
  
   TR::CFGNode *startNode = cfg->getStartForReverseSnapshot();
   TR_ASSERT_FATAL(startNode, "Start Node is NULL");

   TR::Block* startBlock = startNode->asBlock();

   //get start block's AbsState
   AbsState *startBlockState = enterMethod(_idtNode->getResolvedMethodSymbol());

   for (TR::ReversePostorderSnapshotBlockIterator blockIt (startBlock, comp()); blockIt.currentBlock(); ++blockIt)
      {
      TR::Block *block = blockIt.currentBlock();
      printf("B: %d\n",block->getNumber());
      //set start block's absState
      if (block == startBlock)
         block->_absState = startBlockState;
         
      block->setVisitCount(0);
      traverseByteCode(block);
      }
   }

void AbsInterpreter::traverseByteCode(TR::Block* block)
   {
   if (TR::comp()->getOption(TR_TraceAbstractInterpretation)) 
      traceMsg(TR::comp(), "%s:%d:%s\n", __FILE__, __LINE__, __func__);

   int32_t start = block->getBlockBCIndex();
   int32_t end = start + block->getBlockSize();
   if (start <0 || end < 1)
      return;

   if (block->getNumber() == 4) //Start block
      return;

   if (block->getNumber() == 3) //Exit block
      return;

   transferAbsStates(block);
   }