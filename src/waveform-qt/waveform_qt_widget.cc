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

#include "waveform_qt_widget.h"

#include <libaudcore/runtime.h>
#include <math.h>

const QColor WaveformQtWidget::backgroundColor = QColor(16, 16, 16, 255);
const QColor WaveformQtWidget::text_color = QColor(255, 255, 255);
const QColor WaveformQtWidget::line_color = QColor(120, 120, 120);

void WaveformQtWidget::draw_background(QPainter & p)
{
    p.fillRect(0, 0, width(), height(), backgroundColor);
}

void WaveformQtWidget::resizeEvent(QResizeEvent *) {}

void WaveformQtWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);

    draw_background(p);
    draw_visualizer(p);
}

void WaveformQtWidget::toggle_display_legend() { update(); }
