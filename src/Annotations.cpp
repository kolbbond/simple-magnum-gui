#include "Annotations.hh"

#include <cmath>

#include <Magnum/ImGuiIntegration/Integration.h>
#include <Magnum/ImGuiIntegration/Widgets.h>

#include <imgui.h>

namespace smg {

namespace {
ImU32 to_col(const Magnum::Color4& c, float alpha_scale) {
    return ImGui::ColorConvertFloat4ToU32(ImVec4{ c.r(), c.g(), c.b(), c.a() * alpha_scale });
}
ImVec2 iv(const Magnum::Vector2& v) { return ImVec2{ v.x(), v.y() }; }
} // namespace

void AnnotationLayer::draw(const char* title, Magnum::GL::Texture2D& background, const Magnum::Vector2i& size) {
    ImGui::Begin(title);
    Magnum::ImGuiIntegration::image(background, Magnum::Vector2{ size });
    const ImVec2 rmin = ImGui::GetItemRectMin();
    const ImVec2 rsz = ImGui::GetItemRectSize();
    const Magnum::Vector2 image_min{ rmin.x, rmin.y };
    const Magnum::Vector2 image_size{ rsz.x, rsz.y };
    const float t = _timeline.time();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    for(const Annotation& a : _annotations) {
        const float p = annotation_progress(a, t);
        if(p <= 0.0f) continue;
        const float alpha = a.fade_in ? p : 1.0f;
        const ImU32 col = to_col(a.color, alpha);

        if(a.kind == AnnotationKind::Stroke && a.points.size() >= 2) {
            // reveal the first p-fraction of the polyline (by segment count)
            const std::size_t segs = a.points.size() - 1;
            const float fseg = p * float(segs);
            const std::size_t full = std::size_t(fseg);
            std::vector<ImVec2> pts;
            for(std::size_t i = 0; i <= full && i < a.points.size(); ++i) pts.push_back(iv(to_screen(a.points[i], image_min, image_size)));
            if(full < segs) { // partial last segment
                const float frac = fseg - float(full);
                const Magnum::Vector2 mid = a.points[full] + (a.points[full + 1] - a.points[full]) * frac;
                pts.push_back(iv(to_screen(mid, image_min, image_size)));
            }
            if(pts.size() >= 2) dl->AddPolyline(pts.data(), int(pts.size()), col, ImDrawFlags_None, a.thickness);
        } else if((a.kind == AnnotationKind::Line || a.kind == AnnotationKind::Arrow) && a.points.size() >= 2) {
            const Magnum::Vector2 s = a.points[0];
            const Magnum::Vector2 tip = s + (a.points[1] - s) * p; // grows in
            const ImVec2 ps = iv(to_screen(s, image_min, image_size));
            const ImVec2 pt = iv(to_screen(tip, image_min, image_size));
            dl->AddLine(ps, pt, col, a.thickness);
            if(a.kind == AnnotationKind::Arrow) {
                const float dx = pt.x - ps.x, dy = pt.y - ps.y;
                const float len = std::sqrt(dx * dx + dy * dy);
                if(len > 1.0f) {
                    const float ux = dx / len, uy = dy / len;
                    const float h = 12.0f; // arrowhead size px
                    const ImVec2 b1{ pt.x - ux * h - uy * h * 0.5f, pt.y - uy * h + ux * h * 0.5f };
                    const ImVec2 b2{ pt.x - ux * h + uy * h * 0.5f, pt.y - uy * h - ux * h * 0.5f };
                    dl->AddTriangleFilled(pt, b1, b2, col);
                }
            }
        } else if(a.kind == AnnotationKind::Circle && a.points.size() >= 2) {
            const Magnum::Vector2 c = to_screen(a.points[0], image_min, image_size);
            const Magnum::Vector2 rim = to_screen(a.points[1], image_min, image_size);
            const float r = (rim - c).length() * p; // grows in
            dl->AddCircle(iv(c), r, col, 0, a.thickness);
        } else if(a.kind == AnnotationKind::Text && !a.points.empty() && !a.text.empty()) {
            dl->AddText(iv(to_screen(a.points[0], image_min, image_size)), col, a.text.c_str());
        }
    }

    ImGui::End();
}

} // namespace smg
