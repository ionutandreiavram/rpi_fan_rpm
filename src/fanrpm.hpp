/*============================================================================
All rights reserved.

Copyright (c) 2025-2026  RAlexandru Trifan,  Ionut Andrei Avram

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
============================================================================*/

#ifndef WIDGETS_FANRPM_HPP
#define WIDGETS_FANRPM_HPP

#include <widget.hpp>
#include <gtkmm/button.h>
#include <gtkmm/gesturelongpress.h>

extern "C" {
#include "lxutils.h"
#include "fanrpm.h"
}

class WayfireFANRpm : public WayfireWidget
{
    std::unique_ptr <Gtk::Button> plugin;
    Glib::RefPtr<Gtk::GestureLongPress> gesture;

    WfOption <int> icon_size {"panel/icon_size"};
    WfOption <std::string> bar_pos {"panel/position"};
    sigc::connection icon_timer;

    WfOption <std::string> foreground_colour {"panel/fanrpm_foreground"};
    WfOption <std::string> background_colour {"panel/fanrpm_background"};
    WfOption <int> low_rpm {"panel/fanrpm_low_rpm"};
    WfOption <int> high_rpm {"panel/fanrpm_high_rpm"};

    /* plugin */
    FANRpmPlugin *cput;

  public:

    void init (Gtk::HBox *container) override;
    virtual ~WayfireFANRpm ();
    void icon_size_changed_cb (void);
    void bar_pos_changed_cb (void);
    bool set_icon (void);
    void settings_changed_cb (void);
};

#endif /* end of include guard: WIDGETS_CPUTEMP_HPP */

/* End of file */
/*----------------------------------------------------------------------------*/
