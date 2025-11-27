#include "objlib/object.hpp"

using namespace Toolbox::Object;

static void HelperSetKinopioMaterialColors(Toolbox::RefPtr<J3DModelData> model_data, int body_color_idx, int clothes_color_idx,
                                    int pollution_strength) {
    static const std::vector<glm::vec4> cap_colors = {
        {30,  30,  200, 255},
        {230, 0,   0,   255},
        {30,  120, 30,  255},
        {250, 220, 30,  255},
        {150, 0,   200, 255},
    };

    static const std::vector<glm::vec4> cloth_colors = {
        {30,  30,  200, 255},
        {230, 0,   0,   255},
        {30,  120, 30,  255},
        {250, 220, 30,  255},
        {150, 0,   200, 255},
    };

    float weighted_pol_strength = (((float)pollution_strength / 255.0f) * 150.0f) / 255.0f;

    std::shared_ptr<J3DMaterial> mat_cap = model_data->GetMaterial("_mat_cap");
    if (mat_cap) {
        mat_cap->TevBlock->mTevColors[1]        = cap_colors[body_color_idx];
        mat_cap->TevBlock->mTevKonstColors[0].a = weighted_pol_strength;
    }

    std::shared_ptr<J3DMaterial> mat_cloth = model_data->GetMaterial("_mat_cloth");
    if (mat_cloth) {
        mat_cloth->TevBlock->mTevColors[2]        = cloth_colors[clothes_color_idx];
        mat_cloth->TevBlock->mTevKonstColors[0].a = weighted_pol_strength;
    }

    std::shared_ptr<J3DMaterial> mat_body = model_data->GetMaterial("_mat_body");
    if (mat_body) {
        mat_body->TevBlock->mTevKonstColors[0].a = weighted_pol_strength;
    }

    std::shared_ptr<J3DMaterial> mat_mouth = model_data->GetMaterial("_mat_mouth");
    if (mat_mouth) {
        mat_mouth->TevBlock->mTevKonstColors[0].a = weighted_pol_strength;
    }
}

void PhysicalSceneObject::HelperUpdateKinopioRender() {
    RefPtr<Object::MetaMember> body_color_member    = getMember("BodyColor").value_or(nullptr);
    RefPtr<Object::MetaMember> clothes_color_member    = getMember("ClothesColor").value_or(nullptr);
    RefPtr<Object::MetaMember> pollute_state_member = getMember("PolluteState").value_or(nullptr);
    if (!body_color_member || !clothes_color_member || !pollute_state_member) {
        TOOLBOX_DEBUG_LOG("Failed to get parameter for NPCKinopio!");
    } else {
        int body_color_idx   = getMetaValue<int>(body_color_member, 0).value();
        int clothes_color_idx   = getMetaValue<int>(clothes_color_member, 0).value();
        int pollute_strength = getMetaValue<int>(pollute_state_member, 0).value();
        HelperSetKinopioMaterialColors(m_model_data, body_color_idx,
                                       clothes_color_idx, pollute_strength);
    }

    // TODO: Figure out good solution for optional clothing models
}