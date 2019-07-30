#pragma once

#include "compiler/optimizer/AbsVarArray.hpp"
#include "compiler/optimizer/IDT.hpp"
#include "compile/Compilation.hpp"
#include "compiler/optimizer/VPConstraint.hpp"
#include "optimizer/GlobalValuePropagation.hpp"
#include "optimizer/LocalValuePropagation.hpp"
#include "compiler/optimizer/AbsValue.hpp"

class PotentialOptimization
   {
   public:
   PotentialOptimization(int bytecode_idx, AbsValue *constraint) :
      _bytecode_idx(bytecode_idx),
      _constraint(constraint)
   {};
   protected:
   int _bytecode_idx;
   AbsValue *_constraint;
   };

class BranchFolding : PotentialOptimization
   {
   public:
   BranchFolding(int bytecode_idx, AbsValue *constraint) :
      PotentialOptimization(bytecode_idx, constraint)
   {};
   };

class NullCheckFolding : PotentialOptimization
   {
   public:
   NullCheckFolding(int bytecode_idx, AbsValue *constraint) :
      PotentialOptimization(bytecode_idx, constraint)
   {};
   };

class InstanceOfFolding : PotentialOptimization
   {
   public:
   InstanceOfFolding(int bytecode_idx, AbsValue *constraint):
      PotentialOptimization(bytecode_idx, constraint)
   {};
   };

class CheckCastFolding : PotentialOptimization
   {
   public:
   CheckCastFolding(int bytecode_idx, AbsValue *constraint):
      PotentialOptimization(bytecode_idx, constraint)
   {};
   static const char *name;
   };





class MethodSummaryRow {
public:
  MethodSummaryRow(TR::Compilation *comp, TR::Region &region);
  TR::VPConstraint *at(int n);
  void at(int n, TR::VPConstraint* constraint);
  void benefit(int n) { this->_benefit = n;}
  int benefit() { return this->_benefit;}
  enum PotentialTransform {UNK = 0, IFLT, IFLE, IFEQ, IFNE, IFGE, IFGT, INOF, IFNU, IFNN, NLCK, SIZE, STR_LEN, CHECK_CAST };
  void transform(MethodSummaryRow::PotentialTransform pt);
  MethodSummaryRow::PotentialTransform transform();
private:
  int _benefit;
  AbsVarArray *_row;
  TR::Region &_region;
  TR::Compilation *_comp;
  MethodSummaryRow::PotentialTransform _pt;
public:
  int _caller_idx;
  int _callee_idx;
  int _bytecode_idx;
  int caller_idx() { return this->_caller_idx; }
  int callee_idx() { return this->_callee_idx; }
  int bc_idx() { return this->_bytecode_idx; }
};

class MethodSummary {
public:
  MethodSummary(TR::Compilation *comp, TR::Region &region, TR::ValuePropagation*, IDT::Node*);
  int getIndex(void);
  //TODO: Rename all of these. These are not new anymore...
  MethodSummaryRow* newCHECK_CAST(IDT::Node*, int);
  MethodSummaryRow* newIFLT(IDT::Node*, int);
  MethodSummaryRow* newIFLE(IDT::Node*, int);
  MethodSummaryRow* newIFEQ(IDT::Node*, int);
  MethodSummaryRow* newIFNE(IDT::Node*, int);
  MethodSummaryRow* newIFGE(IDT::Node*, int);
  MethodSummaryRow* newIFGT(IDT::Node*, int);
  MethodSummaryRow* newINOF(IDT::Node*, int);
  MethodSummaryRow* newIFNU(IDT::Node*, int);
  MethodSummaryRow* newIFNN(IDT::Node*, int);
  MethodSummaryRow* newNLCK(IDT::Node*, int);
  MethodSummaryRow* newSIZE(IDT::Node*, int);
  MethodSummaryRow* newSTR_LEN(IDT::Node*, int);
  MethodSummaryRow* getRowSummary(void);
  MethodSummaryRow* getRowSummary(MethodSummaryRow::PotentialTransform, IDT::Node*, int);
  TR::VPConstraint *getClassConstraint(TR_OpaqueClassBlock *implicitParameterClass);
  int compareInformationAtCallSite(TR_ResolvedMethod *resolvedMethod, AbsVarArray *argumentConstraints);
  //int apply(AbsVarArray* argumentConstraints);
  int applyVerbose(AbsVarArray* argumentConstraints, int);
  int applyVerbose(AbsVarArray* argumentConstraints, IDT::Node*);
  char* toString();
  void printMethodSummary(int);
private:
  IDT::Node *_hunk;
  TR::ValuePropagation *_vp;
  TR::Region &_region;
  TR::Compilation *_comp;
  List<MethodSummaryRow> _methodSummaryNew;
  int _index;
};
