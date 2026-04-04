#include "objlib/object.hpp"

using namespace Toolbox::Object;

// Pianta body colors in Sunshine are 10 bit (0-1023 range)
static constexpr float nrm(int val) { return static_cast<float>(val) / 255.0f; };

static constexpr std::array s_body_colors_montem = {
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

static constexpr std::array s_body_colors_montemb = {
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

static constexpr std::array s_body_colors_montew = {
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

static std::unordered_map<Toolbox::u16, decltype(&s_body_colors_montem)> s_body_colors_monte_map = {
    {NameRef::calcKeyCode("NPCMonteM"),  &s_body_colors_montem },
    {NameRef::calcKeyCode("NPCMonteMA"), &s_body_colors_montem },
    {NameRef::calcKeyCode("NPCMonteMB"), &s_body_colors_montemb},
    {NameRef::calcKeyCode("NPCMonteMC"), &s_body_colors_montem },
    {NameRef::calcKeyCode("NPCMonteMD"), &s_body_colors_montem },
    {NameRef::calcKeyCode("NPCMonteME"), &s_body_colors_montem },
    {NameRef::calcKeyCode("NPCMonteMF"), &s_body_colors_montem },
    {NameRef::calcKeyCode("NPCMonteMG"), &s_body_colors_montem },
    {NameRef::calcKeyCode("NPCMonteMH"), &s_body_colors_montem },
    {NameRef::calcKeyCode("NPCMonteW"),  &s_body_colors_montew },
    {NameRef::calcKeyCode("NPCMonteWA"), &s_body_colors_montew },
    {NameRef::calcKeyCode("NPCMonteWB"), &s_body_colors_montew },
    {NameRef::calcKeyCode("NPCMonteWC"), &s_body_colors_montew },
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

// clang-format off
static constexpr std::array s_action_to_anim_montem = {
    14, // Standing
    8,  // Sitting
    15, // Looking up
    16, // Looking down
    12, // Talking
    9,  // Sitting and talking
    11, // Walking
    7,  // Running
    5,  // Angry
    -1, // Playing Ukelele
    -1, // Hula Dance
    1,  // Dancing
    -1, // Swimming
    11, // Walking and stopping
    11, // Walking and stopping to look up
    11, // Walking and stopping to look down
    11, // Walking and stopping to talk
    7,  // Running and stopping
    7,  // Running and stopping to look up
    7,  // Running and stopping to look down
    7,  // Running and stopping to talk
    -1, // Walking and playing Ukelele
    -1, // Mopping
    -1, // Holding sign
    7,  // Caught on fire
};

static constexpr std::array s_action_to_anim_montemf = {
    14, // Standing
    8,  // Sitting
    15, // Looking up
    16, // Looking down
    12, // Talking
    9,  // Sitting and talking
    11, // Walking
    7,  // Running
    5,  // Angry
    -1, // Playing Ukelele
    -1, // Hula Dance
    1,  // Dancing
    23, // Swimming
    11, // Walking and stopping
    11, // Walking and stopping to look up
    11, // Walking and stopping to look down
    11, // Walking and stopping to talk
    7,  // Running and stopping
    7,  // Running and stopping to look up
    7,  // Running and stopping to look down
    7,  // Running and stopping to talk
    -1, // Walking and playing Ukelele
    -1, // Mopping
    -1, // Holding sign
    7,  // Caught on fire
};

static constexpr std::array s_action_to_anim_montemg = {
    14, // Standing
    8,  // Sitting
    15, // Looking up
    16, // Looking down
    12, // Talking
    9,  // Sitting and talking
    11, // Walking
    7,  // Running
    5,  // Angry
    -1, // Playing Ukelele
    -1, // Hula Dance
    1,  // Dancing
    -1, // Swimming
    11, // Walking and stopping
    11, // Walking and stopping to look up
    11, // Walking and stopping to look down
    11, // Walking and stopping to talk
    7,  // Running and stopping
    7,  // Running and stopping to look up
    7,  // Running and stopping to look down
    7,  // Running and stopping to talk
    -1, // Walking and playing Ukelele
    23, // Mopping
    -1, // Holding sign
    7,  // Caught on fire
};

static constexpr std::array s_action_to_anim_montemh = {
    14, // Standing
    8,  // Sitting
    15, // Looking up
    16, // Looking down
    12, // Talking
    9,  // Sitting and talking
    11, // Walking
    7,  // Running
    5,  // Angry
    23, // Playing Ukelele
    -1, // Hula Dance
    1,  // Dancing
    -1, // Swimming
    11, // Walking and stopping
    11, // Walking and stopping to look up
    11, // Walking and stopping to look down
    11, // Walking and stopping to talk
    7,  // Running and stopping
    7,  // Running and stopping to look up
    7,  // Running and stopping to look down
    7,  // Running and stopping to talk
    25, // Walking and playing Ukelele
    -1, // Mopping
    -1, // Holding sign
    7,  // Caught on fire
};

static constexpr std::array s_action_to_anim_montew = {
    14, // Standing
    8,  // Sitting
    15, // Looking up
    16, // Looking down
    12, // Talking
    9,  // Sitting and talking
    11, // Walking
    7,  // Running
    5,  // Angry
    -1, // Playing Ukelele
    -1, // Hula Dance
    1,  // Dancing
    -1, // Swimming
    11, // Walking and stopping
    11, // Walking and stopping to look up
    11, // Walking and stopping to look down
    11, // Walking and stopping to talk
    7,  // Running and stopping
    7,  // Running and stopping to look up
    7,  // Running and stopping to look down
    7,  // Running and stopping to talk
    -1, // Walking and playing Ukelele
    -1, // Mopping
    16, // Holding sign
    -1,  // Caught on fire
};

static constexpr std::array s_action_to_anim_montewc = {
    14, // Standing
    8,  // Sitting
    15, // Looking up
    16, // Looking down
    12, // Talking
    9,  // Sitting and talking
    11, // Walking
    7,  // Running
    5,  // Angry
    -1, // Playing Ukelele
    23, // Hula Dance
    1,  // Dancing
    -1, // Swimming
    11, // Walking and stopping
    11, // Walking and stopping to look up
    11, // Walking and stopping to look down
    11, // Walking and stopping to talk
    7,  // Running and stopping
    7,  // Running and stopping to look up
    7,  // Running and stopping to look down
    7,  // Running and stopping to talk
    -1, // Walking and playing Ukelele
    -1, // Mopping
    16, // Holding sign
    -1,  // Caught on fire
};
// clang-format on

static std::unordered_map<Toolbox::u16, decltype(&s_action_to_anim_montem)>
    s_action_to_anim_monte_map = {
        {NameRef::calcKeyCode("NPCMonteM"),  &s_action_to_anim_montem },
        {NameRef::calcKeyCode("NPCMonteMA"), &s_action_to_anim_montem },
        {NameRef::calcKeyCode("NPCMonteMB"), &s_action_to_anim_montem },
        {NameRef::calcKeyCode("NPCMonteMC"), &s_action_to_anim_montem },
        {NameRef::calcKeyCode("NPCMonteMD"), &s_action_to_anim_montem },
        {NameRef::calcKeyCode("NPCMonteME"), &s_action_to_anim_montem },
        {NameRef::calcKeyCode("NPCMonteMF"), &s_action_to_anim_montemf},
        {NameRef::calcKeyCode("NPCMonteMG"), &s_action_to_anim_montemg},
        {NameRef::calcKeyCode("NPCMonteMH"), &s_action_to_anim_montemh},
        {NameRef::calcKeyCode("NPCMonteW"),  &s_action_to_anim_montew },
        {NameRef::calcKeyCode("NPCMonteWA"), &s_action_to_anim_montew },
        {NameRef::calcKeyCode("NPCMonteWB"), &s_action_to_anim_montew },
        {NameRef::calcKeyCode("NPCMonteWC"), &s_action_to_anim_montewc},
};

static glm::vec4 SelectBodyColorByTypeAndColorIdx(const NameRef &obj_type, int body_color_idx) {
    if (s_body_colors_monte_map.find(obj_type.code()) == s_body_colors_monte_map.end()) {
        return glm::vec4(255.0f, 255.0f, 255.0f, 255.0f);
    }

    const auto *body_colors = s_body_colors_monte_map.at(obj_type.code());
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

static void HelperSetWoodBlockMaterialColors(Toolbox::RefPtr<J3DModelData> model_data, Toolbox::Color::RGB32 color) {
    std::shared_ptr<J3DMaterial> mat_main = model_data->GetMaterials()[0];

    if (mat_main) {
        mat_main->TevBlock->mTevColors[0] = glm::vec4(color.m_r, color.m_g, color.m_b, 255);
    }
}

void PhysicalSceneObject::HelperUpdateWoodblockRender() {
    RefPtr<Object::MetaMember> color_member = getMember("Color").value_or(nullptr);
    if (!color_member) {
        TOOLBOX_DEBUG_LOG("Failed to get parameter for WoodBlock!");
    } else {
        Color::RGB32 color        = getMetaValue<Color::RGB32>(color_member, 0).value();
        HelperSetWoodBlockMaterialColors(m_model_data, color);
    }
    // TODO: Figure out good solution for optional clothing models
}