libcosimc - OSP C co-simulation API
===================================
![libcosimc CI Conan](https://github.com/open-simulation-platform/libcosimc/workflows/libcosimc%20CI%20Conan/badge.svg)

This repository contains the [OSP] C library for co-simulations, which wraps and
exposes a subset of the [libcosim] C++ library's functions.

See [`CONTRIBUTING.md`] for contributor guidelines and [`LICENSE`] for
terms of use.

How to build
------------
Please read the [libcosim build instructions].  The commands you should run
to build libcosimc are exactly the same, except that the option you need to
add to `conan install` to enable [proxy-fmu] support is
 `--options="libcosim/*:proxyfmu=True`.

[`CONTRIBUTING.md`]: ./CONTRIBUTING.md
[libcosim]: https://github.com/open-simulation-platform/libcosim
[libcosim build instructions]: https://github.com/open-simulation-platform/libcosim#how-to-build
[`LICENSE`]: ./LICENSE
[OSP]: https://opensimulationplatform.com/
[proxy-fmu]: https://github.com/open-simulation-platform/proxy-fmu/
