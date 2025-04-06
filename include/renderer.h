#pragma once

#include <functional>
#include <memory>
#include <string>
#include "cglm/cglm.h"
#include "geo_render_pass.h"
#include "multithreaded_job_system_public.h"
namespace phys_obj { class Transform_holder; }


class Monolithic_renderer : public Job_source
{
public:
    Monolithic_renderer(const std::string& name,
                        int32_t content_width,
                        int32_t content_height,
                        int32_t fallback_content_width,
                        int32_t fallback_content_height);
    ~Monolithic_renderer();

    bool is_renderer_requesting_close();
    void request_shutdown_renderer();
    bool is_renderer_finished_shutdown();

#if _WIN64
    void notify_windowevent_uniconification();
#endif  // _WIN64

    // Render geometry objects.
    using render_geo_obj_key_t = uint64_t;
    render_geo_obj_key_t create_render_geo_obj(const std::string& model_name,
                                               const std::string& material_set_name,
                                               geo_instance::Geo_render_pass render_pass,
                                               bool is_shadow_caster,
                                               phys_obj::Transform_holder* transform_holder);
    void destroy_render_geo_obj(render_geo_obj_key_t key);
    void set_render_geo_obj_transform(render_geo_obj_key_t key,
                                      mat4 transform);

    class Impl;

private:
    Job_next_jobs_return_data fetch_next_jobs_callback() override;

    std::unique_ptr<Impl> m_pimpl;
};
