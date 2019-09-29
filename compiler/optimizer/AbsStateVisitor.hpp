#pragma once

class AbstractState;
class AbsOpStackStatic;
class AbsVarArrayStatic;
class AbsValue;
class AbstractStateVisitor
    {
    public:
    virtual void visitState(AbstractState *a);
    virtual void visitState(AbstractState *a, AbstractState *b);
    virtual void visitStack(AbsOpStackStatic *a);
    virtual void visitStack(AbsOpStackStatic *a, AbsOpStackStatic *b);
    virtual void visitArray(AbsVarArrayStatic *a);
    virtual void visitArray(AbsVarArrayStatic *a, AbsVarArrayStatic *b);
    virtual void visitValue(AbsValue *a, AbsValue *b);;
    virtual void visitValue(AbsValue *a);;
    };
