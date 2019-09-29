#include "AbsStateVisitor.hpp"
#include "AbsEnvStatic.hpp"
#include "AbsOpStackStatic.hpp"
#include "AbsValue.hpp"
#include "AbsVarArrayStatic.hpp"

void
AbstractStateVisitor::visitState(AbstractState* a)
    {}
void
AbstractStateVisitor::visitState(AbstractState* a, AbstractState* b)
    {}
void
AbstractStateVisitor::visitStack(AbsOpStackStatic* a)
    {}
void
AbstractStateVisitor::visitStack(AbsOpStackStatic* a, AbsOpStackStatic* b)
    {}
void
AbstractStateVisitor::visitArray(AbsVarArrayStatic* a)
    {}
void
AbstractStateVisitor::visitArray(AbsVarArrayStatic* a, AbsVarArrayStatic* b)
    {}
void
AbstractStateVisitor::visitValue(AbsValue* a, AbsValue* b)
    {}
void
AbstractStateVisitor::visitValue(AbsValue* a)
    {}