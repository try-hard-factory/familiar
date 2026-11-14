# Privacy

familiar is a desktop application. Your boards, the images on them and
everything you type stay on your computer. There is no account, nothing
to sign into, and the application works with no network connection at
all.

One feature talks to the network: an anonymous counter of installations
and launches. This document describes exactly what it does. It is a
plain-language description of what the software actually does, written
against the source in `src/telemetry/`, not a legal document.

Last updated: 2026-11-11.

## What is sent

At most once per calendar day, and only after you have agreed, the
application sends one small request containing three values:

| Field | What it is |
|---|---|
| `schema` | the number `1`, so the server can tell this format from a later one |
| `install_id` | a random identifier generated on this computer when the counter first runs |
| `version` | the version of familiar you are running, for example `0.1.0` |

That is the whole body of the request. It looks like this:

```json
{"schema": 1, "install_id": "0842050c-1917-462a-b2e5-9fbcdb75cebb", "version": "0.1.0"}
```

The `install_id` is a random number, generated on this computer and
kept in a file beside your settings. It is not derived from anything
about you, your computer or your hardware — not from a serial number,
a MAC address, an account name or a disk — so it identifies the
installation and nothing else, and it matches nothing anywhere outside
this computer. It is generated only once you have agreed: declining
leaves no identifier behind at all.

The request itself is also kept as plain as possible, which took
deliberate effort rather than defaults:

- **No cookies** are sent or stored.
- The `User-Agent` is the fixed string `familiar-telemetry`. Qt's
  default would have been a browser-shaped string carrying library
  versions.
- `Accept-Language` is forced to `en`. Qt would otherwise fill it in
  from your system locale, which is a weak identifier that nothing in
  the consent dialog mentions — and anything the dialog does not
  mention must not be sent.

### What counts as one installation

The identifier lives beside your settings, not inside the program's own
folder:

| | |
|---|---|
| Linux | `~/.config/familiar/install_id` |
| Windows | `%LOCALAPPDATA%\familiar\install_id` |
| macOS | `~/Library/Preferences/familiar/install_id` |

Uninstalling the application does not normally remove that folder, so
**reinstalling keeps the same identifier** — a reinstall is not counted
as a new installation, and neither is an upgrade. Your answer to the
question survives with it: if you turned the counter off, it stays off
and you are not asked again.

A new identifier appears when there is a new configuration folder: a
different user account on the same computer, a different computer, or
that folder being deleted.

So "installations" in the published figures means **configuration
profiles that have reported at least once**. It is not a count of
people and not a count of computers.

## What is never sent

No file names, no file paths, no images, no canvas contents, no board
titles, no notes, no settings, no name, no email address, no hardware
details, no operating system version, no language, no screen size, no
location, and no crash data.

## When it happens

Once per calendar day at most, shortly after the application starts. A
day on which you open familiar five times produces one request, not
five. A day on which you do not open it produces none.

Nothing is sent before the question on first run has been answered.

Be aware of how that dialog behaves: the checkbox starts **ticked**, and
closing the window with the × is not a third answer — it means "whatever
the checkbox currently says", so it leaves the counter on. To decline,
untick the box before closing or continuing. The answer is remembered
either way, so the question is asked once and never again.

## What the server stores

The server is run by the project and is not a third-party analytics
service. Your data is not sold, shared or handed to anyone, because
there is nothing to hand over: what is kept is a count.

It keeps, per installation:

- a **salted SHA-256 hash** of the `install_id`, never the identifier
  itself. The salt is generated once on the server and never published,
  so a copy of the database cannot be matched against the identifier on
  your computer;
- the first and last day it reported, as a **calendar date with no time
  of day**;
- the version it reported most recently.

And, per day and version, the number of reports and the number of
installations that appeared for the first time.

## What the server does not store

**No IP addresses, anywhere — not in the database, not in a log file,
not even hashed.**

Your address is unavoidably visible to the server while it receives the
request, the same way it is to every website you open. It is used for
two things, both in memory only: to limit how many requests one address
may send per minute, and to limit how many new installations one
network may create per day. Neither leaves memory, and both are
forgotten when the window passes or the server restarts.

No timestamp finer than a calendar date is recorded. The database
cannot answer "when was this person using it", and that is a property
of its design rather than a gap to be closed later.

## What is published

The totals are public: how many installations there are, how many
reported on each day, and which versions are in use. They are published
as a web page and as a JSON endpoint, and they are the same numbers the
project sees. Nothing per-installation is published, because nothing
per-installation exists in a form anyone could read.

The published counts are a **lower bound**, not a measurement: the
counter is optional, and the endpoint is open to anyone.

## Turning it off

**Settings → the "Send the anonymous count" option.** It takes effect
immediately and permanently; nothing is sent again until you turn it
back on.

The first-run dialog is the same switch asked once in advance, and it
is set the same way: whatever you answer there can be changed later in
Settings.

Turning it off does not delete the counts already collected. It cannot:
there is no identifier stored that could be matched to your
installation, which is the same property that keeps the data anonymous
in the first place.

## Builds without the counter

The counter can be compiled out of the application entirely, and a
build made that way contains no reporting code at all. A build that was
not configured with a server address also sends nothing, whatever the
setting says. If you installed familiar from a distribution's
repository rather than from this project, the package maintainer
decides which of these applies.

## Changes

This file is part of the source repository, so every change to it is in
the project's history alongside the change to the code it describes.
