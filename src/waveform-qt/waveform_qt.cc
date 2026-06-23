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

#include <math.h>

#include <QPainter>
#include <QWidget>

#include <libaudcore/hook.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudqt/libaudqt.h>

class WaveformWidget : public QWidget
{
public:
    WaveformWidget(QWidget * parent = nullptr);
    ~WaveformWidget();

protected:
    void resizeEvent(QResizeEvent *) override;
    void paintEvent(QPaintEvent *) override;

private:
    void paint_background(QPainter &);
    void paint_spectrum(QPainter &);
};

static WaveformWidget * spect_widget = nullptr;

WaveformWidget::WaveformWidget(QWidget * parent) : QWidget(parent) {}

WaveformWidget::~WaveformWidget() { spect_widget = nullptr; }

void WaveformWidget::paint_background(QPainter & p)
{
    auto & base = palette().color(QPalette::Window);
    p.fillRect(0, 0, width(), height(), base);
}

void WaveformWidget::paint_spectrum(QPainter & p)
{
    auto & fg = palette().color(QPalette::WindowText);

    p.setPen(fg);

    int w = width();
    int h = height();

    for (int i = 0; i < w; i++)
    {
        float x = (float)i / w;
        float y = 0.5f * (1.0f + sinf(2 * M_PI * x));
        int y_pixel = h - (int)(y * h);
        p.drawLine(i, h, i, y_pixel);
    }
}

void WaveformWidget::resizeEvent(QResizeEvent * event) { update(); }

void WaveformWidget::paintEvent(QPaintEvent * event)
{
    QPainter p(this);

    paint_background(p);
    paint_spectrum(p);
}

class QtWaveform : public VisPlugin
{
public:
    static constexpr PluginInfo info = {N_("Waveform"), PACKAGE,
                                        nullptr, // about
                                        nullptr, // prefs
                                        PluginQtOnly};

    constexpr QtWaveform() : VisPlugin(info, Visualizer::Freq) {}

    void * get_qt_widget() override;

    void clear() override;
    void render_freq(const float * freq) override;
};

EXPORT QtWaveform aud_plugin_instance;

void QtWaveform::render_freq(const float * freq) {}

void QtWaveform::clear()
{
    if (spect_widget)
        spect_widget->update();
}

void * QtWaveform::get_qt_widget()
{
    if (spect_widget)
        return spect_widget;

    spect_widget = new WaveformWidget();
    return spect_widget;
}
