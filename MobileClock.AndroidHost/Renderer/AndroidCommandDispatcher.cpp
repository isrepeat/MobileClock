#include <jni.h>

#include "Renderer/AndroidCommandDispatcher.h"

namespace mobileclock::renderer::_details {
    const char* AndroidActionName(AndroidAction action) {
        switch (action) {
        case AndroidAction::createAlarm:
            return "createAlarm";
        case AndroidAction::chooseAlarmMelody:
            return "chooseAlarmMelody";
        case AndroidAction::resetAlarmMelodySelection:
            return "resetAlarmMelodySelection";
        case AndroidAction::toggleAlarm:
            return "toggleAlarm";
        case AndroidAction::updateApplication:
            return "updateApplication";
        case AndroidAction::uploadScreenshot:
            return "uploadScreenshot";
        case AndroidAction::shareLogs:
            return "shareLogs";
        case AndroidAction::exportLogs:
            return "exportLogs";
        }
        return "";
    }
}

namespace mobileclock::renderer {
    AndroidCommandDispatcher::~AndroidCommandDispatcher() {
        this->ClearDispatcher();
    }

    //
    // API
    //
    void AndroidCommandDispatcher::Dispatch(AndroidAction action) const {
        if (this->javaVm == nullptr || this->dispatcher == nullptr || this->dispatchMethod == nullptr) {
            return;
        }
        JNIEnv* env = nullptr;
        bool isAttached = false;
        if (this->javaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
            if (this->javaVm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
                return;
            }
            isAttached = true;
        }
        jstring javaAction = env->NewStringUTF(_details::AndroidActionName(action));
        if (javaAction != nullptr) {
            env->CallVoidMethod(this->dispatcher, this->dispatchMethod, javaAction);
            env->DeleteLocalRef(javaAction);
        }
        if (isAttached) {
            this->javaVm->DetachCurrentThread();
        }
    }

    void AndroidCommandDispatcher::SetDispatcher(JNIEnv* env, jobject value) {
        this->ClearDispatcher();
        env->GetJavaVM(&this->javaVm);
        this->dispatcher = env->NewGlobalRef(value);
        const jclass dispatcherClass = env->GetObjectClass(value);
        this->dispatchMethod = env->GetMethodID(dispatcherClass, "dispatch", "(Ljava/lang/String;)V");
        env->DeleteLocalRef(dispatcherClass);
    }

    //
    // Internal
    //
    void AndroidCommandDispatcher::ClearDispatcher() {
        if (this->dispatcher == nullptr || this->javaVm == nullptr) {
            return;
        }
        JNIEnv* env = nullptr;
        if (this->javaVm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_OK) {
            env->DeleteGlobalRef(this->dispatcher);
        }
        this->dispatcher = nullptr;
        this->dispatchMethod = nullptr;
    }
}