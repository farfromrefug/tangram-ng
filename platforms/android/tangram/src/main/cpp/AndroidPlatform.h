#pragma once

#include "platform.h"
#include "JniWorker.h"
#include "util/asyncWorker.h"

#include <jni.h>
#include <android/asset_manager.h>
#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Tangram {

struct LabelPickResult;
struct FeaturePickResult;
struct MarkerPickResult;
class Map;
struct SceneUpdate;
struct SceneError;
using SceneID = int32_t;

class AndroidPlatform : public Platform {

public:

    AndroidPlatform(JNIEnv* jniEnv, jobject mapController, jobject assetManager);
    void shutdown() override;
    void requestRender() const override;
    bool notifyRender() const override;
    void setContinuousRendering(bool isContinuous) override;
    FontSourceHandle systemFont(const std::string& name, const std::string& weight, const std::string& face) const override;
    std::vector<FontSourceHandle> systemFontFallbacksHandle() const override;
    bool startUrlRequestImpl(const Url& url, const HttpOptions& options, const UrlRequestHandle request, UrlRequestId& id) override;
    void cancelUrlRequestImpl(const UrlRequestId id) override;

    void onUrlComplete(JNIEnv* jniEnv, jlong jRequestHandle, jbyteArray jBytes, jstring jError);

    static void jniOnLoad(JavaVM* javaVM, JNIEnv* jniEnv, const char* ctrlClass = "com/mapzen/tangram/MapController");

private:

    std::vector<char> bytesFromFile(const Url& url) const;
    bool bytesFromAssetManager(const char* path, std::function<char*(size_t)> allocator) const;
    std::string fontPath(const std::string& family, const std::string& weight, const std::string& style) const;

    jobject m_mapController;
    AAssetManager* m_assetManager;

    mutable JniWorker m_jniWorker;
    AsyncWorker m_fileWorker;

    mutable std::atomic_bool m_renderRequested{false};
};

} // namespace Tangram
