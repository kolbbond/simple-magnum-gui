#include "SectionShaders.hh"

#include <Corrade/Utility/Assert.h>
#include <Magnum/GL/Shader.h>
#include <Magnum/GL/Version.h>
#include <Magnum/Shaders/GenericGL.h>

namespace smg {

namespace {

#ifdef MAGNUM_TARGET_GLES
constexpr Magnum::GL::Version glsl_version = Magnum::GL::Version::GLES300;
constexpr const char* precision = "precision highp float;\n";
#else
constexpr Magnum::GL::Version glsl_version = Magnum::GL::Version::GL330;
constexpr const char* precision = "";
#endif

// plane is in view space; disabled plane is (0,0,0,-1), which never discards
constexpr const char* phong_vert = R"(
in vec4 position;
in vec3 normal;
in vec3 color;
uniform mat4 transformationMatrix;
uniform mat4 projectionMatrix;
uniform mat3 normalMatrix;
out vec3 viewPosition;
out vec3 viewNormal;
out vec3 vertexColor;
void main() {
    vec4 p = transformationMatrix*position;
    viewPosition = p.xyz/p.w;
    viewNormal = normalMatrix*normal;
    vertexColor = color;
    gl_Position = projectionMatrix*p;
}
)";

// lighting mirrors Magnum's Phong.frag for one directional white light
constexpr const char* phong_frag = R"(
uniform vec4 lightPosition;
uniform vec3 ambientColor;
uniform vec3 diffuseColor;
uniform vec3 specularColor;
uniform float shininess;
uniform vec4 sectionPlane;
uniform vec3 capColor;
in vec3 viewPosition;
in vec3 viewNormal;
in vec3 vertexColor;
out vec4 fragmentColor;
void main() {
    if(dot(sectionPlane, vec4(viewPosition, 1.0)) > 0.0) discard;
    if(!gl_FrontFacing) {
        fragmentColor = vec4(capColor, 1.0);
        return;
    }
    vec3 n = normalize(viewNormal);
    vec3 l = normalize(lightPosition.xyz - viewPosition*lightPosition.w);
    vec3 color = ambientColor*vertexColor;
    float intensity = max(0.0, dot(n, l));
    color += diffuseColor*vertexColor*intensity;
    if(intensity > 0.001) {
        vec3 r = reflect(-l, n);
        color += specularColor*clamp(pow(max(0.0, dot(normalize(-viewPosition), r)), shininess), 0.0, 1.0);
    }
    fragmentColor = vec4(color, 1.0);
}
)";

constexpr const char* vcolor_vert = R"(
in vec4 position;
in vec3 color;
uniform mat4 transformationMatrix;
uniform mat4 projectionMatrix;
out vec3 viewPosition;
out vec3 vertexColor;
void main() {
    vec4 p = transformationMatrix*position;
    viewPosition = p.xyz/p.w;
    vertexColor = color;
    gl_Position = projectionMatrix*p;
}
)";

constexpr const char* vcolor_frag = R"(
uniform vec4 sectionPlane;
in vec3 viewPosition;
in vec3 vertexColor;
out vec4 fragmentColor;
void main() {
    if(dot(sectionPlane, vec4(viewPosition, 1.0)) > 0.0) discard;
    fragmentColor = vec4(vertexColor, 1.0);
}
)";

struct ShaderPair {
    Magnum::GL::Shader vert;
    Magnum::GL::Shader frag;
};

ShaderPair compile(const char* vert_src, const char* frag_src) {
    ShaderPair s{ Magnum::GL::Shader{ glsl_version, Magnum::GL::Shader::Type::Vertex },
        Magnum::GL::Shader{ glsl_version, Magnum::GL::Shader::Type::Fragment } };
    s.vert.addSource(precision).addSource(vert_src);
    s.frag.addSource(precision).addSource(frag_src);
    CORRADE_INTERNAL_ASSERT_OUTPUT(s.vert.compile() && s.frag.compile());
    return s;
}

} // namespace

SectionPhongGL::SectionPhongGL() {
    ShaderPair s = compile(phong_vert, phong_frag);
    attachShaders({ s.vert, s.frag });
    bindAttributeLocation(Magnum::Shaders::GenericGL3D::Position::Location, "position");
    bindAttributeLocation(Magnum::Shaders::GenericGL3D::Normal::Location, "normal");
    bindAttributeLocation(Magnum::Shaders::GenericGL3D::Color3::Location, "color");
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());
    _transformation = uniformLocation("transformationMatrix");
    _projection = uniformLocation("projectionMatrix");
    _normal = uniformLocation("normalMatrix");
    _light = uniformLocation("lightPosition");
    _ambient = uniformLocation("ambientColor");
    _diffuse = uniformLocation("diffuseColor");
    _specular = uniformLocation("specularColor");
    _shininess = uniformLocation("shininess");
    _section = uniformLocation("sectionPlane");
    _cap = uniformLocation("capColor");
}

SectionPhongGL& SectionPhongGL::set_transformation(const Magnum::Matrix4& m) {
    setUniform(_transformation, m);
    return *this;
}
SectionPhongGL& SectionPhongGL::set_projection(const Magnum::Matrix4& m) {
    setUniform(_projection, m);
    return *this;
}
SectionPhongGL& SectionPhongGL::set_normal_matrix(const Magnum::Matrix3x3& m) {
    setUniform(_normal, m);
    return *this;
}
SectionPhongGL& SectionPhongGL::set_light_direction(const Magnum::Vector3& view_dir) {
    setUniform(_light, Magnum::Vector4{ view_dir, 0.0f });
    return *this;
}
SectionPhongGL& SectionPhongGL::set_ambient(const Magnum::Color3& c) {
    setUniform(_ambient, c);
    return *this;
}
SectionPhongGL& SectionPhongGL::set_diffuse(const Magnum::Color3& c) {
    setUniform(_diffuse, c);
    return *this;
}
SectionPhongGL& SectionPhongGL::set_specular(const Magnum::Color3& c) {
    setUniform(_specular, c);
    return *this;
}
SectionPhongGL& SectionPhongGL::set_shininess(float s) {
    setUniform(_shininess, s);
    return *this;
}
SectionPhongGL& SectionPhongGL::set_section(const Magnum::Vector4& view_plane) {
    setUniform(_section, view_plane);
    return *this;
}
SectionPhongGL& SectionPhongGL::set_cap_color(const Magnum::Color3& c) {
    setUniform(_cap, c);
    return *this;
}

SectionVertexColorGL::SectionVertexColorGL() {
    ShaderPair s = compile(vcolor_vert, vcolor_frag);
    attachShaders({ s.vert, s.frag });
    bindAttributeLocation(Magnum::Shaders::GenericGL3D::Position::Location, "position");
    bindAttributeLocation(Magnum::Shaders::GenericGL3D::Color3::Location, "color");
    CORRADE_INTERNAL_ASSERT_OUTPUT(link());
    _transformation = uniformLocation("transformationMatrix");
    _projection = uniformLocation("projectionMatrix");
    _section = uniformLocation("sectionPlane");
}

SectionVertexColorGL& SectionVertexColorGL::set_transformation(const Magnum::Matrix4& m) {
    setUniform(_transformation, m);
    return *this;
}
SectionVertexColorGL& SectionVertexColorGL::set_projection(const Magnum::Matrix4& m) {
    setUniform(_projection, m);
    return *this;
}
SectionVertexColorGL& SectionVertexColorGL::set_section(const Magnum::Vector4& view_plane) {
    setUniform(_section, view_plane);
    return *this;
}

} // namespace smg
