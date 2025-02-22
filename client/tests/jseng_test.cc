#include <cstring>

#include <gtest/gtest.h>

#include "consts.h"
#include "jseng.h"
#include "module.h"
#include "state.h"
#include "utils.h"

static JsEng* ENG = nullptr;

class JsEngTest : public testing::Test {
protected:
    static void SetUpTestSuite() {
        ENG = new JsEng();
    }

    static void TearDownTestSuite() {
        delete ENG;
        ENG = nullptr;
    }
};

bool run_script(JsEng* eng, std::string path) {
    auto f = Utils::Fs::open(std::string(TEST_JS_SCRIPTS) + "lib.js", CONST_MODE_R);
    if (!f) return false;
    auto script = Utils::Fs::read_all(f);
    Utils::Fs::close(f);
    auto lib_str = std::string(script->begin(), script->end());
    delete script;

    path = TEST_JS_SCRIPTS + path;
    f = Utils::Fs::open(path, CONST_MODE_R);
    if (!f) return false;
    script = Utils::Fs::read_all(f);
    Utils::Fs::close(f);
    auto script_str = std::string(script->begin(), script->end());
    delete script;

    Module m;
    m.code = lib_str + "\n" + script_str;
#ifdef LINUX
    m.code += "\nif (TEST_LINUX) { TEST_LINUX(); }";
#endif
#ifdef WINDOWS
    m.code += "\nif (TEST_WINDOWS) { TEST_WINDOWS(); }";
#endif
    m.debug = false;

    State::Initialize(nullptr);
    return eng->RunModule(&m);
}

TEST_F(JsEngTest, Ctx) {
    ASSERT_TRUE(run_script(ENG, "test_ctx.js"));
    ASSERT_FALSE(STATE->get_errord());
    std::string output = STATE->get_output();
    ASSERT_EQ(output, "hello world\n");
}

TEST_F(JsEngTest, Mem) {
    ASSERT_TRUE(run_script(ENG, "test_mem.js"));
    ASSERT_FALSE(STATE->get_errord());
}

TEST_F(JsEngTest, Fs) {
    ASSERT_TRUE(run_script(ENG, "test_fs.js"));
    ASSERT_FALSE(STATE->get_errord());
}

TEST_F(JsEngTest, Ffi) {
    ASSERT_TRUE(run_script(ENG, "test_ffi.js"));
    ASSERT_FALSE(STATE->get_errord());
}

TEST_F(JsEngTest, Err) {
    // this module should be successfully processed but set the error state
    ASSERT_TRUE(run_script(ENG, "test_error.js"));
    ASSERT_TRUE(STATE->get_errord());
}