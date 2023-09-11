#include "objlib/object.hpp"
#include <expected>

namespace Toolbox::Object {

    std::vector<s8> BasicSceneObject::getData() const { return {}; }
    size_t BasicSceneObject::getDataSize() const { return 0; }
    
    bool BasicSceneObject::hasMember(const QualifiedName &name) const { return false; }
    void *BasicSceneObject::getMemberPtr(const QualifiedName &name) const { return nullptr; }
    size_t BasicSceneObject::getMemberOffset(const QualifiedName &name) const { return 0; }
    size_t BasicSceneObject::getMemberSize(const QualifiedName &name) const { return 0; }

    void BasicSceneObject::dump(std::ostream &out, int indention, int indention_width) const {}

    void Toolbox::Object::BasicSceneObject::serialize(Serializer &out) const {}

    void Toolbox::Object::BasicSceneObject::deserialize(Deserializer &in) {}

}  // namespace Toolbox::Object
