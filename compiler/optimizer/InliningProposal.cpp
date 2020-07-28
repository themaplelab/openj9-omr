#include "optimizer/InliningProposal.hpp"
#include "infra/deque.hpp"
#include "compile/Compilation.hpp"
#include "env/Region.hpp"
#include "infra/BitVector.hpp" // for BitVector


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
   bool traceBIProposal = comp->getOption(TR_TraceBIProposal);
   bool verboseInlining = comp->getOptions()->getVerboseOption(TR_VerboseInlining);

   if (!traceBIProposal && !verboseInlining) //no need to run the following code if neither flag is set
      return; 

   if (!_nodes)
      { 
      traceMsg(comp, "Inlining Proposal is NULL\n");
      return;
      }

   int32_t numMethodsInlined =  _nodes->elementCount()-1;
  
   TR_ASSERT_FATAL(_idt, "Must have an IDT");

   char header[1024];
   sprintf(header,"#Proposal: %d methods inlined into %s", numMethodsInlined, _idt->getRoot()->getName());

   if (traceBIProposal)
      traceMsg(comp, "%s\n", header);
   if (verboseInlining)
      TR_VerboseLog::writeLineLocked(TR_Vlog_SIP, header);

   IDTNodeDeque idtNodeQueue(comp->trMemory()->currentStackRegion());
   idtNodeQueue.push_back(_idt->getRoot());

   //BFS 
   while (!idtNodeQueue.empty()) 
      {
      IDTNode* currentNode = idtNodeQueue.front();
      idtNodeQueue.pop_front();
      int index = currentNode->getGlobalIndex();

      if (index != -1) //do not print the root node
         { 
         char line[1024];
         sprintf(line, "#Proposal: #%d : #%d %s @%d -> bcsz=%d %s target %s, benefit = %d, cost = %d, budget = %d",
            currentNode->getGlobalIndex(),
            currentNode->getParentGloablIndex(),
            _nodes->isSet(index + 1) ? "inlined" : "not inlined",
            currentNode->getByteCodeIndex(),
            currentNode->getByteCodeSize(),
            currentNode->getCallTarget()->_calleeSymbol ? currentNode->getCallTarget()->_calleeSymbol->signature(comp->trMemory()) : "no callee symbol???",
            currentNode->getName(),
            currentNode->getBenefit(),
            currentNode->getCost(),
            currentNode->getBudget()
         );

         if (traceBIProposal)
            traceMsg(comp, "%s\n",line);
         if (verboseInlining)
            TR_VerboseLog::writeLineLocked(TR_Vlog_SIP, line);
         } 
      
      //process children
      int32_t numChildren = currentNode->getNumChildren();

      for (int32_t i = 0; i < numChildren; i++)
         idtNodeQueue.push_back(currentNode->getChild(i));
         
      } 

   traceMsg(comp, "\nBit Vector\n");
   this->_nodes->print(comp, comp->getOutFile());
   traceMsg(comp, "\n");
}


void InliningProposal::addNode(IDTNode *node)
   {
   ensureBitVectorInitialized();

   int32_t calleeIdx = node->getGlobalIndex() + 1;
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
   if (!_idt)
      return;

   this->_cost = 0;
   this->_benefit = 0;
   TR_BitVectorIterator bvi(*this->_nodes);
   int32_t igNodeIndex;
   
   while (bvi.hasMoreElements()) 
      {
      igNodeIndex = bvi.getNextElement();
      IDTNode *node = _idt->getNodeByGlobalIndex(igNodeIndex - 1);
      if (node == NULL) 
         {
         continue;
         }
      this->_cost += node->getCost();
      this->_benefit += node->getBenefit();
      }
   }

void InliningProposal::ensureBitVectorInitialized()
   {
   if (!_nodes)
      _nodes = new (_region) TR_BitVector(_region);
   }

void InliningProposal::clear()
   {
   if (!_nodes)
      return;
   this->_cost = -1;
   this->_benefit = -1;
   this->_nodes->empty();
   }

bool InliningProposal::inSet(IDTNode* node)
   {
   if (node == NULL)
      return false;
   if (this->_nodes == NULL)
      return false;
   if (this->_nodes->isEmpty())
      return false;
   int32_t callee_idx = node->getGlobalIndex() + 1;

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
   ensureBitVectorInitialized();
   a.ensureBitVectorInitialized();
   b.ensureBitVectorInitialized();

   *this->_nodes = *a._nodes;
   *this->_nodes &= *b._nodes;
   this->_cost = -1;
   this->_benefit = -1;
   }

void InliningProposal::unionInPlace(InliningProposal &a, InliningProposal &b)
   {
   ensureBitVectorInitialized();
   a.ensureBitVectorInitialized();
   b.ensureBitVectorInitialized();
   
   *this->_nodes = *a._nodes;
   *this->_nodes |= *b._nodes;
   this->_cost = -1;
   this->_benefit = -1;
   }

bool InliningProposal::overlaps(InliningProposal *p)
   {
   if (!_nodes || !p->_nodes)
      return false;

   TR_BitVectorIterator bvi(*p->_nodes);
   int32_t igNodeIndex;
   while (bvi.hasMoreElements())
      {
      igNodeIndex = bvi.getNextElement();
      if (!this->_nodes->isSet(igNodeIndex))
         return false;
      }

   return true;
   }

bool InliningProposal::intersects(InliningProposal* other)
   {
   if (!_nodes || !other->_nodes)
      return false;
   
   return _nodes->intersects(*other->_nodes);
   }


InliningProposalTable::InliningProposalTable(unsigned int rows, unsigned int cols, TR::Region& region) :
      _rows(rows),
      _cols(cols),
      _region(region)
   {
   _table = new (region) InliningProposal**[rows];

   for (int i = 0; i < rows; i ++)
      {
      _table[i] = new (region) InliningProposal*[cols];
      memset(_table[i], 0, sizeof(InliningProposal*)*cols);
      }
   }

InliningProposal* InliningProposalTable::get(unsigned int row, unsigned int col)
   {
   InliningProposal* proposal = NULL;

   if (row <0 || col <0 || row >= _rows || col >= _cols)
      proposal = getEmptyProposal();
   else
      proposal = _table[row][col] ? _table[row][col] : getEmptyProposal();

   return proposal;
   }

void InliningProposalTable::set(unsigned int row, unsigned int col, InliningProposal* proposal)
   {
   TR_ASSERT_FATAL(proposal, "proposal is NULL");
   TR_ASSERT_FATAL(row >=0 && row < _rows, "Invalid row index" );
   TR_ASSERT_FATAL(col >= 0 && col < _cols, "Invalid col index" );

   _table[row][col] = proposal; 
   }