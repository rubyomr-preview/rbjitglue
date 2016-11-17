/*******************************************************************************
 *
 * (c) Copyright IBM Corp. 2000, 2016
 *
 *  This program and the accompanying materials are made available
 *  under the terms of the Eclipse Public License v1.0 and
 *  Apache License v2.0 which accompanies this distribution.
 *
 *      The Eclipse Public License is available at
 *      http://www.eclipse.org/legal/epl-v10.html
 *
 *      The Apache License v2.0 is available at
 *      http://www.opensource.org/licenses/apache2.0.php
 *
 * Contributors:
 *    Multiple authors (IBM Corp.) - initial implementation and documentation
 *******************************************************************************/


#ifndef RUBYMETHOD_INCL
#define RUBYMETHOD_INCL

#include "compile/Method.hpp"
#include "compile/ResolvedMethod.hpp"
#include "env/VMHeaders.hpp" 

class RubyMethodBlock
   {
   public:
   RubyMethodBlock(rb_iseq_t *iseq, const char *name)
      : _iseq(iseq), _name(name)
      {
      // Ensure that the original iseq exists, as in v222, raw 
      // iseqs are deleted. 
      //
      // To save memory, we could free this if created here, 
      // hoewver for now, we leave it behind to fastpath 
      // future accesses. 
      _original_bytecodes = rb_iseq_original_iseq(iseq); // FIXME: Currently will leak!
      }

   const rb_iseq_t         *iseq()             const { return _iseq; }
   const char              *name()             const { return _name; }
   unsigned long            size()             const { return _iseq->body->iseq_size; }
   const VALUE             *bytecodes()        const { return _original_bytecodes; }
   const VALUE             *bytecodesEncoded() const { return _iseq->body->iseq_encoded; }
   size_t                   stack_max()        const { return _iseq->body->stack_max; }

   private:
   const rb_iseq_t          *_iseq;
   const char               *_name;
   const VALUE              *_original_bytecodes;  
   };


class RubyMethod : public TR_Method
   {
   public:
   TR_ALLOC(TR_Memory::Method);

   RubyMethod() : TR_Method(TR_Method::Ruby) {}

   // Better answers could happen here.  
   virtual uint16_t    classNameLength() { return strlen(classNameChars()); }
   virtual uint16_t    nameLength() { return strlen(nameChars()); }
   virtual uint16_t    signatureLength() { return strlen(signatureChars()); }
   virtual char       *classNameChars() { return "noclass"; }
   virtual char       *signatureChars() { return "(Lrb_thread_t;)V"; }

   virtual bool        isConstructor() { return false; }
   virtual bool        isFinalInObject() { return false; }
   };

class ResolvedRubyMethodBase : public TR_ResolvedMethod
   {
   public:
   ResolvedRubyMethodBase(RubyMethodBlock &method)
      : _method(method)
         { }

   virtual const char * signature(TR_Memory *, TR_AllocationKind) { return _method.name(); }

   // This group of functions only make sense for Java - we ought to provide answers from that definition
   virtual bool                  isConstructor() { return false; } // returns true if this method is object constructor.
   virtual bool                  isStatic() { return true; }       // ie. has no corresponding class
   virtual bool                  isAbstract() { return false; }    // we do have a concrete impl.
   virtual bool                  isCompilable(TR_Memory *) { return true; }   
   virtual bool                  isNative() { return false; }      // ie. not a JNI native
   virtual bool                  isSynchronized() { return false; }// no monitors required
   virtual bool                  isPrivate() { return false; }     //
   virtual bool                  isProtected() { return false; }   //
   virtual bool                  isPublic() { return true; }       // public scope
   virtual bool                  isFinal() { return false; }       
   virtual bool                  isStrictFP() { return false; }    
   virtual bool                  isSubjectToPhaseChange(TR::Compilation *comp) { return false; }
   virtual bool                  hasBackwardBranches() { return false; }
   virtual void *                resolvedMethodAddress() {   return (void *)getPersistentIdentifier(); }

   virtual uint16_t              numberOfParameterSlots() { return 1; }
   virtual TR::DataType          parmType(uint32_t slot)  { return TR::Address; }
   virtual uint16_t              numberOfTemps()          { return 0; } 

   virtual uint32_t              maxBytecodeIndex() { return _method.size(); }

   virtual bool                  isNewInstanceImplThunk() { return false; }
   virtual bool                  isJNINative() { return false; }
   virtual bool                  isJITInternalNative() { return false; }

   virtual TR_OpaqueMethodBlock *getPersistentIdentifier() { return (TR_OpaqueMethodBlock *) &_method; }
   RubyMethodBlock &getRubyMethodBlock()    { return _method; }
   const VALUE*           code()            { return _method.bytecodes(); }

   uint32_t                      numberOfExceptionHandlers() { return 0; }

   virtual bool                  isInterpreted() { return true; } 
   virtual void *                startAddressForJittedMethod() { return 0; } 
   virtual void *                startAddressForInterpreterOfJittedMethod() { return 0; }

   virtual bool                  isSameMethod(TR_ResolvedMethod *other)
      { return getPersistentIdentifier() == other->getPersistentIdentifier(); }
   virtual TR_OpaqueClassBlock   *classOfMethod() { return 0; }

   protected:
   RubyMethodBlock &_method;
   };

class ResolvedRubyMethod : public ResolvedRubyMethodBase, public RubyMethod
   {
public:
   ResolvedRubyMethod(RubyMethodBlock &method) : ResolvedRubyMethodBase(method), RubyMethod() { }

   virtual TR_Method *convertToMethod() { return this; }

   virtual uint16_t    classNameLength() { return RubyMethod::classNameLength(); }
   virtual uint16_t    nameLength() { return RubyMethod::nameLength(); }
   virtual uint16_t    signatureLength() { return RubyMethod::signatureLength(); }
   virtual char       *classNameChars() { return RubyMethod::signatureChars(); }
   virtual char       *nameChars()      { return RubyMethod::signatureChars(); }
   virtual char       *signatureChars() { return RubyMethod::signatureChars(); }

   virtual bool        isConstructor() { return false; }
   virtual bool        isFinalInObject() { return false; }
   virtual bool        isNonEmptyObjectConstructor() { return false; }
   virtual TR::DataType        returnType()                           { return TR::NoType; }
   };

#endif
