#include <android/asset_manager_jni.h>
#include <android/asset_manager.h>

#include "AssetsManager.h"

#include <stdexcept>
#include <string>
#include <memory>

namespace mobileclock::renderer {
    AssetsManager::AssetsManager(JNIEnv* env, jobject javaAssetManager)
        : assetManager(AAssetManager_fromJava(env, javaAssetManager)) {
        if (this->assetManager == nullptr) {
            throw std::invalid_argument("javaAssetManager must refer to Android AssetManager");
        }
    }

    std::vector<unsigned char> AssetsManager::ReadBytes(std::string_view path) const {
        const std::string pathString(path);
        std::unique_ptr<AAsset, decltype(&AAsset_close)> asset(
            AAssetManager_open(this->assetManager, pathString.c_str(), AASSET_MODE_BUFFER),
            AAsset_close);

        if (asset == nullptr) {
            throw std::runtime_error("Cannot open asset: " + pathString);
        }

        const size_t size = static_cast<size_t>(AAsset_getLength(asset.get()));
        std::vector<unsigned char> data(size);
        const int bytesRead = AAsset_read(asset.get(), data.data(), size);

        if (bytesRead != static_cast<int>(size)) {
            throw std::runtime_error("Cannot read asset: " + pathString);
        }

        return data;
    }
}