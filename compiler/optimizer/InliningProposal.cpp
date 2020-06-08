#include "compiler/optimizer/InliningProposal.hpp"
#include "infra/deque.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "compile/Compilation.hpp"
#include "env/Region.hpp"
#include "infra/BitVector.hpp" // for BitVector
#endif

InliningProposal::InliningProposal(TR::Region& region, IDT *idt):
   _cost(-1),
   _benefit(-1),
   _idt(idt),
   _region(region),
   _nodes(NULL) // Lazy Initialization of BitVector
   {
   }

InliningProposal::InliningProposal(InliningProposal &proposal, TR::Region& region):
   _cost(proposal._cost),
   _benefit(proposal._benefit),
   _idt(proposal._idt),
   _region(region)
   {
   this->_nodes = new (region) TR_BitVector(proposal._nodes->getHighestBitPosition(), region);
   *this->_nodes = *proposal._nodes;
   }

void InliningProposal::print(TR::Compilation* comp)
   {
   bool ifTrace = comp->getOption(TR_TraceBIProposal);
   bool ifVerbose = comp->getOptions()->getVerboseOption(TR_VerboseInlining);

   if (!ifTrace && !ifVerbose) //no need to run the following code if neither flag is set
      return; 

   if (!_nodes)
      { 
      traceMsg(comp, "Inlining Proposal is NULL\n");
      return;
      }

   int32_t numMethodsInlined =  _nodes->elementCount()-1;

   char header[1024];
   sprintf(header,"#Proposal: %d methods inlined into %s", numMethodsInlined, _idt->getRoot()->getName());

   if (ifTrace)
      traceMsg(comp, "%s\n", header);
   if (ifVerbose)
      TR_VerboseLog::writeLineLocked(TR_Vlog_SIP, header);

   TR::deque<IDT::Node*, TR::Region&> *idtNodeQueue =  new (_region) TR::deque<IDT::Node*, TR::Region&>(_region) ;
   idtNodeQueue->push_back(_idt->getRoot());

   //BFS 
   while (!idtNodeQueue->empty()) 
      {
      IDT::Node* currentNode = idtNodeQueue->front();
      idtNodeQueue->pop_front();
      int calleeIndex = currentNode->getCalleeIndex();

      if (calleeIndex != -1) //do not print the root node
         { 
         char line[1024];
         sprintf(line, "#Proposal: #%d : #%d %s @%d -> bcsz=%d %s target %s, benefit = %d, cost = %d, budget = %d",
            currentNode->getCalleeIndex(),
            currentNode->getCallerIndex(),
            _nodes->isSet(calleeIndex+1) ? "inlined" : "not inlined",
            currentNode->getByteCodeIndex(),
            currentNode->getBcSz(),
            currentNode->getCallTarget()->_calleeSymbol ? currentNode->getCallTarget()->_calleeSymbol->signature(comp->trMemory()) : "no callee symbol???",
            currentNode->getName(),
            currentNode->getBenefit(),
            currentNode->getCost(),
            currentNode->budget()
         );

         if (ifTrace)
            traceMsg(comp, "%s\n",line);
         if (ifVerbose)
            TR_VerboseLog::writeLineLocked(TR_Vlog_SIP, line);
         } 
      
      //process children
      int32_t numChildren = currentNode->getNumChildren();

      for (int32_t i = 0; i < numChildren; i++)
         {
         idtNodeQueue->push_back(currentNode->getChild(i));
         }
      } 

   traceMsg(comp, "\nBit Vector\n");
   this->_nodes->print(comp, comp->getOutFile());
   traceMsg(comp, "\n");
}


void InliningProposal::pushBack(IDT::Node *node)
   {
   //Ensure initialization
   if (!_nodes) 
      _nodes = new (_region) TR_BitVector(_region);

   int32_t calleeIdx = node->getCalleeIndex() + 1;
   if (this->_nodes->isSet(calleeIdx))
      {
      return;
      }

   _nodes->set(calleeIdx);
   }

bool InliningProposal::isEmpty() const
   {
   if (!_nodes)
      return true;

   return this->_nodes->isEmpty();
   }

int InliningProposal::getCost()
   {
   if (this->_cost == -1) 
      {
      this->computeCostAndBenefit();
      }

   return this->_cost;
   }

int InliningProposal::getBenefit()
   {
   if (this->_benefit == -1)
      {
      this->computeCostAndBenefit();
      }

   return this->_benefit;
   }

void InliningProposal::computeCostAndBenefit()
   {   
   this->_cost = 0;
   this->_benefit = 0;
   TR_BitVectorIterator bvi(*this->_nodes);
   int32_t igNodeIndex;
   
   while (bvi.hasMoreElements())
      {
      igNodeIndex = bvi.getNextElement();
      IDT::Node *node = _idt->getNodeByCalleeIndex(igNodeIndex - 1);
      if (node == NULL) 
         continue;
      this->_cost += node->getCost();
      this->_benefit += node->getBenefit();
      }
   }

void InliningProposal::clear()
   {
   if (!_nodes)
      return;
   this->_cost = -1;
   this->_benefit = -1;
   this->_nodes->empty();
   }

bool InliningProposal::inSet(IDT::Node* node)
   {
   if (node == NULL)
      return false;
   if (this->_nodes == NULL)
      return false;
   if (this->_nodes->isEmpty())
      return false;
   int32_t callee_idx = node->getCalleeIndex() + 1;

   return this->_nodes->isSet(callee_idx);
   }

bool InliningProposal::inSet(int calleeIdx)
   {
   if (!_nodes)
      return false;

   return this->_nodes->isSet(calleeIdx + 1);
   }

void InliningProposal::intersectInPlace(InliningProposal &a, InliningProposal &b)
   {
   //Ensure initialization
   if (!_nodes)
      _nodes = new (_region) TR_BitVector(_region);
   if (!a._nodes)
      a._nodes = new (a._region) TR_BitVector(a._region);
   if (!b._nodes)
      b._nodes = new (b._region) TR_BitVector(b._region);

   *this->_nodes = *a._nodes;
   *this->_nodes &= *b._nodes;
   this->_cost = -1;
   this->_benefit = -1;
   }

void InliningProposal::unionInPlace(InliningProposal &a, InliningProposal &b)
   {
   //Ensure initialization
   if (!_nodes)
      _nodes = new (_region) TR_BitVector(_region);
   if (!a._nodes)
      a._nodes = new (a._region) TR_BitVector(a._region);
   if (!b._nodes)
      b._nodes = new (b._region) TR_BitVector(b._region);
   
   *this->_nodes = *a._nodes;
   *this->_nodes |= *b._nodes;
   this->_cost = -1;
   this->_benefit = -1;
   }

bool InliningProposal::overlaps(InliningProposal *p)
   {
   if (!_nodes)
      return false;
   TR_BitVectorIterator bvi(*p->_nodes);
   int32_t igNodeIndex;
   while (bvi.hasMoreElements())
      {
      igNodeIndex = bvi.getNextElement();
      if (!this->_nodes->isSet(igNodeIndex)) return false;
      }

   return true;
   }
