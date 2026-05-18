#pragma once

#include "clog_b_trace_loader.hpp"
#include "flit_record.hpp"

#include <QString>
#include <QStringList>

#include <vector>

namespace CHIron::Gui {

struct FlitEditCellState {
    bool applicable = false;
    bool editable = false;
    bool fixed = false;
    QString reason;
};

struct FlitEditResult {
    bool ok = false;
    QString summary;
    QString detail;
};

class FlitEditAdapter final {
public:
    static QString fieldNameForColumn(int column,
                                      const QString& optionalFieldName,
                                      const FlitRecord& row);

    static QStringList templateFieldNames();

    static FlitEditCellState cellState(const FlitRecord& row,
                                       const CLogBTraceMetadata* metadata,
                                       const QString& fieldName);

    static FlitEditResult applyEdit(FlitRecord& row,
                                    const CLogBTraceMetadata* metadata,
                                    const QString& fieldName,
                                    const QString& text);

    static bool makeTemplate(const CLogBTraceMetadata* metadata,
                             FlitChannel channel,
                             FlitDirection direction,
                             qint64 timestamp,
                             FlitRecord& row,
                             QString* errorText = nullptr);

private:
    FlitEditAdapter() = delete;
};

}  // namespace CHIron::Gui
