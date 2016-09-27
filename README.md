The Ruby+OMR Just-In-Time Compiler
==================================

This repository contains the Ruby-specific JIT glue code that combines with the
[Eclipse OMR](https://github.com/eclipse/omr) to form a Just-In-Time compiler
for Ruby.  

Read more about the Ruby+OMR Preview in the [rubyomr-preview repo][omrpreview].

Current Status
--------------

This JIT compiler currently only supports a specially modified version of Ruby
2.2, and currently exercises only a small fraction of the optimizations that 
exist in OMR. 

Platforms: 
----------

Currently this JIT has only been built and tested on Linux, x86, POWER, and
z/Architecture (s390x). 

Currently, the OMR JIT doesn't support OS X, however [an issue is open][osx]. 


Prerequisites: 
--------------


Building this JIT requires 

1. A copy of the CRuby source code, modified with the OMR JIT interface. 
   We currently have a copy of said VM in the [Ruby+OMR Preview Github 
   organization.][rubyvm]

   Branches that support building against this JIT, have the current repository
   added as a submodule. 

2. A copy of the Eclipse OMR project. This too should exist as a submodule 
   in one of the branches of the VM above. 


Recent Talks
------------

* Matthew Gaudet's talk at RubyKaigi 2015 _Experiments in sharing Java VM
  technology with CRuby_ -- [(slides)][kaigi2015slides] [(video)][kaigi2015video]




[osx]:             https://github.com/eclipse/omr/issues/223 
[rubyvm]:          https://github.com/rubyomr-preview/ruby
[kaigi2015slides]: http://www.slideshare.net/MatthewGaudet/experiments-in-sharing-java-vm-technology-with-cruby 
[kaigi2015video]:  https://www.youtube.com/watch?v=EDxoaEdR-_M 
[omrpreview]:      https://github.com/rubyomr-preview/rubyomr-preview
