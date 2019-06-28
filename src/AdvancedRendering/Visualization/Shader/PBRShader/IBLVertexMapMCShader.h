//
// Created by wei on 4/13/19.
//

#pragma once

#include <Open3D/Open3D.h>
#include "AdvancedRendering/Visualization/Utility/BufferHelper.h"
#include <AdvancedRendering/Geometry/ExtendedTriangleMesh.h>

namespace open3d {
namespace visualization {

namespace glsl {
/** Lighting should have been processed before being passed here **/
class IBLVertexMapMCShader : public ShaderWrapper {
public:
    IBLVertexMapMCShader() : IBLVertexMapMCShader("IBLVertexMapMCShader") {}
    ~IBLVertexMapMCShader() override { Release(); }

protected:
    explicit IBLVertexMapMCShader(const std::string &name)
        : ShaderWrapper(name) { Compile(); }

protected:
    bool Compile() final;
    void Release() final;

    bool BindGeometry(const geometry::Geometry &geometry,
                      const RenderOption &option,
                      const ViewControl &view) final;

    bool RenderGeometry(const geometry::Geometry &geometry,
                        const RenderOption &option,
                        const ViewControl &view) final;

    void UnbindGeometry() final;

protected:
    bool PrepareRendering(const geometry::Geometry &geometry,
                          const RenderOption &option,
                          const ViewControl &view);
    bool PrepareBinding(const geometry::Geometry &geometry,
                        const RenderOption &option,
                        const ViewControl &view,
                        std::vector<Eigen::Vector3f> &points,
                        std::vector<Eigen::Vector3f> &normals,
                        std::vector<Eigen::Vector3f> &colors,
                        std::vector<Eigen::Vector3f> &materials,
                        std::vector<Eigen::Vector3i> &triangles);

protected:
    const int kNumEnvTextures = 2;

    /** locations **/
    /* vertex shader */
    GLuint M_;
    GLuint V_;
    GLuint P_;

    /* fragment shader */
    std::vector<GLuint> tex_env_symbols_;    /* 3 textures for env */
    GLuint camera_position_;

    /** buffers **/
    GLuint vertex_position_buffer_;
    GLuint vertex_normal_buffer_;
    GLuint vertex_color_buffer_;
    GLuint vertex_material_buffer_;
    GLuint triangle_buffer_;
};

}
}
}

