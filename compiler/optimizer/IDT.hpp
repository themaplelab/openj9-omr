#pragma once

#include "Inliner.hpp"
#include "env/Region.hpp"
#include "env/TypedAllocator.hpp"
#include "infra/Cfg.hpp"
#include "infra/deque.hpp"
#include "infra/Stack.hpp"
#include <queue>
#include "infra/vector.hpp"

#include "compiler/optimizer/AbsEnvStatic.hpp"
#include "compiler/optimizer/AbsOpStackStatic.hpp"
#include "compiler/optimizer/AbsVarArrayStatic.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"
#include "compiler/il/OMRBlock.hpp"
#include "infra/Cfg.hpp"

/**
 * Class IDT 
 * =========
 * 
 * The inlining dependency tree maintains a tree of methods and can be used by an inlining optimization
 * The tree is designed to be built in a DFS-manner and maintains a current child which is updated when 
 * New children are added via addToCurrentChild. popCurrent will rever to the previous child.
 */

class IDT
  {
  public:
  //TODO: can we move this definition outside?
  class Node;
  typedef TR::deque<Node *, TR::Region&> Indices;
  typedef TR::vector<Node*, TR::Region&> NodePtrVector;
    struct NodePtrOrder
      {
      bool operator()(IDT::Node *left, IDT::Node *right)
        {
        return left->getCost() < right->getCost() || left->getBenefit() < right->getBenefit();
        }
      };
  typedef std::priority_queue<Node*, NodePtrVector, IDT::NodePtrOrder> NodePtrPriorityQueue;
  class Node
    {
    public:
    Node(IDT* idt, int idx, int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, Node *parent);
    Node(IDT* idt, int idx, int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, Node *parent, unsigned int benefit);
    unsigned int howManyDescendantsIncludingMe() const;
    Node* addChildIfNotExists(IDT* idt, int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, int benefit);
    const char* getName(const IDT* idt) const;
    const char* getName() const;
    void printNodeThenChildren(const IDT* idt, int callerIndex) const;
    Node *getParent() const;
    int getCalleeIndex() const;
    unsigned int getCost() const;
    unsigned int getBenefit() const;
    void setBenefit(unsigned int);
    void buildIndices(IDT::Indices &indices);
    void enqueue_subordinates(IDT::NodePtrPriorityQueue *q) const;
    unsigned int getNumChildren() const;
    bool isRoot() const;
    IDT::Node* findChildWithBytecodeIndex(int bcIndex);
    bool isSameMethod(IDT::Node* aNode) const;
    bool isSameMethod(TR::ResolvedMethodSymbol *) const;
    int numberOfParameters();
    TR::ResolvedMethodSymbol* getResolvedMethodSymbol() const;
    void setResolvedMethodSymbol(TR::ResolvedMethodSymbol* rms) { this->_rms = rms; }
    int getCallerIndex() const;
    unsigned int howManyDescendants() const;
    AbsEnvStatic* enterMethod();
    void getMethodSummary();
    private:

    typedef TR::deque<Node, TR::Region&> Children;
    Node *_parent;
    IDT* _head;
    int _idx;
    int _callsite_bci;
    // NULL if 0, (Node* & 1) if 1, otherwise a deque*
    Children* _children;
    unsigned int _benefit;
    TR::ResolvedMethodSymbol* _rms;

    bool nodeSimilar(int32_t callsite_bci, TR::ResolvedMethodSymbol* rms) const;
    uint32_t getBcSz() const;
    // Returns NULL if 0 or > 1 children
    Node* getOnlyChild() const;
    void setOnlyChild(Node* child);
    UDATA maxStack() const;
    IDATA maxLocals() const;
    TR::Region &getAbsOpStackMemoryRegion() const;
    TR::Region &getAbsVarArrayMemoryRegion() const;
    TR::Region &getAbsEnvMemoryRegion() const;
    AbsOpStackStatic* createAbsOpStack();
    AbsVarArrayStatic* createAbsVarArray();
    AbsEnvStatic* createAbsEnv();
    TR::ValuePropagation *getValuePropagation();
    AbsEnvStatic* analyzeBasicBlock(OMR::Block *, AbsEnvStatic*);
    AbsEnvStatic* analyzeBasicBlock(OMR::Block*, AbsEnvStatic*, unsigned int, unsigned int);
    };
  
private:
  TR::Region& _mem;
  TR::ValuePropagation *_vp;
  TR_InlinerBase* _inliner;
  int _max_idx = -1;
  Node *_root;
  Node *_current;
  Indices *_indices = nullptr;
  void buildIndices();
  typedef TR::deque<Node, TR::Region&> BuildStack;
  int nextIdx();
  TR::ValuePropagation *getValuePropagation();

public:
  TR::Region &getMemoryRegion() const;
  IDT(TR_InlinerBase* inliner, TR::Region& mem, TR::ResolvedMethodSymbol* rms);
  Node* getRoot() const;
  TR::Compilation* comp() const;
  bool addToCurrentChild(int32_t callsite_bci, TR::ResolvedMethodSymbol* rms, float callRatio);
  void popCurrent();
  unsigned int howManyNodes() const;
  void printTrace() const;
  IDT::Node *getNodeByCalleeIndex(int calleeIndex);
};
