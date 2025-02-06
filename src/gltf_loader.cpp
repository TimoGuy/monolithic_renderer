#include "gltf_loader.h"

#include <filesystem>
#include <iostream>
#include "fastgltf/core.hpp"
#include "fastgltf/types.hpp"
#include "fastgltf/tools.hpp"
#include "material_bank.h"


namespace gltf_loader
{

static std::vector<Model> s_all_models;

}  // namespace gltf_loader

void gltf_loader::load_gltf(const std::string& path_str)
{
    std::filesystem::path path{ path_str };
    if (!std::filesystem::exists(path))
    {
        std::cerr << "ERROR: Load gltf " << path << std::endl;
        return;
    }

    static constexpr auto gltf_extensions{ fastgltf::Extensions::None };
    static constexpr auto gltf_options{ fastgltf::Options::DontRequireValidAssetMember |
                                        fastgltf::Options::LoadExternalBuffers };

    fastgltf::Parser parser{ gltf_extensions };
    auto gltf_file{ fastgltf::MappedGltfFile::FromPath(path) };
    if (!gltf_file)
    {
        std::cerr << "ERROR: Opening gltf file failed." << std::endl;
        return;
    }

    auto asset_expect{ parser.loadGltf(gltf_file.get(), path.parent_path(), gltf_options) };
    if (asset_expect.error() != fastgltf::Error::None)
    {
        std::cerr << "ERROR: Loading gltf file asset failed." << std::endl;
        return;
    }

    auto asset{ std::move(asset_expect.get()) };

    // @TODO: load in the geometry, animations, and material names
    // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

    // Check that all material names are valid.
    std::vector<uint32_t> mesh_indexes;
    mesh_indexes.reserve(asset.meshes.size());
    for (auto& mesh : asset.meshes)
    {
        std::string mesh_name{ mesh.name };
        auto mesh_idx{
            material_bank::get_mat_idx_from_name(mesh_name) };
        if (mesh_idx == material_bank::k_invalid_material_idx)
        {
            std::cerr
                << "ERROR: Validating material \"" << mesh_name << "\" failed."
                << std::endl;
            return;
        }
        mesh_indexes.emplace_back(mesh_idx);
    }

    // Assign material id to each mesh.


    // Load geometry onto gpu with material information.

    // If animated:

        // Load in animations and flag meshes into staging buffer.

    // If not animated:

        // Generate distance field if not generated.
        // (Do it by cpu or gpu??)

        // Save generated distance field.

    // Create bounding sphere for meshes.

#if _DEBUG
    // Give aabb score for mesh vs bounding sphere
    // (make sure all 3 dimensions are as similar as possible so that occlusion check is easiest).
    // This just goes into the console or in a stats sheet for improving the assets.
    // There could also be something like a warning if the score is bad enough.
#endif  // _DEBUG
}

