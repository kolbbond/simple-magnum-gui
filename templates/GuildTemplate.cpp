// GuildTemplate.cpp
// ###### -

// include header
#include "GuildTemplate.hh"

namespace guild {

// constructor
GuildTemplate::GuildTemplate() {}

// destructor
GuildTemplate::~GuildTemplate() {}

// setters

// getters

// static

// factory
ShGuildTemplatePr GuildTemplate::create() {
	return std::make_shared<GuildTemplate>();
}
} // namespace guild
