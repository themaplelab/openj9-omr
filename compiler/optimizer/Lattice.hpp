#ifndef LATTICE_INCL
#define LATTICE_INCL

#include <stdint.h>
#include "il/OMRDataTypes.hpp"
#include "env/Region.hpp"
#include "env/jittypes.h"

class IntLattice;
class LongLattice;
class ComposedLattice;

class Lattice
   {
   public:
   Lattice();

   virtual bool isTop() { return true; };

   static Lattice* createTop(TR::Region& region) { return new (region) Lattice(); };

   virtual Lattice* merge(Lattice* other, TR::Region& region) { return new (region) Lattice(); };

   virtual IntLattice* asIntLattice() { return NULL; };
   virtual LongLattice* asLongLattice() { return NULL; };
   virtual ComposedLattice* asComposedLattice() { return NULL; };
   };

class BooleanLattice : public Lattice
   {
   public:
   BooleanLattice(TR_YesNoMaybe yesNoMaybe)
      : Lattice(),
      _yesNoMaybe(yesNoMaybe)
   {};

   static BooleanLattice* createTop(TR::Region& region) { return new (region) BooleanLattice(TR_YesNoMaybe::TR_maybe); };
   static BooleanLattice* createTrue(TR::Region& region) { return new (region) BooleanLattice(TR_YesNoMaybe::TR_yes); };
   static BooleanLattice* createFalse(TR::Region& region) { return new (region) BooleanLattice(TR_YesNoMaybe::TR_no); };

   virtual Lattice* merge(Lattice* other, TR::Region& region);

   bool isTop() { return _yesNoMaybe == TR_YesNoMaybe::TR_maybe; };
   bool isTrue() { return _yesNoMaybe == TR_YesNoMaybe::TR_yes; };
   bool isFalse() { return _yesNoMaybe == TR_YesNoMaybe::TR_no; };

   private:
   TR_YesNoMaybe _yesNoMaybe;
   
   };

class IntLattice : public Lattice
   {
   public:
   IntLattice(int32_t value)
         : Lattice(),
         _low(value),
         _high(value)
      {};

   IntLattice(int32_t low, int32_t high)
         : Lattice(),
         _low(low),
         _high(high)
      {};

    
   bool isConst() { return _low == _high; };
   bool isRange() { return !isConst(); };

   int32_t getLow() { return _low; };
   int32_t getHigh() { return _high; };

   virtual bool isTop() { return _low == INT32_MIN && _high == INT32_MAX; };

   static IntLattice* createTop(TR::Region& region) { return new (region) IntLattice(INT_MIN, INT_MAX); };

   virtual Lattice* merge(Lattice* other, TR::Region& region);

   IntLattice* add(IntLattice* other, TR::Region& region);
   IntLattice* sub(IntLattice* other, TR::Region& region);
   IntLattice* mul(IntLattice* other, TR::Region& region);
   IntLattice* div(IntLattice* other, TR::Region& region);
   IntLattice* rem(IntLattice* other, TR::Region& region);
   
   IntLattice* _and(IntLattice* other, TR::Region& region);
   IntLattice* _or(IntLattice* other, TR::Region& region);
   IntLattice* _xor(IntLattice* other, TR::Region& region);

   IntLattice* shl(IntLattice* other, TR::Region& region);
   IntLattice* shr(IntLattice* other, TR::Region& region);
   IntLattice* ushr(IntLattice* other, TR::Region& region);

   IntLattice* neg(TR::Region& region);
   LongLattice* toLong(TR::Region& region);
   Lattice* toDouble(TR::Region& region);
   Lattice* toFloat(TR::Region& region);

   virtual IntLattice* asIntLattice() { return this; };
   virtual LongLattice* asLongLattice() { return NULL; };
   virtual ComposedLattice* asComposedLattice() { return NULL; };

   private:
   int32_t _low;
   int32_t _high;

   };

class LongLattice : public Lattice
   {
   public:
   LongLattice(int64_t value)
         : Lattice(),
         _low(value),
         _high(value)
      {};

   LongLattice(int64_t low, int64_t high)
         : Lattice(),
         _low(low),
         _high(high)
      {};

   bool isConst() { return _low == _high; };
   bool isRange() { return !isConst(); };

   int64_t getLow() { return _low; };
   int64_t getHigh() { return _high; };

   virtual bool isTop() { return _low == INT64_MIN && _high == INT64_MAX; };

   static LongLattice* createTop(TR::Region& region) { return new (region) LongLattice(INT64_MIN, INT64_MAX); }

   virtual Lattice* merge(Lattice* other, TR::Region& region);

   LongLattice* add(LongLattice* other, TR::Region& region);
   LongLattice* sub(LongLattice* other, TR::Region& region);
   LongLattice* mul(LongLattice* other, TR::Region& region);
   LongLattice* div(LongLattice* other, TR::Region& region);
   LongLattice* rem(LongLattice* other, TR::Region& region);
   
   LongLattice* _and(LongLattice* other, TR::Region& region);
   LongLattice* _or(LongLattice* other, TR::Region& region);
   LongLattice* _xor(LongLattice* other, TR::Region& region);

   LongLattice* shl(IntLattice* other, TR::Region& region);
   LongLattice* shr(IntLattice* other, TR::Region& region);
   LongLattice* ushr(IntLattice* other, TR::Region& region);

   LongLattice* neg(TR::Region& region);

   LongLattice* toLong(TR::Region& region);
   Lattice* toDouble(TR::Region& region);
   Lattice* toFloat(TR::Region& region);
   IntLattice* toInt(TR::Region& region);

   virtual IntLattice* asIntLattice() { return NULL; };
   virtual LongLattice* asLongLattice() { return this; };
   virtual ComposedLattice* asComposedLattice() { return NULL; };

   private:
   int64_t _low;
   int64_t _high;
   };

class ComposedLattice: public Lattice
   {
   public:
   ComposedLattice(TR_OpaqueClassBlock* klass, bool isNull, IntLattice* arrayLength):
         Lattice(),
         _klass(klass),
         _isNull(isNull),
         _arrayLength(arrayLength)
      {};
   
   bool isNull() { return _isNull; };
   bool isArray() { return _arrayLength->getHigh() > 0 && _arrayLength->getLow() > 0; };

   private:
   TR_OpaqueClassBlock* _klass;
   bool _isNull;
   IntLattice* _arrayLength;

   };


#endif