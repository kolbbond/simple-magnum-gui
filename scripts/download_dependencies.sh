#!/bin/bash

mkdir dependencies

echo "download Corrade"
git clone git@github.com:mosra/corrade.git dependencies/corrade

echo "download Magnum"
git clone git@github.com:mosra/magnum.git dependencies/magnum

echo "download magnum-plugins"
git clone git@github.com:mosra/magnum-plugins.git dependencies/magnum-plugins

echo "download magnum-integration"
#git clone git@github.com:kolbbond/magnum-integration.git dependencies/magnum-integration
git clone git@github.com:mosra/magnum-integration.git dependencies/magnum-integration

echo "download ImGui into magnum-integration"
git clone git@github.com:ocornut/imgui.git dependencies/magnum-integration/src/MagnumExternal/ImGui

#echo "download lib directories"

#mkdir lib
#echo "download implot"
#git clone git@github.com:epezent/implot.git lib/implot/

echo "download ImGuiFileDialog"
git clone git@github.com:aiekick/ImGuiFileDialog.git lib/ImGuiFileDialog/


