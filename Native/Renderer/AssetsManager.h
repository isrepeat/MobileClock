#pragma once

#include <jni.h>

#include <string_view>
#include <vector>

struct AAssetManager;

namespace mobileclock::renderer {
    class AssetsManager {
    public:
        AssetsManager(JNIEnv* env, jobject javaAssetManager);

        std::vector<unsigned char> ReadBytes(std::string_view path) const;

    private:
        AAssetManager* assetManager;
    };
}