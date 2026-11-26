/****************************************************************************
 *
 * Daeyoung Ground Control - colour palette and font (UI deck, slide 1)
 *
 * Single source for the brand colours. CustomPlugin::paletteOverride() maps
 * them onto QGC's palette; QML gets them through QGroundControl.corePlugin
 * (kevaColors) for parts the QGC palette has no slot for.
 *
 ****************************************************************************/

#pragma once

#include <QtGui/QColor>

namespace KevaTheme {

// Slide 1 "기본 색상 팔레트"
inline const QColor DeepSkyBlue { 0x00, 0xBF, 0xFF };   // #00BFFF  default text, brand
inline const QColor NeonGreen   { 0x39, 0xFF, 0x14 };   // #39FF14  toolbar text, good state
inline const QColor Yellow      { 0xFF, 0xFF, 0x00 };   // #FFFF00  caution
inline const QColor Orange      { 0xFF, 0xA5, 0x00 };   // #FFA500  warning
inline const QColor Red         { 0xFF, 0x00, 0x00 };   // #FF0000  error / critical
inline const QColor White       { 0xFF, 0xFF, 0xFF };   // #FFFFFF  status data panel text
inline const QColor Brown       { 0x96, 0x4B, 0x00 };   // #964B00  HUD ground

// Surfaces (black UI as in the deck)
inline const QColor Black       { 0x00, 0x00, 0x00 };
inline const QColor Panel       { 0x14, 0x14, 0x14 };   // panels, dialogs
inline const QColor PanelLight  { 0x2A, 0x2A, 0x2A };   // raised elements
inline const QColor PanelDark   { 0x0A, 0x0A, 0x0A };
inline const QColor Control     { 0x1A, 0x1A, 0x1A };   // button face
inline const QColor Border      { 0x1F, 0x4F, 0x60 };   // subtle blue grey borders
inline const QColor Disabled    { 0x4D, 0x66, 0x70 };   // disabled text
inline const QColor Grey        { 0x80, 0x80, 0x80 };

inline constexpr const char *FontFamily = "Arial";

} // namespace KevaTheme
