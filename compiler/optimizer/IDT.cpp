#include "optimizer/IDT.hpp"

IDT::IDT(TR::Region& region, TR::ResolvedMethodSymbol* rms, int budget, TR::Compilation* comp):
      _region(region),
      _vp(NULL),
      _max_idx(-1),
      _comp(comp),
      _root(new (_region) IDTNode(getNextGlobalIDTNodeIdx(), TR::MethodSymbol::Static, -1,rms,NULL,0,budget,NULL,1)),
      _indices(NULL)
   {   
   increaseGlobalIDTNodeIndex();
   }

void IDT::printTrace() const
   {
   bool verboseInlining = comp()->getOptions()->getVerboseOption(TR_VerboseInlining);
   bool traceBIIDTGen = comp()->getOption(TR_TraceBIIDTGen);

   if (!verboseInlining && !traceBIIDTGen)
      return;
   
   // print header line
   char header[1024];
   const int candidates = getNumNodes() - 1;
   sprintf(header,"#IDT: %d candidate methods inlinable into %s with a budget %d", 
      candidates,
      getRoot()->getName(comp()->trMemory()),
      getRoot()->getBudget()
   );

   if (verboseInlining)
      TR_VerboseLog::writeLineLocked(TR_Vlog_SIP, header);
   if (traceBIIDTGen)
      traceMsg(comp(), "%s\n", header);

   if (candidates <= 0) 
      {
      return;
      }

   //print the IDT nodes in BFS
   TR::deque<IDTNode*, TR::Region&> *idtNodeQueue = new (_region)TR::deque<IDTNode*, TR::Region&>(_region);
   idtNodeQueue->push_back(getRoot());

   while (!idtNodeQueue->empty())
      {
      IDTNode* currentNode = idtNodeQueue->front();
      idtNodeQueue->pop_front();

      int calleeIndex = currentNode->getCalleeIndex();

      //print IDT node info
      if (calleeIndex != -1) //skip root node
         {
         char line[1024];
         sprintf(line, "#IDT: #%d: #%d inlinable @%d -> bcsz=%d %s target %s, static benefit = %d, benefit = %d, cost = %d, budget = %d, callratio = %f, rootcallratio = %f", 
            calleeIndex,
            currentNode->getCallerIndex(),
            currentNode->getByteCodeIndex(),
            currentNode->getByteCodeSize(),
            currentNode->getCallTarget()->_calleeSymbol ? currentNode->getCallTarget()->_calleeSymbol->signature(comp()->trMemory()) : "no callee symbol???",
            currentNode->getName(),
            currentNode->getStaticBenefit(),
            currentNode->getBenefit(),
            currentNode->getCost(),
            currentNode->getBudget(),
            currentNode->getCallRatio(),
            currentNode->getRootCallRatio()
         );

         if (verboseInlining)
            TR_VerboseLog::writeLineLocked(TR_Vlog_SIP, line);
         if (traceBIIDTGen) 
            traceMsg(comp(), "%s\n", line);
         }
         

      //process children
      for (unsigned int i = 0; i < currentNode->getNumChildren(); i ++)
         {
         idtNodeQueue->push_back(currentNode->getChild(i));
         }
      }
         
   }

void IDT::buildIndices()
   {
   //initialize nodes index array
   unsigned int numNodes = getNumNodes()+1;
   _indices = new (_region) IDTNode *[numNodes];
   memset(_indices,0,sizeof(IDTNode*) * numNodes);

   //add all the descendents of the root node to the indices array
   TR::deque<IDTNode*, TR::Region&> *idtNodeQueue = new (_region)TR::deque<IDTNode*, TR::Region&>(_region);
   idtNodeQueue->push_back(getRoot());

   while (!idtNodeQueue->empty())
      {
      IDTNode* currentNode = idtNodeQueue->front();
      idtNodeQueue->pop_front();

      int calleeIndex = currentNode->getCalleeIndex();
      TR_ASSERT_FATAL(_indices[calleeIndex+1] == 0, "Callee index not unique!\n");

      _indices[calleeIndex + 1] = currentNode;

      for (unsigned int i = 0;i < currentNode->getNumChildren(); i ++)
         {
         idtNodeQueue->push_back(currentNode->getChild(i));
         }
      }
   }

int IDT::getNextGlobalIDTNodeIdx() 
   {
   return _max_idx;
   }

void IDT::increaseGlobalIDTNodeIndex()
   {
   _max_idx ++;
   }

TR::Compilation* IDT::comp() const
   {
   return _comp;
   }

unsigned int IDT::getNumNodes() const
   {
   return getRoot()->getNumDescendantsIncludingMe();
   }

IDTNode* IDT::getRoot() const
   {
   return _root;
   }

IDTNode *IDT::getNodeByCalleeIndex(int calleeIndex)
   {
   TR_ASSERT_FATAL(calleeIndex < getNextGlobalIDTNodeIdx(), "CalleeIndex out of range!");
   return _indices[calleeIndex + 1];
   }

TR::ValuePropagation* IDT::getValuePropagation()
   {
   if (_vp != NULL)
      return _vp;
   TR::OptimizationManager* manager = comp()->getOptimizer()->getOptimization(OMR::globalValuePropagation);
   _vp = (TR::ValuePropagation*) manager->factory()(manager);
   _vp->initialize();
   return _vp;
   }

TR::Region& IDT::getMemoryRegion() const
   {
   return _region;
   }

void IDT::copyChildren(IDTNode* fromNode, IDTNode* toNode, IDTNodeDeque& children)
   {
   TR_ASSERT_FATAL(
      fromNode->getResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier() 
      == toNode->getResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier(), 
      "Copying different nodes is not allowed!");

   unsigned int numChildren = fromNode->getNumChildren();
   if (numChildren == 0)
      return;
   
   bool traceBIIDTGen = comp()->getOption(TR_TraceBIIDTGen);
   
   for (unsigned int i = 0;i < numChildren; i ++)
      {
      IDTNode *child = fromNode->getChild(i);
      if (traceBIIDTGen)
         traceMsg(comp(), "Copying %d into %d\n", child->getCalleeIndex(), toNode->getCalleeIndex());
      
      IDTNode* newChild = toNode->addChildIfNotExists(
                              getNextGlobalIDTNodeIdx(),
                              child->getMethodKind(),
                              child->getByteCodeIndex(),
                              child->getResolvedMethodSymbol(),
                              child->getStaticBenefit(),
                              child->getCallSite(),
                              child->getCallRatio(),
                              getMemoryRegion());
      if (newChild)
         {
         increaseGlobalIDTNodeIndex();
         newChild->setCallTarget(child->getCallTarget());
         newChild->setMethodSummary(child->getMethodSummary());
         //children.push_back(newChild);
         }
      }
      

   }
