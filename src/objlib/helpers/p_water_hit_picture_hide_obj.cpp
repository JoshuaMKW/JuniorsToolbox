#include "objlib/object.hpp"

using namespace Toolbox::Object;

static void HelperSetWaterHitPictureHideObjMaterialColors(Toolbox::RefPtr<J3DModelData> model_data, Toolbox::Color::RGB32 color) {
    std::shared_ptr<J3DMaterial> mat_main = model_data->GetMaterials()[0];

    if (mat_main) {
        mat_main->TevBlock->mTevColors[0] = glm::vec4(color.m_r, color.m_g, color.m_b, 255);
    }
}

void PhysicalSceneObject::HelperUpdateWaterHitPictureHideObjRender() {
    RefPtr<Object::MetaMember> color_member = getMember("Color").value_or(nullptr);
    if (!color_member) {
        TOOLBOX_DEBUG_LOG("Failed to get parameter for WaterHitPictureHideObj!");
    } else {
        Color::RGB32 color        = getMetaValue<Color::RGB32>(color_member, 0).value();
        HelperSetWaterHitPictureHideObjMaterialColors(m_model_data, color);
    }
    // TODO: Figure out good solution for optional clothing models
}