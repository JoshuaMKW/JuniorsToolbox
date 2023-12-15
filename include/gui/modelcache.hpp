#include <map>
#include <string>

#include <J3D/J3DModelLoader.hpp>
#include <J3D/Data/J3DModelData.hpp>
#include <J3D/Material/J3DUniformBufferObject.hpp>
#include <J3D/Rendering/J3DLight.hpp>
#include <J3D/Data/J3DModelInstance.hpp>
#include <J3D/Rendering/J3DRendering.hpp>

extern std::map<std::string, std::shared_ptr<J3DModelData>> ModelCache;