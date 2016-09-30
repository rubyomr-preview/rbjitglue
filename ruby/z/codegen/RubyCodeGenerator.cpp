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


#include "codegen/CodeGenerator.hpp"
#include "z/codegen/TRSystemLinkage.hpp"
#include "compile/Compilation.hpp"
#include "env/CompilerEnv.hpp"

Ruby::Z::CodeGenerator::CodeGenerator() :
   OMR::CodeGeneratorConnector()
   {
   }


TR::Linkage *
Ruby::Z::CodeGenerator::createLinkage(TR_LinkageConventions lc)
   {
   // *this    swipeable for debugging purposes
   bool lockLitPoolRegister = false;
   TR::Linkage * linkage;
   TR::Compilation *comp = self()->comp();
   switch (lc)
      {
      case TR_Helper:
         linkage = new (self()->trHeapMemory()) TR_S390zLinuxSystemLinkage(self());
         lockLitPoolRegister = true;
         break;

      case TR_Private:
        // no private linkage, fall through to system

      case TR_System:
         if (TR::Compiler->target.isLinux())
            {
            linkage = new (self()->trHeapMemory()) TR_S390zLinuxSystemLinkage(self());
            lockLitPoolRegister = true;
            }
         else
            {
            linkage = new (self()->trHeapMemory()) TR_S390zOSSystemLinkage(self());
            }
         break;

      default :
         TR_ASSERT(0, "\nTestarossa error: Illegal linkage convention %d\n", lc);
      }

   if (lockLitPoolRegister)
      {
      // TR_S390zLinuxSystemLinkage uses GPR13 for the literal pool, yet that
      // register remains unlocked as most other projects seem to be managing
      // that register separately. In the future, we should instead have a
      // Ruby::Z::Linux::Linkage (or whatever the class would work out to),
      // however, in the short term I'm going to lock the single register to avoid
      // having to create a whole new subclass of TR_S390zLinuxSystemLinkage.
      self()->machine()->getRegisterFile( linkage->getLitPoolRegister() )->setState(TR::RealRegister::Locked);
      }

   self()->setLinkage(lc, linkage);
   return linkage;
   }

