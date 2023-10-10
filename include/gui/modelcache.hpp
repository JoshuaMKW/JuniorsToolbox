#include <map>
#include <string>

#include <J3D/J3DModelLoader.hpp>
#include <J3D/J3DModelData.hpp>
#include <J3D/J3DUniformBufferObject.hpp>
#include <J3D/J3DLight.hpp>
#include <J3D/J3DModelInstance.hpp>
#include <J3D/J3DRendering.hpp>

extern std::map<std::string, std::shared_ptr<J3DModelData>> ModelCache;