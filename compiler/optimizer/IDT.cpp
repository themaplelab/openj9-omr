#include "optimizer/IDT.hpp"

IDT::IDT(TR::Region& region, TR_CallTarget* callTarget, TR::ResolvedMethodSymbol* symbol, int budget, TR::Compilation* comp):
      _region(region),
      _maxIdx(-1),
      _comp(comp),
      _root(new (_region) IDTNode(getNextGlobalIDTNodeIndex(), callTarget, symbol, -1, 1, NULL, budget)),
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
            currentNode->getResolvedMethodSymbol()->signature(comp()->trMemory()),
            currentNode->getName(comp()->trMemory()),
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
   unsigned int numNodes = getNumNodes();
   _indices = new (_region) IDTNode *[numNodes];

   memset(_indices, 0, sizeof(IDTNode*) * numNodes);

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

      for (unsigned int i = 0; i < currentNode->getNumChildren(); i ++)
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
         continue;
         

      IDTNode* copiedChild = toNode->addChild(
                           getNextGlobalIDTNodeIndex(),
                           child->getCallTarget(),
                           child->getResolvedMethodSymbol(),
                           child->getByteCodeIndex(),
                           child->getCallRatio(),
                           getMemoryRegion()
                           );
      if (copiedChild)
            {
            increaseGlobalIDTNodeIndex();
            copiedChild->setMethodSummary(child->getMethodSummary());
            copiedChild->setStaticBenefit(child->getStaticBenefit());
            copyDescendants(child, copiedChild);
            }
      }
  
   }

IDTPreorderPriorityQueue::IDTPreorderPriorityQueue(IDT* idt, TR::Region& region)  :
      _entries(region),
      _idt(idt),
      _pQueue(IDTNodeCompare(), IDTNodeVector(region))
   {
   _pQueue.push(idt->getRoot());
   }

IDTNode* IDTPreorderPriorityQueue::get(unsigned int index)
   {
   size_t entriesSize = _entries.size();

   if (entriesSize > index) //already in entries
      return _entries.at(index);

   unsigned int idtSize = size();

   if (index > idtSize - 1)
      return NULL;

   //not in entries yet. Update entries.
   while (_entries.size() <= index) 
      {
      IDTNode *newEntry = _pQueue.top();
      _pQueue.pop();

      _entries.push_back(newEntry);
      for (unsigned int j = 0; j < newEntry->getNumChildren(); j++)
         {
         _pQueue.push(newEntry->getChild(j));
         }
      }

   return _entries.at(index);
   }