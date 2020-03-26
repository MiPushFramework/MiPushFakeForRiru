#include <jni.h>
#include <sys/types.h>
#include <stdio.h>
#include <string>
#include <vector>
#include <unistd.h>
#include "logging.h"
#include "hook.h"

#if PLATFORM_SDK_VERSION >= 24
#define CONFIG_PATH "/data/user_de/0/top.trumeet.mipush"
#else
#define CONFIG_PATH "/data/data/top.trumeet.mipush"
#endif

// You can remove functions you don't need
static char package_name[256];
static bool enable_hook;
static int uid;

static std::vector<std::string> globalPkgBlackList = {"com.xiaomi.xmsf",
                                                      "top.trumeet.mipush",
                                                      // Android
                                                      "com.google.android",
                                                      "android",
                                                      "com.android",
                                                      // Samsung
                                                      "com.bst",
                                                      "com.sec",
                                                      "com.sem",
                                                      "com.sgmc",
                                                      "com.dsi.ant",
                                                      "com.wsomacp",
                                                      "com.samsung",
                                                      "com.diotek.sec",
                                                      "com.enhance.gameservice",
                                                      // XDA
                                                      "com.xda",
                                                      // Wechat
                                                      "com.tencent.mm",
                                                      // Quickpay
                                                      "com.example",
                                                      // Magisk
                                                      "com.topjohnwu.magisk",
                                                      // Xposed
                                                      "org.meowcat.edxposed.manager",
                                                      "de.robv.android.xposed.installer",
                                                      // Rikka apps
                                                      "rikka.appops",
                                                      "moe.shizuku.redirectstorage"};


bool isAppNeedHook(JNIEnv *pEnv, jstring pJstring);

void injectBuild(JNIEnv *pEnv);

static void appProcessPre(JNIEnv *env, jint _uid, jstring appDataDir, jstring packageName);

static void appProcessPost(JNIEnv *env);

extern "C" {
#define EXPORT __attribute__((visibility("default"))) __attribute__((used))
EXPORT void nativeForkAndSpecializePre(
        JNIEnv *env, jclass clazz, jint *_uid, jint *gid, jintArray *gids, jint *runtimeFlags,
        jobjectArray *rlimits, jint *mountExternal, jstring *seInfo, jstring *niceName,
        jintArray *fdsToClose, jintArray *fdsToIgnore, jboolean *is_child_zygote,
        jstring *instructionSet, jstring *appDataDir, jstring *packageName,
        jobjectArray *packagesForUID, jstring *sandboxId) {
    // packageName, packagesForUID, sandboxId are added from Android Q beta 2, removed from beta 5

    appProcessPre(env, *_uid, *appDataDir, *packageName);
}

EXPORT int nativeForkAndSpecializePost(JNIEnv *env, jclass clazz, jint res) {
    if (res == 0) {
        // in app process
        appProcessPost(env);
    } else {
        // in zygote process, res is child pid
        // don't print log here, see https://github.com/RikkaApps/Riru/blob/77adfd6a4a6a81bfd20569c910bc4854f2f84f5e/riru-core/jni/main/jni_native_method.cpp#L55-L66
    }
    return 0;
}

EXPORT __attribute__((visibility("default"))) void specializeAppProcessPre(
        JNIEnv *env, jclass clazz, jint *_uid, jint *gid, jintArray *gids, jint *runtimeFlags,
        jobjectArray *rlimits, jint *mountExternal, jstring *seInfo, jstring *niceName,
        jboolean *startChildZygote, jstring *instructionSet, jstring *appDataDir,
        jstring *packageName, jobjectArray *packagesForUID, jstring *sandboxId) {
    // this is added from Android Q beta, but seems Google disabled this in following updates

    // packageName, packagesForUID, sandboxId are added from Android Q beta 2, removed from beta 5

    appProcessPre(env, *_uid, *appDataDir, *packageName);
}

EXPORT __attribute__((visibility("default"))) int specializeAppProcessPost(
        JNIEnv *env, jclass clazz) {
    // this is added from Android Q beta, but seems Google disabled this in following updates

    appProcessPost(env);
    return 0;
}

EXPORT void nativeForkSystemServerPre(
        JNIEnv *env, jclass clazz, uid_t *uid, gid_t *gid, jintArray *gids, jint *runtimeFlags,
        jobjectArray *rlimits, jlong *permittedCapabilities, jlong *effectiveCapabilities) {

}

EXPORT int nativeForkSystemServerPost(JNIEnv *env, jclass clazz, jint res) {
    if (res == 0) {
        // in system server process
    } else {
        // in zygote process, res is child pid
        // don't print log here, see https://github.com/RikkaApps/Riru/blob/77adfd6a4a6a81bfd20569c910bc4854f2f84f5e/riru-core/jni/main/jni_native_method.cpp#L55-L66
    }
    return 0;
}

EXPORT int shouldSkipUid(int uid) {
    // by default, Riru only call module functions in "normal app processes" (10000 <= uid % 100000 <= 19999)
    // false = don't skip
    return false;
}

EXPORT void onModuleLoaded() {
    // called when the shared library of Riru core is loaded
}
}

void injectBuild(JNIEnv *env) {
    if (env == nullptr) {
        LOGW("failed to inject android.os.Build for %s due to env is null", package_name);
        return;
    }
    LOGI("inject android.os.Build for %s ", package_name);

    jclass build_class = env->FindClass("android/os/Build");
    if (build_class == nullptr) {
        LOGW("failed to inject android.os.Build for %s due to build is null", package_name);
        return;
    }

    jstring new_str = env->NewStringUTF("Xiaomi");

    jfieldID brand_id = env->GetStaticFieldID(build_class, "BRAND", "Ljava/lang/String;");
    if (brand_id != nullptr) {
        env->SetStaticObjectField(build_class, brand_id, new_str);
    }

    jfieldID manufacturer_id = env->GetStaticFieldID(build_class, "MANUFACTURER", "Ljava/lang/String;");
    if (manufacturer_id != nullptr) {
        env->SetStaticObjectField(build_class, manufacturer_id, new_str);
    }

    jfieldID product_id = env->GetStaticFieldID(build_class, "PRODUCT", "Ljava/lang/String;");
    if (product_id != nullptr) {
        env->SetStaticObjectField(build_class, product_id, new_str);
    }

    if(env->ExceptionCheck())
    {
        env->ExceptionClear();
    }

    env->DeleteLocalRef(new_str);
}

bool isAppNeedHook(JNIEnv *env, jstring jAppDataDir) {
    if (jAppDataDir == nullptr) {
        return false;
    }

    const char *appDataDir = env->GetStringUTFChars(jAppDataDir, nullptr);

    int user = 0;
    while (true) {
        // /data/user/<user_id>/<package>
        if (sscanf(appDataDir, "/data/%*[^/]/%d/%s", &user, package_name) == 2)
            break;

        // /mnt/expand/<id>/user/<user_id>/<package>
        if (sscanf(appDataDir, "/mnt/expand/%*[^/]/%*[^/]/%d/%s", &user, package_name) == 2)
            break;

        // /data/data/<package>
        if (sscanf(appDataDir, "/data/%*[^/]/%s", package_name) == 1)
            break;

        package_name[0] = '\0';
        return false;
    }
    env->ReleaseStringUTFChars(jAppDataDir, appDataDir);

    std::string pkgName = package_name;
    for (auto &s : globalPkgBlackList) {
        if (pkgName.find(s) != std::string::npos) {
            return false;
        }
    }

    if (access(CONFIG_PATH "/packages/ALL", F_OK) == 0) {
        return true;
    }

    if (access(CONFIG_PATH "/packages", R_OK) == 0) {
        char path[PATH_MAX];
        snprintf(path, PATH_MAX, CONFIG_PATH "/packages/%d.%s", user, package_name);
        return access(path, F_OK) == 0;
    }

    return false;
}



static void appProcessPre(JNIEnv *env, jint _uid, jstring appDataDir, jstring packageName) {

    uid = _uid;
    enable_hook = isAppNeedHook(env, appDataDir);
}


static void appProcessPost(JNIEnv *env) {

    if (enable_hook) {
        injectBuild(env);
        install_hook(package_name, uid / 100000);
    }
}