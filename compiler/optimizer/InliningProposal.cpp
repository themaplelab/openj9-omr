#include "InliningProposal.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "compile/Compilation.hpp"
#include "env/Region.hpp"
#include "optimizer/Inliner.hpp"
#include "infra/BitVector.hpp" // for BitVector
#endif

InliningProposal::InliningProposal(TR::Region& region, IDT *idt, int max):
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

void
InliningProposal::print()
{
   if (TR::comp()->getOption(TR_TraceBIProposal))
   { // make a flag for printing inlining proposals
     traceMsg(TR::comp(), "Printing bit vector with nodes\n");
     if (!_nodes)
     { 
        traceMsg(TR::comp(), "bit vector is NULL\n");
        return;
     }
    this->_nodes->print(TR::comp(), TR::comp()->getOutFile());
    traceMsg(TR::comp(), "\n");
   }
}


void
InliningProposal::pushBack(IDT::Node *node)
  {
  //Ensure initialization
  if (!_nodes) _nodes = new (_region) TR_BitVector(_region);

  int32_t calleeIdx = node->getCalleeIndex() + 1;
  if (this->_nodes->isSet(calleeIdx))
    {
        return;
    }
  _nodes->set(calleeIdx);

  //this->_cost += node->getCost();
  //this->_benefit += node->getBenefit();
  }

bool
InliningProposal::isEmpty() const
  {

  if (!_nodes) return true;

  return this->_nodes->isEmpty();
  }

int
InliningProposal::getCost()
  {
  if (this->_cost == -1) 
    {
    this->computeCostAndBenefit();
    }
  return this->_cost;
  }

int
InliningProposal::getBenefit()
  {
  if (this->_benefit == -1)
    {
    this->computeCostAndBenefit();
    }
  return this->_benefit;
  }

void
InliningProposal::computeCostAndBenefit()
    {   
    this->_cost = 0;
    this->_benefit = 0;
    TR_BitVectorIterator bvi(*this->_nodes);
    int32_t igNodeIndex;
    
    while (bvi.hasMoreElements()) {
        igNodeIndex = bvi.getNextElement();
        IDT::Node *node = _idt->getNodeByCalleeIndex(igNodeIndex - 1);
        if (node == NULL) 
            {
            continue;
            }
        this->_cost += node->getCost();
        this->_benefit += node->getBenefit();
      }
    }

void
InliningProposal::clear()
  {
  if (!_nodes) return;
  this->_cost = -1;
  this->_benefit = -1;
  this->_nodes->empty();
  }

bool
InliningProposal::inSet(IDT::Node* node)
  {
  if (node == NULL) return false;
  if (this->_nodes == NULL) return false;
  if (this->_nodes->isEmpty()) return false;
  int32_t callee_idx = node->getCalleeIndex() + 1;
  return this->_nodes->isSet(callee_idx);
  }

bool
InliningProposal::inSet(int calleeIdx)
  {
  if (!_nodes) return false;
  return this->_nodes->isSet(calleeIdx + 1);
  }

void
InliningProposal::intersectInPlace(InliningProposal &a, InliningProposal &b)
  {
  //Ensure initialization
  if (!_nodes) _nodes = new (_region) TR_BitVector(_region);
  if (!a._nodes) a._nodes = new (a._region) TR_BitVector(a._region);
  if (!b._nodes) b._nodes = new (b._region) TR_BitVector(b._region);

  *this->_nodes = *a._nodes;
  *this->_nodes &= *b._nodes;
  this->_cost = -1;
  this->_benefit = -1;
  // compute the cost and benefit lazily
  // TODO: recompute cost, recompute benefit. Which method computes them?
  }

void
InliningProposal::unionInPlace(InliningProposal &a, InliningProposal &b)
  {
  //Ensure initialization
  if (!_nodes) _nodes = new (_region) TR_BitVector(_region);
  if (!a._nodes) a._nodes = new (a._region) TR_BitVector(a._region);
  if (!b._nodes) b._nodes = new (b._region) TR_BitVector(b._region);
  
  *this->_nodes = *a._nodes;
  *this->_nodes |= *b._nodes;
  this->_cost = -1;
  this->_benefit = -1;
  // compute the cost and benefit lazily
  // TODO: recompute cost, recompute benefit. Which method computes them?
  }

bool
InliningProposal::overlaps(InliningProposal *p)
{
  if (!_nodes) return false;
  TR_BitVectorIterator bvi(*p->_nodes);
  int32_t igNodeIndex;
  while (bvi.hasMoreElements()) {
     igNodeIndex = bvi.getNextElement();
     if (!this->_nodes->isSet(igNodeIndex)) return false;
  }
  return true;
}
