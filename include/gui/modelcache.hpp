#pragma once

#include <map>
#include <string>

#include <J3D/J3DModelLoader.hpp>
#include <J3D/Data/J3DModelData.hpp>
#include <J3D/Material/J3DUniformBufferObject.hpp>
#include <J3D/Rendering/J3DLight.hpp>
#include <J3D/Data/J3DModelInstance.hpp>
#include <J3D/Rendering/J3DRendering.hpp>

#include "core/memory.hpp"

extern std::map<std::string, Toolbox::RefPtr<J3DModelData>> ModelCache;