#pragma once

#include <map>
#include <set>
#include <functional>

#include "AbsEnvStatic.hpp"
#include "AbsValue.hpp"
#include "ValuePropagation.hpp"
#include "env/Region.hpp"
#include "env/TypedAllocator.hpp"
#include "infra/deque.hpp"
#include "il/OMRBlock.hpp"
#include "IDT.hpp"

class AbsLoopAnalyzer
{
public:
  AbsLoopAnalyzer(TR::Region& region, IDT::Node *method, TR::ValuePropagation* vp, TR::CFG *cfg, TR::Compilation *comp);
  void interpretGraph();
private:

  typedef std::less<TR::CFGNode *> CFGNodeComparator;
  typedef TR::typed_allocator<TR::CFGNode *, TR::Region&> CFGNodeAllocator;
  typedef std::set<TR::CFGNode *, CFGNodeComparator, CFGNodeAllocator> CFGNodeSet;

  class FlowPath
    {
    private:
    TR::CFGNode *_head;
    TR::Region& _region;
    CFGNodeSet _nodes;
    public:
    bool nodeOnPath(TR::CFGNode *);
    FlowPath(TR::CFGNode *head, TR::Region &region);
    // Add all nodes from head to end
    void addEndNode(TR::CFGNode *end);
    };
  
  public:
  class Node
  {
  public:
    Node(AbsLoopAnalyzer *graph, OMR::Block *cfgNode);
    void interpretFlowPath(FlowPath *path = nullptr);
    bool predecessorsHaveBeenInterpreted();
    bool hasBeenInterpretedAtLeastOnce();
    AbsEnvStatic* getPostState();
    // Interpret current block. Returns true if resulting state is narrower
    virtual bool interpret() = 0;
  protected:
    void setPostState(AbsEnvStatic* state);
    void setOldPostState(AbsEnvStatic *state);
    AbsEnvStatic* mergeIncomingStateFromPredecessors();
    AbsLoopAnalyzer *_graph;
    OMR::Block *_block;
    AbsEnvStatic* _postState = nullptr;
    AbsEnvStatic* _oldPostState = nullptr;
    TR::Region &getRegion();
  };

  typedef TR::typed_allocator<std::pair<OMR::Block * const, AbsLoopAnalyzer::Node *>, TR::Region&> NodeMapAllocator;
  typedef std::less<OMR::Block *> NodeMapComparator;
  typedef std::map<OMR::Block *, AbsLoopAnalyzer::Node *, NodeMapComparator, NodeMapAllocator> NodeMap;

  TR::Region &getRegion();

  private:
  Node *blockToAbsNode(OMR::Block *node);
  void addBlock(OMR::Block *node);
  bool seenBlock(OMR::Block *node);
  AbsEnvStatic *emptyEnv();

  TR::Region& _region;
  IDT::Node *_method;
  TR::ValuePropagation* _vp;
  TR::CFG *_cfg;
  TR::Compilation *_comp;
  NodeMap _blockToAbsHead;
  Node *_root = nullptr;
  class NoPredecessor : public Node
    {
    public:
      using Node::Node;
    protected:
      bool interpret();
    };
  class SingePredecessor : public Node
    {
    public: 
      using Node::Node;
    protected:
      bool interpret();
    };
  class MultiplePredecessors : public Node
    {
    public: 
      using Node::Node;
    protected:
      bool interpret();
    };
  class LoopHeader : public Node
    {
    private:
    FlowPath *_pathsToBackEdges = nullptr;
    CFGNodeSet _backedges;
    AbsEnvStatic *getSizedStartState();
    FlowPath *getPathsToBackEdges();
    public: 
      LoopHeader(AbsLoopAnalyzer *graph, OMR::Block *cfgNode);
      void addBackEdge(TR::CFGNode *node);
    protected:
      bool interpret();
    };
};
