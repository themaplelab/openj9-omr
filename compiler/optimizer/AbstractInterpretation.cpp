#include "AbstractInterpretation.hpp"


// Iterate in reverse post order
class RegionIterator
  {
  public:
  RegionIterator(TR_RegionStructure *region, TR::Region &mem):
    _region(region),
    _nodes(mem),
    _seen(mem)
    {
    TR_StructureSubGraphNode *entry = region->getEntry();
    _seen.set(entry->getNumber());
    addSuccessors(entry);
    _nodes.push_back(entry);
    _current = _nodes.size() - 1;
    }
  private:
  TR_RegionStructure *_region;
  TR::deque<TR_StructureSubGraphNode *, TR::Region &> _nodes;
  TR_BitVector _seen;
  int _current;
  void addSuccessors(TR_StructureSubGraphNode *from)
    {
    for (TR_SuccessorIterator si(from); si.getCurrent(); si.getNext())
      {
      TR_StructureSubGraphNode *successor = toStructureSubGraphNode(si.getCurrent()->getTo());
      if (!_seen.isSet(successor->getNumber()) && _region->contains(successor->getStructure()))
        {
        _seen.set(successor->getNumber());
        addSuccessors(successor);
        _nodes.push_back(successor);
        }
      }
    }
  public:
  TR_StructureSubGraphNode *getCurrent()
    {
    if (_current < 0)
      {
      return nullptr;
      }
    return _nodes[_current];
    }
  TR_StructureSubGraphNode *getNext()
    {
    _current--;
    return getCurrent();
    }
  };

class AnalysisInfo
    {
    private:
    const bool _isEntry;
    AbsEnvStatic *_state;
    AbsEnvStatic *_preState;
    public:
    AnalysisInfo(bool isEntry):
        _isEntry(isEntry),
        _state(nullptr),
        _preState(nullptr) {}
    AnalysisInfo(bool isEntry, AbsEnvStatic *state):
        _isEntry(isEntry),
        _state(state),
        _preState(nullptr) {}
    static AnalysisInfo *get(TR_Structure *structure)
        {
        return (AnalysisInfo *)structure->getAnalysisInfo();
        }
    static AnalysisInfo *get(TR_StructureSubGraphNode *node)
        {
        return get(node->getStructure());
        }
    bool isEntry()
        {
        return _isEntry;
        }
    void setState(AbsEnvStatic *state)
        {
        _state = state;
        }
    AbsEnvStatic *getState()
        {
        return _state;
        }
    AbsEnvStatic *getPreState()
        {
        return _preState;
        }
    void setPreState(AbsEnvStatic *v)
        {
        _preState = v;
        }
    };


TR_AbstractInterpretation::TR_AbstractInterpretation(TR::Region& region,
                         IDT::Node *method,
                         TR::ValuePropagation* vp,
                         TR_RegionStructure *rootStructure,
                         TR::Compilation *comp,
                         bool trace): 
    
    _region(region),
    _method(method),
    _vp(vp),
    _rootStructure(rootStructure),
    _comp(comp),
    _trace(trace),
    _edgeStates(EdgeMapComparator(), EdgeMapAllocator(_region))
    {}
    

int32_t
TR_AbstractInterpretation::perform()
    {
    initializeAnalysisState();
    analyzeRegionStructure(_rootStructure, false);
    return 1;
    }

bool
TR_AbstractInterpretation::analyzeStructure(TR_Structure *structure, bool checkChange)
    {
    if (structure->asBlock() != nullptr)
        {
        return analyzeBlockStructure(structure->asBlock(), checkChange);
        }
    else
        {
        return analyzeRegionStructure(structure->asRegion(), checkChange);
        }
    }

bool
TR_AbstractInterpretation::analyzeBlockStructure(TR_BlockStructure *structure, bool checkChange)
    {
    AnalysisInfo *ainfo = AnalysisInfo::get(structure);
    ainfo->setState(getIncomingState(structure));
    //ainfo->getState()->interpretBlock(structure->getBlock());
    ainfo->getState()->getFrame()->interpret(structure->getBlock());
    setSuccessorEdgeStates(structure);
    return true;
    }

bool
TR_AbstractInterpretation::analyzeRegionStructure(TR_RegionStructure *structure, bool checkChange)
    {
    if (structure->isNaturalLoop())
        {
        // TODO first pass doesn't need to walk entire graph
        for (RegionIterator ri(structure, _region); ri.getCurrent(); ri.getNext())
            {
            TR_StructureSubGraphNode *node = ri.getCurrent();
            analyzeStructure(node->getStructure(), /* checkChange */ false);
            }
        }
    else 
        {
        TR_ASSERT(structure->isAcyclic(), "Improper regions not implemented");
        }
    for (RegionIterator ri(structure, _region); ri.getCurrent(); ri.getNext())
        {
        TR_StructureSubGraphNode *node = ri.getCurrent();
        analyzeStructure(node->getStructure(), checkChange);
        }
    AnalysisInfo *ainfo = AnalysisInfo::get(structure);
    ainfo->setState(nullptr);
    for (ListElement<TR::CFGEdge> *iter = structure->getExitEdges().getListHead(); iter != nullptr; iter = iter->getNextElement())
        {
        TR::CFGEdge *edge = iter->getData();
        AnalysisInfo *edgeInfo = _edgeStates[edge];
        if (ainfo->getState() == nullptr)
            {    
            ainfo->setState(new (_region) AbsEnvStatic(*edgeInfo->getState()));
            }
        else
            {
            ainfo->getState()->merge(*edgeInfo->getState());
            }        
        }
    setSuccessorEdgeStates(structure);
    return true;
    }

AbsEnvStatic *
TR_AbstractInterpretation::getExitState()
    {
    return AnalysisInfo::get(_rootStructure)->getState();
    }

void
TR_AbstractInterpretation::initializeAnalysisState()
    {
    TR::deque<TR_RegionStructure *, TR::Region &> work(_region);
    work.push_back(_rootStructure);
    _rootStructure->setAnalysisInfo(new (_region) AnalysisInfo(/* isEntry */ false));
    _rootStructure->setAnalyzedStatus(false);
    while (!work.empty())
        {
        TR_RegionStructure *region = work.back();
        work.pop_back();
        for (RegionIterator ri(region, _region); ri.getCurrent(); ri.getNext())
            {
            TR_StructureSubGraphNode *node = ri.getCurrent();
            TR_Structure *structure = node->getStructure();
            bool isEntry = region->getEntry() == node;
            structure->setAnalysisInfo(new (_region) AnalysisInfo(isEntry));
            structure->setAnalyzedStatus(false);
            if (structure->asRegion() != nullptr)
                {
                work.push_back(structure->asRegion());
                }
            }
        }
    }

AbsEnvStatic *
TR_AbstractInterpretation::getIncomingState(TR_Structure *structure)
    {
    AnalysisInfo *ainfo = AnalysisInfo::get(structure);
    if (ainfo->isEntry() && structureHasUnanalyzedPredecessorEdges(structure))
        {
        AbsEnvStatic *top = getSizedStartState(structure);
        ainfo->setPreState(top);
        return top;
        }
    AbsEnvStatic *state = nullptr;
    if (structure->getSubGraphNode() != nullptr)
        {
        TR::CFGEdgeList &predecessors = structure->getSubGraphNode()->getPredecessors();
        for (TR::CFGEdgeList::iterator predEdgeIter = predecessors.begin(); predEdgeIter != predecessors.end(); ++predEdgeIter)
            {
            TR::CFGEdge *predEdge = *predEdgeIter;
            TR_ASSERT(_edgeStates.count(predEdge) != 0, "Predecessor edge hasn't been analyzed yet");
            AbsEnvStatic *incomingState = _edgeStates[predEdge]->getState();
            TR_ASSERT(incomingState != nullptr, "Predecessor edge hasn't been analyzed yet");
            if (state == nullptr)
                {
                state = new (_region) AbsEnvStatic(*incomingState);
                }
            else
                {
                state->merge(*incomingState);
                }
            }
        }
    if (ainfo->isEntry() && ainfo->getPreState() != nullptr)
        {
        AbsEnvStatic *parentState = getIncomingState(structure->getParent());
        state = AbsEnvStatic::mergeIdenticalValuesBottom(ainfo->getPreState(), state);
        state->merge(*parentState);
        }
    if (state == nullptr)
        {
        //FIXME TODO
        //state = new (_region) AbsEnvStatic(_region, _method);
        state = new (_region) AbsEnvStatic(_region, _method, NULL);
        }
    return state;
    }

void 
TR_AbstractInterpretation::setSuccessorEdgeStates(TR_Structure *from)
    {
    if (from->getSubGraphNode() == nullptr)
        {
        return;
        }
    AbsEnvStatic *state = AnalysisInfo::get(from)->getState();
    TR::CFGEdgeList &successors = from->getSubGraphNode()->getSuccessors();
    for (TR::CFGEdgeList::iterator iter = successors.begin(); iter != successors.end(); ++iter)
        {
        TR::CFGEdge *edge = *iter;
        _edgeStates[edge] = new (_region) AnalysisInfo(false, state);
        }
    }

AbsEnvStatic *
TR_AbstractInterpretation::getSizedStartState(TR_Structure *structure)
  {
  // Get initial state by finding a predeccessor that has already been visited
  TR::CFGEdgeList &edges = structure->getSubGraphNode()->getPredecessors();
  for (TR::CFGEdgeList::iterator edgeIt = edges.begin(); edgeIt != edges.end(); ++edgeIt)
    {
    TR::CFGEdge *edge = *edgeIt;
    if (_edgeStates.count(edge) != 0)
        {
        return _edgeStates[edge]->getState()->getWidened();
        }
    }
  if (structure->getParent() == nullptr)
    {
    //FIXME TODO:
    //return new (_region) AbsEnvStatic(_region, _method);
    return new (_region) AbsEnvStatic(_region, _method, NULL);
    }
  return getSizedStartState(structure->getParent());
  }

bool
TR_AbstractInterpretation::structureHasUnanalyzedPredecessorEdges(TR_Structure *structure)
    {
    TR::CFGEdgeList &edges = structure->getSubGraphNode()->getPredecessors();
    for (TR::CFGEdgeList::iterator edgeIt = edges.begin(); edgeIt != edges.end(); ++edgeIt)
        {
        TR::CFGEdge *edge = *edgeIt;
        if (_edgeStates.count(edge) == 0)
            {
            return true;
            }
        }
    return false;
    }
