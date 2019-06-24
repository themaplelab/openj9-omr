#pragma once

#include "IDT.hpp"

#include <functional>
#include <unordered_set>
#include <iostream>
#include <string>
#include "env/Region.hpp"
#include "infra/List.hpp" // for List
#include "infra/BitVector.hpp" // for BitVector
#include "BenefitInliner.hpp"

class TR_InlinerBase;

class InliningProposal
  {
  public:
  InliningProposal(OMR::BenefitInliner *inliner, IDT *idt, int max);
  InliningProposal(InliningProposal&, OMR::BenefitInliner *inliner);
  bool isEmpty() const;
  void clear();
  int getCost() ;
  int getBenefit() ;
  void pushBack(IDT::Node *);
  bool inSet(IDT::Node* );
  bool inSet(int calleeIndex);
  void intersectInPlace(InliningProposal &, InliningProposal&);
  void unionInPlace(InliningProposal &, InliningProposal&);

  private:
  InliningProposal() = delete;
  InliningProposal(const InliningProposal&) = delete;
  InliningProposal & operator=(const InliningProposal&) = delete;
  void computeCostAndBenefit();
  OMR::BenefitInliner *_inliner;
  TR_BitVector *_nodes;
  int _cost;
  int _benefit;
  IDT *_idt;
};
