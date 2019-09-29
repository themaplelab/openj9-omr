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
#include "compiler/optimizer/AbsStateVisitor.hpp"

class StateTransformation
    {
    public:
    virtual AbsValue *transform(TR::Region &mem, AbsValue *v) = 0;
    };


class ArgumentsEstimator : public AbsEnvStatic
    {
    public:
    ArgumentsEstimator(TR::Region &, IDT::Node *, AbsFrame *);
    ArgumentsEstimator(AbsEnvStatic &);
    void applyTransformation(StateTransformation *transformation);

    };

class AnalysisInfo;

class AbsFrameArgumentsEstimator : public AbsFrame
    {
    private:
    TR::ValuePropagation *_vp;
    TR_RegionStructure *_rootStructure;
    TR::Compilation *_comp;
    bool _trace;
    typedef TR::typed_allocator<std::pair<TR_Structure * const, AnalysisInfo *>, TR::Region&> EdgeMapAllocator;
    typedef std::less<TR::CFGEdge *> EdgeMapComparator;
    typedef std::map<TR::CFGEdge *, AnalysisInfo *, EdgeMapComparator, EdgeMapAllocator> EdgeMap;
    EdgeMap _edgeStates;

    /*class ControlFlowMerger : public AbstractStateVisitor
        {
        private:
        AbsFrameArgumentsEstimator *_frame;
        public:
        ControlFlowMerger(AbsFrameArgumentsEstimator *frame) : _frame(frame) {}
        virtual void merge(AbsValue *a, AbsValue *b);
        };
    class BackEdgeMerger : public AbstractStateVisitor
        {
        private:
        AbsFrameArgumentsEstimator *_frame;
        public:
        BackEdgeMerger(AbsFrameArgumentsEstimator *frame) : ControlFlowMerger(frame) {}
        virtual AbsValue *merge(AbsValue *a, AbsValue *b);
        };
    class DependencyMerger : public AbstractStateVisitor
        {
        public:
        virtual AbsValue *merge(AbsValue *a, AbsValue *b);
        };
    ControlFlowMerger _merger;
    BackEdgeMerger _backEdgeMerger;*/

    public:
    AbsFrameArgumentsEstimator(TR::Region& region,
                         IDT::Node *method,
                         TR::ValuePropagation* vp,
                         TR_RegionStructure *rootStructure,
                         TR::Compilation *comp,
                         bool trace);
    void analyzeStructure(TR_Structure *);
    void analyzeBlockStructure(TR_BlockStructure *);
    void analyzeRegionStructure(TR_RegionStructure *);
    ArgumentsEstimator *getMethodExitState();
    virtual void interpret();
    virtual AbsValue *createAbsValue(TR::VPConstraint *vp, TR::DataType dt);
    private:
    void finishRegionAnalysis(TR_RegionStructure *structure);
    void initializeAnalysisState();
    ArgumentsEstimator *getIncomingState(TR_Structure *structure);
    void setSuccessorEdgeStates(TR_Structure *from);
    ArgumentsEstimator *getSizedStartState(TR_Structure *);
    bool structureHasUnanalyzedPredecessorEdges(TR_Structure *structure);
    void mergeRegionBackedges(TR_RegionStructure *region);
    };
