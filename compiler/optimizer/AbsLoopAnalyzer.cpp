#include "AbsLoopAnalyzer.hpp"
#include "infra/ILWalk.hpp"

AbsLoopAnalyzer::AbsLoopAnalyzer(TR::Region& region, IDT::Node *method, TR::ValuePropagation* vp, TR::CFG *cfg, TR::Compilation *comp):
  _region(region),
  _method(method),
  _vp(vp),
  _cfg(cfg),
  _comp(comp),
  _blockToAbsHead(AbsLoopAnalyzer::NodeMapComparator(), AbsLoopAnalyzer::NodeMapAllocator(_region))
  {
    TR::Block *startBlock = cfg->getStartForReverseSnapshot()->asBlock();
    for (TR::ReversePostorderSnapshotBlockIterator blockIt (startBlock, _comp); blockIt.currentBlock(); ++blockIt)
        {
        OMR::Block *block = (OMR::Block *)blockIt.currentBlock();
        addBlock(block);
        }
  }

AbsLoopAnalyzer::Node::Node(AbsLoopAnalyzer *graph, OMR::Block *cfgNode):
  _graph(graph),
  _block(cfgNode)
  {}

void AbsLoopAnalyzer::Node::interpretFlowPath(FlowPath *path)
  {
    for (TR::ReversePostorderSnapshotBlockIterator blockIt (_block->asBlock(), _graph->_comp); blockIt.currentBlock(); ++blockIt)
        {
        OMR::Block *block = (OMR::Block *)blockIt.currentBlock();
        if (path->nodeOnPath(block))
          {
            _graph->blockToAbsNode(block)->interpret();
          }
        }
  }

TR::Region &AbsLoopAnalyzer::Node::getRegion()
  {
  return _graph->getRegion();
  }

TR::Region &AbsLoopAnalyzer::getRegion()
  {
    return _region;
  }  

// Maybe store inside CFG nodes instead of a map
AbsLoopAnalyzer::Node *AbsLoopAnalyzer::blockToAbsNode(OMR::Block *node)
  {
  if (_blockToAbsHead.count(node) == 0)
    {
    return nullptr;
    }
  return _blockToAbsHead.at(node);
  }

bool AbsLoopAnalyzer::seenBlock(OMR::Block *block)
  {
  return _blockToAbsHead.count(block) != 0;
  }

void AbsLoopAnalyzer::addBlock(OMR::Block *block)
  {
  TR_ASSERT(!seenBlock(block), "Block already added");
  Node *node;
  TR::CFGEdgeList &preds = block->getPredecessors();
  if (block == _cfg->getStartForReverseSnapshot() || preds.size() == 0)
    {
    node = new (_region) NoPredecessor(this, block);
    }
  else if (preds.size() == 1)
    {
    if (seenBlock(block))
      {
      node = new (_region) SingePredecessor(this, block);
      }
    else
      {
      LoopHeader *header = new (_region) LoopHeader(this, block);
      node = header;
      header->addBackEdge(preds.front()->getTo());
      }
    }
  else
    {
    bool seenCycle = false;
    for (TR::CFGEdge *edge : preds)
      {
      OMR::Block *block = (OMR::Block *)edge->getTo();
      if (!seenBlock(block))
        {
        seenCycle = true;
        break;
        }
      }
    if (seenCycle)
      {
      LoopHeader *header = new (_region) LoopHeader(this, block);
      node = header;
      for (TR::CFGEdge *edge : preds)
        {
        OMR::Block *block = (OMR::Block *)edge->getTo();
        if (!seenBlock(block))
          {
          header->addBackEdge(block);
          }
        }
      }
    else
      {
      node = new (_region) MultiplePredecessors(this, block);
      }
    }
  }
  
AbsLoopAnalyzer::FlowPath::FlowPath(TR::CFGNode *head, TR::Region &region):
  _head(head),
  _region(region),
  _nodes(
    CFGNodeComparator(),
    CFGNodeAllocator(_region))
  {}
 
bool AbsLoopAnalyzer::FlowPath::nodeOnPath(TR::CFGNode *node)
  {
  if (this == nullptr)
    {
      return true;
    }
  return _nodes.count(node) != 0;
  }

// Add all nodes from head to end
// Assumes natural loop (that header is found on path)
void AbsLoopAnalyzer::FlowPath::addEndNode(TR::CFGNode *end)
  {
    TR_ASSERT(this != nullptr, "Path is null");
    if (_nodes.count(end) != 0)
      {
      return;
      }
    _nodes.insert(end);
    TR::CFGEdgeList &edges = end->getPredecessors();
    for (TR::CFGEdge *edge : edges)
      {
      TR::CFGNode *node = edge->getTo();
      if (node != _head)
        {
        addEndNode(node);
        }
      }
  }

void AbsLoopAnalyzer::NoPredecessor::interpret()
  {

  }

void AbsLoopAnalyzer::SingePredecessor::interpret()
  {
    
  }

void AbsLoopAnalyzer::MultiplePredecessors::interpret()
  {
    
  }

AbsLoopAnalyzer::LoopHeader::LoopHeader(AbsLoopAnalyzer *graph, OMR::Block *cfgNode):
  AbsLoopAnalyzer::Node::Node(graph, cfgNode),
  _backedges(
    CFGNodeComparator(),
    CFGNodeAllocator(graph->getRegion()))
  {
  
  }

void AbsLoopAnalyzer::LoopHeader::addBackEdge(TR::CFGNode *node)
  {
  _backedges.insert(node);
  }

void AbsLoopAnalyzer::LoopHeader::interpret()
  {
    TR::Region &mem = getRegion();
    // Create paths to backedges if they don't exist yet
    if (_pathsToBackEdges == nullptr)
      {
      _pathsToBackEdges = new (mem) FlowPath(_block, mem);
      for (TR::CFGNode *backedge : _backedges)
        {
          _pathsToBackEdges->addEndNode(backedge);
        }
      }
    // Compute the loop invariant
    interpretFlowPath(_pathsToBackEdges);
    // Merge the loop invariant from backedges

    // Now state is complete
  }
