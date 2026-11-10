#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <QObject>
#include <QString>

#include <cstdint>

class QNetworkAccessManager;

namespace familiar::telemetry {

// Reports "this install launched, running this version" once a day, so
// unique installs and launches can be counted. Nothing else is sent:
// see payload_for() for the exact body, which is three fields.
//
// Four separate things have to be true before a single byte leaves the
// machine, and every one of them is checked in should_report():
//  - the feature was compiled in (FAMILIAR_TELEMETRY);
//  - the user has been shown the consent dialog and did not turn it off;
//  - neither DO_NOT_TRACK nor --no-telemetry is set;
//  - nothing was already reported today.
//
// The install id is a UUID in its own file next to settings.json, NOT a
// key inside it. That is not a style choice: SettingsHandler::
// import_settings_from() replaces the whole document, so an id stored
// there would be overwritten by someone else's id when a user imports
// their settings, and two machines would report as one install.

// Whether the build has telemetry compiled in at all. Packagers can
// turn the whole thing off with -DFAMILIAR_TELEMETRY=OFF, which makes
// this false and every other function here a no-op.
bool is_compiled_in();

// Default state of the consent checkbox in the first-run dialog,
// -DFAMILIAR_TELEMETRY_DEFAULT_ON. A distribution that requires opt-in
// can ship the same code with it OFF.
bool default_enabled();

// True when the first-run dialog has never been answered. The caller
// shows the dialog and then calls set_consent().
bool needs_consent();

// Records the user's answer and marks the dialog as answered. Writing
// the install-id file is deferred until the first actual report, so a
// refusal leaves no identifier on disk at all.
void set_consent(bool enabled);

// Current state, as stored. False when telemetry was never consented to.
bool is_enabled();
void set_enabled(bool enabled);

// Absolute path of the install-id file. Fixed at AppConfigLocation and
// deliberately NOT following --settings: a throwaway settings file is a
// normal thing to pass for testing, and if the id followed it, every
// such run would look like a brand-new install.
QString install_id_path();

// Reads the id, creating the file if this is the first report. Empty
// string when it cannot be read or written, which disables reporting
// rather than inventing a fresh id on every launch.
QString install_id();

// Why a report is not being sent, or an empty string when it is. Split
// out from report_launch() so the reason can be logged and tested
// without a network stack.
QString suppression_reason();
bool should_report();

// Which of the three possible places the endpoint came from. Needed
// because the address alone does not answer the question people
// actually have when reports go somewhere unexpected - a build with
// nothing compiled in and a build pointed elsewhere by an environment
// variable it inherited look identical from the address.
//
// None is not "empty string": it means no source supplied one at all,
// which is the normal state of a build that never set
// FAMILIAR_TELEMETRY_URL, and the state in which nothing is ever sent.
enum class EndpointSource : std::uint8_t {
    None,
    CommandLine, // --telemetry-url
    Environment, // FAMILIAR_TELEMETRY_URL
    CompiledIn   // -DFAMILIAR_TELEMETRY_URL at configure time
};

// For log lines. Returns a fixed string, never user input.
const char* endpoint_source_name(EndpointSource source);

// Endpoint actually used: the --telemetry-url override, else
// FAMILIAR_TELEMETRY_URL from the environment, else the compiled-in
// default.
//
// The overload also reports where the value came from. Both go through
// one implementation on purpose: a second function that re-derived the
// precedence would be free to drift away from the one that picks the
// value, and the log would then confidently name the wrong source.
QString endpoint();
QString endpoint(EndpointSource& source);

// Writes one INFO line naming the endpoint and where it came from.
// Called once from main() after the logger is up.
//
// Deliberately does NOT log suppression_reason(): at startup the
// consent dialog has not been shown yet, so on a first run the reason
// is "consent not answered yet" and a report is nevertheless sent
// moments later. A line claiming nothing will be sent would be wrong
// exactly when a new user is watching. The one case it can state is an
// empty endpoint, which suppresses unconditionally.
void log_configuration();

// The exact JSON body that would be sent, as compact UTF-8. Exposed so
// a test can assert there is nothing in it beyond these three fields.
QByteArray payload_for(const QString& installId, const QString& version);

// Fire-and-forget. Returns immediately; the reply is discarded. Does
// nothing at all when should_report() is false.
//
// `manager` must outlive the request. MainWindow owns one.
void report_launch(QNetworkAccessManager& manager);

} // namespace familiar::telemetry

#endif // TELEMETRY_H
