#include "InliningProposal.hpp"

#ifdef J9_PROJECT_SPECIFIC
#include "compile/Compilation.hpp"
#include "env/Region.hpp"
#include "optimizer/Inliner.hpp"
#include "infra/BitVector.hpp" // for BitVector
#endif

InliningProposal::InliningProposal(OMR::BenefitInliner *inliner, IDT *idt, int max):
   _inliner(inliner),
   _cost(0),
   _benefit(0),
   _idt(idt)
    {
        TR_Memory *mem = inliner->comp()->trMemory();
        this->_nodes = new (mem->currentStackRegion()) TR_BitVector(max, mem);
    }

InliningProposal::InliningProposal(InliningProposal &proposal, OMR::BenefitInliner *inliner):
   _inliner(inliner),
   _cost(proposal._cost),
   _benefit(proposal._benefit),
   _idt(proposal._idt)
    {
        TR_Memory *mem = inliner->comp()->trMemory();
        this->_nodes = new (mem->currentStackRegion()) TR_BitVector(proposal._nodes->getHighestBitPosition(), mem);
       *this->_nodes = *proposal._nodes;
    }


void
InliningProposal::pushBack(IDT::Node *node)
  {
  int32_t calleeIdx = node->getCalleeIndex() + 1;
  if (this->_nodes->isSet(calleeIdx))
    {
        return;
    }
  _nodes->set(calleeIdx);

  this->_cost += node->getCost();
  this->_benefit += node->getBenefit();
  }

bool
InliningProposal::isEmpty() const
  {
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
        if (node == nullptr) 
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
  this->_cost = 0;
  this->_benefit = 0;
  this->_nodes->empty();
  }

bool
InliningProposal::inSet(IDT::Node* node)
  {
  if (node == nullptr) return false;
  if (this->_nodes == nullptr) return false;
  if (this->_nodes->isEmpty()) return false;
  int32_t callee_idx = node->getCalleeIndex() + 1;
  return this->_nodes->isSet(callee_idx);
  }

bool
InliningProposal::inSet(int calleeIdx)
  {
  return this->_nodes->isSet(calleeIdx + 1);
  }

void
InliningProposal::intersectInPlace(InliningProposal &a, InliningProposal &b)
  {
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
  *this->_nodes = *a._nodes;
  *this->_nodes |= *b._nodes;
  this->_cost = -1;
  this->_benefit = -1;
  // compute the cost and benefit lazily
  // TODO: recompute cost, recompute benefit. Which method computes them?
  }
