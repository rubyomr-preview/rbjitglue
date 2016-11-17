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

#include "ilgen/RubyByteCodeIterator.hpp"
#include "ilgen/RubyByteCodeIteratorWithState.hpp"
/* Ruby */
#include "insns_info.inc"
#include "ruby/version.h"
#include "env/IO.hpp"
#include "vm_core.h"

RubyByteCode TR_RubyByteCodeIterator::at(int32_t index) const
   {
   if (index >= _maxByteCodeIndex)
      {
      TR_ASSERT(0, "index out of bounds %d\n", index);
      return VM_INSTRUCTION_SIZE;
      }
   return bytecodes()[index];
   }

RubyByteCode TR_RubyByteCodeIterator::next()
   {
   int32_t len = byteCodeLength(current());
   _bcIndex += len;
   return current();
   }

int32_t     TR_RubyByteCodeIterator::byteCodeLength(RubyByteCode bc) const { return insn_len(bc); }
const char *TR_RubyByteCodeIterator::operandTypes(RubyByteCode bc)   const { return insn_op_types(bc); }
const char *TR_RubyByteCodeIterator::byteCodeName(RubyByteCode bc)   const { return insn_name_info[bc]; }

int32_t
TR_RubyByteCodeIterator::branchDestination(int32_t base) const
   {
   int32_t len = byteCodeLength(at(base));
   int32_t offset = at(base+1); /*getOperand(base, 1);*/
   return base + len + offset;
   }


rb_iseq_t const *
TR_RubyByteCodeIterator::getParentIseq(rb_num_t level) const
   {
   const rb_iseq_t *iseq = mb().iseq();
   for (int i = 0; i < level; ++i)
      {
      iseq = iseq->body->parent_iseq;
      TR_ASSERT(iseq, "parent_iseq for level %d is null", i);
      }
   return iseq;
   }

const char *
TR_RubyByteCodeIterator::getLocalName(lindex_t index, const rb_iseq_t *iseq) const
   {
   static const char* unknownLocalName = "?";

   const ID *tbl = iseq->body->local_table;
   TR_ASSERT(tbl, "local_table must be non-null");
   TR_ASSERT(index <= iseq->body->local_table_size + VM_ENV_DATA_SIZE, "invalid local index %d, iseq->local_size is %d, VM_ENV_DATA_SIZE is %d",
             index,   iseq->body->local_table_size, VM_ENV_DATA_SIZE);

   int32_t tableIndex = iseq->body->local_table_size - index;

   // Should call id_to_name instead of rb_id2name directly
   const char *name = fe()->id2name(tbl[tableIndex]);
   return (name) ? name : unknownLocalName;
   }

void
TR_RubyByteCodeIterator::printLocalTables() const
   {
   const rb_iseq_t *const iseq = mb().iseq();
   const rb_iseq_t *cur  = iseq;
   trfprintf(comp()->getOutFile(), "Local Tables:\n");
   int32_t level = 0;
   do
      {
      printLocalTable(cur, level);
      cur = cur->body->parent_iseq;
      level++;
      }
   while (cur);
   trfprintf(comp()->getOutFile(), "----\n");
   }

void
TR_RubyByteCodeIterator::printLocalTable(rb_iseq_t const *iseq, int32_t const level) const
   {
   const ID *tbl = iseq->body->local_table;

   if (tbl)
      {
      trfprintf(comp()->getOutFile(), "| level %d\n", level);

      for (int i = iseq->body->local_table_size-1; i >= 0; --i)
         {
         lindex_t localIndex = iseq->body->local_table_size - i;
         const char *name = getLocalName(localIndex, iseq);

         if (i == iseq->body->local_table_size-1) // if first-time
            trfprintf(comp()->getOutFile(), "|\t");

         trfprintf(comp()->getOutFile(), "[%d] %s",
                   localIndex,
                   name ? name : "?");

         if (iseq->body->param.flags.has_opt)
            {
            int argc = iseq->body->param.size;
            if (i >= argc && i < (argc + iseq->body->param.opt_num - 1))
               {
               trfprintf(comp()->getOutFile(),
                         "(Opt=%d)", iseq->body->param.opt_table[i - argc]);
               }
            }
         trfprintf(comp()->getOutFile(), "%s", (i == 0) ? "\n" : ", ");
         }
      }
   }

// There is similar code in iseq.c that could be shared with VM changes. 
const char*
catch_type(iseq_catch_table_entry::catch_type t)
   {
   switch (t)
      {
      case iseq_catch_table_entry::CATCH_TYPE_RESCUE: return "rescue";
      case iseq_catch_table_entry::CATCH_TYPE_ENSURE: return "ensure";
      case iseq_catch_table_entry::CATCH_TYPE_RETRY:  return "retry";
      case iseq_catch_table_entry::CATCH_TYPE_BREAK:  return "break";
      case iseq_catch_table_entry::CATCH_TYPE_REDO:   return "redo";
      case iseq_catch_table_entry::CATCH_TYPE_NEXT:   return "next";
      }
   TR_ASSERT(0, "invalid catch type");
   return 0;
   }

void
TR_RubyByteCodeIterator::printArgsTable(const rb_iseq_t *iseq) const
   {
#if RUBY_API_VERSION_MAJOR >= 2 && RUBY_API_VERSION_MINOR >= 2
   trfprintf(comp()->getOutFile(),
             "local table analysis not yet implemented for ruby v2.2.2\n");
#else
   trfprintf(comp()->getOutFile(),
             "local_table_size: %d argc: %d arg_size: %d, arg_simple: %x\n",
             iseq->local_table_size,
             iseq->argc,
             iseq->arg_size,
             iseq->arg_simple);
#endif
   }

void
TR_RubyByteCodeIterator::printCatchTable(const rb_iseq_t *iseq, int32_t level) const
   {
   if (iseq->body->catch_table == 0)
      return;

   trfprintf(comp()->getOutFile(), "Catch Table:\n");
   for (int i = 0; i < iseq->body->catch_table->size; i++)
      {
      const struct iseq_catch_table_entry &entry = iseq->body->catch_table->entries[i];
      trfprintf(comp()->getOutFile(), "| %-6s start:%04d end:%04d sp:%04d cont:%04d iseq:%p\n",
                catch_type(entry.type), (int)entry.start,
                (int)entry.end, (int)entry.sp, (int)entry.cont,
                entry.iseq);
      }
   }

void
TR_RubyByteCodeIteratorWithState::findAndMarkExceptionRanges()
   {
   const rb_iseq_t *iseq = mb().iseq();
   // TODO: this loop is also duplicated in printCatchTable
   for (int i = 0; iseq->body->catch_table && i < iseq->body->catch_table->size; i++)
      {
      const struct iseq_catch_table_entry &entry = iseq->body->catch_table->entries[i];
      markExceptionRange(i, (int)entry.start, (int)entry.end,
                         (int)entry.cont, (int)entry.type);
      }
   }

void
TR_RubyByteCodeIterator::printByteCodePrologue()
   {
   const char *s =
      "index op opcode                      operands\n"
      "----- -- --------------------------- ---------------------------------\n";
   trfprintf(comp()->getOutFile(), s);
   }

void
TR_RubyByteCodeIterator::printByteCodeEpilogue()
   {
   const rb_iseq_t *const iseq = mb().iseq();
   trfprintf(comp()->getOutFile(), "----\n");

   printArgsTable(mb().iseq());
   printCatchTable(mb().iseq(), 0);
   //printLocalTables();
   }

void
TR_RubyByteCodeIterator::printByteCode()
   {
   VALUE insn = current();
   int32_t len = byteCodeLength(insn);
   const char *types = operandTypes(insn);

   trfprintf(comp()->getOutFile(),
             "%04d: %2x %-28s",
             _bcIndex, insn, byteCodeName(insn));

   // Special case the printing of get/setlocal
   if (types[0] == TS_LINDEX)
      {
      lindex_t index = at(_bcIndex + 1);

      rb_num_t level = -1;
      switch (insn)
         {
         case BIN(getlocal_OP__WC__0):
         case BIN(setlocal_OP__WC__0):
            level = 0;
            break;
         case BIN(getlocal_OP__WC__1):
         case BIN(setlocal_OP__WC__1):
            level = 1;
            break;
         case BIN(getlocal):
         case BIN(setlocal):
            TR_ASSERT(types[1] == TS_NUM, "unexpected operand types for get/setlocal");
            level = at(_bcIndex + 2);
            break;
         default:
            TR_ASSERT(0, "Unexpected opcode with local-index (TS_LINDEX) as an operand");
         }
      trfprintf(comp()->getOutFile(), "%s (l%d i%d)\n",
                getLocalName(index, level), level, index);
      }
   else
      {
      // Loop over the operand arguments
      for (int j = 0; types[j]; ++j)
         {
         const char type = types[j];
         VALUE arg_j = at(_bcIndex + j + 1);

         switch (type)
            {
            case TS_OFFSET:
               trfprintf(comp()->getOutFile(), "%04" PRIuVALUE,
                         (VALUE) (_bcIndex + len + arg_j));
               break;

            case TS_NUM:
               trfprintf(comp()->getOutFile(), "%" PRIuVALUE,
                         arg_j, arg_j);
               break;

            case TS_CALLINFO:
               {
               rb_call_info *ci = (rb_call_info *) arg_j;

               if (ci->mid)       trfprintf(comp()->getOutFile(), "%s (mid:%d) ", fe()->id2name(ci->mid), ci->mid);

               trfprintf(comp()->getOutFile(), "<argc:%d", ci->orig_argc);
               //Print out the flags.
               trfprintf(comp()->getOutFile(), " flag:%#x", ci->flag);
               if(ci->flag > 0)
                  {
                  trfprintf(comp()->getOutFile(), " (");
#define PRINTCIFLAG(FLAG) \
                  (ci->flag & (FLAG) ) ? trfprintf(comp()->getOutFile(), #FLAG "|")       : 0

                  PRINTCIFLAG(VM_CALL_ARGS_SPLAT   ); 
                  PRINTCIFLAG(VM_CALL_ARGS_BLOCKARG); 
                  PRINTCIFLAG(VM_CALL_FCALL        ); 
                  PRINTCIFLAG(VM_CALL_VCALL        ); 
                  PRINTCIFLAG(VM_CALL_ARGS_SIMPLE  ); 
                  PRINTCIFLAG(VM_CALL_BLOCKISEQ    ); 
                  PRINTCIFLAG(VM_CALL_KWARG        ); 
                  PRINTCIFLAG(VM_CALL_TAILCALL     ); 
                  PRINTCIFLAG(VM_CALL_SUPER        ); 
                  PRINTCIFLAG(VM_CALL_OPT_SEND     ); 

#undef PRINTCIFLAG
                  trfprintf(comp()->getOutFile(), "0x0");
                  trfprintf(comp()->getOutFile(), ")");
                  }
               trfprintf(comp()->getOutFile(), ">");
               }
               break;

            case TS_IC:
               trfprintf(comp()->getOutFile(), "ic:%p", arg_j);
               break;

            default:
               trfprintf(comp()->getOutFile(), "%c:%lld", type, arg_j);
               break;
            }

         if (types[j+1])
            trfprintf(comp()->getOutFile(), ", ");
         }
      trfprintf(comp()->getOutFile(), "\n");
      }
   }
