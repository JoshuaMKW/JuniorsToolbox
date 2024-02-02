#include "objlib/template.hpp"

namespace Toolbox::Object {

    extern std::unordered_map<std::string, Template> g_template_cache;

    /* FILE FORMAT
    /  -----------
    /  - Header
    /  --------
    /  u32 magic
    /  u32 size
    /  u32
    /
    */

}