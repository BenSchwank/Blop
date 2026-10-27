#pragma once

#include "calendarservice.h"

#include <QDateTime>

class QWidget;

/// Returns a filled event draft. `ok` is false on cancel.
namespace CalendarEventEditor {
bool promptNew(QWidget *parent, const QDateTime &presetStart,
               CalendarEvent *out);
/// Edits a copy of `existing`; `out` keeps its id/source. false on cancel.
bool promptEdit(QWidget *parent, const CalendarEvent &existing,
                CalendarEvent *out);
} // namespace CalendarEventEditor
