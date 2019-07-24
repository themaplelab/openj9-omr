#include "AbsLoopAnalyzer.hpp"
#include "infra/ILWalk.hpp"

// Non nat loops will break this code in two ways
// - construction of flow paths from backedges will include too many nodes
// - The initial pass through the CFG may mislabel some nodes as loops
// Possible Solutions:
// - Find another way to do initial pass and construct flow paths
// - Don't bother analyzing methods with non nat loops

AbsLoopAnalyzer::AbsLoopAnalyzer(TR::Region& region, IDT::Node *method, TR::ValuePropagation* vp, TR::CFG *cfg, TR::Compilation *comp):
  _region(region),
  _method(method),
  _vp(vp),
  _cfg(cfg),
  _comp(comp),
  _blockToAbsHead(AbsLoopAnalyzer::NodeMapComparator(), AbsLoopAnalyzer::NodeMapAllocator(_region))
  {
    // Add all blocks in RPO order to determine their types
    OMR::Block *startBlock = (OMR::Block *)cfg->getStartForReverseSnapshot()->asBlock();
    for (TR::ReversePostorderSnapshotBlockIterator blockIt ((TR::Block *)startBlock, _comp); blockIt.currentBlock(); ++blockIt)
        {
        OMR::Block *block = (OMR::Block *)blockIt.currentBlock();
        addBlock(block);
        }
  }

void
AbsLoopAnalyzer::AbsLoopAnalyzer::interpretGraph()
  {
  _root->interpretFlowPath();
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

void
AbsLoopAnalyzer::Node::setOldPostState(AbsEnvStatic *state)
  {
    if (state == nullptr)
      {
      return;
      }
    TR::Region &region = getRegion();
    if (_oldPostState == nullptr)
      {
      _oldPostState = new (region) AbsEnvStatic(*state);
      }
    else
      {
      // TODO use operator= instead of allocating new memory
      _oldPostState = new (region) AbsEnvStatic(*state);
      }
  }

void
AbsLoopAnalyzer::Node::setPostState(AbsEnvStatic* state)
  {
  _postState = state;
  }

bool
AbsLoopAnalyzer::Node::predecessorsHaveBeenInterpreted()
  {
  TR::CFGEdgeList &edges = _block->getPredecessors();
  for (TR::CFGEdge *edge : edges)
    {
      OMR::Block *block = (OMR::Block *)edge->getFrom();
      if (!_graph->seenBlock(block))
        {
        return false;
        }
      Node *node = _graph->blockToAbsNode(block);
      if (!node->hasBeenInterpretedAtLeastOnce())
        {
        return false;
        }
    }
  return true;
  }

AbsEnvStatic *
AbsLoopAnalyzer::Node::mergeIncomingStateFromPredecessors()
  {
  TR_ASSERT(predecessorsHaveBeenInterpreted(), "Cannot merge from predecessors unless they've already been interpreted");
  AbsEnvStatic *mergedEnv = nullptr;
  TR::CFGEdgeList &edges = _block->getPredecessors();
  if (edges.size() > 1) {
    const int i = 42;
  }
  for (TR::CFGEdge *edge : edges)
    {
      OMR::Block *block = (OMR::Block *)edge->getFrom()->asBlock();
      TR_ASSERT(_graph->seenBlock(block), "Predecessor not visited");
      Node *node = _graph->blockToAbsNode(block);
      TR_ASSERT(node->hasBeenInterpretedAtLeastOnce(), "Node not interpreted");
      AbsEnvStatic *currAbsEnv = node->getPostState();
      if (mergedEnv == nullptr)
        {
        mergedEnv = new (getRegion()) AbsEnvStatic(*currAbsEnv);
        }
      else
        {
        mergedEnv->merge(*currAbsEnv);
        }      
    }
  return mergedEnv;
  }

bool
AbsLoopAnalyzer::Node::hasBeenInterpretedAtLeastOnce()
  {
  return _postState != nullptr;
  }

AbsEnvStatic*
AbsLoopAnalyzer::Node::getPostState()
  {
  return _postState;
  }

TR::Region&
AbsLoopAnalyzer::Node::getRegion()
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

AbsEnvStatic *AbsLoopAnalyzer::emptyEnv()
  {
  return new (getRegion()) AbsEnvStatic(getRegion(), _method);
  }

void AbsLoopAnalyzer::addBlock(OMR::Block *block)
  {
  TR_ASSERT(!seenBlock(block), "Block already added");
  Node *node;
  TR::CFGEdgeList &preds = block->getPredecessors();
  if (block == _cfg->getStartForReverseSnapshot())
    {
    node = new (_region) NoPredecessor(this, block);
    _root = node;
    }
  else if (preds.size() == 0)
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
      header->addBackEdge(preds.front()->getFrom());
      }
    }
  else
    {
    bool seenCycle = false;
    for (TR::CFGEdge *edge : preds)
      {
      OMR::Block *block = (OMR::Block *)edge->getFrom();
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
        OMR::Block *block = (OMR::Block *)edge->getFrom();
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
    _blockToAbsHead.insert(std::pair<OMR::Block *, Node *>(block, node));
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
    TR_ASSERT(end != _head, "Cannot add head to path");
    _nodes.insert(end);
    TR::CFGEdgeList &edges = end->getPredecessors();
    for (TR::CFGEdge *edge : edges)
      {
      TR::CFGNode *node = edge->getFrom();
      if (node != _head)
        {
        addEndNode(node);
        }
      }
  }

bool AbsLoopAnalyzer::NoPredecessor::interpret()
  {
  if (_postState == nullptr)
    {
      TR::Region &region = getRegion();
      _postState = new (region) AbsEnvStatic(region, _graph->_method);
      _postState->interpretBlock(_block);
      return true;
    }
  return false;
  }

bool AbsLoopAnalyzer::SingePredecessor::interpret()
  {
  if (_postState != nullptr)
    {
      setOldPostState(_postState);
    }
  setPostState(mergeIncomingStateFromPredecessors());
  _postState->interpretBlock(_block);
  return _postState->isNarrowerThan(_oldPostState);
  }

bool AbsLoopAnalyzer::MultiplePredecessors::interpret()
  {
  if (_postState != nullptr)
    {
      setOldPostState(_postState);
    }
  setPostState(mergeIncomingStateFromPredecessors());
  _postState->interpretBlock(_block);
  return _postState->isNarrowerThan(_oldPostState);
  }

AbsLoopAnalyzer::LoopHeader::LoopHeader(AbsLoopAnalyzer *graph, OMR::Block *cfgNode):
  AbsLoopAnalyzer::Node::Node(graph, cfgNode),
  _backedges(
    CFGNodeComparator(),
    CFGNodeAllocator(graph->getRegion()))
  {}

void AbsLoopAnalyzer::LoopHeader::addBackEdge(TR::CFGNode *node)
  {
  _backedges.insert(node);
  }

AbsEnvStatic *AbsLoopAnalyzer::LoopHeader::getSizedStartState()
  {
  // Get initial state by finding a predeccessor that has already been visited
  TR::CFGEdgeList &edges = _block->getPredecessors();
  for (TR::CFGEdge *edge : edges)
    {
      OMR::Block *block = (OMR::Block *)edge->getFrom()->asBlock();
      if (_graph->seenBlock(block))
        {
          Node *absNode = _graph->blockToAbsNode(block);
          if (absNode->hasBeenInterpretedAtLeastOnce())
            {
            return absNode->getPostState()->getWidened();
            }
        }
    }
  // If no predeccessor has been visited, we must be the entry node
  return _graph->emptyEnv();
  }

AbsLoopAnalyzer::FlowPath *AbsLoopAnalyzer::LoopHeader::getPathsToBackEdges()
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
  return _pathsToBackEdges;
  }

bool AbsLoopAnalyzer::LoopHeader::interpret()
  {
  setOldPostState(_postState);
  AbsEnvStatic *firstPassState = getSizedStartState();
  firstPassState->interpretBlock(_block);
  setPostState(firstPassState);
  interpretFlowPath(getPathsToBackEdges());
  AbsEnvStatic *loopInvariant = mergeIncomingStateFromPredecessors();
  loopInvariant->interpretBlock(_block);
  setPostState(loopInvariant);
  return _postState->isNarrowerThan(_oldPostState);
  }
