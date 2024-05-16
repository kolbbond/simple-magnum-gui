// RealtimePlot.cpp
// ###### -

// include header
#include "RealtimePlot.hh"

namespace guild {

// constructor
RealtimePlot::RealtimePlot() {}

// destructor
RealtimePlot::~RealtimePlot() {}

// setters

// getters

// static

// factory
ShRealtimePlotPr RealtimePlot::create() {
	return std::make_shared<RealtimePlot>();
}

int RealtimePlot::callback(void* data) {
	// callback for realtime plotting

    // unused data here
    (void)data;

    // from ImPlot example 
    // does not require any class data
    ImGui::Begin("RealtimePlot");

	ImGui::BulletText("Move your mouse to change the data!");
	ImGui::BulletText("This example assumes 60 FPS. Higher FPS requires larger buffer size.");

	static ScrollingBuffer sdata1, sdata2;
	static RollingBuffer rdata1, rdata2;

	ImVec2 mouse = ImGui::GetMousePos();
	static float t = 0;

	t += ImGui::GetIO().DeltaTime;
	sdata1.AddPoint(t, mouse.x * 0.0005f);
	rdata1.AddPoint(t, mouse.x * 0.0005f);
	sdata2.AddPoint(t, mouse.y * 0.0005f);
	rdata2.AddPoint(t, mouse.y * 0.0005f);

	static float history = 10.0f;
	ImGui::SliderFloat("History", &history, 1, 30, "%.1f s");
	rdata1.Span = history;
	rdata2.Span = history;

	static ImPlotAxisFlags flags = ImPlotAxisFlags_NoTickLabels;

	if(ImPlot::BeginPlot("##Scrolling", ImVec2(-1, 150))) {
		ImPlot::SetupAxes(nullptr, nullptr, flags, flags);
		ImPlot::SetupAxisLimits(ImAxis_X1, t - history, t, ImGuiCond_Always);
		ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1);
		ImPlot::SetNextFillStyle(IMPLOT_AUTO_COL, 0.5f);
		ImPlot::PlotShaded("Mouse X",
						   &sdata1.Data[0].x,
						   &sdata1.Data[0].y,
						   sdata1.Data.size(),
						   -INFINITY,
						   0,
						   sdata1.Offset,
						   2 * sizeof(float));
		ImPlot::PlotLine("Mouse Y",
						 &sdata2.Data[0].x,
						 &sdata2.Data[0].y,
						 sdata2.Data.size(),
						 0,
						 sdata2.Offset,
						 2 * sizeof(float));
		ImPlot::EndPlot();
	}
	if(ImPlot::BeginPlot("##Rolling", ImVec2(-1, 150))) {
		ImPlot::SetupAxes(nullptr, nullptr, flags, flags);
		ImPlot::SetupAxisLimits(ImAxis_X1, 0, history, ImGuiCond_Always);
		ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 1);
		ImPlot::PlotLine("Mouse X",
						 &rdata1.Data[0].x,
						 &rdata1.Data[0].y,
						 rdata1.Data.size(),
						 0,
						 0,
						 2 * sizeof(float));
		ImPlot::PlotLine("Mouse Y",
						 &rdata2.Data[0].x,
						 &rdata2.Data[0].y,
						 rdata2.Data.size(),
						 0,
						 0,
						 2 * sizeof(float));
		ImPlot::EndPlot();
	}

    ImGui::End();

	// return success !
	return 0;
}

} // namespace guild
