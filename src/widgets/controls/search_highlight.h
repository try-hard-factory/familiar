#pragma once

#include <QString>

// HTML-escapes `text` and wraps the first case-insensitive occurrence of
// `query` in <b>, for feeding into a rich-text QLabel. Falls back to a
// plain escape when there's no match (or no active search), so labels
// render identically to before search existed. Shared between the
// Keyboard Shortcuts row list (bindings_tree_widget.cpp) and the
// settings-window category sidebar (ui/settings_window.cpp), which
// both bold the matched substring of whatever the search box found.
inline QString highlightSearchMatch(const QString& text, const QString& query)
{
    if (query.isEmpty()) {
        return text.toHtmlEscaped();
    }
    const int idx = text.indexOf(query, 0, Qt::CaseInsensitive);
    if (idx < 0) {
        return text.toHtmlEscaped();
    }
    return text.left(idx).toHtmlEscaped() + QStringLiteral("<b>")
           + text.mid(idx, query.length()).toHtmlEscaped()
           + QStringLiteral("</b>")
           + text.mid(idx + query.length()).toHtmlEscaped();
}
