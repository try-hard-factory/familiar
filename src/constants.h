#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QColor>

// ─── App metadata ─────────────────────────────────────────────────────────────

namespace AppConstants {
constexpr const char* APPNAME = "Familiar";
constexpr const char* APPNAME_FULL = "Familiar Reference Image Viewer";
constexpr const char* VERSION = "0.1.0-dev";
constexpr const char* WEBSITE = "???";
constexpr const char* COPYRIGHT = "Copyright © 2024 max";

constexpr const char* CHANGED_SYMBOL = "✎";
}  // namespace AppConstants

// ─── Colors ───────────────────────────────────────────────────────────────────

namespace Colors {

namespace Active {
inline const QColor Base{60, 60, 60};
inline const QColor AlternateBase{70, 70, 70};
inline const QColor Window{40, 40, 40};
inline const QColor Button{40, 40, 40};
inline const QColor Text{200, 200, 200};
inline const QColor HighlightedText{255, 255, 255};
inline const QColor WindowText{200, 200, 200};
inline const QColor ButtonText{200, 200, 200};
inline const QColor Highlight{83, 167, 165};
inline const QColor Link{90, 181, 179};
}  // namespace Active

namespace Disabled {
inline const QColor Base{40, 40, 40};
inline const QColor Window{40, 40, 40, 50};
inline const QColor WindowText{120, 120, 120};
inline const QColor Light{0, 0, 0, 0};
inline const QColor Text{140, 140, 140};
}  // namespace Disabled

namespace Scene {
inline const QColor Selection{116, 234, 231};
inline const QColor Canvas{60, 60, 60};
inline const QColor Text{200, 200, 200};
}  // namespace Scene

}  // namespace Colors

#endif  // CONSTANTS_H
