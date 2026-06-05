// include header
#include "Statics.hh"

namespace smg {

// constructor
Statics::Statics() {}

// destructor
Statics::~Statics() {}

// factory
ShStaticsPr Statics::create() {
	return std::make_shared<Statics>();
}

void Statics::HelpMarker(const char* desc) {
	ImGui::TextDisabled("(?)");
	if(ImGui::BeginItemTooltip()) {
		ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
		ImGui::TextUnformatted(desc);
		ImGui::PopTextWrapPos();
		ImGui::EndTooltip();
	}
}

} // namespace smg
