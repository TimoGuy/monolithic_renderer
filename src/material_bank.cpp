#include "material_bank.h"

#include <cassert>
#include <mutex>
#include <unordered_map>


namespace material_bank
{

// Pipeline containers.
static std::unordered_map<std::string, uint32_t> s_pipe_name_to_idx;
static std::mutex s_pipe_name_to_idx_mutex;

static std::vector<GPU_pipeline> s_all_pipelines;
static std::mutex s_all_pipelines_mutex;

// Material containers.
static std::unordered_map<std::string, uint32_t> s_mat_name_to_idx;
static std::mutex s_mat_name_to_idx_mutex;

static std::vector<GPU_material> s_all_materials;
static std::mutex s_all_materials_mutex;

// Material set containers.
static std::unordered_map<std::string, uint32_t> s_mat_set_name_to_idx;
static std::mutex s_mat_set_name_to_idx_mutex;

static std::vector<GPU_material_set> s_all_material_sets;
static std::mutex s_all_material_sets_mutex;

}  // namespace material_bank


// Pipeline.
uint32_t material_bank::register_pipeline(const std::string& pipe_name,
                                          GPU_pipeline&& new_pipeline)
{
    size_t emplace_idx;
    {
        std::lock_guard<std::mutex> lock{ s_all_pipelines_mutex };
        emplace_idx = s_all_pipelines.size();
        s_all_pipelines.emplace_back(std::move(new_pipeline));
    }
    {
        std::lock_guard<std::mutex> lock{ s_pipe_name_to_idx_mutex };
        s_pipe_name_to_idx.emplace(std::string(pipe_name), static_cast<uint32_t>(emplace_idx));
    }
    return emplace_idx;
}

uint32_t material_bank::get_pipeline_idx_from_name(const std::string& pipe_name)
{
    std::lock_guard<std::mutex> lock{ s_pipe_name_to_idx_mutex };
    auto it{ s_pipe_name_to_idx.find(pipe_name) };
    if (it == s_pipe_name_to_idx.end())
    {
        // Return invalid idx.
        return (uint32_t)-1;
    }
    return it->second;
}

const material_bank::GPU_pipeline& material_bank::get_pipeline(uint32_t idx)
{
    std::lock_guard<std::mutex> lock{ s_all_pipelines_mutex };
    assert(idx < s_all_pipelines.size());
    return s_all_pipelines[idx];
}

bool material_bank::teardown_all_pipelines()
{
    // @TODO: implement.
    assert(false);
    return true;
}

// Material.
uint32_t material_bank::register_material(const std::string& mat_name,
                                          GPU_material&& new_material)
{
    size_t emplace_idx;
    {
        std::lock_guard<std::mutex> lock{ s_all_materials_mutex };
        emplace_idx = s_all_materials.size();
        s_all_materials.emplace_back(std::move(new_material));
    }
    {
        std::lock_guard<std::mutex> lock{ s_mat_name_to_idx_mutex };
        s_mat_name_to_idx.emplace(std::string(mat_name), static_cast<uint32_t>(emplace_idx));
    }
    return emplace_idx;
}

bool material_bank::cook_all_material_param_indices()
{
    std::lock_guard<std::mutex> lock1{ s_all_pipelines_mutex };
    std::lock_guard<std::mutex> lock2{ s_all_materials_mutex };

    std::vector<uint32_t> local_mat_counts;
    local_mat_counts.resize(s_all_pipelines.size(), 0);

    for (auto& material : s_all_materials)
    {
        uint32_t local_idx{ local_mat_counts[material.pipeline_idx]++ };
        material.cooked_material_param_local_idx = local_idx;
    }

    return true;
}

uint32_t material_bank::get_mat_idx_from_name(const std::string& mat_name)
{
    std::lock_guard<std::mutex> lock{ s_mat_name_to_idx_mutex };
    auto it{ s_mat_name_to_idx.find(mat_name) };
    if (it == s_mat_name_to_idx.end())
    {
        // Return invalid idx.
        return (uint32_t)-1;
    }
    return it->second;
}

const material_bank::GPU_material& material_bank::get_material(uint32_t idx)
{
    std::lock_guard<std::mutex> lock{ s_all_materials_mutex };
    assert(idx < s_all_materials.size());
    return s_all_materials[idx];
}

bool material_bank::teardown_all_materials()
{
    // @TODO: implement.
    assert(false);
    return true;
}

// Material set.
uint32_t material_bank::register_material_set(const std::string& mat_set_name,
                                              std::vector<uint32_t>&& new_material_set)
{
    size_t emplace_idx;
    {
        std::lock_guard<std::mutex> lock{ s_all_material_sets_mutex };
        emplace_idx = s_all_material_sets.size();
        s_all_material_sets.emplace_back(std::move(new_material_set));
    }
    {
        std::lock_guard<std::mutex> lock{ s_mat_set_name_to_idx_mutex };
        s_mat_set_name_to_idx.emplace(std::string(mat_set_name), static_cast<uint32_t>(emplace_idx));
    }
    return emplace_idx;
}

uint32_t material_bank::get_mat_set_idx_from_name(const std::string& mat_set_name)
{
    std::lock_guard<std::mutex> lock{ s_mat_set_name_to_idx_mutex };
    auto it{ s_mat_set_name_to_idx.find(mat_set_name) };
    if (it == s_mat_set_name_to_idx.end())
    {
        // Return invalid idx.
        return (uint32_t)-1;
    }
    return it->second;
}

const material_bank::GPU_material_set& material_bank::get_material_set(uint32_t idx)
{
    std::lock_guard<std::mutex> lock{ s_all_material_sets_mutex };
    assert(idx < s_all_material_sets.size());
    return s_all_material_sets[idx];
}
