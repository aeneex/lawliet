// tests/test_runner.cpp
// Minimal test harness — replaces a heavy framework dependency in the skeleton.
// Each test function returns true on pass, false on failure.

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// ── Harness ───────────────────────────────────────────────────────────────────

struct TestCase {
    std::string name;
    std::function<bool()> fn;
};

static std::vector<TestCase>& registry()
{
    static std::vector<TestCase> r;
    return r;
}

void registerTest(const std::string& name, std::function<bool()> fn)
{
    registry().push_back({name, std::move(fn)});
}

int main()
{
    int passed = 0, failed = 0;
    for (const auto& tc : registry()) {
        bool ok = false;
        try {
            ok = tc.fn();
        } catch (const std::exception& e) {
            std::printf("  EXCEPTION: %s\n", e.what());
        } catch (...) {
            std::printf("  EXCEPTION: unknown\n");
        }

        if (ok) {
            std::printf("  PASS  %s\n", tc.name.c_str());
            ++passed;
        } else {
            std::printf("  FAIL  %s\n", tc.name.c_str());
            ++failed;
        }
    }

    std::printf("\n%d passed, %d failed.\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
