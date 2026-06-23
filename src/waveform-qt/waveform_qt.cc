/*
 * Copyright (c) 2026 0000xFFFF.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "waveform_qt.h"
#include "waveform_qt_widget.h"

#include <QPointer>
#include <libaudcore/interface.h>
#include <libaudcore/runtime.h>

EXPORT WaveformQt aud_plugin_instance;

const char WaveformQt::about[] = N_("Waveform Plugin for Audacious\n"
                                    "Copyright 2026 0000xFFFF");

const PreferencesWidget WaveformQt::widgets[] = {
    WidgetLabel(N_("<b>Waveform Settings</b>")),
    WidgetSpin(N_("Peak hold time:"), WidgetFloat("waveform", "peak_hold_time"),
               {0.1, 30, 0.1, N_("seconds")}),
    WidgetSpin(N_("Fall-off time:"), WidgetFloat("waveform", "falloff"),
               {0.1, 96, 0.1, N_("dB/second")}),
    WidgetCheck(N_("Display legend"), WidgetBool("waveform", "display_legend",
                                                 toggle_display_legend)),
};

const PluginPreferences WaveformQt::prefs = {{widgets}};

const char * const WaveformQt::prefs_defaults[] = {nullptr};

static QPointer<WaveformQtWidget> spect_widget;

bool WaveformQt::init()
{
    aud_config_set_defaults("waveform", prefs_defaults);
    return true;
}

void WaveformQt::render_multi_pcm(const float * pcm, int channels)
{
    if (spect_widget)
    {
        spect_widget->render_multi_pcm(pcm, channels);
    }
}

void WaveformQt::clear()
{
    if (spect_widget)
    {
        spect_widget->reset();
        spect_widget->update();
    }
}

void * WaveformQt::get_qt_widget()
{
    if (spect_widget)
    {
        return spect_widget;
    }

    spect_widget = new WaveformQtWidget();
    return spect_widget;
}

void WaveformQt::toggle_display_legend()
{
    if (spect_widget)
    {
        spect_widget->toggle_display_legend();
    }
}
