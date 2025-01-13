#pragma once

#include <functional>
#include <memory>
#include <string>
#include "multithreaded_job_system_public.h"


class Monolithic_renderer : public Job_source
{
public:
    Monolithic_renderer(const std::string& name, int32_t content_width, int32_t content_height);
    ~Monolithic_renderer();

    bool build();
    bool teardown();

    void tick();
    bool get_requesting_close();

private:
    std::vector<Job_ifc*> fetch_next_jobs_callback() override;

    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};
