# Dependencies - URL and SHA256 definitions for ExternalProject
#
# This file replaces deps.txt with native CMake syntax.
# Each dependency has a _URL and _SHA256 variable.

# Armadillo - C++ linear algebra library
set(ARMADILLO_URL "https://gitlab.com/conradsnicta/armadillo-code/-/archive/14.6.2/armadillo-code-14.6.2.tar.gz")
set(ARMADILLO_SHA256 "c86559b590e757caa2b32bc60602a14b37cecf66f7aff49b1d90916f4d89f105")

# SuperLU - sparse direct solver
set(SUPERLU_URL "https://github.com/xiaoyeli/superlu/archive/refs/tags/v5.3.0.zip")
set(SUPERLU_SHA256 "76aa425af2ffc763bf48fdd2a73f84762748f37901cb84d312bf03b8b54e9b72")

# JsonCpp - JSON parser
set(JSONCPP_URL "https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.9.6.zip")
set(JSONCPP_SHA256 "0d445d6e956fb62d69ec6895b0000ea6caddf0680373a0d5a37b719c7fbf369b")

# Corrade - C++11 multiplatform utility library (Magnum dependency)
set(CORRADE_URL "https://github.com/mosra/corrade/archive/a959501f16432d5780553ef2111e84031c161333.zip")
set(CORRADE_SHA256 "1a0d7123495adcd91a9b0b29cb909fa2e8c04f3e121029e8000e6b087623cbe6")

# Magnum - graphics middleware
set(MAGNUM_URL "https://github.com/mosra/magnum/archive/c8779838934fed294e84ab19977cdcc5106bbcf2.zip")
set(MAGNUM_SHA256 "873e28fb4b6327e57c318ce2263d4e530338e25a417b2ff0a8d14dda918ea994")

# Magnum Plugins
set(MAGNUMPLUGINS_URL "https://github.com/mosra/magnum-plugins/archive/cdc7b46923a2217b7941df257a077497cd9c36b2.zip")
set(MAGNUMPLUGINS_SHA256 "8c6977a2efe982e7b008b1597f0e152f8ae732ee9b1172b8fb482ecfd4dd84d5")

# Magnum Integration (ImGui, etc.)
set(MAGNUMINTEGRATION_URL "https://github.com/mosra/magnum-integration/archive/30d179f341eafb2b69d9c29d9b7af2b736122786.zip")
set(MAGNUMINTEGRATION_SHA256 "a45e129ec2c9c77198bab8a9beaaa8812a67cb5dfe58c3cc85d24433c55f8a67")

# Dear ImGui
set(IMGUI_URL "https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9.zip")
set(IMGUI_SHA256 "873e28fb4b6327e57c318ce2263d4e530338e25a417b2ff0a8d14dda918ea994")

# ImPlot - plotting for ImGui
set(IMPLOT_URL "https://github.com/epezent/implot/archive/3da8bd34299965d3b0ab124df743fe3e076fa222.zip")
set(IMPLOT_SHA256 "873e28fb4b6327e57c318ce2263d4e530338e25a417b2ff0a8d14dda918ea994")

# ImPlot3D - 3D plotting for ImGui
set(IMPLOT3D_URL "https://github.com/brenocq/implot3d/archive/743cf425d74568986e4a16dd0ca9e021f0cdfb34.zip")
set(IMPLOT3D_SHA256 "873e28fb4b6327e57c318ce2263d4e530338e25a417b2ff0a8d14dda918ea994")

# pybind11 - Python bindings
set(PYBIND11_URL "https://github.com/pybind/pybind11/archive/refs/tags/v3.0.0rc3.zip")
set(PYBIND11_SHA256 "47e1b511bfca5f4d6467ab5731bf9a7d3e81d5dbfc13f58f78a39de3285a47bd")
