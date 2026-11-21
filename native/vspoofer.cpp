// V Spoofer - Zygisk component. Author: @Xverno
// Spoofs device identity only inside the selected game processes.
#include <jni.h>
#include <android/log.h>
#include <sys/system_properties.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "zygisk.hpp"

#define TAG "VSpoofer"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

static const char *DATA = "/data/adb/vspoofer";
static const char *PRESETS = "/data/adb/modules/vspoofer/presets.conf";

struct Spoof {
    bool on = false;
    std::string brand, manu, model, dev, name;
};

// ---------------------------------------------------------------- IO helpers
static bool rd(int fd, void *b, size_t n) {
    auto p = static_cast<char *>(b);
    while (n) {
        ssize_t r = read(fd, p, n);
        if (r <= 0) return false;
        p += r; n -= static_cast<size_t>(r);
    }
    return true;
}
static bool wr(int fd, const void *b, size_t n) {
    auto p = static_cast<const char *>(b);
    while (n) {
        ssize_t r = write(fd, p, n);
        if (r <= 0) return false;
        p += r; n -= static_cast<size_t>(r);
    }
    return true;
}
static bool rd_str(int fd, std::string &s) {
    uint32_t l = 0;
    if (!rd(fd, &l, 4) || l > 4096) return false;
    s.resize(l);
    return l == 0 || rd(fd, &s[0], l);
}
static bool wr_str(int fd, const std::string &s) {
    uint32_t l = static_cast<uint32_t>(s.size());
    return wr(fd, &l, 4) && (l == 0 || wr(fd, s.data(), l));
}

// ------------------------------------------------- companion (runs as root)
static std::string trim(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ')) s.pop_back();
    return s;
}
static std::string cfg_get(const std::string &key) {
    std::ifstream f(std::string(DATA) + "/config");
    std::string l;
    while (std::getline(f, l)) {
        l = trim(l);
        if (l.rfind(key + "=", 0) == 0) return l.substr(key.size() + 1);
    }
    return "";
}
static bool in_games(const std::string &pkg) {
    std::ifstream f(std::string(DATA) + "/games.list");
    std::string l;
    while (std::getline(f, l)) if (trim(l) == pkg) return true;
    return false;
}
static bool find_preset(const std::string &file, const std::string &id, std::vector<std::string> &out) {
    out.clear();
    std::ifstream f(file);
    std::string l;
    while (std::getline(f, l)) {
        l = trim(l);
        if (l.rfind(id + "|", 0) != 0) continue;
        size_t start = 0, pos;
        while ((pos = l.find('|', start)) != std::string::npos) {
            out.push_back(l.substr(start, pos - start));
            start = pos + 1;
        }
        out.push_back(l.substr(start));
        return out.size() >= 7;
    }
    return false;
}
static Spoof decide(const std::string &pkg) {
    Spoof s;
    std::string mode = cfg_get("MODE"), eng = cfg_get("ENGINE");
    if (eng.empty()) eng = "zygisk";
    if (mode != "perapp" || eng != "zygisk") return s;
    if (!in_games(pkg)) return s;
    std::string id = cfg_get("PRESET");
    if (id.empty()) id = "rog7";
    std::vector<std::string> p;
    if (!find_preset(std::string(DATA) + "/custom.conf", id, p) && !find_preset(PRESETS, id, p)) return s;
    s.brand = p[2]; s.manu = p[3]; s.model = p[4]; s.dev = p[5]; s.name = p[6];
    s.on = true;
    return s;
}
static void companion_handler(int fd) {
    std::string pkg;
    if (!rd_str(fd, pkg)) return;
    Spoof s = decide(pkg);
    uint8_t on = s.on ? 1 : 0;
    if (!wr(fd, &on, 1) || !on) return;
    wr_str(fd, s.brand); wr_str(fd, s.manu); wr_str(fd, s.model);
    wr_str(fd, s.dev);   wr_str(fd, s.name);
}

// ------------------------------------------------------- in-process hooks
static Spoof g;

static const char *lookup(const char *n) {
    if (!n || strncmp(n, "ro.", 3) != 0) return nullptr;
    if (!strcmp(n, "ro.build.product")) return g.dev.c_str();
    if (strncmp(n, "ro.product.", 11) != 0) return nullptr;
    const char *r = n + 11;
    static const char *parts[] = {"system.", "vendor.", "odm.", "product.", "system_ext.", "bootimage."};
    for (const char *p : parts) {
        size_t l = strlen(p);
        if (!strncmp(r, p, l)) { r += l; break; }
    }
    if (!strcmp(r, "model")) return g.model.c_str();
    if (!strcmp(r, "manufacturer")) return g.manu.c_str();
    if (!strcmp(r, "brand")) return g.brand.c_str();
    if (!strcmp(r, "device")) return g.dev.c_str();
    if (!strcmp(r, "name")) return g.name.c_str();
    return nullptr;
}

static int (*old_get)(const char *, char *) = nullptr;
static int my_get(const char *name, char *value) {
    const char *v = lookup(name);
    if (v && value) {
        size_t l = strlen(v);
        if (l >= PROP_VALUE_MAX) l = PROP_VALUE_MAX - 1;
        memcpy(value, v, l);
        value[l] = 0;
        return static_cast<int>(l);
    }
    return old_get ? old_get(name, value) : 0;
}

typedef void (*cb_t)(void *, const char *, const char *, uint32_t);
struct Ctx { cb_t cb; void *cookie; };
static void tramp(void *c, const char *n, const char *v, uint32_t s) {
    auto x = static_cast<Ctx *>(c);
    const char *o = lookup(n);
    x->cb(x->cookie, n, o ? o : v, s);
}
static void (*old_rcb)(const prop_info *, cb_t, void *) = nullptr;
static void my_rcb(const prop_info *pi, cb_t cb, void *cookie) {
    Ctx c{cb, cookie};
    if (old_rcb) old_rcb(pi, tramp, &c);
}

class VSpoof : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        std::string pkg;
        if (args->nice_name) {
            const char *nn = env->GetStringUTFChars(args->nice_name, nullptr);
            if (nn) { pkg = nn; env->ReleaseStringUTFChars(args->nice_name, nn); }
        }
        size_t c = pkg.find(':');
        if (c != std::string::npos) pkg.resize(c);
        if (pkg.empty()) return skip();

        int fd = api->connectCompanion();
        if (fd < 0) return skip();
        uint8_t on = 0;
        bool ok = wr_str(fd, pkg) && rd(fd, &on, 1);
        if (ok && on) {
            Spoof s;
            ok = rd_str(fd, s.brand) && rd_str(fd, s.manu) && rd_str(fd, s.model) &&
                 rd_str(fd, s.dev) && rd_str(fd, s.name);
            if (ok) { s.on = true; g = s; }
        }
        close(fd);
        if (!g.on) return skip();
        installHooks();
        LOGI("spoofing %s as %s", pkg.c_str(), g.model.c_str());
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *) override {
        if (g.on) setBuildFields();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *) override { skip(); }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;

    void skip() { api->setOption(zygisk::Option::DLCLOSE_MODULE_LIBRARY); }

    void installHooks() {
#ifdef VS_PLT_REGEX
        api->pltHookRegister(".*", "__system_property_get", reinterpret_cast<void *>(my_get), reinterpret_cast<void **>(&old_get));
        api->pltHookRegister(".*", "__system_property_read_callback", reinterpret_cast<void *>(my_rcb), reinterpret_cast<void **>(&old_rcb));
#else
        std::ifstream m("/proc/self/maps");
        std::set<std::pair<dev_t, ino_t>> seen;
        std::string l;
        while (std::getline(m, l)) {
            size_t p = l.find('/');
            if (p == std::string::npos) continue;
            std::string path = l.substr(p);
            if (path.size() < 4 || path.compare(path.size() - 3, 3, ".so") != 0) continue;
            struct stat st;
            if (stat(path.c_str(), &st) != 0) continue;
            if (!seen.insert({st.st_dev, st.st_ino}).second) continue;
            api->pltHookRegister(st.st_dev, st.st_ino, "__system_property_get", reinterpret_cast<void *>(my_get), reinterpret_cast<void **>(&old_get));
            api->pltHookRegister(st.st_dev, st.st_ino, "__system_property_read_callback", reinterpret_cast<void *>(my_rcb), reinterpret_cast<void **>(&old_rcb));
        }
#endif
        api->pltHookCommit();
    }

    void setStr(jclass cls, const char *field, const std::string &v) {
        jfieldID f = env->GetStaticFieldID(cls, field, "Ljava/lang/String;");
        if (!f) { env->ExceptionClear(); return; }
        jstring s = env->NewStringUTF(v.c_str());
        env->SetStaticObjectField(cls, f, s);
    }

    void setBuildFields() {
        jclass cls = env->FindClass("android/os/Build");
        if (!cls) { env->ExceptionClear(); return; }
        setStr(cls, "MODEL", g.model);
        setStr(cls, "BRAND", g.brand);
        setStr(cls, "MANUFACTURER", g.manu);
        setStr(cls, "DEVICE", g.dev);
        setStr(cls, "PRODUCT", g.name);
    }
};

REGISTER_ZYGISK_MODULE(VSpoof)
REGISTER_ZYGISK_COMPANION(companion_handler)
