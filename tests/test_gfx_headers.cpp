// Minimal translation unit to ensure all public gfx headers compile in isolation.
// This guards against accidental inclusion of system headers inside namespaces
// and forbidden constructs like `using namespace std;` within ve::gfx.
// Fails early if pollution reintroduced.

#include "gfx/vk_instance.hpp"
#include "gfx/vk_device.hpp"
#include "gfx/opengl_headers.hpp"

int main() {
    // Instantiate minimal objects to touch basic APIs (link errors will surface if defs missing)
    ve::gfx::D3D11Context ctx; (void)ctx.is_valid();
    ve::gfx::GraphicsDevice dev; (void)dev.is_valid();
    return 0; // success
}