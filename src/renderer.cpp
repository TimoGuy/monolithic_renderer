#include "renderer.h"

// Platform-specific implementations.
#if _WIN64
#include "renderer_win64_impl.h"
#else
#error "Unsupported OS"
#endif  // _WIN64, etc.


// Implementation wrapper.
Monolithic_renderer::Monolithic_renderer(
    const std::string& name,
    int32_t content_width,
    int32_t content_height)
    : m_pimpl(std::make_unique<Impl>(name, content_width, content_height, *this))
{
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

std::vector<Job_ifc*> Monolithic_renderer::fetch_next_jobs_callback()
{
    return m_pimpl->fetch_next_jobs_callback();
}
