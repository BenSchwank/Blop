#pragma once

#include "calendarservice.h"

#include <QDateTime>

class QWidget;

/// Returns a filled event draft. `ok` is false on cancel.
namespace CalendarEventEditor {
bool promptNew(QWidget *parent, const QDateTime &presetStart,
               CalendarEvent *out);
} // namespace CalendarEventEditor
