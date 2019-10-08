#pragma once
#ifndef _IDTConstructor_
#define _IDTConstructor_

#include "compiler/optimizer/AbsEnvStatic.hpp"
#include "compiler/optimizer/IDT.hpp"
#include "compiler/optimizer/BenefitInliner.hpp"
#include "compiler/optimizer/MethodSummary.hpp"

class AbsFrameIDTConstructor;

class IDTConstructor : public AbsEnvStatic 
{
public:
  IDTConstructor(TR::Region &region, IDT::Node *node, AbsFrame*);
  IDTConstructor(IDTConstructor&);
  IDTConstructor(AbsEnvStatic&);
  static AbsEnvStatic *enterMethod(TR::Region&region, IDT::Node* node, AbsFrame* absFrame, TR::ResolvedMethodSymbol*);
  TR::Region& getCallSitesRegion();
protected:
  virtual AbstractState& invokevirtual(AbstractState&, int, int);
  virtual AbstractState& invokespecial(AbstractState&, int, int);
  virtual AbstractState& invokestatic(AbstractState&, int, int);
  virtual AbstractState& invokeinterface(AbstractState&, int, int);
  virtual AbstractState& ifeq(AbstractState&, int, int);
private:
  void addIfeq(int, int);
  IDT::Indices *getDeque();
  TR_CallSite* findCallSiteTargets(TR::ResolvedMethodSymbol *callerSymbol, int, int, TR::MethodSymbol::Kinds, OMR::Block *block, TR::CFG* cfg = NULL);
  TR::SymbolReference* getSymbolReference(TR::ResolvedMethodSymbol *callerSymbol, int cpIndex, TR::MethodSymbol::Kinds kind);
  int getCallerIndex() const;
  TR_CallStack *getCallStack();
  OMR::BenefitInliner* inliner();
   TR_CallSite * getCallSite(TR::MethodSymbol::Kinds kind,
                                    TR_ResolvedMethod *callerResolvedMethod,
                                    TR::TreeTop *callNodeTreeTop,
                                    TR::Node *parent,
                                    TR::Node *callNode,
                                    TR::Method * interfaceMethod,
                                    TR_OpaqueClassBlock *receiverClass,
                                    int32_t vftSlot,
                                    int32_t cpIndex,
                                    TR_ResolvedMethod *initialCalleeMethod,
                                    TR::ResolvedMethodSymbol * initialCalleeSymbol,
                                    bool isIndirectCall,
                                    bool isInterface,
                                    TR_ByteCodeInfo & bcInfo,
                                    TR::Compilation *comp,
                                    int32_t depth=-1,
                                    bool allConsts=false,
                                    TR::SymbolReference *symRef=nullptr);
};

class AbsFrameIDTConstructor : public AbsFrame
{
public:
   AbsFrameIDTConstructor(TR::Region &region, IDT::Node *node, int callerIndex, TR_CallStack* callStack, OMR::BenefitInliner* inliner);
   virtual void interpret();
   TR::Region &getCallSitesRegion();
   TR_CallStack* getCallStack();
   IDT::Node *getNode();
  MethodSummaryExtension _summary;
  void traceMethodSummary();
protected:
  OMR::BenefitInliner* _inliner;
  TR::Region &_callSitesRegion;
  TR_CallStack *_inliningCallStack;
  int _callerIndex;
  virtual void factFlow(OMR::Block *);
  virtual AbsEnvStatic *mergeAllPredecessors(OMR::Block *);
  IDT::Indices *_deque;
public:
  void setDeque(IDT::Indices * deque) { this->_deque=deque; };
  IDT::Indices *getDeque() { return this->_deque; };
   int getCallerIndex() const;
  OMR::BenefitInliner* inliner();
};

#endif
