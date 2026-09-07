#pragma once

#include <jni.h>

namespace mobileclock::renderer {
    enum class AndroidAction {
        createAlarm,
        toggleAlarm,
        updateApplication,
        uploadScreenshot,
        shareLogs,
        exportLogs,
    };

    class AndroidCommandDispatcher final {
    public:
        AndroidCommandDispatcher() = default;
        ~AndroidCommandDispatcher();

        AndroidCommandDispatcher(const AndroidCommandDispatcher&) = delete;
        AndroidCommandDispatcher& operator=(const AndroidCommandDispatcher&) = delete;

        void Dispatch(AndroidAction action) const;
        void SetDispatcher(JNIEnv* env, jobject value);

    private:
        void ClearDispatcher();

    private:
        JavaVM* javaVm = nullptr;
        jobject dispatcher = nullptr;
        jmethodID dispatchMethod = nullptr;
    };
}