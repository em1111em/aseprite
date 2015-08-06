// Aseprite
// Copyright (C) 2001-2015  David Capello
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 2 as
// published by the Free Software Foundation.

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "app/ui/color_wheel.h"

#include "app/color_utils.h"
#include "app/pref/preferences.h"
#include "app/ui/skin/button_icon_impl.h"
#include "app/ui/skin/skin_theme.h"
#include "app/ui/status_bar.h"
#include "base/bind.h"
#include "base/pi.h"
#include "ui/graphics.h"
#include "ui/menu.h"
#include "ui/message.h"
#include "ui/paint_event.h"
#include "ui/preferred_size_event.h"
#include "ui/resize_event.h"
#include "ui/system.h"

namespace app {

using namespace app::skin;
using namespace gfx;
using namespace ui;

ColorWheel::ColorWheel()
  : Widget(kGenericWidget)
  , m_discrete(Preferences::instance().colorBar.discreteWheel())
  , m_options("", kButtonWidget, kButtonWidget, kCheckWidget)
{
  SkinTheme* theme = SkinTheme::instance();

  setBorder(gfx::Border(3*ui::guiscale()));

  m_options.Click.connect(Bind<void>(&ColorWheel::onOptions, this));
  m_options.setBgColor(theme->colors.editorFace());
  m_options.setIconInterface(
    new ButtonIconImpl(theme->parts.palOptions(),
                       theme->parts.palOptions(),
                       theme->parts.palOptions(),
                       CENTER | MIDDLE));

  addChild(&m_options);
}

ColorWheel::~ColorWheel()
{
}

app::Color ColorWheel::pickColor(const gfx::Point& pos) const
{
  int u = (pos.x - (m_wheelBounds.x+m_wheelBounds.w/2));
  int v = (pos.y - (m_wheelBounds.y+m_wheelBounds.h/2));
  double d = std::sqrt(u*u + v*v);

  if (d < m_wheelRadius) {
    double a = std::atan2(-v, u);

    int hue = (int(180.0 * a / PI)
               + 180            // To avoid [-180,0) range
               + 180 + 30       // To locate green at 12 o'clock
               );
    if (m_discrete) {
      hue += 15;
      hue /= 30;
      hue *= 30;
    }
    hue %= 360;                 // To leave hue in [0,360) range

    int sat = int(120.0 * d / m_wheelRadius);
    if (m_discrete) {
      sat /= 20;
      sat *= 20;
    }

    return app::Color::fromHsv(
      MID(0, hue, 360),
      MID(0, sat, 100),
      100);
  }
  else {
    return app::Color::fromMask();
  }
}

void ColorWheel::setDiscrete(bool state)
{
  m_discrete = state;
  Preferences::instance().colorBar.discreteWheel(m_discrete);

  invalidate();
}

void ColorWheel::onPreferredSize(PreferredSizeEvent& ev)
{
  ev.setPreferredSize(gfx::Size(32*ui::guiscale(), 32*ui::guiscale()));
}

void ColorWheel::onResize(ui::ResizeEvent& ev)
{
  Widget::onResize(ev);

  gfx::Rect rc = getClientChildrenBounds();
  int r = MIN(rc.w/2, rc.h/2);

  m_clientBounds = rc;
  m_wheelRadius = r;
  m_wheelBounds = gfx::Rect(rc.x+rc.w/2-r,
                            rc.y+rc.h/2-r,
                            r*2, r*2);

  gfx::Size prefSize = m_options.getPreferredSize();
  rc = getChildrenBounds();
  rc.x += rc.w-prefSize.w;
  rc.w = prefSize.w;
  rc.h = prefSize.h;
  m_options.setBounds(rc);
}

void ColorWheel::onPaint(ui::PaintEvent& ev)
{
  ui::Graphics* g = ev.getGraphics();
  SkinTheme* theme = static_cast<SkinTheme*>(getTheme());

  theme->drawRect(g, getClientBounds(),
                  theme->parts.editorNormal().get(),
                  getBgColor());

  const gfx::Rect& rc = m_clientBounds;

  for (int y=rc.y; y<rc.y+rc.h; ++y) {
    for (int x=rc.x; x<rc.x+rc.w; ++x) {
      app::Color appColor =
        ColorWheel::pickColor(gfx::Point(x, y));

      gfx::Color color;
      if (appColor.getType() != app::Color::MaskType) {
        color = color_utils::color_for_ui(appColor);
      }
      else {
        color = theme->colors.editorFace();
      }

      g->putPixel(color, x, y);
    }
  }
}

bool ColorWheel::onProcessMessage(ui::Message* msg)
{
  switch (msg->type()) {

    case kMouseDownMessage:
      captureMouse();
      // Continue...

    case kMouseMoveMessage: {
      MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);

      app::Color color = pickColor(
        mouseMsg->position()
        - getBounds().getOrigin());

      if (color != app::Color::fromMask()) {
        StatusBar::instance()->showColor(0, "", color);
        if (hasCapture())
          ColorChange(color, mouseMsg->buttons());
      }
      break;
    }

    case kMouseUpMessage:
      if (hasCapture()) {
        releaseMouse();
      }
      return true;

    case kSetCursorMessage: {
      MouseMessage* mouseMsg = static_cast<MouseMessage*>(msg);
      app::Color color = pickColor(
        mouseMsg->position()
        - getBounds().getOrigin());

      if (color.getType() != app::Color::MaskType) {
        ui::set_mouse_cursor(kEyedropperCursor);
        return true;
      }
      break;
    }

  }

  return Widget::onProcessMessage(msg);
}

void ColorWheel::onOptions()
{
  Menu menu;
  MenuItem discrete("Discrete");
  menu.addChild(&discrete);

  if (isDiscrete())
    discrete.setSelected(true);

  discrete.Click.connect(
    [this]() {
      setDiscrete(!isDiscrete());
    });

  gfx::Rect rc = m_options.getBounds();
  menu.showPopup(gfx::Point(rc.x+rc.w, rc.y));
}

} // namespace app
