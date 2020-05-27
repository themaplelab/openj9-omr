#include <stdio.h>
#include <iostream>
#include <algorithm>

#include "compiler/optimizer/Growable_2d_array.hpp"
#include "compiler/optimizer/InliningProposal.hpp"
#include "compile/Compilation.hpp"
#include "env/TypedAllocator.hpp"
#include "env/StackMemoryRegion.hpp"
#include "optimizer/BenefitInliner.hpp"
#include "infra/BitVector.hpp"


Growable_2d_array_BitVectorImpl::Growable_2d_array_BitVectorImpl(TR::Compilation *comp, size_t rows, size_t cols, OMR::BenefitInliner*inliner)
  : _rows(rows + 1)
  , _default(NULL)
  , _cols(cols)
  , _inliner(inliner)
{

  this->_default = new (region()) InliningProposal(inliner->comp()->trMemory()->currentStackRegion(), NULL, 1);
  this->_table = new(region()) InliningProposal**[_rows];
  for (int i = 0; i < _rows; i++) {
    this->_table[i] =  new(region()) InliningProposal*[cols];
    for (int j = 0; j < _cols; j++) {
      this->_table[i][j] = NULL;
    }
  }
}

TR::Region&
Growable_2d_array_BitVectorImpl::region() {
  return this->_inliner->_holdingProposalRegion;
}

const InliningProposal&
Growable_2d_array_BitVectorImpl::proposal() {
  return *this->_default;
}

const InliningProposal&
Growable_2d_array_BitVectorImpl::get(int row, int col) {
  if (row < 0 or col < 0) {
    return this->proposal();
  }

  
  if (row >= this->_rows or col >= this->_cols) {
    return this->proposal();
  }

  InliningProposal*temp = this->_get(row, col);
  return temp ? *temp : this->proposal();
}

InliningProposal*
Growable_2d_array_BitVectorImpl::_get(const int row, const int col) {
  if (row < 0 or col < 0) {
    return this->_default;
  }
  if (row >= this->_rows or col >= this->_cols) {
    return this->_default;
  }
  InliningProposal* temp = this->_table[row][col];
  return temp ? temp : this->_default;
}

InliningProposal&
Growable_2d_array_BitVectorImpl::get_best() {
  size_t row = this->_rows;
  size_t col = this->_cols;
  return *this->_get(row - 1, col);
}

InliningProposal*
Growable_2d_array_BitVectorImpl::get_new(const int row, const int col) {
  InliningProposal*heap = new (region()) InliningProposal(*this->_default, this->_inliner->comp()->trMemory()->currentStackRegion());
  this->_table[row][col] = heap;
  return heap;
}

void
Growable_2d_array_BitVectorImpl::put(int row, int col, InliningProposal& proposal) {
  if (row < 0 or col < 0) {
    TR_ASSERT(false, "putting in negative is not cool, this might fail first");
    traceMsg(TR::comp(), "putting in %d, %d\n", row, col);
    return;
  }

  if (row >= this->_rows or col >= this->_cols) {
    TR_ASSERT(false, "putting in overflow is not cool, this might fail first");
    traceMsg(TR::comp(), "putting in %d, %d\n", row, col);
    return;
  }

  InliningProposal*heap = new (region()) InliningProposal(proposal, this->_inliner->comp()->trMemory()->currentStackRegion());
  this->_put(row, col, heap);
}

void
Growable_2d_array_BitVectorImpl::_put(int row, int col, InliningProposal* proposal) {
  if (row < 0 or col < 0) {
    TR_ASSERT(false, "putting in negative is not cool, this might fail first");
    traceMsg(TR::comp(), "putting in %d, %d\n", row, col);
    return;
  }
  if (row >= this->_rows or col >= this->_cols) {
    TR_ASSERT(false, "putting in overflow is not cool, this might fail first");
    traceMsg(TR::comp(), "putting in %d, %d\n", row, col);
    return;
  }
  
  this->_table[row][col] = proposal;
}
