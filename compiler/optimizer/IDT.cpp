#include "optimizer/IDT.hpp"

IDT::IDT(TR::Region& region, TR::ResolvedMethodSymbol* symbol, TR_CallTarget* callTarget, int budget, TR::Compilation* comp):
      _region(region),
      _maxIdx(-1),
      _comp(comp),
      _totalCost(0),
      _root(new (_region) IDTNode(getNextGlobalIDTNodeIndex(), callTarget, -1, symbol, 1, NULL, budget)),
      _indices(NULL)
   {   
   increaseGlobalIDTNodeIndex();
   }

void IDT::printTrace()
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
   IDTNodeDeque idtNodeQueue(comp()->trMemory()->currentStackRegion());

   idtNodeQueue.push_back(getRoot());

   while (!idtNodeQueue.empty())
      {
      IDTNode* currentNode = idtNodeQueue.front();
      idtNodeQueue.pop_front();

      int index = currentNode->getGlobalIndex();

      //print IDT node info
      if (index != -1) //skip root node
         {
         char line[1024];
         sprintf(line, "#IDT: #%d: #%d inlinable @%d -> bcsz=%d %s target %s, static benefit = %d, benefit = %d, cost = %d, budget = %d, callratio = %f, rootcallratio = %f", 
            index,
            currentNode->getParentGloablIndex(),
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
         idtNodeQueue.push_back(currentNode->getChild(i));
         
      }
         
   }

void IDT::buildIndices()
   {
   if (_indices != NULL)
      return;

   //initialize nodes index array
   unsigned int numNodes = getNumNodes()+1;
   _indices = new (_region) IDTNode *[numNodes];
   memset(_indices,0,sizeof(IDTNode*) * numNodes);

   //add all the descendents of the root node to the indices array
   IDTNodeDeque idtNodeQueue(comp()->trMemory()->currentStackRegion());
   idtNodeQueue.push_back(getRoot());

   while (!idtNodeQueue.empty())
      {
      IDTNode* currentNode = idtNodeQueue.front();
      idtNodeQueue.pop_front();

      int calleeIndex = currentNode->getGlobalIndex();
      TR_ASSERT_FATAL(_indices[calleeIndex+1] == 0, "Callee index not unique!\n");

      _indices[calleeIndex + 1] = currentNode;

      for (unsigned int i = 0;i < currentNode->getNumChildren(); i ++)
         {
         idtNodeQueue.push_back(currentNode->getChild(i));
         }
      }
   }

IDTNode *IDT::getNodeByGlobalIndex(int index)
   {
   TR_ASSERT_FATAL(_indices, "Call buildIndices() first");
   TR_ASSERT_FATAL(index < getNextGlobalIDTNodeIndex(), "Index out of range!");
   return _indices[index + 1];
   }

void IDT::copyDescendants(IDTNode* fromNode, IDTNode* toNode)
   {
   TR_ASSERT_FATAL(
      fromNode->getResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier() 
      == toNode->getResolvedMethodSymbol()->getResolvedMethod()->getPersistentIdentifier(), 
      "Copying different nodes is not allowed!");

   for (unsigned int i =0 ; i < fromNode->getNumChildren(); i ++)
      {
      IDTNode* child = fromNode->getChild(i);

      if (toNode->getBudget() - child->getCost() < 0)
         {
         continue;
         }

      IDTNode* copiedChild = toNode->addChild(
                           getNextGlobalIDTNodeIndex(),
                           child->getCallTarget(),
                           child->getByteCodeIndex(),
                           child->getResolvedMethodSymbol(),
                           child->getCallRatio(),
                           getMemoryRegion()
                           );
      if (copiedChild)
            {
            increaseGlobalIDTNodeIndex();
            addCost(child->getCost());
            copiedChild->setMethodSummary(child->getMethodSummary());
            copyDescendants(child, copiedChild);
            }
      }
  
   }
