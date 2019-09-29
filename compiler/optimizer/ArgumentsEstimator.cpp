#include "compiler/optimizer/ArgumentsEstimator.hpp"

class DependencyNode
    {
    private:
    bool _mustWiden;
    DependencyNode *_dependsOn;
    int _nestingDepth;
    public:
    DependencyNode(int nestingDepth) :
        _mustWiden(false),
        _dependsOn(NULL),
        _nestingDepth(nestingDepth)
     {}
    void setDependsOn(DependencyNode *with)
        {
        TR_ASSERT(with->_dependsOn != this, "Cannot chain dependencies cyclically");
        if (with == this)
            {
            return;
            }
        _dependsOn = with;
        }
    bool mustWiden()
        {
        if (_dependsOn == NULL)
            {
            return _mustWiden;
            }
        return _dependsOn->mustWiden();
        }
    };

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
    ArgumentsEstimator *_state;
    ArgumentsEstimator *_regionPreState;
    public:
    AnalysisInfo(bool isEntry):
        _isEntry(isEntry),
        _state(nullptr),
        _regionPreState(nullptr) {}
    AnalysisInfo(bool isEntry, ArgumentsEstimator *state):
        _isEntry(isEntry),
        _state(state),
        _regionPreState(nullptr) {}
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
    void setState(ArgumentsEstimator *state)
        {
        _state = state;
        }
    ArgumentsEstimator *getState()
        {
        return _state;
        }
    ArgumentsEstimator *getRegionPreState()
        {
        return _regionPreState;
        }
    void setRegionPreState(ArgumentsEstimator *v)
        {
        _regionPreState = v;
        }
    };

class InitializePreLoop : public StateTransformation
    {
    private:
    int _nestingDepth;
    public:
    InitializePreLoop(int nestingDepth) :
        _nestingDepth(nestingDepth) {}
    virtual AbsValue *transform(TR::Region &mem, AbsValue *v)
        {
        DependencyNode *dn = new (mem) DependencyNode(_nestingDepth);
        return v;
        }
    };

class Widen : public StateTransformation
    {
    public:
    virtual AbsValue *transform(TR::Region &mem, AbsValue *v)
        {
        return v->getWidened(mem);
        }
    };

ArgumentsEstimator::ArgumentsEstimator(TR::Region &region, IDT::Node *method, AbsFrame *frame) :
    AbsEnvStatic(region, method, frame)
    { }

ArgumentsEstimator::ArgumentsEstimator(AbsEnvStatic &frame) :
    AbsEnvStatic(frame)
    { }

void
ArgumentsEstimator::applyTransformation(StateTransformation *t)
    {
    TR::Region &mem = getRegion();
    for (auto iter = getStack()._stack.begin(); iter != getStack()._stack.end(); ++iter)
        {
        *iter = t->transform(mem, *iter);
        }
    for (auto iter = getArray()._array.begin(); iter != getArray()._array.end(); ++iter)
        {
        *iter = t->transform(mem, *iter);
        }
    }

  
AbsFrameArgumentsEstimator::AbsFrameArgumentsEstimator(TR::Region& region,
                         IDT::Node *method,
                         TR::ValuePropagation* vp,
                         TR_RegionStructure *rootStructure,
                         TR::Compilation *comp,
                         bool trace): 
    AbsFrame(region, method),
    _vp(vp),
    _rootStructure(rootStructure),
    _comp(comp),
    _trace(trace),
    _edgeStates(EdgeMapComparator(), EdgeMapAllocator(_region))//,
    //_merger(this),
    //_backEdgeMerger(this)
    {}

void
AbsFrameArgumentsEstimator::interpret()
    {
    initializeAnalysisState();
    analyzeRegionStructure(_rootStructure);
    }

void
AbsFrameArgumentsEstimator::analyzeStructure(TR_Structure *structure)
    {
    if (structure->asBlock() != nullptr)
        {
        analyzeBlockStructure(structure->asBlock());
        }
    else
        {
        analyzeRegionStructure(structure->asRegion());
        }
    }

void
AbsFrameArgumentsEstimator::analyzeBlockStructure(TR_BlockStructure *structure)
    {
    AnalysisInfo *ainfo = AnalysisInfo::get(structure);
    ainfo->setState(getIncomingState(structure));
    ainfo->getState()->getFrame()->interpret(structure->getBlock(), ainfo->getState());
    setSuccessorEdgeStates(structure);
    }

void
AbsFrameArgumentsEstimator::analyzeRegionStructure(TR_RegionStructure *structure)
    {
    AnalysisInfo *ainfo = AnalysisInfo::get(structure);

    ArgumentsEstimator *preState = getIncomingState(structure);
    ainfo->setRegionPreState(preState);


    // Improper region
    if (!structure->isNaturalLoop() && !structure->isAcyclic())
        {
        Widen w;
        ainfo->getRegionPreState()->applyTransformation(&w);
        }
    if (structure->isNaturalLoop())
        {
        InitializePreLoop init(structure->getNestingDepth());
        }
    for (RegionIterator ri(structure, _region); ri.getCurrent(); ri.getNext())
        {
        TR_StructureSubGraphNode *node = ri.getCurrent();
        analyzeStructure(node->getStructure());
        }
    mergeRegionBackedges(structure);
    finishRegionAnalysis(structure);
    }

void
AbsFrameArgumentsEstimator::finishRegionAnalysis(TR_RegionStructure *structure)
    {
    AnalysisInfo *ainfo = AnalysisInfo::get(structure);
    ainfo->setState(nullptr);
    // Could this be more precise? We don't know the from node of exit edges?
    for (ListElement<TR::CFGEdge> *iter = structure->getExitEdges().getListHead(); iter != nullptr; iter = iter->getNextElement())
        {
        TR::CFGEdge *edge = iter->getData();
        AnalysisInfo *edgeInfo = _edgeStates[edge];
        if (ainfo->getState() == nullptr)
            {    
            ainfo->setState(new (_region) ArgumentsEstimator(*edgeInfo->getState()));
            }
        else
            {
            ainfo->getState()->merge(*edgeInfo->getState());
            }        
        }
    setSuccessorEdgeStates(structure);
    }

ArgumentsEstimator *
AbsFrameArgumentsEstimator::getMethodExitState()
    {
    return AnalysisInfo::get(_rootStructure)->getState();
    }

void
AbsFrameArgumentsEstimator::initializeAnalysisState()
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

ArgumentsEstimator *
AbsFrameArgumentsEstimator::getIncomingState(TR_Structure *structure)
    {
    AnalysisInfo *ainfo = AnalysisInfo::get(structure);
    ArgumentsEstimator *state = nullptr;
    if (ainfo->isEntry())
        {
        TR_RegionStructure *parent = structure->getParent();
        ArgumentsEstimator *preRegion = AnalysisInfo::get(parent)->getRegionPreState();
        state = new (_region) ArgumentsEstimator(*preRegion);
        if (structureHasUnanalyzedPredecessorEdges(structure))
            {
            return state;
            }
        }
    if (structure->getSubGraphNode() != nullptr)
        {
        TR::CFGEdgeList &predecessors = structure->getSubGraphNode()->getPredecessors();
        for (TR::CFGEdgeList::iterator predEdgeIter = predecessors.begin(); predEdgeIter != predecessors.end(); ++predEdgeIter)
            {
            TR::CFGEdge *predEdge = *predEdgeIter;
            TR_ASSERT(_edgeStates.count(predEdge) != 0, "Predecessor edge hasn't been analyzed yet");
            ArgumentsEstimator *incomingState = _edgeStates[predEdge]->getState();
            TR_ASSERT(incomingState != nullptr, "Predecessor edge hasn't been analyzed yet");
            if (state == nullptr)
                {
                state = new (_region) ArgumentsEstimator(*incomingState);
                }
            else
                {
                state->merge(*incomingState);
                }
            }
        }
    // TODO handle widening of backedge values here
    // TODO Also don't recurse parent state
    /*if (ainfo->isEntry() && ainfo->getLoopPreState() != nullptr)
        {
        ArgumentsEstimator *parentState = getIncomingState(structure->getParent());
        state = ArgumentsEstimator::mergeIdenticalValuesBottom(ainfo->getLoopPreState(), state);
        state->merge(*parentState);
        }
    else */if (state == nullptr)
        {
        state = new (_region) ArgumentsEstimator(_region, _node, this);
        }
    return state;
    }

void 
AbsFrameArgumentsEstimator::setSuccessorEdgeStates(TR_Structure *from)
    {
    if (from->getSubGraphNode() == nullptr)
        {
        return;
        }
    ArgumentsEstimator *state = AnalysisInfo::get(from)->getState();
    TR::CFGEdgeList &successors = from->getSubGraphNode()->getSuccessors();
    for (TR::CFGEdgeList::iterator iter = successors.begin(); iter != successors.end(); ++iter)
        {
        TR::CFGEdge *edge = *iter;
        _edgeStates[edge] = new (_region) AnalysisInfo(false, state);
        }
    }

bool
AbsFrameArgumentsEstimator::structureHasUnanalyzedPredecessorEdges(TR_Structure *structure)
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

void 
AbsFrameArgumentsEstimator::mergeRegionBackedges(TR_RegionStructure *region)
    {
    if (!region->isNaturalLoop())
        {
        return;
        }
    
    }


AbsValue *
AbsFrameArgumentsEstimator::createAbsValue(TR::VPConstraint *vp, TR::DataType dt)
    {
    return NULL;
    }


/*
AbsValue *
AbsFrameArgumentsEstimator::ControlFlowMerger::merge(AbsValue *a, AbsValue *b)
    {
    if (a == b)
      {
      return a;
      }
    if (a == AbsValue::getBottom())
      {
      return b;
      }
    if (b == AbsValue::getBottom())
      {
      return a;
      }
    if (!a->_vp) return a;
    if (!b->_vp) return b;
    TR::VPConstraint *constraint = a->_vp->merge(b->_vp, _frame->getValuePropagation());
    AbsValue *v = _frame->createAbsValue(constraint, a->_dt);
    return v;
    }

AbsValue *
AbsFrameArgumentsEstimator::BackEdgeMerger::merge(AbsValue *a, AbsValue *b)
    {
    
    }

AbsValue *
AbsFrameArgumentsEstimator::DependencyMerger::merge(AbsValue *a, AbsValue *b)
    {
    
    */