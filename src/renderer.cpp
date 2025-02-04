#include "renderer.h"
#include "gltf_loader.h"


// Platform-specific implementations.
#if _WIN64
#include "renderer_win64_impl.h"
#else
#error "Unsupported OS"
#endif  // _WIN64, etc.


extern std::atomic<Monolithic_renderer*> s_mr_singleton_ptr;

// Implementation wrapper.
Monolithic_renderer::Monolithic_renderer(
    const std::string& name,
    int32_t content_width,
    int32_t content_height,
    int32_t fallback_content_width,
    int32_t fallback_content_height)
    : m_pimpl(std::make_unique<Impl>(name,
                                     content_width,
                                     content_height,
                                     fallback_content_width,
                                     fallback_content_height,
                                     *this))
{
    gltf_loader::load_gltf("C:\\Users\\Timo\\Documents\\Repositories\\solanine_vulkan\\solanine_vulkan\\res\\models\\SlimeGirl.glb");
    Monolithic_renderer* expected{ nullptr };
    if (!s_mr_singleton_ptr.compare_exchange_strong(expected, this))
    {
        // Trigger assert if multiple monolithic renderers are initialized. Only
        // one is allowed.
        assert(false);
    }
}

// @NOTE: For smart pointer pimpl, must define destructor
//        in the .cpp file, even if it's `default`.
Monolithic_renderer::~Monolithic_renderer() = default;

bool Monolithic_renderer::is_renderer_requesting_close()
{
    return m_pimpl->is_requesting_close();
}

void Monolithic_renderer::request_shutdown_renderer()
{
    m_pimpl->request_shutdown();
}

bool Monolithic_renderer::is_renderer_finished_shutdown()
{
    return m_pimpl->is_finished_shutdown();
}

#if _WIN64
void Monolithic_renderer::notify_windowevent_uniconification()
{
    m_pimpl->notify_uniconification();
}
#endif  // _WIN64

Job_source::Job_next_jobs_return_data Monolithic_renderer::fetch_next_jobs_callback()
{
    return m_pimpl->fetch_next_jobs_callback();
}
