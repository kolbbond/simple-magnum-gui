// gui smoke test: render one annotated frame over a generated texture, then exit
#include "Annotations.hh"
#include "GuiBase.hh"
#include "SpriteSheet.hh" // reuse from_pixels to make a background texture

#include <array>
#include <cstdio>
#include <cstdlib>

using namespace smg;

static AnnotationLayer* g_layer = nullptr;
static Magnum::GL::Texture2D* g_bg = nullptr;

int anno_cb(void* /*data*/) {
    g_layer->timeline().advance(0.016f); // ~60fps step
    g_layer->draw("Telestration", *g_bg, Magnum::Vector2i{ 320, 240 });
    std::printf("OK annotation frame\n");
    std::exit(0);
    return 0;
}

class AnnoTest: public GuiBase {
public:
    explicit AnnoTest(const Arguments& arguments) : GuiBase(arguments) {
        std::array<unsigned char, 4> px{ 40, 60, 40, 255 };
        _sheet = SpriteSheet::from_pixels(Corrade::Containers::arrayView(reinterpret_cast<const char*>(px.data()), px.size()),
            Magnum::Vector2i{ 1, 1 },
            Magnum::PixelFormat::RGBA8Unorm,
            1,
            1);
        g_bg = &_sheet->texture();

        _layer.timeline().set_duration(3.0f);
        _layer.timeline().play();

        Annotation stroke;
        stroke.kind = AnnotationKind::Stroke;
        stroke.points = { { 0.1f, 0.1f }, { 0.4f, 0.5f }, { 0.8f, 0.2f } };
        stroke.color = Magnum::Color4{ 1.0f, 0.9f, 0.2f, 1.0f };
        stroke.draw_on = 1.0f;
        _layer.add(stroke);

        Annotation arrow;
        arrow.kind = AnnotationKind::Arrow;
        arrow.points = { { 0.2f, 0.8f }, { 0.7f, 0.6f } };
        arrow.color = Magnum::Color4{ 0.3f, 0.7f, 1.0f, 1.0f };
        arrow.appear_time = 0.5f;
        arrow.draw_on = 0.5f;
        _layer.add(arrow);

        Annotation label;
        label.kind = AnnotationKind::Text;
        label.points = { { 0.3f, 0.05f } };
        label.text = "play";
        label.color = Magnum::Color4{ 1.0f };
        _layer.add(label);

        g_layer = &_layer;
        _cb = DrawCallback::create();
        _cb->set_callback(anno_cb);
        add_callback(_cb);
    }

private:
    ShSpriteSheetPr _sheet;
    AnnotationLayer _layer;
    ShDrawCallbackPr _cb;
};

MAGNUM_APPLICATION_MAIN(AnnoTest)
