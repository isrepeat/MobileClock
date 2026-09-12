#include <jni.h>

#include "Renderer/AndroidCommandDispatcher.h"

namespace mobileclock::renderer {
    AndroidCommandDispatcher::~AndroidCommandDispatcher() {
        this->ClearDispatcher();
    }

    //
    // API
    //
    void AndroidCommandDispatcher::Dispatch(
        mobileclock::ui::AppSessionSignal signal,
        const mobileclock::ui::AppSessionSignalData& data) const {
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
        jstring javaValue = env->NewStringUTF(data.value.c_str());
        jstring javaAdditionalValue = env->NewStringUTF(data.additionalValue.c_str());
        if (javaValue != nullptr && javaAdditionalValue != nullptr) {
            env->CallVoidMethod(
                this->dispatcher,
                this->dispatchMethod,
                static_cast<jint>(signal),
                javaValue,
                javaAdditionalValue);
        }
        if (javaAdditionalValue != nullptr) {
            env->DeleteLocalRef(javaAdditionalValue);
        }
        if (javaValue != nullptr) {
            env->DeleteLocalRef(javaValue);
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
        this->dispatchMethod = env->GetMethodID(dispatcherClass, "dispatch", "(ILjava/lang/String;Ljava/lang/String;)V");
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