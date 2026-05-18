#include "flit_details_model.hpp"

#include <QTimer>

namespace CHIron::Gui {

FlitDetailsModel::FlitDetailsModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int FlitDetailsModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }

    if (loading_) {
        return 1;
    }
    if (!record_) {
        return 0;
    }
    return static_cast<int>(record_->fields.size());
}

int FlitDetailsModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return 4;
}

QVariant FlitDetailsModel::data(const QModelIndex& index, const int role) const
{
    if (loading_) {
        if (!index.isValid() || index.row() != 0) {
            return {};
        }

        if (role == Qt::DisplayRole) {
            switch (index.column()) {
            case 0:
                return QStringLiteral("Session");
            case 1:
                return QStringLiteral("Detail");
            case 2:
                return loadingMessage_.isEmpty()
                    ? QStringLiteral("Loading selected flit details...")
                    : loadingMessage_;
            case 3:
                return QString();
            default:
                return {};
            }
        }

        if (role == Qt::TextAlignmentRole) {
            return index.column() < 2 ? static_cast<int>(Qt::AlignCenter)
                                      : static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        }

        return {};
    }

    if (!record_ || index.row() < 0 || index.row() >= static_cast<int>(record_->fields.size())) {
        return {};
    }

    const FieldEntry& field = record_->fields[static_cast<std::size_t>(index.row())];

    if (role == Qt::DisplayRole) {
        switch (index.column()) {
        case 0:
            return FieldScopeText(field);
        case 1:
            return FieldNameText(field);
        case 2:
            return field.value;
        case 3:
            return field.raw;
        default:
            return {};
        }
    }

    if (role == Qt::TextAlignmentRole) {
        return index.column() < 2 ? static_cast<int>(Qt::AlignCenter)
                                  : static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
    }

    return {};
}

QVariant FlitDetailsModel::headerData(const int section,
                                      const Qt::Orientation orientation,
                                      const int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return {};
    }

    switch (section) {
    case 0:
        return QStringLiteral("Scope");
    case 1:
        return QStringLiteral("Field");
    case 2:
        return QStringLiteral("Decoded");
    case 3:
        return QStringLiteral("Raw");
    default:
        return {};
    }
}

void FlitDetailsModel::setTraceSession(std::shared_ptr<TraceSession> session)
{
    beginResetModel();
    traceSession_ = std::move(session);
    ownedRecord_.reset();
    record_ = nullptr;
    loading_ = false;
    loadingMessage_.clear();
    ++loadGeneration_;
    endResetModel();
}

void FlitDetailsModel::setLogicalRow(const int logicalRow)
{
    ++loadGeneration_;

    if (!traceSession_ || logicalRow < 0) {
        beginResetModel();
        ownedRecord_.reset();
        record_ = nullptr;
        loading_ = false;
        loadingMessage_.clear();
        endResetModel();
        return;
    }

    beginResetModel();
    ownedRecord_.reset();
    record_ = nullptr;
    loading_ = true;
    loadingMessage_ = QStringLiteral("Loading selected flit details...");
    endResetModel();

    const int generation = loadGeneration_;
    QTimer::singleShot(0, this, [this, generation, logicalRow]() {
        if (generation != loadGeneration_) {
            return;
        }

        std::vector<FlitRecord> rows;
        CLogBTraceLoadError error;
        if (!traceSession_->readRows(static_cast<std::uint64_t>(logicalRow), 1, rows, error)
            || rows.empty()) {
            beginResetModel();
            ownedRecord_.reset();
            record_ = nullptr;
            loading_ = true;
            loadingMessage_ = error.summary.isEmpty()
                ? QStringLiteral("Unable to load selected flit details.")
                : error.summary;
            endResetModel();
            return;
        }

        beginResetModel();
        ownedRecord_ = std::move(rows.front());
        record_ = &*ownedRecord_;
        loading_ = false;
        loadingMessage_.clear();
        endResetModel();
    });
}

void FlitDetailsModel::setRecord(const FlitRecord* record)
{
    ++loadGeneration_;
    beginResetModel();
    if (record) {
        ownedRecord_ = *record;
        record_ = &*ownedRecord_;
    } else {
        ownedRecord_.reset();
        record_ = nullptr;
    }
    loading_ = false;
    loadingMessage_.clear();
    endResetModel();
}

}  // namespace CHIron::Gui
