#pragma once

#include "flit_record.hpp"
#include "trace_session.hpp"

#include <QAbstractTableModel>

#include <memory>
#include <optional>

namespace CHIron::Gui {

class FlitDetailsModel final : public QAbstractTableModel {
public:
    explicit FlitDetailsModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void setTraceSession(std::shared_ptr<TraceSession> session);
    void setLogicalRow(int logicalRow);
    void setRecord(const FlitRecord* record);

private:
    std::shared_ptr<TraceSession> traceSession_;
    std::optional<FlitRecord> ownedRecord_;
    const FlitRecord* record_ = nullptr;
    bool loading_ = false;
    QString loadingMessage_;
    int loadGeneration_ = 0;
};

}  // namespace CHIron::Gui
