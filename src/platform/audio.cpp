#include "platform/audio.hpp"
#include "core/core.hpp"

#include <magic_enum/magic_enum.hpp>

#ifdef TOOLBOX_PLATFORM_WINDOWS

#include <Windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

namespace Toolbox::Platform {

    static const char *ConvertSystemSoundToID(SystemSound sound_type) {
        switch (sound_type) {
        case SystemSound::S_NOTIFICATION:
            return TEXT("SystemAsterisk");
        case SystemSound::S_ERROR:
            return TEXT("SystemExclamation");
        case SystemSound::S_WARNING:
            return TEXT("SystemQuestion");
        case SystemSound::S_SUCCESS:
            return TEXT("SystemAsterisk");  // No direct success sound, use OK sound
        default:
            return TEXT("SystemAsterisk");
        }
    }

    Result<void, BaseError> PlayAudio(const fs_path &path, bool loop) {
        // Play sound asynchronously
        UINT flags = SND_FILENAME | SND_ASYNC;
        if (loop) {
            flags |= SND_LOOP;
        }

        const std::string path_str = path.string();
        if (!PlaySound(path_str.c_str(), NULL, flags)) {
            return make_error<void>("[SYSTEM_SOUND]",
                                    std::format("Failed to play audio: `{}'", path.string()));
        }

        return {};
    }

    Result<void, BaseError> PlaySystemSound(SystemSound sound_type) {
        // Play sound asynchronously
        UINT flags = SND_ASYNC | SND_SYSTEM;

        if (!PlaySound(ConvertSystemSoundToID(sound_type), NULL, flags)) {
            return make_error<void>(
                "[SYSTEM_SOUND]",
                std::format("Failed to play system audio: {}", magic_enum::enum_name(sound_type)));
        }

        return {};
    }

}  // namespace Toolbox::Platform

#endif