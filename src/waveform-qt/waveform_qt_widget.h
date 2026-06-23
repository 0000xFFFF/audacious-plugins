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

#ifndef __WAVEFORM_QT_WIDGET_H
#define __WAVEFORM_QT_WIDGET_H

#include <QColor>
#include <QElapsedTimer>
#include <QLinearGradient>
#include <QPainter>
#include <QString>
#include <QTimer>
#include <QWidget>

class WaveformQtWidget : public QWidget
{
private:
    static const QColor backgroundColor;
    static const QColor text_color;
    static const QColor line_color;

    QLinearGradient waveform_pattern;
    QLinearGradient background_vumeter_pattern;
    float legend_width;
    float waveform_height;
    float waveform_width;
    float top_padding;
    float bottom_padding;

    void draw_background(QPainter & p);
    void draw_visualizer(QPainter & p);

public:
    WaveformQtWidget(QWidget * parent = nullptr);

    void reset();
    void render_multi_pcm(const float * pcm, int channels);
    void toggle_display_legend();

protected:
    void resizeEvent(QResizeEvent *);
    void paintEvent(QPaintEvent *);
};

#endif
