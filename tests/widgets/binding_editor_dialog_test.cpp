#include "widgets/controls/binding_dialogs.h"

#include "actions/actions.h"
#include "core/settingshandler.h"
#include "widgets/controls/binding_target.h"

#include <gtest/gtest.h>

#include <QPushButton>
#include <QTest>

namespace {

// BindingEditorDialogBase is abstract (onAccepted() pure virtual) and
// populateFrom()/collectBinding() are protected - a subclass is needed
// to reach either at all. tryAccept()'s CONFLICT branches are
// deliberately NOT exercised anywhere in this file - they open a real
// showMessageBox() (blocking, needs a live user to dismiss); its
// no-conflict fast path is covered further below instead. What's tested
// first here is the pure populate/collect round trip every accept path
// (conflicting or not) shares underneath that.
class TestBindingEditorDialog : public BindingEditorDialogBase
{
public:
    explicit TestBindingEditorDialog(BindingTarget* target)
        : BindingEditorDialogBase(target, nullptr)
    {}

    using BindingEditorDialogBase::collectBinding;
    using BindingEditorDialogBase::populateFrom;

protected:
    void onAccepted(const Binding&) override {}
};

// tryAccept() itself IS exercised here, but only its no-conflict fast
// path: collectBinding() on a freshly-constructed dialog (no
// populateFrom() call) yields an all-empty Binding, which short-circuits
// every one of tryAccept()'s conflict checks (each is guarded by
// "!candidate.X.isEmpty()") without ever reaching showMessageBox() -
// straight through to onAccepted()+accept(). The actual conflict
// branches (real modal, needs a live user) stay untested, same reasoning
// as the class comment above.
class RecordingBindingEditorDialog : public BindingEditorDialogBase
{
public:
    explicit RecordingBindingEditorDialog(BindingTarget* target)
        : BindingEditorDialogBase(target, nullptr)
    {}

    using BindingEditorDialogBase::tryAccept;

    bool onAcceptedCalled = false;

protected:
    void onAccepted(const Binding&) override { onAcceptedCalled = true; }
};

// Finds a QDialogButtonBox's button by its (translated) label - both
// AddAliasDialogTest/RebindDialogTest below need this to reach the real
// "Apply"/"Default" buttons a real user would click, exercising the
// concrete dialogs' own constructor wiring end to end (not just
// BindingEditorDialogBase's shared base, covered above).
QPushButton* findButton(const QDialog& dialog, const QString& text)
{
    for (QPushButton* b : dialog.findChildren<QPushButton*>()) {
        if (b->text() == text) {
            return b;
        }
    }
    return nullptr;
}

} // namespace

// AddAliasDialog/RebindDialog both persist through target_->setBindings()
// on accept - deliberately targeting a throwaway ActionBindingTarget
// (own "Actions/<id>"/"Actions/<id>_mouse" storage keys, cleaned up
// below) here rather than one of the shared, real MouseConfig/
// MouseWheelConfig objects KeyboardSettings::mouseActions()/
// mousewheelActions() return (as BindingEditorDialogBaseTest above does,
// safely, only because IT never calls tryAccept()/setBindings()) -
// clicking Apply here for real would otherwise permanently rewrite one
// of the app's actual built-in canvas controls for the rest of this test
// binary's run.

TEST(AddAliasDialogTest, ClickingApplyWithNoConflictAppendsBindingToTarget)
{
    Action action = Action::make(QStringLiteral("test_add_alias_dialog"),
                                 QStringLiteral("Test"));
    SettingsHandler::getInstance()->removeJsonValue(QStringLiteral("Actions"),
                                                     action.id);
    SettingsHandler::getInstance()->removeJsonValue(
        QStringLiteral("Actions"), action.id + QStringLiteral("_mouse"));
    ActionBindingTarget target(&action);
    ASSERT_TRUE(target.bindings().isEmpty());

    AddAliasDialog dialog(&target, nullptr);
    dialog.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&dialog));

    auto* keyField = dialog.findChild<KeySequenceCaptureField*>();
    ASSERT_NE(keyField, nullptr);
    // An exotic combo no real Action/Control ships with by default -
    // guaranteed not to trip tryAccept()'s conflict detection (which
    // would otherwise open a real, blocking showMessageBox()).
    keyField->setSequence(QStringLiteral("Ctrl+Alt+Shift+F24"));

    QPushButton* applyBtn = findButton(dialog, QObject::tr("Apply"));
    ASSERT_NE(applyBtn, nullptr);
    QTest::mouseClick(applyBtn, Qt::LeftButton);

    const QList<Binding> bindings = target.bindings();
    ASSERT_EQ(bindings.size(), 1);
    EXPECT_EQ(bindings.first().keySequence,
             QStringLiteral("Ctrl+Alt+Shift+F24"));
    EXPECT_EQ(dialog.result(), int(QDialog::Accepted));

    SettingsHandler::getInstance()->removeJsonValue(QStringLiteral("Actions"),
                                                     action.id);
    SettingsHandler::getInstance()->removeJsonValue(
        QStringLiteral("Actions"), action.id + QStringLiteral("_mouse"));
}

TEST(RebindDialogTest, ClickingApplyReplacesBindingAtIndex)
{
    // Ctrl+Alt+Shift+F20 - an exotic default, deliberately NOT a real
    // shortcut ("Ctrl+Q" is Quit's, for instance - see actions.cpp's
    // buildRegistry()) - tryAccept()'s conflict detection runs against
    // the real, process-wide getActions() registry even for a throwaway
    // ActionBindingTarget like this one, and a genuine conflict would
    // open a real, blocking showMessageBox() here.
    Action action = Action::make(QStringLiteral("test_rebind_dialog"),
                                 QStringLiteral("Test"),
                                 {},
                                 {QStringLiteral("Ctrl+Alt+Shift+F20")});
    SettingsHandler::getInstance()->removeJsonValue(QStringLiteral("Actions"),
                                                     action.id);
    ActionBindingTarget target(&action);
    ASSERT_EQ(target.bindings().size(), 1); // the default, not yet overridden

    RebindDialog dialog(&target, /*bindingIndex=*/0, nullptr);
    dialog.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&dialog));

    auto* keyField = dialog.findChild<KeySequenceCaptureField*>();
    ASSERT_NE(keyField, nullptr);
    EXPECT_EQ(keyField->sequence(),
             QStringLiteral("Ctrl+Alt+Shift+F20")); // pre-filled

    keyField->setSequence(QStringLiteral("Ctrl+Alt+Shift+F23"));
    QPushButton* applyBtn = findButton(dialog, QObject::tr("Apply"));
    ASSERT_NE(applyBtn, nullptr);
    QTest::mouseClick(applyBtn, Qt::LeftButton);

    const QList<Binding> bindings = target.bindings();
    ASSERT_EQ(bindings.size(), 1); // replaced in place, not appended
    EXPECT_EQ(bindings.first().keySequence,
             QStringLiteral("Ctrl+Alt+Shift+F23"));

    SettingsHandler::getInstance()->removeJsonValue(QStringLiteral("Actions"),
                                                     action.id);
}

TEST(RebindDialogTest, ClickingDefaultRestoresDefaultBindingAndApplies)
{
    Action action = Action::make(QStringLiteral("test_rebind_default"),
                                 QStringLiteral("Test"),
                                 {},
                                 {QStringLiteral("Ctrl+Alt+Shift+F20")});
    SettingsHandler::getInstance()->removeJsonValue(QStringLiteral("Actions"),
                                                     action.id);
    ActionBindingTarget target(&action);
    // Diverge from the default first, same as RebindDialogTest above.
    target.setBindings({Binding{QStringLiteral("Ctrl+Alt+Shift+F22")}});

    RebindDialog dialog(&target, /*bindingIndex=*/0, nullptr);
    dialog.show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(&dialog));

    // "Default" only exists because defaultBindings() has an entry at
    // bindingIndex_ (RebindDialog's own constructor) - true here since
    // action's compile-time shortcuts default is {"Ctrl+Alt+Shift+F20"}.
    QPushButton* defaultBtn = findButton(dialog, QObject::tr("Default"));
    ASSERT_NE(defaultBtn, nullptr);
    QTest::mouseClick(defaultBtn, Qt::LeftButton);

    const QList<Binding> bindings = target.bindings();
    ASSERT_EQ(bindings.size(), 1);
    EXPECT_EQ(bindings.first().keySequence,
             QStringLiteral("Ctrl+Alt+Shift+F20"));

    SettingsHandler::getInstance()->removeJsonValue(QStringLiteral("Actions"),
                                                     action.id);
}

TEST(BindingEditorDialogBaseTest,
    TryAcceptWithNoConflictAcceptsWithoutShowingAMessageBox)
{
    const MouseConfig& panCfg = KeyboardSettings::mouseActions()[1];
    MouseConfigBindingTarget target(&panCfg, BindingTargetKind::MouseControl);
    RecordingBindingEditorDialog dialog(&target);

    dialog.tryAccept();

    EXPECT_TRUE(dialog.onAcceptedCalled);
    EXPECT_EQ(dialog.result(), int(QDialog::Accepted));
}

TEST(BindingEditorDialogBaseTest,
    PopulateThenCollectRoundTripsKeyboardOnlyBinding)
{
    // "pan" (MouseConfig, not invertible) - see
    // KeyboardSettings::mouseActions() (core/controls.cpp).
    const MouseConfig& panCfg = KeyboardSettings::mouseActions()[1];
    MouseConfigBindingTarget target(&panCfg, BindingTargetKind::MouseControl);
    TestBindingEditorDialog dialog(&target);

    Binding original;
    original.keySequence = QStringLiteral("Ctrl+P");
    dialog.populateFrom(original);

    const Binding collected = dialog.collectBinding();
    EXPECT_EQ(collected.keySequence, QStringLiteral("Ctrl+P"));
    EXPECT_TRUE(collected.mouseButton.isEmpty());
}

TEST(BindingEditorDialogBaseTest,
    PopulateThenCollectRoundTripsMouseButtonBinding)
{
    const MouseConfig& panCfg = KeyboardSettings::mouseActions()[1];
    MouseConfigBindingTarget target(&panCfg, BindingTargetKind::MouseControl);
    TestBindingEditorDialog dialog(&target);

    Binding original;
    original.mouseButton = QStringLiteral("Middle");
    dialog.populateFrom(original);

    EXPECT_EQ(dialog.collectBinding().mouseButton, QStringLiteral("Middle"));
}

TEST(BindingEditorDialogBaseTest, WheelControlUsesModifierCheckboxesNotButton)
{
    // "pan_horizontal" (MouseWheelConfig, invertible=true) - see
    // KeyboardSettings::mousewheelActions().
    const MouseWheelConfig& panHCfg
        = KeyboardSettings::mousewheelActions()[0];
    MouseConfigBindingTarget target(&panHCfg,
                                    BindingTargetKind::MouseWheelControl);
    TestBindingEditorDialog dialog(&target);

    Binding original;
    original.mouseModifiers = {QStringLiteral("Shift")};
    original.inverted = true;
    dialog.populateFrom(original);

    const Binding collected = dialog.collectBinding();
    EXPECT_EQ(collected.mouseModifiers, QStringList{QStringLiteral("Shift")});
    // Wheel targets never populate the button field - see
    // BindingEditorDialogBase's own constructor comment (no button to
    // combine modifiers with for a wheel scroll).
    EXPECT_TRUE(collected.mouseButton.isEmpty());
    // invertCheck_ only exists because pan_horizontal isInvertible().
    EXPECT_TRUE(collected.inverted);
}
