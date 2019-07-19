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
  protected:
    virtual void interpret() = 0;
    AbsLoopAnalyzer *_graph;
    OMR::Block *_block;
    AbsEnvStatic* _postState = nullptr;
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
      void interpret();
    };
  class SingePredecessor : public Node
    {
    public: 
      using Node::Node;
    protected:
      void interpret();
    };
  class MultiplePredecessors : public Node
    {
    public: 
      using Node::Node;
    protected:
      void interpret();
    };
  class LoopHeader : public Node
    {
    private:
    FlowPath *_pathsToBackEdges = nullptr;
    CFGNodeSet _backedges;
    public: 
      LoopHeader(AbsLoopAnalyzer *graph, OMR::Block *cfgNode);
      void addBackEdge(TR::CFGNode *node);
    protected:
      void interpret();
    };
};
