#include "telemetry/telemetry.h"

#include "core/settings.h"

#include <gtest/gtest.h>

#include <QByteArray>
#include <QDate>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QLatin1String>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <Qt>
#include <QtEnvironmentVariables>

// Everything here is reached without a network stack: report_launch() is
// the only function that opens a socket, and it is not called below.
// suppression_reason() exists precisely so the decision to send can be
// tested apart from the sending (see telemetry/telemetry.h).
//
// suppression_reason() checks its conditions in a fixed order and returns
// at the first one that holds, so each test has to put the state past
// every earlier condition before the one it is actually asserting can be
// observed. grant_consent() below does that for the later ones.
//
// Note that QStandardPaths::isTestModeEnabled() is true for this whole
// binary (main.cpp turns it on for "-t"), which is why the test-mode
// check sits LAST in suppression_reason() - were it first, every
// assertion in this file would see only "test mode". Nothing here can
// therefore observe an empty reason; TestModeSuppressesEvenWhenEverything
// ElseAllows asserts exactly that.

namespace {

constexpr char kenabledKey[] = "Telemetry/enabled";
constexpr char kconsentAskedKey[] = "Telemetry/consent_asked";
constexpr char klastSentKey[] = "Telemetry/last_sent_date";

// Storage is shared by every TEST() in this binary and the keys are
// global singleton state, so each test starts and ends from the same
// known baseline rather than assuming a clean slate - same convention as
// tests/core/famsettings_test.cpp. remove() rather than set_value()
// because the defaults are what the real first launch sees.
void reset_telemetry_state()
{
    FamSettings::remove(QLatin1String(kenabledKey));
    FamSettings::remove(QLatin1String(kconsentAskedKey));
    FamSettings::remove(QLatin1String(klastSentKey));
    qunsetenv("DO_NOT_TRACK");
    qunsetenv("FAMILIAR_TELEMETRY_URL");
    // Resets no_telemetry(): CommandlineArgs::parse() assigns that flag
    // unconditionally, unlike --settings and --telemetry-url which it
    // only overwrites when actually present (core/settings.cpp). That
    // asymmetry is why the flag is restorable here and the URL is not,
    // and why the endpoint tests below go through the environment
    // variable instead of --telemetry-url: a telemetryUrl_ set once has
    // no way back to empty, and would leak into every later test.
    CommandlineArgs::instance().parse({QStringLiteral("familiar")});
}

// Puts the state past the consent and endpoint checks so that a later
// condition is the one suppression_reason() reports.
void grant_consent(const char* url)
{
    FamSettings::set_value(QLatin1String(kconsentAskedKey), true);
    FamSettings::set_value(QLatin1String(kenabledKey), true);
    qputenv("FAMILIAR_TELEMETRY_URL", QByteArray(url));
}

class TelemetryTest : public ::testing::Test
{
protected:
    void SetUp() override { reset_telemetry_state(); }
    void TearDown() override { reset_telemetry_state(); }
};

// The id file is the one thing here that lives outside the throwaway
// settings file, at a fixed AppConfigLocation path that deliberately
// ignores --settings. Under test mode Qt rewrites that location into a
// "qttest" sandbox; this refuses to run at all if that is ever not the
// case, because the alternative is a test suite overwriting the install
// id of whoever is running it.
class TelemetryInstallIdTest : public TelemetryTest
{
protected:
    void SetUp() override
    {
        TelemetryTest::SetUp();
        ASSERT_TRUE(QStandardPaths::isTestModeEnabled());
        path_ = familiar::telemetry::install_id_path();
        ASSERT_TRUE(path_.contains(QLatin1String("qttest")))
            << "refusing to touch " << path_.toStdString()
            << ": not inside Qt's test-mode sandbox";
        QFile::remove(path_);
    }

    void TearDown() override
    {
        QFile::remove(path_);
        TelemetryTest::TearDown();
    }

    QString read_file() const
    {
        QFile file(path_);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return {};
        }
        return QString::fromUtf8(file.readAll()).trimmed();
    }

    void write_file(const QByteArray& contents) const
    {
        QFile file(path_);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        ASSERT_EQ(file.write(contents), contents.size());
    }

    QString path_;
};

} // namespace

// ─── payload ──────────────────────────────────────────────────────────

// The dialog's wording is "That is all", and this is what holds that
// claim to the body: anything added to payload_for() later fails here
// instead of silently shipping.
TEST_F(TelemetryTest, PayloadCarriesExactlyTheThreeDocumentedFields)
{
    const QByteArray body
        = familiar::telemetry::payload_for(QStringLiteral("abc"),
                                           QStringLiteral("1.2.3"));

    QJsonParseError error{};
    const QJsonDocument document = QJsonDocument::fromJson(body, &error);
    ASSERT_EQ(error.error, QJsonParseError::NoError);
    ASSERT_TRUE(document.isObject());

    const QJsonObject object = document.object();
    EXPECT_EQ(object.keys(),
              (QStringList{QStringLiteral("install_id"),
                           QStringLiteral("schema"),
                           QStringLiteral("version")}));
    EXPECT_EQ(object.value(QStringLiteral("install_id")).toString(),
              QStringLiteral("abc"));
    EXPECT_EQ(object.value(QStringLiteral("version")).toString(),
              QStringLiteral("1.2.3"));
    // An integer, not a string: the server rejects {"schema":"1"} with a
    // 400 (familiar-web, backend/src/handlers.cpp), so a change of type
    // here would make every report fail while still looking well-formed.
    EXPECT_TRUE(object.value(QStringLiteral("schema")).isDouble());
    EXPECT_EQ(object.value(QStringLiteral("schema")).toInt(), 1);
}

TEST_F(TelemetryTest, PayloadIsCompactWithNoTrailingNewline)
{
    const QByteArray body
        = familiar::telemetry::payload_for(QStringLiteral("abc"),
                                           QStringLiteral("1.2.3"));
    EXPECT_FALSE(body.contains('\n'));
}

// ─── suppression reasons, in the order they are checked ───────────────

TEST_F(TelemetryTest, NotCompiledInSuppressesEverything)
{
    if (familiar::telemetry::is_compiled_in()) {
        GTEST_SKIP() << "built with -DFAMILIAR_TELEMETRY=ON";
    }
    // The -DFAMILIAR_TELEMETRY=OFF build runs the same suite, and this
    // is the assertion that matters there: nothing else about the state
    // can make a report happen.
    grant_consent("http://127.0.0.1:1/report");
    EXPECT_EQ(familiar::telemetry::suppression_reason(),
              QStringLiteral("not compiled in"));
    EXPECT_FALSE(familiar::telemetry::should_report());
    EXPECT_FALSE(familiar::telemetry::is_enabled());
    EXPECT_FALSE(familiar::telemetry::needs_consent());
}

TEST_F(TelemetryTest, NoTelemetryFlagSuppresses)
{
    if (!familiar::telemetry::is_compiled_in()) {
        GTEST_SKIP() << "built with -DFAMILIAR_TELEMETRY=OFF";
    }
    grant_consent("http://127.0.0.1:1/report");
    CommandlineArgs::instance().parse(
        {QStringLiteral("familiar"), QStringLiteral("--no-telemetry")});
    EXPECT_EQ(familiar::telemetry::suppression_reason(),
              QStringLiteral("--no-telemetry"));
}

TEST_F(TelemetryTest, DoNotTrackSuppresses)
{
    if (!familiar::telemetry::is_compiled_in()) {
        GTEST_SKIP() << "built with -DFAMILIAR_TELEMETRY=OFF";
    }
    grant_consent("http://127.0.0.1:1/report");
    qputenv("DO_NOT_TRACK", QByteArray("1"));
    EXPECT_EQ(familiar::telemetry::suppression_reason(),
              QStringLiteral("DO_NOT_TRACK is set"));
}

// "DO_NOT_TRACK=0" means "do not do not track" - an empty value or a
// literal zero is the documented way to say no, and honouring it is the
// difference between reading the convention and pattern-matching the
// variable name.
TEST_F(TelemetryTest, DoNotTrackSetToZeroDoesNotSuppress)
{
    if (!familiar::telemetry::is_compiled_in()) {
        GTEST_SKIP() << "built with -DFAMILIAR_TELEMETRY=OFF";
    }
    grant_consent("http://127.0.0.1:1/report");
    qputenv("DO_NOT_TRACK", QByteArray("0"));
    EXPECT_NE(familiar::telemetry::suppression_reason(),
              QStringLiteral("DO_NOT_TRACK is set"));
    qputenv("DO_NOT_TRACK", QByteArray(""));
    EXPECT_NE(familiar::telemetry::suppression_reason(),
              QStringLiteral("DO_NOT_TRACK is set"));
}

// A fresh install: the dialog has never been answered, so nothing is
// sent even though the stored "enabled" default is whatever the build
// was configured with.
TEST_F(TelemetryTest, UnansweredConsentSuppresses)
{
    if (!familiar::telemetry::is_compiled_in()) {
        GTEST_SKIP() << "built with -DFAMILIAR_TELEMETRY=OFF";
    }
    qputenv("FAMILIAR_TELEMETRY_URL", QByteArray("http://127.0.0.1:1/report"));
    EXPECT_TRUE(familiar::telemetry::needs_consent());
    EXPECT_EQ(familiar::telemetry::suppression_reason(),
              QStringLiteral("consent not answered yet"));
}

TEST_F(TelemetryTest, DisabledInSettingsSuppresses)
{
    if (!familiar::telemetry::is_compiled_in()) {
        GTEST_SKIP() << "built with -DFAMILIAR_TELEMETRY=OFF";
    }
    qputenv("FAMILIAR_TELEMETRY_URL", QByteArray("http://127.0.0.1:1/report"));
    FamSettings::set_value(QLatin1String(kconsentAskedKey), true);
    FamSettings::set_value(QLatin1String(kenabledKey), false);
    EXPECT_FALSE(familiar::telemetry::needs_consent());
    EXPECT_EQ(familiar::telemetry::suppression_reason(),
              QStringLiteral("disabled in settings"));
}

// The shipped default until Max supplies a real one: with no endpoint
// configured the client is silent rather than guessing a host.
TEST_F(TelemetryTest, MissingEndpointSuppresses)
{
    if (!familiar::telemetry::is_compiled_in()) {
        GTEST_SKIP() << "built with -DFAMILIAR_TELEMETRY=OFF";
    }
    if (!familiar::telemetry::endpoint().isEmpty()) {
        GTEST_SKIP() << "built with a compiled-in FAMILIAR_TELEMETRY_URL";
    }
    FamSettings::set_value(QLatin1String(kconsentAskedKey), true);
    FamSettings::set_value(QLatin1String(kenabledKey), true);
    EXPECT_EQ(familiar::telemetry::suppression_reason(),
              QStringLiteral("no endpoint configured"));
}

// QUrl accepts a bare word as a valid relative URL, so isValid() alone
// is not enough - the host check is what rejects it.
TEST_F(TelemetryTest, MalformedEndpointSuppresses)
{
    if (!familiar::telemetry::is_compiled_in()) {
        GTEST_SKIP() << "built with -DFAMILIAR_TELEMETRY=OFF";
    }
    grant_consent("definitely-not-a-url");
    EXPECT_EQ(familiar::telemetry::suppression_reason(),
              QStringLiteral("endpoint is not a valid URL"));
}

TEST_F(TelemetryTest, AlreadyReportedTodaySuppresses)
{
    if (!familiar::telemetry::is_compiled_in()) {
        GTEST_SKIP() << "built with -DFAMILIAR_TELEMETRY=OFF";
    }
    grant_consent("http://127.0.0.1:1/report");
    FamSettings::set_value(QLatin1String(klastSentKey),
                           QDate::currentDate().toString(Qt::ISODate));
    EXPECT_EQ(familiar::telemetry::suppression_reason(),
              QStringLiteral("already reported today"));

    // Yesterday's date must not suppress: the whole point is one report
    // per day, not one per install.
    FamSettings::set_value(QLatin1String(klastSentKey),
                           QDate::currentDate().addDays(-1).toString(
                               Qt::ISODate));
    EXPECT_NE(familiar::telemetry::suppression_reason(),
              QStringLiteral("already reported today"));
}

// The backstop: with every other condition deliberately satisfied, a
// test run still refuses to report. If this ever fails, the suite has
// started sending real traffic from whoever ran it.
TEST_F(TelemetryTest, TestModeSuppressesEvenWhenEverythingElseAllows)
{
    if (!familiar::telemetry::is_compiled_in()) {
        GTEST_SKIP() << "built with -DFAMILIAR_TELEMETRY=OFF";
    }
    grant_consent("http://127.0.0.1:1/report");
    EXPECT_EQ(familiar::telemetry::suppression_reason(),
              QStringLiteral("test mode"));
    EXPECT_FALSE(familiar::telemetry::should_report());
}

// ─── consent and endpoint plumbing ────────────────────────────────────

TEST_F(TelemetryTest, ConsentAnswerIsRecordedForBothAnswers)
{
    if (!familiar::telemetry::is_compiled_in()) {
        GTEST_SKIP() << "built with -DFAMILIAR_TELEMETRY=OFF";
    }
    ASSERT_TRUE(familiar::telemetry::needs_consent());

    familiar::telemetry::set_consent(false);
    EXPECT_FALSE(familiar::telemetry::needs_consent());
    EXPECT_FALSE(familiar::telemetry::is_enabled());

    // Answering "no" must not make the dialog come back on the next
    // launch, which is the reason consent_asked is a separate key from
    // enabled (see core/settings.cpp's field table).
    familiar::telemetry::set_consent(true);
    EXPECT_FALSE(familiar::telemetry::needs_consent());
    EXPECT_TRUE(familiar::telemetry::is_enabled());
}

TEST_F(TelemetryTest, EndpointReadsTheEnvironmentOverride)
{
    qputenv("FAMILIAR_TELEMETRY_URL", QByteArray("http://example.test/x"));
    EXPECT_EQ(familiar::telemetry::endpoint(),
              QStringLiteral("http://example.test/x"));
    qunsetenv("FAMILIAR_TELEMETRY_URL");
    // Back to the compiled-in value, which is empty unless the build set
    // -DFAMILIAR_TELEMETRY_URL.
    EXPECT_NE(familiar::telemetry::endpoint(),
              QStringLiteral("http://example.test/x"));
}

// ─── install id ───────────────────────────────────────────────────────

// Documented in telemetry.h and worth pinning: a throwaway --settings
// file is a normal thing to pass, and if the id followed it every such
// run would report as a brand-new install.
TEST_F(TelemetryTest, InstallIdPathIgnoresTheSettingsOverride)
{
    const QString before = familiar::telemetry::install_id_path();
    CommandlineArgs::instance().parse({QStringLiteral("familiar"),
                                       QStringLiteral("--settings"),
                                       QStringLiteral("/tmp/nowhere.json")});
    EXPECT_EQ(familiar::telemetry::install_id_path(), before);
    EXPECT_FALSE(before.contains(QLatin1String("nowhere")));
}

TEST_F(TelemetryInstallIdTest,
       FirstCallCreatesALowercaseUuidAndLaterCallsReuseIt)
{
    ASSERT_FALSE(QFile::exists(path_));

    const QString first = familiar::telemetry::install_id();
    ASSERT_FALSE(first.isEmpty());
    EXPECT_EQ(first, first.toLower());
    EXPECT_FALSE(QUuid(first).isNull());
    EXPECT_FALSE(first.contains(QLatin1Char('{')));
    EXPECT_TRUE(QFile::exists(path_));
    EXPECT_EQ(read_file(), first);

    EXPECT_EQ(familiar::telemetry::install_id(), first);
}

// Regression test for a real bug: install_id() used to leave the file
// open after reading it, and QFile::open() on an already-open file fails
// outright. A file that failed the UUID check could therefore never be
// rewritten - the regeneration below returned an empty string on every
// launch and that install stayed silent permanently.
TEST_F(TelemetryInstallIdTest, CorruptedIdFileIsRegeneratedInPlace)
{
    write_file(QByteArray("this is not a uuid\n"));

    const QString regenerated = familiar::telemetry::install_id();
    ASSERT_FALSE(regenerated.isEmpty());
    EXPECT_FALSE(QUuid(regenerated).isNull());
    EXPECT_EQ(read_file(), regenerated);
    EXPECT_EQ(familiar::telemetry::install_id(), regenerated);
}

TEST_F(TelemetryInstallIdTest, EmptyIdFileIsRegenerated)
{
    write_file(QByteArray());
    ASSERT_TRUE(QFileInfo(path_).exists());

    const QString regenerated = familiar::telemetry::install_id();
    ASSERT_FALSE(regenerated.isEmpty());
    EXPECT_FALSE(QUuid(regenerated).isNull());
    EXPECT_EQ(read_file(), regenerated);
}

// Trailing whitespace and a different case are accepted rather than
// treated as corruption: a hand-copied id should keep counting as the
// same install instead of silently becoming a second one.
TEST_F(TelemetryInstallIdTest, StoredIdIsTrimmedAndLowercased)
{
    const QString uuid
        = QUuid::createUuid().toString(QUuid::WithoutBraces).toUpper();
    write_file(("  " + uuid + "  \n").toUtf8());

    const QString read = familiar::telemetry::install_id();
    EXPECT_EQ(read, uuid.toLower());
}
