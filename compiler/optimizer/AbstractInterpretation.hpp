#pragma once

#include <map>
#include <functional>

#include "compiler/optimizer/DataFlowAnalysis.hpp"
#include "compiler/optimizer/Structure.hpp"
#include "compiler/optimizer/AbsEnvStatic.hpp"
#include "compiler/optimizer/ValuePropagation.hpp"
#include "compiler/optimizer/IDT.hpp"
#include "compiler/infra/deque.hpp"

#include "compiler/env/Region.hpp"
#include "compiler/env/TypedAllocator.hpp"

class AnalysisInfo;

class TR_AbstractInterpretation
    {
    private:
    TR::Region &_region;
    IDT::Node *_method;
    TR::ValuePropagation *_vp;
    TR_RegionStructure *_rootStructure;
    TR::Compilation *_comp;
    bool _trace;
    typedef TR::typed_allocator<std::pair<TR_Structure * const, AnalysisInfo *>, TR::Region&> EdgeMapAllocator;
    typedef std::less<TR::CFGEdge *> EdgeMapComparator;
    typedef std::map<TR::CFGEdge *, AnalysisInfo *, EdgeMapComparator, EdgeMapAllocator> EdgeMap;
    EdgeMap _edgeStates;

    public:
    TR_AbstractInterpretation(TR::Region& region,
                         IDT::Node *method,
                         TR::ValuePropagation* vp,
                         TR_RegionStructure *rootStructure,
                         TR::Compilation *comp,
                         bool trace);
    int32_t perform();
    bool analyzeStructure(TR_Structure *, bool);
    bool analyzeBlockStructure(TR_BlockStructure *, bool);
    bool analyzeRegionStructure(TR_RegionStructure *, bool);
    AbsEnvStatic *getExitState();
    private:
    void initializeAnalysisState();
    AbsEnvStatic *getIncomingState(TR_Structure *structure);
    void setSuccessorEdgeStates(TR_Structure *from);
    AbsEnvStatic *getSizedStartState(TR_Structure *);
    bool structureHasUnanalyzedPredecessorEdges(TR_Structure *structure);
    };
