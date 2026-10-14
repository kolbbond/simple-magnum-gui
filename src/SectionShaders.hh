// internal ScenePanel shaders: Magnum Phong / VertexColor equivalents plus a
// fragment-discard section plane (stock Magnum shaders have no clip hook)
#pragma once

#include <Magnum/GL/AbstractShaderProgram.h>
#include <Magnum/Magnum.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix3.h>
#include <Magnum/Math/Matrix4.h>
#include <Magnum/Math/Vector4.h>

namespace smg {

// single directional light, vertex colour multiplies ambient + diffuse;
// back faces render as flat cap_color so a cut solid reads as filled
class SectionPhongGL: public Magnum::GL::AbstractShaderProgram {
public:
    SectionPhongGL();

    SectionPhongGL& set_transformation(const Magnum::Matrix4& m);
    SectionPhongGL& set_projection(const Magnum::Matrix4& m);
    SectionPhongGL& set_normal_matrix(const Magnum::Matrix3x3& m);
    SectionPhongGL& set_light_direction(const Magnum::Vector3& view_dir);
    SectionPhongGL& set_ambient(const Magnum::Color3& c);
    SectionPhongGL& set_diffuse(const Magnum::Color3& c);
    SectionPhongGL& set_specular(const Magnum::Color3& c);
    SectionPhongGL& set_shininess(float s);
    SectionPhongGL& set_section(const Magnum::Vector4& view_plane);
    SectionPhongGL& set_cap_color(const Magnum::Color3& c);

private:
    Magnum::Int _transformation{};
    Magnum::Int _projection{};
    Magnum::Int _normal{};
    Magnum::Int _light{};
    Magnum::Int _ambient{};
    Magnum::Int _diffuse{};
    Magnum::Int _specular{};
    Magnum::Int _shininess{};
    Magnum::Int _section{};
    Magnum::Int _cap{};
};

class SectionVertexColorGL: public Magnum::GL::AbstractShaderProgram {
public:
    SectionVertexColorGL();

    SectionVertexColorGL& set_transformation(const Magnum::Matrix4& m);
    SectionVertexColorGL& set_projection(const Magnum::Matrix4& m);
    SectionVertexColorGL& set_section(const Magnum::Vector4& view_plane);

private:
    Magnum::Int _transformation{};
    Magnum::Int _projection{};
    Magnum::Int _section{};
};

} // namespace smg
