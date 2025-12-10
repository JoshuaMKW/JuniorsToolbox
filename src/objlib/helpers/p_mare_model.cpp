#include "objlib/object.hpp"

using namespace Toolbox::Object;

// Pianta body colors in Sunshine are 10 bit (0-1023 range)
static constexpr float nrm(int val) { return static_cast<float>(val) / 255.0f; };

static constexpr std::array s_body_colors_marem = {
    glm::vec4{nrm(100), nrm(255), nrm(300), 1.0f},
    glm::vec4{nrm(120), nrm(120), nrm(300), 1.0f},
    glm::vec4{nrm(350), nrm(300), nrm(0),   1.0f},
    glm::vec4{nrm(200), nrm(70),  nrm(0),   1.0f},
    glm::vec4{nrm(300), nrm(130), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(350), nrm(0),   1.0f},
    glm::vec4{nrm(400), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(320), nrm(140), nrm(0),   1.0f},
    glm::vec4{nrm(200), nrm(255), nrm(400), 1.0f},
    glm::vec4{nrm(400), nrm(250), nrm(100), 1.0f},
    // ---- INVALID COLORS BELOW ----
    glm::vec4{nrm(0),   nrm(366), nrm(0),   1.0f},
};

static constexpr std::array s_body_colors_maremb = {
    glm::vec4{nrm(160), nrm(200), nrm(300), 1.0f},
    glm::vec4{nrm(255), nrm(160), nrm(150), 1.0f},
    glm::vec4{nrm(300), nrm(200), nrm(80),  1.0f},
    glm::vec4{nrm(200), nrm(300), nrm(100), 1.0f},
    // ---- INVALID COLORS BELOW ----
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
};

static constexpr std::array s_body_colors_marew = {
    glm::vec4{nrm(300), nrm(100), nrm(200), 1.0f},
    glm::vec4{nrm(400), nrm(150), nrm(0),   1.0f},
    glm::vec4{nrm(300), nrm(330), nrm(0),   1.0f},
    glm::vec4{nrm(400), nrm(330), nrm(0),   1.0f},
    glm::vec4{nrm(330), nrm(40),  nrm(0),   1.0f},
    glm::vec4{nrm(400), nrm(200), nrm(255), 1.0f},
    // ---- INVALID COLORS BELOW ----
    glm::vec4{nrm(0),   nrm(1),   nrm(57),  1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
};

static std::unordered_map<Toolbox::u16, decltype(&s_body_colors_marem)> s_body_colors_mare_map = {
    {NameRef::calcKeyCode("NPCMareM"),  &s_body_colors_marem },
    {NameRef::calcKeyCode("NPCMareMA"), &s_body_colors_marem },
    {NameRef::calcKeyCode("NPCMareMB"), &s_body_colors_maremb},
    {NameRef::calcKeyCode("NPCMareMC"), &s_body_colors_marem },
    {NameRef::calcKeyCode("NPCMareMD"), &s_body_colors_marem },
    {NameRef::calcKeyCode("NPCMareW"),  &s_body_colors_marew },
    {NameRef::calcKeyCode("NPCMareWA"), &s_body_colors_marew },
    {NameRef::calcKeyCode("NPCMareWB"), &s_body_colors_marew },
};

static constexpr std::array s_clothes_colors_montema_buf0 = {
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(200), nrm(200), nrm(170), 1.0f},
    glm::vec4{nrm(50),  nrm(50),  nrm(50),  1.0f},
    glm::vec4{nrm(150), nrm(200), nrm(255), 1.0f},
    glm::vec4{nrm(0),   nrm(70),  nrm(150), 1.0f},
    glm::vec4{nrm(400), nrm(300), nrm(200), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(150), 1.0f},
    // ---- INVALID COLORS BELOW ----
};

static constexpr std::array s_clothes_colors_montema_buf1 = {
    glm::vec4{nrm(250), nrm(130), nrm(50),  1.0f},
    glm::vec4{nrm(50),  nrm(130), nrm(100), 1.0f},
    glm::vec4{nrm(150), nrm(180), nrm(20),  1.0f},
    glm::vec4{nrm(200), nrm(200), nrm(170), 1.0f},
    glm::vec4{nrm(50),  nrm(50),  nrm(50),  1.0f},
    glm::vec4{nrm(150), nrm(200), nrm(255), 1.0f},
    glm::vec4{nrm(0),   nrm(70),  nrm(150), 1.0f},
    glm::vec4{nrm(230), nrm(150), nrm(100), 1.0f},
    glm::vec4{nrm(60),  nrm(150), nrm(230), 1.0f},
    glm::vec4{nrm(180), nrm(150), nrm(200), 1.0f},
    glm::vec4{nrm(100), nrm(220), nrm(300), 1.0f},
    // ---- INVALID COLORS BELOW ----
};

static constexpr std::array s_clothes_colors_montemb = {
    glm::vec4{nrm(70),  nrm(130), nrm(200), 1.0f},
    glm::vec4{nrm(200), nrm(20),  nrm(20),  1.0f},
    glm::vec4{nrm(130), nrm(30),  nrm(80),  1.0f},
    glm::vec4{nrm(130), nrm(200), nrm(80),  1.0f},
    glm::vec4{nrm(230), nrm(200), nrm(80),  1.0f},
    glm::vec4{nrm(50),  nrm(100), nrm(150), 1.0f},
    // ---- INVALID COLORS BELOW ----
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
};

static constexpr std::array s_clothes_colors_montemc_buf0 = {
    glm::vec4{nrm(230), nrm(230), nrm(210), 1.0f},
    glm::vec4{nrm(150), nrm(70),  nrm(0),   1.0f},
    glm::vec4{nrm(230), nrm(230), nrm(210), 1.0f},
    glm::vec4{nrm(0),   nrm(70),  nrm(150), 1.0f},
    glm::vec4{nrm(50),  nrm(150), nrm(130), 1.0f},
    glm::vec4{nrm(60),  nrm(40),  nrm(0),   1.0f},
    glm::vec4{nrm(0),   nrm(100), nrm(100), 1.0f},
    glm::vec4{nrm(0),   nrm(150), nrm(200), 1.0f},
    glm::vec4{nrm(0),   nrm(50),  nrm(100), 1.0f},
    glm::vec4{nrm(100), nrm(100), nrm(0),   1.0f},
    glm::vec4{nrm(100), nrm(0),   nrm(0),   1.0f},
    // ---- INVALID COLORS BELOW ----
};

static constexpr std::array s_clothes_colors_montemc_buf1 = {
    glm::vec4{nrm(230), nrm(230), nrm(210), 1.0f},
    glm::vec4{nrm(150), nrm(70),  nrm(0),   1.0f},
    glm::vec4{nrm(0),   nrm(70),  nrm(150), 1.0f},
    glm::vec4{nrm(230), nrm(230), nrm(210), 1.0f},
    glm::vec4{nrm(230), nrm(230), nrm(210), 1.0f},
    glm::vec4{nrm(160), nrm(150), nrm(50),  1.0f},
    glm::vec4{nrm(0),   nrm(100), nrm(100), 1.0f},
    glm::vec4{nrm(0),   nrm(150), nrm(200), 1.0f},
    glm::vec4{nrm(0),   nrm(50),  nrm(100), 1.0f},
    glm::vec4{nrm(0),   nrm(0),   nrm(0),   1.0f},
    glm::vec4{nrm(0),   nrm(0),   nrm(0),   1.0f},
    // ---- INVALID COLORS BELOW ----
};

static constexpr std::array s_clothes_colors_montemd = {
    glm::vec4{nrm(350), nrm(360), nrm(340), 1.0f},
    glm::vec4{nrm(50),  nrm(100), nrm(0),   1.0f},
    glm::vec4{nrm(100), nrm(0),   nrm(0),   1.0f},
    glm::vec4{nrm(0),   nrm(300), nrm(350), 1.0f},
    glm::vec4{nrm(0),   nrm(100), nrm(250), 1.0f},
    // ---- INVALID COLORS BELOW ----
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
};

static constexpr std::array s_clothes_colors_montewa = {
    glm::vec4{nrm(380), nrm(330), nrm(150), 1.0f},
    glm::vec4{nrm(300), nrm(100), nrm(200), 1.0f},
    glm::vec4{nrm(360), nrm(350), nrm(300), 1.0f},
    glm::vec4{nrm(300), nrm(50),  nrm(0),   1.0f},
    glm::vec4{nrm(400), nrm(150), nrm(100), 1.0f},
    glm::vec4{nrm(120), nrm(150), nrm(300), 1.0f},
    // ---- INVALID COLORS BELOW ----
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
};

static constexpr std::array s_clothes_colors_montewb_buf0 = {
    glm::vec4{nrm(220), nrm(200), nrm(220), 1.0f},
    glm::vec4{nrm(200), nrm(220), nrm(220), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(220), nrm(230), nrm(220), 1.0f},
    glm::vec4{nrm(180), nrm(100), nrm(110), 1.0f},
    glm::vec4{nrm(200), nrm(100), nrm(0),   1.0f},
    glm::vec4{nrm(0),   nrm(100), nrm(150), 1.0f},
    glm::vec4{nrm(255), nrm(200), nrm(100), 1.0f},
    // ---- INVALID COLORS BELOW ----
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
};

static constexpr std::array s_clothes_colors_montewb_buf1 = {
    glm::vec4{nrm(100), nrm(80),  nrm(200), 1.0f},
    glm::vec4{nrm(100), nrm(170), nrm(300), 1.0f},
    glm::vec4{nrm(150), nrm(0),   nrm(60),  1.0f},
    glm::vec4{nrm(180), nrm(120), nrm(200), 1.0f},
    glm::vec4{nrm(140), nrm(180), nrm(300), 1.0f},
    glm::vec4{nrm(180), nrm(100), nrm(110), 1.0f},
    glm::vec4{nrm(200), nrm(100), nrm(0),   1.0f},
    glm::vec4{nrm(0),   nrm(100), nrm(150), 1.0f},
    glm::vec4{nrm(255), nrm(200), nrm(100), 1.0f},
    // ---- INVALID COLORS BELOW ----
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
    glm::vec4{nrm(255), nrm(255), nrm(255), 1.0f},
};

using ClothesColorPair =
    std::pair<decltype(&s_clothes_colors_montema_buf0), decltype(&s_clothes_colors_montema_buf0)>;

static std::unordered_map<Toolbox::u16, ClothesColorPair> s_clothes_colors_monte_map = {
    {NameRef::calcKeyCode("NPCMonteM"),  {nullptr, nullptr}                  },
    {NameRef::calcKeyCode("NPCMonteMA"),
     {&s_clothes_colors_montema_buf0, &s_clothes_colors_montema_buf1}        },
    {NameRef::calcKeyCode("NPCMonteMB"), {&s_clothes_colors_montemb, nullptr}},
    {NameRef::calcKeyCode("NPCMonteMC"),
     {&s_clothes_colors_montemc_buf0, &s_clothes_colors_montemc_buf1}        },
    {NameRef::calcKeyCode("NPCMonteMD"), {&s_clothes_colors_montemd, nullptr}},
    {NameRef::calcKeyCode("NPCMonteME"), {nullptr, nullptr}                  },
    {NameRef::calcKeyCode("NPCMonteMF"), {nullptr, nullptr}                  },
    {NameRef::calcKeyCode("NPCMonteMG"), {nullptr, nullptr}                  },
    {NameRef::calcKeyCode("NPCMonteMH"), {nullptr, nullptr}                  },
    {NameRef::calcKeyCode("NPCMonteW"),  {nullptr, nullptr}                  },
    {NameRef::calcKeyCode("NPCMonteWA"), {&s_clothes_colors_montewa, nullptr}},
    {NameRef::calcKeyCode("NPCMonteWB"),
     {&s_clothes_colors_montewb_buf0, &s_clothes_colors_montewb_buf1}        },
    {NameRef::calcKeyCode("NPCMonteWC"), {nullptr, nullptr}                  },
};

static std::unordered_map<Toolbox::u16, std::vector<int>> s_clothes_tev_color_idx_map = {
    {NameRef::calcKeyCode("NPCMonteM"),  std::vector<int>{}    },
    {NameRef::calcKeyCode("NPCMonteMA"), std::vector<int>{1, 2}},
    {NameRef::calcKeyCode("NPCMonteMB"), std::vector<int>{0}   },
    {NameRef::calcKeyCode("NPCMonteMC"), std::vector<int>{1, 2}},
    {NameRef::calcKeyCode("NPCMonteMD"), std::vector<int>{0}   },
    {NameRef::calcKeyCode("NPCMonteME"), std::vector<int>{}    },
    {NameRef::calcKeyCode("NPCMonteMF"), std::vector<int>{}    },
    {NameRef::calcKeyCode("NPCMonteMG"), std::vector<int>{}    },
    {NameRef::calcKeyCode("NPCMonteMH"), std::vector<int>{}    },
    {NameRef::calcKeyCode("NPCMonteW"),  std::vector<int>{}    },
    {NameRef::calcKeyCode("NPCMonteWA"), std::vector<int>{0}   },
    {NameRef::calcKeyCode("NPCMonteWB"), std::vector<int>{1, 2}},
    {NameRef::calcKeyCode("NPCMonteWC"), std::vector<int>{}    },
};

static glm::vec4 SelectBodyColorByTypeAndColorIdx(const NameRef &obj_type, int body_color_idx) {
    if (s_body_colors_mare_map.find(obj_type.code()) == s_body_colors_mare_map.end()) {
        return glm::vec4(255.0f, 255.0f, 255.0f, 255.0f);
    }

    const auto *body_colors = s_body_colors_mare_map.at(obj_type.code());
    if (!body_colors) {
        return glm::vec4(255.0f, 255.0f, 255.0f, 255.0f);
    }

    if (body_color_idx < 0 || body_color_idx >= static_cast<int>(body_colors->size())) {
        return glm::vec4(255.0f, 255.0f, 255.0f, 255.0f);
    }

    glm::vec4 color = (*body_colors)[body_color_idx];
    color *= 255.0f;
    return color;
}

static glm::vec4 SelectClothesColorByTypeAndColorIdx(const NameRef &obj_type, int clothes_color_idx,
                                                     int buf_idx) {
    if (s_clothes_colors_monte_map.find(obj_type.code()) == s_clothes_colors_monte_map.end()) {
        return glm::vec4(255.0f, 255.0f, 255.0f, 255.0f);
    }

    const ClothesColorPair &clothes_colors = s_clothes_colors_monte_map.at(obj_type.code());
    if (buf_idx == 0) {
        if (!clothes_colors.first) {
            return glm::vec4(255.0f, 255.0f, 255.0f, 255.0f);
        }
        if (clothes_color_idx < 0 ||
            clothes_color_idx >= static_cast<int>(clothes_colors.first->size())) {
            return glm::vec4(255.0f, 255.0f, 255.0f, 255.0f);
        }
        glm::vec4 color = (*(clothes_colors.first))[clothes_color_idx];
        color *= 255.0f;
        return color;
    } else {
        if (!clothes_colors.second) {
            return glm::vec4(255.0f, 255.0f, 255.0f, 255.0f);
        }
        if (clothes_color_idx < 0 ||
            clothes_color_idx >= static_cast<int>(clothes_colors.second->size())) {
            return glm::vec4(255.0f, 255.0f, 255.0f, 255.0f);
        }
        glm::vec4 color = (*(clothes_colors.second))[clothes_color_idx];
        color *= 255.0f;
        return color;
    }
}

static void HelperSetMareMaterialColors(Toolbox::RefPtr<J3DModelData> model_data,
                                         const NameRef &obj_type, int body_color_idx,
                                         int clothes_color_idx, int pollution_strength) {

    float weighted_pol_strength = (((float)pollution_strength / 255.0f) * 150.0f) / 255.0f;

    std::shared_ptr<J3DMaterial> mat_body     = model_data->GetMaterial("_body");

    //if (mat_fuku) {
    //    if (s_clothes_tev_color_idx_map.find(obj_type.code()) !=
    //        s_clothes_tev_color_idx_map.end()) {
    //        const std::vector<int> &tev_indices = s_clothes_tev_color_idx_map.at(obj_type.code());
    //        for (int i = 0; i < tev_indices.size(); ++i) {
    //            int tev_idx = tev_indices[i];
    //            if (tev_idx < 0) {
    //                continue;
    //            }
    //            mat_fuku->TevBlock->mTevColors[tev_idx] =
    //                SelectClothesColorByTypeAndColorIdx(obj_type, clothes_color_idx, i);
    //        }
    //    }
    //    mat_fuku->TevBlock->mTevKonstColors[0].a = weighted_pol_strength;
    //}

    if (mat_body) {
        mat_body->TevBlock->mTevColors[0] =
            SelectBodyColorByTypeAndColorIdx(obj_type, body_color_idx);
        mat_body->TevBlock->mTevKonstColors[0].a = weighted_pol_strength;
    }
}

void PhysicalSceneObject::HelperUpdateMareRender() {
    RefPtr<Object::MetaMember> body_color_member    = getMember("BodyColor").value_or(nullptr);
    RefPtr<Object::MetaMember> clothes_color_member = getMember("ClothesColor").value_or(nullptr);
    RefPtr<Object::MetaMember> pollute_state_member = getMember("PolluteState").value_or(nullptr);
    if (!body_color_member || !pollute_state_member) {
        TOOLBOX_DEBUG_LOG("Failed to get parameter for NPCKinopio!");
    } else {
        int body_color_idx    = getMetaValue<int>(body_color_member, 0).value();
        int pollute_strength  = getMetaValue<int>(pollute_state_member, 0).value();
        int clothes_color_idx = getMetaValue<int>(clothes_color_member, 0).value();
        HelperSetMareMaterialColors(m_model_data, m_type, body_color_idx, clothes_color_idx,
                                     pollute_strength);
    }

    // TODO: Figure out good solution for optional clothing models
}