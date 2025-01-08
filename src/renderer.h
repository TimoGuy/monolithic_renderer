#pragma once

#include <functional>
#include <memory>
#include <string>


class Monolithic_renderer
{
public:
    Monolithic_renderer(const std::string& name);
    ~Monolithic_renderer();

    bool build();
    bool teardown();

    void tick();
    bool get_requesting_close();

private:
    class Impl;
    std::unique_ptr<Impl> m_pimpl;
};
