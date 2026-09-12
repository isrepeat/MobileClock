#pragma once
#include <jni.h>

#include "UI/AppSessionController.h"

namespace mobileclock::renderer {
    class AndroidCommandDispatcher final {
    public:
        AndroidCommandDispatcher() = default;
        ~AndroidCommandDispatcher();

        AndroidCommandDispatcher(const AndroidCommandDispatcher&) = delete;
        AndroidCommandDispatcher& operator=(const AndroidCommandDispatcher&) = delete;

        void Dispatch(
            mobileclock::ui::AppSessionSignal signal,
            const mobileclock::ui::AppSessionSignalData& data) const;
        void SetDispatcher(JNIEnv* env, jobject value);

    private:
        void ClearDispatcher();

    private:
        JavaVM* javaVm = nullptr;
        jobject dispatcher = nullptr;
        jmethodID dispatchMethod = nullptr;
    };
}