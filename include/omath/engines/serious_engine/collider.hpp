//
// Created by Orange on 10/21/2026.
//
#pragma once
#include "mesh.hpp"
#include "omath/3d_primitives/mesh.hpp"
#include "omath/collision/mesh_collider.hpp"

namespace omath::serious_engine
{
    using MeshCollider = collision::MeshCollider<Mesh>;
}
