#pragma once

#ifndef GROWABLE_2D_ARRAY_INCL
#define GROWABLE_2D_ARRAY_INCL

#include "compiler/optimizer/InliningProposal.hpp"
#include "compile/Compilation.hpp"
#include "infra/BitVector.hpp"
#include "compiler/optimizer/BenefitInliner.hpp"


class Growable_2d_array_BitVectorImpl {
public:
  Growable_2d_array_BitVectorImpl(TR::Compilation*, size_t, size_t, OMR::BenefitInliner*);
  const InliningProposal& proposal();
  const InliningProposal& get(int, int);
  InliningProposal* get_new(const int, const int);
  InliningProposal* _get(int, int) ;
  void put(int, int, InliningProposal&);
  void _put(int, int, InliningProposal*);
  InliningProposal& get_best();
  InliningProposal* _default;
  TR::Region& region();
private:
  size_t _rows;
  size_t _cols;
  OMR::BenefitInliner* _inliner;
  InliningProposal*** _table;
};
#endif
