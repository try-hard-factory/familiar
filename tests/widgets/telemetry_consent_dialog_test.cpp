#include "telemetry/telemetry.h"
#include "widgets/telemetry_consent_dialog.h"

#include <gtest/gtest.h>

#include <QCheckBox>
#include <QLabel>
#include <QPushButton>

TEST(TelemetryConsentDialogTest, InitialChoiceMatchesConfiguredDefault)
{
    TelemetryConsentDialog dialog;
    auto* choice = dialog.findChild<QCheckBox*>();
    ASSERT_NE(choice, nullptr);
    EXPECT_EQ(dialog.accepted_telemetry(),
              familiar::telemetry::default_enabled());
    EXPECT_EQ(choice->isChecked(), dialog.accepted_telemetry());

    bool explainsData = false;
    for (auto* label : dialog.findChildren<QLabel*>()) {
        explainsData |= label->text().contains(QStringLiteral("No file names"));
    }
    EXPECT_TRUE(explainsData);
}

TEST(TelemetryConsentDialogTest, UserCanDeclineOrAcceptBeforeContinuing)
{
    TelemetryConsentDialog dialog;
    auto* choice = dialog.findChild<QCheckBox*>();
    ASSERT_NE(choice, nullptr);
    QPushButton* continueButton = nullptr;
    for (auto* button : dialog.findChildren<QPushButton*>()) {
        if (button->text() == QStringLiteral("Continue")) {
            continueButton = button;
        }
    }
    ASSERT_NE(continueButton, nullptr);

    choice->setChecked(false);
    EXPECT_FALSE(dialog.accepted_telemetry());
    choice->setChecked(true);
    EXPECT_TRUE(dialog.accepted_telemetry());
    continueButton->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST(TelemetryConsentDialogTest, CloseButtonUsesCurrentChoice)
{
    TelemetryConsentDialog dialog;
    auto* choice = dialog.findChild<QCheckBox*>();
    auto* close = dialog.findChild<QPushButton*>(QStringLiteral("tcdCloseBtn"));
    ASSERT_NE(choice, nullptr);
    ASSERT_NE(close, nullptr);
    choice->setChecked(false);
    close->click();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
    EXPECT_FALSE(dialog.accepted_telemetry());
}
