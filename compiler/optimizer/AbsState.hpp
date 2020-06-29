#ifndef ABS_STATE_INCL
#define ASB_STATE_INCL

#include "env/Region.hpp"
#include "optimizer/IDTNode.hpp"
#include "optimizer/AbsVarArrayStatic.hpp"
#include "optimizer/AbsOpStackStatic.hpp"
#include "optimizer/AbsValue.hpp"

class AbsState
    {
    public:
    AbsState(TR::Region &region, TR::ResolvedMethodSymbol* symbol);
    AbsState(const AbstractState&);
    //For AbsArray
    void set(unsigned int, AbsValue*);
    AbsValue *at(unsigned int);
    //For AbsOpStack
    void push(AbsValue *);
    AbsValue* pop();
    AbsValue* top();
    
    void merge(AbstractState &, TR::ValuePropagation*);
    size_t getStackSize();
    size_t getArraySize();
    void trace(TR::ValuePropagation*);

    private:
    UDATA maxStack();
    IDATA maxLocal();
    TR::Region& _region;
    AbsVarArrayStatic _array;
    AbsOpStackStatic _stack;
    };


#endif