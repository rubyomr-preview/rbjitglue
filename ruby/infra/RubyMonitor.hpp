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

#ifndef RUBY_MONITOR_INCL
#define RUBY_MONITOR_INCL

#ifndef RUBY_MONITOR_CONNECTOR
#define RUBY_MONITOR_CONNECTOR
namespace Ruby { class Monitor; }
namespace Ruby { typedef Ruby::Monitor MonitorConnector; }
#endif

#include "infra/OMRMonitor.hpp"

#include <pthread.h>

namespace Ruby
{

class Monitor : public OMR::MonitorConnector
   {
   public:

   Monitor(char const *name);

   ~Monitor();

   static TR::Monitor *create(char *name);

   static void *operator new(size_t size);

   static void operator delete(void *p);

   void enter();

   int32_t exit();

   private:

   char const *_name;
   pthread_mutex_t _monitor;
   };

}

#endif
