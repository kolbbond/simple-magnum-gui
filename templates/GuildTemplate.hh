
// GuildTemplate.hh

#pragma once

#include <memory>

namespace guild {
typedef std::shared_ptr<class GuildTemplate> ShGuildTemplatePr;

class GuildTemplate {
    
    // properties
    protected:

    // methods
    public:
    // constructor
        GuildTemplate();

        // destructor
        ~GuildTemplate();

  // factory
  static ShGuildTemplatePr create();


};
}

