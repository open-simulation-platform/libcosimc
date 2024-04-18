libcosimc - OSP C co-simulation API
===================================
![libcosimc CI Conan](https://github.com/open-simulation-platform/libcosimc/workflows/libcosimc%20CI%20Conan/badge.svg)
 
This repository contains the OSP C library for co-simulations which wraps and exposes a subset of the [`libcosim`] 
library's functions.
   
See [`CONTRIBUTING.md`] for contributor guidelines and [`LICENSE`] for
terms of use.
 
 
How to build
------------

`libcosimc` can be built in the same way as libcosim with the following differences in [step 2]

To include FMU-proxy support use `-o libcosim:'proxyfmu=True'` when installing dependencies in 
     
     conan install ..  -o libcosim:'proxyfmu=True' --build=missing
     
When running cmake use `-DLIBCOSIMC_USING_CONAN=TRUE`
 
[`CONTRIBUTING.md`]: ./CONTRIBUTING.md
[`LICENSE`]: ./LICENSE
[Step 2]: https://github.com/open-simulation-platform/libcosim#step-2-prepare-build-system
[`libcosim`]: https://github.com/open-simulation-platform/libcosim
