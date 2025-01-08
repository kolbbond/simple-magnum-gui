// include header
#include "Statics.hh"

namespace smg {

// constructor
Statics::Statics() {}

// destructor
Statics::~Statics() {}

// setters

// getters

// static

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

// convert our arma type to vector3 type
Vector3 Statics::conv2Vector3(arma::Col<fltp> Rn) {
    // check size?
    assert(Rn.n_elem==3);
	return Vector3{static_cast<float>(Rn(0)), static_cast<float>(Rn(1)), static_cast<float>(Rn(2))};
}

// convert Vector3 to armadillo column
arma::Col<fltp> Statics::conv2arma(Vector3 Rn) {

	// allocate
	return arma::Col<fltp>{Rn.x(), Rn.y(), Rn.z()};
}

} // namespace smg
