#include "widgets/main_controls.h"

#include <QGraphicsView>
#include <QWidget>

#include "canvasview.h"
#include "widgets/welcome_overlay.h"

// Explicit instantiation of every specialization this app actually uses.
// Not redundant with the implicit instantiations at the two use sites:
// [temp.inst]/11 leaves it UNSPECIFIED whether a compiler instantiates a
// class template's virtual member functions when nothing would otherwise
// require them, and MainControlsMixin's dragEnterEvent()/dragMoveEvent()/
// dropEvent() are exactly that case - both derived classes override
// dropEvent(), and the other two are never called by name anywhere, so
// their bodies are reachable only through the vtable. That means one
// compiler type-checks them and another silently doesn't; this project
// has already been bitten by that class of divergence (a -Werror=
// return-type error in selector.h that only GCC in CI ever reported).
// An explicit instantiation definition forces every member, virtual
// included, to be instantiated and checked here, on every compiler.
template class MainControlsMixin<CanvasView, QGraphicsView>;
template class MainControlsMixin<WelcomeOverlay, QWidget>;
