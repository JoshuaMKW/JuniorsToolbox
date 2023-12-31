#include <imgui.h>
#include <string>
#include <unordered_map>

#include "gui/IconsForkAwesome.h"
#include <GLFW/glfw3.h>

namespace Toolbox::UI {

    std::string keyNameFromEnum(int key) {
        static std::unordered_map<int, std::string> s_key_names = {
            {GLFW_KEY_UNKNOWN,       "Unknown"          },
            {GLFW_KEY_SPACE,         "Space"            },
            {GLFW_KEY_APOSTROPHE,    "'"                },
            {GLFW_KEY_COMMA,         ","                },
            {GLFW_KEY_MINUS,         "-"                },
            {GLFW_KEY_PERIOD,        "."                },
            {GLFW_KEY_SLASH,         "/"                },
            {GLFW_KEY_0,             "0"                },
            {GLFW_KEY_1,             "1"                },
            {GLFW_KEY_2,             "2"                },
            {GLFW_KEY_3,             "3"                },
            {GLFW_KEY_4,             "4"                },
            {GLFW_KEY_5,             "5"                },
            {GLFW_KEY_6,             "6"                },
            {GLFW_KEY_7,             "7"                },
            {GLFW_KEY_8,             "8"                },
            {GLFW_KEY_9,             "9"                },
            {GLFW_KEY_SEMICOLON,     ";"                },
            {GLFW_KEY_EQUAL,         "="                },
            {GLFW_KEY_A,             "A"                },
            {GLFW_KEY_B,             "B"                },
            {GLFW_KEY_C,             "C"                },
            {GLFW_KEY_D,             "D"                },
            {GLFW_KEY_E,             "E"                },
            {GLFW_KEY_F,             "F"                },
            {GLFW_KEY_G,             "G"                },
            {GLFW_KEY_H,             "H"                },
            {GLFW_KEY_I,             "I"                },
            {GLFW_KEY_J,             "J"                },
            {GLFW_KEY_K,             "K"                },
            {GLFW_KEY_L,             "L"                },
            {GLFW_KEY_M,             "M"                },
            {GLFW_KEY_N,             "N"                },
            {GLFW_KEY_O,             "O"                },
            {GLFW_KEY_P,             "P"                },
            {GLFW_KEY_Q,             "Q"                },
            {GLFW_KEY_R,             "R"                },
            {GLFW_KEY_S,             "S"                },
            {GLFW_KEY_T,             "T"                },
            {GLFW_KEY_U,             "U"                },
            {GLFW_KEY_V,             "V"                },
            {GLFW_KEY_W,             "W"                },
            {GLFW_KEY_X,             "X"                },
            {GLFW_KEY_Y,             "Y"                },
            {GLFW_KEY_Z,             "Z"                },
            {GLFW_KEY_LEFT_BRACKET,  "["                },
            {GLFW_KEY_BACKSLASH,     "\\"               },
            {GLFW_KEY_RIGHT_BRACKET, "]"                },
            {GLFW_KEY_GRAVE_ACCENT,  "`"                },
            {GLFW_KEY_WORLD_1,       "World1"           }, /* non-US #1 */
            {GLFW_KEY_WORLD_2,       "World2"           }, /* non-US #2 */
            {GLFW_KEY_ESCAPE,        "Esc"              },
            {GLFW_KEY_ENTER,         "Enter"            },
            {GLFW_KEY_TAB,           "Tab"              },
            {GLFW_KEY_BACKSPACE,     "Backspace"        },
            {GLFW_KEY_INSERT,        "Ins"              },
            {GLFW_KEY_DELETE,        "Del"              },
            {GLFW_KEY_RIGHT,         ICON_FK_ARROW_RIGHT},
            {GLFW_KEY_LEFT,          ICON_FK_ARROW_LEFT },
            {GLFW_KEY_DOWN,          ICON_FK_ARROW_DOWN },
            {GLFW_KEY_UP,            ICON_FK_ARROW_UP   },
            {GLFW_KEY_PAGE_UP,       "PgUp"             },
            {GLFW_KEY_PAGE_DOWN,     "PgDn"             },
            {GLFW_KEY_HOME,          "Home"             },
            {GLFW_KEY_END,           "End"              },
            {GLFW_KEY_CAPS_LOCK,     "CapsLk"           },
            {GLFW_KEY_SCROLL_LOCK,   "ScrollLk"         },
            {GLFW_KEY_NUM_LOCK,      "NumLk"            },
            {GLFW_KEY_PRINT_SCREEN,  "PrintScrn"        },
            {GLFW_KEY_PAUSE,         "Pause"            },
            {GLFW_KEY_F1,            "F1"               },
            {GLFW_KEY_F2,            "F2"               },
            {GLFW_KEY_F3,            "F3"               },
            {GLFW_KEY_F4,            "F4"               },
            {GLFW_KEY_F5,            "F5"               },
            {GLFW_KEY_F6,            "F6"               },
            {GLFW_KEY_F7,            "F7"               },
            {GLFW_KEY_F8,            "F8"               },
            {GLFW_KEY_F9,            "F9"               },
            {GLFW_KEY_F10,           "F10"              },
            {GLFW_KEY_F11,           "F11"              },
            {GLFW_KEY_F12,           "F12"              },
            {GLFW_KEY_F13,           "F13"              },
            {GLFW_KEY_F14,           "F14"              },
            {GLFW_KEY_F15,           "F15"              },
            {GLFW_KEY_F16,           "F16"              },
            {GLFW_KEY_F17,           "F17"              },
            {GLFW_KEY_F18,           "F18"              },
            {GLFW_KEY_F19,           "F19"              },
            {GLFW_KEY_F20,           "F20"              },
            {GLFW_KEY_F21,           "F21"              },
            {GLFW_KEY_F22,           "F22"              },
            {GLFW_KEY_F23,           "F23"              },
            {GLFW_KEY_F24,           "F24"              },
            {GLFW_KEY_F25,           "F25"              },
            {GLFW_KEY_KP_0,          "KP0"              },
            {GLFW_KEY_KP_1,          "KP1"              },
            {GLFW_KEY_KP_2,          "KP2"              },
            {GLFW_KEY_KP_3,          "KP3"              },
            {GLFW_KEY_KP_4,          "KP4"              },
            {GLFW_KEY_KP_5,          "KP5"              },
            {GLFW_KEY_KP_6,          "KP6"              },
            {GLFW_KEY_KP_7,          "KP7"              },
            {GLFW_KEY_KP_8,          "KP8"              },
            {GLFW_KEY_KP_DECIMAL,    "KPDec"            },
            {GLFW_KEY_KP_DIVIDE,     "KPDiv"            },
            {GLFW_KEY_KP_MULTIPLY,   "KPMul"            },
            {GLFW_KEY_KP_SUBTRACT,   "KPSub"            },
            {GLFW_KEY_KP_ADD,        "KPAdd"            },
            {GLFW_KEY_KP_ENTER,      "KPEnter"          },
            {GLFW_KEY_KP_EQUAL,      "KPEqu"            },
            {GLFW_KEY_LEFT_SHIFT,    "Shift"            },
            {GLFW_KEY_LEFT_CONTROL,  "Ctrl"             },
            {GLFW_KEY_LEFT_ALT,      "Alt"              },
            {GLFW_KEY_LEFT_SUPER,    "Super"            },
            {GLFW_KEY_RIGHT_SHIFT,   "ShiftR"           },
            {GLFW_KEY_RIGHT_CONTROL, "CtrlR"            },
            {GLFW_KEY_RIGHT_ALT,     "AltR"             },
            {GLFW_KEY_RIGHT_SUPER,   "SuperR"           },
            {GLFW_KEY_MENU,          "Menu"             },
        };
        return s_key_names[key];
    }

}  // namespace Toolbox::UI