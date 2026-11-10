#include "telemetry/telemetry.h"

#include <core/settings.h>
#include <log/log.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLatin1String>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QStandardPaths>
#include <QString>
#include <QUrl>
#include <QUuid>
#include <Qt>
#include <QtEnvironmentVariables>

using familiar::log::Ch;

namespace familiar::telemetry {

namespace {

constexpr int kpayloadSchema = 1;

// Keys live in settings.json's "Telemetry" group, registered in
// FamSettings::fields() (core/settings.cpp) like every other setting.
constexpr char kenabledKey[] = "Telemetry/enabled";
constexpr char kconsentAskedKey[] = "Telemetry/consent_asked";
constexpr char klastSentKey[] = "Telemetry/last_sent_date";

constexpr char kinstallIdFileName[] = "install_id";

// Anything slower than this is not worth holding a socket open for a
// counter nobody is waiting on.
constexpr int krequestTimeoutMs = 10000;

#ifdef FAMILIAR_TELEMETRY_URL
constexpr char kcompiledEndpoint[] = FAMILIAR_TELEMETRY_URL;
#else
constexpr char kcompiledEndpoint[] = "";
#endif

QString config_dir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

bool do_not_track_set()
{
    // A proposed convention rather than a standard, and with no
    // enforcement behind it, but honouring it costs one lookup:
    // https://consoledonottrack.com
    const QByteArray value = qgetenv("DO_NOT_TRACK");
    return !value.isEmpty() && value != "0";
}

QString today_string()
{
    return QDate::currentDate().toString(Qt::ISODate);
}

} // namespace

bool is_compiled_in()
{
#ifdef FAMILIAR_TELEMETRY
    return true;
#else
    return false;
#endif
}

bool default_enabled()
{
#ifdef FAMILIAR_TELEMETRY_DEFAULT_ON
    return true;
#else
    return false;
#endif
}

bool needs_consent()
{
    if (!is_compiled_in()) {
        return false;
    }
    return !FamSettings::value_or_default(QLatin1String(kconsentAskedKey))
                .toBool();
}

void set_consent(bool enabled)
{
    FamSettings::set_value(QLatin1String(kconsentAskedKey), true);
    FamSettings::set_value(QLatin1String(kenabledKey), enabled);
    FLOG_INFO(Ch::Settings,
              "Telemetry consent answered: {}",
              enabled ? "enabled" : "disabled");
}

bool is_enabled()
{
    if (!is_compiled_in()) {
        return false;
    }
    return FamSettings::value_or_default(QLatin1String(kenabledKey)).toBool();
}

void set_enabled(bool enabled)
{
    FamSettings::set_value(QLatin1String(kenabledKey), enabled);
}

QString install_id_path()
{
    return QDir(config_dir()).filePath(QLatin1String(kinstallIdFileName));
}

QString install_id()
{
    const QString path = install_id_path();

    QFile file(path);
    if (file.exists()) {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            FLOG_WARN(Ch::IO,
                      "Telemetry: could not read {}: {}",
                      path,
                      file.errorString());
            return {};
        }
        const QString stored
            = QString::fromUtf8(file.readAll()).trimmed().toLower();
        // Closed before the write path below can reopen it. QFile::open()
        // on an already-open file fails outright ("File already open"),
        // so without this a corrupted id file could never be replaced -
        // the regeneration below would fail every time and telemetry
        // would stay silent for that user permanently.
        file.close();
        // A truncated or hand-edited file would otherwise be posted as
        // an id the server rejects with a 400 forever. Regenerating is
        // the lesser evil: it counts as one extra install, once.
        const QUuid parsed(stored);
        if (!parsed.isNull()) {
            return stored;
        }
        FLOG_WARN(Ch::IO,
                  "Telemetry: {} does not hold a UUID, regenerating",
                  path);
    }

    // Created only here, on the first report that actually happens -
    // never while asking for consent, so declining leaves no identifier
    // on disk.
    QString generated
        = QUuid::createUuid().toString(QUuid::WithoutBraces).toLower();

    if (!QDir().mkpath(config_dir())) {
        FLOG_WARN(Ch::IO,
                  "Telemetry: could not create {}, not reporting",
                  config_dir());
        return {};
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate
                   | QIODevice::Text)) {
        FLOG_WARN(Ch::IO,
                  "Telemetry: could not write {}: {}",
                  path,
                  file.errorString());
        return {};
    }
    if (file.write(generated.toUtf8() + '\n') < 0) {
        FLOG_WARN(Ch::IO, "Telemetry: could not write {}", path);
        return {};
    }
    file.close();

    FLOG_INFO(Ch::IO, "Telemetry: created a new install id file");
    return generated;
}

const char* endpoint_source_name(EndpointSource source)
{
    switch (source) {
    case EndpointSource::CommandLine:
        return "--telemetry-url";
    case EndpointSource::Environment:
        return "FAMILIAR_TELEMETRY_URL";
    case EndpointSource::CompiledIn:
        return "compiled in";
    case EndpointSource::None:
        break;
    default:
        // This build compiles with -Wswitch-default as an error, so a
        // default is required even though every enumerator above is
        // handled. The cost is that -Wswitch can no longer flag an
        // enumerator added later - it would land here and be named
        // "nowhere" instead of failing the build.
        break;
    }
    return "nowhere";
}

QString endpoint(EndpointSource& source)
{
    const QString fromArgs = CommandlineArgs::instance().telemetry_url();
    if (!fromArgs.isEmpty()) {
        source = EndpointSource::CommandLine;
        return fromArgs;
    }
    const QByteArray fromEnv = qgetenv("FAMILIAR_TELEMETRY_URL");
    if (!fromEnv.isEmpty()) {
        source = EndpointSource::Environment;
        return QString::fromUtf8(fromEnv);
    }
    const QString compiled = QString::fromUtf8(kcompiledEndpoint);
    // Empty is None rather than CompiledIn: "the build compiled in an
    // empty string" and "the build compiled in nothing" are the same
    // thing here, and CMakeLists.txt only defines the macro when the
    // value is non-empty anyway (see its if(FAMILIAR_TELEMETRY_URL)).
    source = compiled.isEmpty() ? EndpointSource::None
                                : EndpointSource::CompiledIn;
    return compiled;
}

QString endpoint()
{
    EndpointSource ignored = EndpointSource::None;
    return endpoint(ignored);
}

void log_configuration()
{
    if (!is_compiled_in()) {
        FLOG_INFO(Ch::Net, "Telemetry: not compiled in");
        return;
    }

    EndpointSource source = EndpointSource::None;
    const QString url = endpoint(source);
    if (url.isEmpty()) {
        // Unconditional, unlike every other suppression reason: with no
        // address there is nothing to send to, whatever the settings or
        // the consent dialog later say.
        FLOG_INFO(Ch::Net,
                  "Telemetry: no endpoint configured, nothing will be sent");
        return;
    }
    FLOG_INFO(Ch::Net,
              "Telemetry: endpoint {} (from {})",
              url,
              endpoint_source_name(source));
}

QString suppression_reason()
{
    if (!is_compiled_in()) {
        return QStringLiteral("not compiled in");
    }
    if (CommandlineArgs::instance().no_telemetry()) {
        return QStringLiteral("--no-telemetry");
    }
    if (do_not_track_set()) {
        return QStringLiteral("DO_NOT_TRACK is set");
    }
    if (needs_consent()) {
        return QStringLiteral("consent not answered yet");
    }
    if (!is_enabled()) {
        return QStringLiteral("disabled in settings");
    }
    if (endpoint().isEmpty()) {
        return QStringLiteral("no endpoint configured");
    }
    const QUrl url(endpoint());
    if (!url.isValid() || url.host().isEmpty()) {
        return QStringLiteral("endpoint is not a valid URL");
    }
    if (FamSettings::value_or_default(QLatin1String(klastSentKey)).toString()
        == today_string()) {
        return QStringLiteral("already reported today");
    }
    // Checked LAST on purpose, even though it is the cheapest test here.
    // "familiar -t" turns test mode on and returns before main() ever
    // reaches the reporting code, so this is belt-and-braces either way
    // - what it buys is that "a test run never sends anything" is a
    // property of this function rather than of main()'s control flow.
    //
    // Were it checked first, it would shadow every reason above and the
    // suite could only ever observe this one string, which is why the
    // order matters: each reason above stays individually assertable,
    // while the value here is still never empty under "-t", so
    // should_report() is still false no matter what a test sets up.
    if (QStandardPaths::isTestModeEnabled()) {
        return QStringLiteral("test mode");
    }
    return {};
}

bool should_report()
{
    return suppression_reason().isEmpty();
}

QByteArray payload_for(const QString& installId, const QString& version)
{
    QJsonObject object;
    object[QStringLiteral("schema")] = kpayloadSchema;
    object[QStringLiteral("install_id")] = installId;
    object[QStringLiteral("version")] = version;
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

void report_launch(QNetworkAccessManager& manager)
{
    const QString reason = suppression_reason();
    if (!reason.isEmpty()) {
        FLOG_DEBUG(Ch::Net, "Telemetry: not reporting - {}", reason);
        return;
    }

    const QString id = install_id();
    if (id.isEmpty()) {
        return; // install_id() already logged why.
    }

    const QUrl url(endpoint());
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    // Both of these are set explicitly to overwrite what Qt would send
    // on its own. Measured by capturing a real request: Qt fills in
    // "Accept-Language" from the system locale ("en,*" here), which is a
    // weak identifier the consent dialog does not mention and therefore
    // must not be sent - its wording is "That is all".
    //
    // Accept-Encoding is left alone: the response is 204 with no body,
    // so there is nothing to encode, and it carries no user-specific
    // information anyway.
    request.setRawHeader("Accept-Language", "en");
    // A fixed product token rather than Qt's default "Mozilla/5.0",
    // which is both a lie and version-dependent. The version is already
    // in the body, so this adds nothing new.
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("familiar-telemetry"));
    request.setTransferTimeout(krequestTimeoutMs);
    // Nothing here needs cookies or any other per-user state, and
    // sending them would attach an identifier beyond the one in the
    // body - which is the whole thing this feature promises not to do.
    request.setAttribute(QNetworkRequest::CookieLoadControlAttribute,
                         QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CookieSaveControlAttribute,
                         QNetworkRequest::Manual);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::AlwaysNetwork);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    // Marked as sent BEFORE the reply arrives, on purpose. The
    // alternative - recording success only - retries on every launch
    // while a server is down, which is exactly when retrying is least
    // welcome. One lost count beats a retry loop.
    FamSettings::set_value(QLatin1String(klastSentKey), today_string());

    const QByteArray body = payload_for(id,
                                        QCoreApplication::applicationVersion());

    FLOG_INFO(Ch::Net, "Telemetry: reporting to {}", url.toString());

    QNetworkReply* reply = manager.post(request, body);

    // Deliberately NOT the nested-QEventLoop pattern used for image
    // downloads (fileio.cpp): that one is synchronous because a caller
    // is waiting for the image. Here nothing waits, and blocking
    // startup on a counter would be indefensible.
    QObject::connect(reply, &QNetworkReply::finished, reply, [reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            FLOG_DEBUG(Ch::Net, "Telemetry: reported");
        } else {
            // Not an error for the user: being offline is normal and
            // there is nothing for them to do about it.
            FLOG_DEBUG(Ch::Net,
                       "Telemetry: report failed - {}",
                       reply->errorString());
        }
        reply->deleteLater();
    });
}

} // namespace familiar::telemetry
