#pragma once

#include "serial.hpp"

namespace Toolbox {

    // This interface adds methods on top of ISerializable that
    // give the object traits for game-ready I/O. This is to
    // differentiate it from project I/O which may not match
    // the format that a game project expects.
    class IGameSerializable : public ISerializable {
    public:
        IGameSerializable() = default;
        virtual ~IGameSerializable() = default;

        // Method to serialize the object in a game-ready format
        virtual Result<void, SerialError> gameSerialize(Serializer &out) const = 0;
        // Method to deserialize the object in a game-ready format
        virtual Result<void, SerialError> gameDeserialize(Deserializer &in)    = 0;

        void operator<<(Serializer &out) { serialize(out); }
        void operator>>(Deserializer &in) { deserialize(in); }
    };

}