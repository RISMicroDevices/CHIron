#include "main_window_internal.hpp"

#include <QColorDialog>

#include <algorithm>

namespace CHIron::Gui {
using namespace MainWindowDetail;

namespace {

bool MarkerEquals(const TraceMarker& lhs, const TraceMarker& rhs)
{
    return lhs.id == rhs.id
        && lhs.logicalRow == rhs.logicalRow
        && lhs.timestamp == rhs.timestamp
        && lhs.label == rhs.label
        && lhs.color == rhs.color
        && lhs.memo == rhs.memo
        && lhs.createdAt == rhs.createdAt
        && lhs.updatedAt == rhs.updatedAt;
}

bool MarkerListEquals(const std::vector<TraceMarker>& lhs,
                      const std::vector<TraceMarker>& rhs)
{
    if (lhs.size() != rhs.size()) {
        return false;
    }
    for (std::size_t index = 0; index < lhs.size(); ++index) {
        if (!MarkerEquals(lhs[index], rhs[index])) {
            return false;
        }
    }
    return true;
}

bool MarkerIdExists(const std::vector<TraceMarker>& markers, const QString& markerId)
{
    return std::any_of(markers.cbegin(), markers.cend(), [&](const TraceMarker& marker) {
        return marker.id == markerId;
    });
}

QString MarkerCommandText(const QString& fallback, const TraceMarker& marker)
{
    const QString label = marker.label.trimmed();
    return label.isEmpty()
        ? fallback
        : QStringLiteral("%1 \"%2\"").arg(fallback, label);
}

}  // namespace

TraceMarker* MainWindow::markerById(TraceViewSession& session, const QString& markerId)
{
    const auto found = std::find_if(session.markers.begin(), session.markers.end(), [&](const TraceMarker& marker) {
        return marker.id == markerId;
    });
    return found == session.markers.end() ? nullptr : &*found;
}

const TraceMarker* MainWindow::markerById(const TraceViewSession& session, const QString& markerId) const
{
    const auto found = std::find_if(session.markers.cbegin(), session.markers.cend(), [&](const TraceMarker& marker) {
        return marker.id == markerId;
    });
    return found == session.markers.cend() ? nullptr : &*found;
}

void MainWindow::recordUnifiedRoute(TraceViewSession& session,
                                    const UnifiedUndoRouteKind kind,
                                    const QString& text)
{
    if (session.applyingUnifiedUndoRedo) {
        return;
    }
    if (session.markerUndoIndex < session.markerUndoCommands.size()) {
        session.markerUndoCommands.erase(session.markerUndoCommands.begin()
                                             + static_cast<std::ptrdiff_t>(session.markerUndoIndex),
                                         session.markerUndoCommands.end());
    }
    session.unifiedRedoRoutes.clear();
    session.unifiedUndoRoutes.push_back(UnifiedUndoRoute{kind, text});
    updateEditActions();
    scheduleDiagnosticsSnapshotRefresh();
}

void MainWindow::pushMarkerUndoCommand(TraceViewSession& session, MarkerUndoCommand command)
{
    if (session.applyingUnifiedUndoRedo) {
        return;
    }
    if (MarkerListEquals(command.beforeMarkers, command.afterMarkers)
        && command.beforeSelectedMarkerId == command.afterSelectedMarkerId) {
        return;
    }
    if (session.markerUndoIndex < session.markerUndoCommands.size()) {
        session.markerUndoCommands.erase(session.markerUndoCommands.begin()
                                             + static_cast<std::ptrdiff_t>(session.markerUndoIndex),
                                         session.markerUndoCommands.end());
    }
    session.markerUndoCommands.push_back(std::move(command));
    session.markerUndoIndex = session.markerUndoCommands.size();
    recordUnifiedRoute(session, UnifiedUndoRouteKind::Marker, session.markerUndoCommands.back().text);
}

void MainWindow::applyMarkerSnapshot(TraceViewSession& session,
                                     std::vector<TraceMarker> markers,
                                     const QString& selectedMarkerId)
{
    session.markers = std::move(markers);
    session.selectedMarkerId = MarkerIdExists(session.markers, selectedMarkerId)
        ? selectedMarkerId
        : QString();
    refreshMarkerViews();
    updateEditActions();
    scheduleDiagnosticsSnapshotRefresh();
}

void MainWindow::clearMarkerUndoHistory(TraceViewSession& session)
{
    session.markerUndoCommands.clear();
    session.markerUndoIndex = 0;
    const auto isMarkerRoute = [](const UnifiedUndoRoute& route) {
        return route.kind == UnifiedUndoRouteKind::Marker;
    };
    session.unifiedUndoRoutes.erase(std::remove_if(session.unifiedUndoRoutes.begin(),
                                                   session.unifiedUndoRoutes.end(),
                                                   isMarkerRoute),
                                    session.unifiedUndoRoutes.end());
    session.unifiedRedoRoutes.erase(std::remove_if(session.unifiedRedoRoutes.begin(),
                                                   session.unifiedRedoRoutes.end(),
                                                   isMarkerRoute),
                                    session.unifiedRedoRoutes.end());
    updateEditActions();
    scheduleDiagnosticsSnapshotRefresh();
}

bool MainWindow::dispatchNativeUndoRedoForFocus(const bool redo) const
{
    QWidget* focusWidget = QApplication::focusWidget();
    if (auto* lineEdit = qobject_cast<QLineEdit*>(focusWidget)) {
        redo ? lineEdit->redo() : lineEdit->undo();
        return true;
    }
    if (auto* plainTextEdit = qobject_cast<QPlainTextEdit*>(focusWidget)) {
        redo ? plainTextEdit->redo() : plainTextEdit->undo();
        return true;
    }
    if (auto* textEdit = qobject_cast<QTextEdit*>(focusWidget)) {
        redo ? textEdit->redo() : textEdit->undo();
        return true;
    }
    return false;
}

bool MainWindow::canUndoUnified() const
{
    const TraceViewSession* session = activeTraceViewSession();
    if (!session || session->unifiedUndoRoutes.empty()) {
        return false;
    }
    const UnifiedUndoRoute& route = session->unifiedUndoRoutes.back();
    if (route.kind == UnifiedUndoRouteKind::Marker) {
        return session->markerUndoIndex > 0;
    }
    return flitModel_ && flitModel_->canUndo();
}

bool MainWindow::canRedoUnified() const
{
    const TraceViewSession* session = activeTraceViewSession();
    if (!session || session->unifiedRedoRoutes.empty()) {
        return false;
    }
    const UnifiedUndoRoute& route = session->unifiedRedoRoutes.back();
    if (route.kind == UnifiedUndoRouteKind::Marker) {
        return session->markerUndoIndex < session->markerUndoCommands.size();
    }
    return flitModel_ && flitModel_->canRedo();
}

QString MainWindow::unifiedUndoText() const
{
    const TraceViewSession* session = activeTraceViewSession();
    return session && !session->unifiedUndoRoutes.empty()
        ? session->unifiedUndoRoutes.back().text
        : QString();
}

QString MainWindow::unifiedRedoText() const
{
    const TraceViewSession* session = activeTraceViewSession();
    return session && !session->unifiedRedoRoutes.empty()
        ? session->unifiedRedoRoutes.back().text
        : QString();
}

void MainWindow::undoUnifiedEdit()
{
    if (dispatchNativeUndoRedoForFocus(false)) {
        return;
    }
    TraceViewSession* session = activeTraceViewSession();
    if (!session || session->unifiedUndoRoutes.empty()) {
        return;
    }

    UnifiedUndoRoute route = session->unifiedUndoRoutes.back();
    session->unifiedUndoRoutes.pop_back();
    bool applied = false;

    if (route.kind == UnifiedUndoRouteKind::Marker) {
        if (session->markerUndoIndex > 0
            && session->markerUndoIndex <= session->markerUndoCommands.size()) {
            --session->markerUndoIndex;
            const MarkerUndoCommand& command =
                session->markerUndoCommands[session->markerUndoIndex];
            session->applyingUnifiedUndoRedo = true;
            applyMarkerSnapshot(*session,
                                command.beforeMarkers,
                                command.beforeSelectedMarkerId);
            applied = true;
        }
    } else if (flitModel_ && flitModel_->canUndo()) {
        session->applyingUnifiedUndoRedo = true;
        flitModel_->undoEdit();
        applied = true;
    }
    session = activeTraceViewSession();
    if (!session) {
        return;
    }
    session->applyingUnifiedUndoRedo = false;

    if (applied) {
        session->unifiedRedoRoutes.push_back(std::move(route));
    } else {
        session->unifiedUndoRoutes.push_back(std::move(route));
    }
    updateEditActions();
    scheduleDiagnosticsSnapshotRefresh();
}

void MainWindow::redoUnifiedEdit()
{
    if (dispatchNativeUndoRedoForFocus(true)) {
        return;
    }
    TraceViewSession* session = activeTraceViewSession();
    if (!session || session->unifiedRedoRoutes.empty()) {
        return;
    }

    UnifiedUndoRoute route = session->unifiedRedoRoutes.back();
    session->unifiedRedoRoutes.pop_back();
    bool applied = false;

    if (route.kind == UnifiedUndoRouteKind::Marker) {
        if (session->markerUndoIndex < session->markerUndoCommands.size()) {
            const MarkerUndoCommand& command =
                session->markerUndoCommands[session->markerUndoIndex];
            ++session->markerUndoIndex;
            session->applyingUnifiedUndoRedo = true;
            applyMarkerSnapshot(*session,
                                command.afterMarkers,
                                command.afterSelectedMarkerId);
            applied = true;
        }
    } else if (flitModel_ && flitModel_->canRedo()) {
        session->applyingUnifiedUndoRedo = true;
        flitModel_->redoEdit();
        applied = true;
    }
    session = activeTraceViewSession();
    if (!session) {
        return;
    }
    session->applyingUnifiedUndoRedo = false;

    if (applied) {
        session->unifiedUndoRoutes.push_back(std::move(route));
    } else {
        session->unifiedRedoRoutes.push_back(std::move(route));
    }
    updateEditActions();
    scheduleDiagnosticsSnapshotRefresh();
}

TraceMarker* MainWindow::markerForLogicalRow(TraceViewSession& session, const int logicalRow)
{
    const auto found = std::find_if(session.markers.begin(), session.markers.end(), [logicalRow](const TraceMarker& marker) {
        return marker.logicalRow == logicalRow;
    });
    return found == session.markers.end() ? nullptr : &*found;
}

const TraceMarker* MainWindow::markerForLogicalRow(const TraceViewSession& session, const int logicalRow) const
{
    const auto found = std::find_if(session.markers.cbegin(), session.markers.cend(), [logicalRow](const TraceMarker& marker) {
        return marker.logicalRow == logicalRow;
    });
    return found == session.markers.cend() ? nullptr : &*found;
}

void MainWindow::addMarkerAtLogicalRow(const int logicalRow)
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session || !flitModel_ || logicalRow < 0 || logicalRow >= flitModel_->totalCount()) {
        return;
    }
    if (session->applyingUnifiedUndoRedo) {
        return;
    }
    if (TraceMarker* existing = markerForLogicalRow(*session, logicalRow)) {
        selectMarker(existing->id, false);
        editMarker(existing->id);
        return;
    }

    const FlitRecord* record = flitModel_->recordForLogicalRow(logicalRow);
    TraceMarker marker = MakeTraceMarker(logicalRow,
                                         record ? record->timestamp : 0,
                                         QStringLiteral("Marker %1").arg(logicalRow + 1),
                                         static_cast<int>(session->markers.size()));
    const std::vector<TraceMarker> beforeMarkers = session->markers;
    const QString beforeSelectedMarkerId = session->selectedMarkerId;
    std::vector<TraceMarker> afterMarkers = beforeMarkers;
    const QString newMarkerId = marker.id;
    const QString commandText = MarkerCommandText(QStringLiteral("Add marker"), marker);
    afterMarkers.push_back(std::move(marker));
    applyMarkerSnapshot(*session, afterMarkers, newMarkerId);
    pushMarkerUndoCommand(*session,
                          MarkerUndoCommand{commandText,
                                            beforeMarkers,
                                            beforeSelectedMarkerId,
                                            std::move(afterMarkers),
                                            newMarkerId});
    showMarkerDock();
    statusBar()->showMessage(QStringLiteral("Added marker at row %1.").arg(logicalRow + 1), 2500);
}

void MainWindow::editMarker(const QString& markerId)
{
    TraceViewSession* session = activeTraceViewSession();
    const TraceMarker* marker = session ? markerById(*session, markerId) : nullptr;
    if (!marker) {
        return;
    }
    const TraceMarker original = *marker;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("Edit Marker"));
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    auto* labelRow = new QWidget(&dialog);
    auto* labelLayout = new QHBoxLayout(labelRow);
    labelLayout->setContentsMargins(0, 0, 0, 0);
    labelLayout->addWidget(new QLabel(QStringLiteral("Label"), labelRow));
    auto* labelEdit = new QLineEdit(original.label, labelRow);
    labelLayout->addWidget(labelEdit, 1);
    layout->addWidget(labelRow);

    auto* colorRow = new QWidget(&dialog);
    auto* colorLayout = new QHBoxLayout(colorRow);
    colorLayout->setContentsMargins(0, 0, 0, 0);
    colorLayout->addWidget(new QLabel(QStringLiteral("Color"), colorRow));
    auto* colorButton = new QPushButton(TraceMarkerColorName(marker->color), colorRow);
    colorButton->setObjectName(QStringLiteral("actionButton"));
    colorButton->setAutoFillBackground(true);
    QColor selectedColor = original.color.isValid() ? original.color : DefaultTraceMarkerColor(0);
    const auto updateColorButton = [&]() {
        colorButton->setText(TraceMarkerColorName(selectedColor));
        colorButton->setStyleSheet(QStringLiteral("QPushButton { background:%1; color:%2; }")
                                       .arg(TraceMarkerColorName(selectedColor),
                                            selectedColor.lightness() < 128 ? QStringLiteral("#FFFFFF")
                                                                           : QStringLiteral("#111827")));
    };
    updateColorButton();
    colorLayout->addWidget(colorButton);
    colorLayout->addStretch(1);
    layout->addWidget(colorRow);

    auto* memoEdit = new QPlainTextEdit(&dialog);
    memoEdit->setPlainText(original.memo);
    memoEdit->setMinimumHeight(120);
    layout->addWidget(memoEdit);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    connect(colorButton, &QPushButton::clicked, &dialog, [&]() {
        const QColor chosen = QColorDialog::getColor(selectedColor, &dialog, QStringLiteral("Marker Color"));
        if (chosen.isValid()) {
            selectedColor = chosen;
            updateColorButton();
        }
    });
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    session = activeTraceViewSession();
    marker = session ? markerById(*session, markerId) : nullptr;
    if (!session || !marker) {
        return;
    }

    TraceMarker updated = *marker;
    updated.label = labelEdit->text().trimmed();
    if (updated.label.isEmpty()) {
        updated.label = QStringLiteral("Marker %1").arg(updated.logicalRow + 1);
    }
    updated.color = selectedColor;
    updated.memo = memoEdit->toPlainText();
    updated.updatedAt = QDateTime::currentDateTimeUtc();
    updateMarker(std::move(updated));
}

void MainWindow::removeMarker(const QString& markerId)
{
    if (traceMarkerOverlay_) {
        traceMarkerOverlay_->cancelMarkerMove();
    }
    TraceViewSession* session = activeTraceViewSession();
    if (!session) {
        return;
    }
    if (session->applyingUnifiedUndoRedo) {
        return;
    }
    const TraceMarker* marker = markerById(*session, markerId);
    if (!marker) {
        return;
    }
    const std::vector<TraceMarker> beforeMarkers = session->markers;
    const QString beforeSelectedMarkerId = session->selectedMarkerId;
    const QString commandText = MarkerCommandText(QStringLiteral("Delete marker"), *marker);
    std::vector<TraceMarker> afterMarkers = beforeMarkers;
    afterMarkers.erase(std::remove_if(afterMarkers.begin(),
                                      afterMarkers.end(),
                                      [&](const TraceMarker& stored) {
                                          return stored.id == markerId;
                                      }),
                       afterMarkers.end());
    const QString afterSelectedMarkerId = beforeSelectedMarkerId == markerId
        ? QString()
        : beforeSelectedMarkerId;
    applyMarkerSnapshot(*session, afterMarkers, afterSelectedMarkerId);
    pushMarkerUndoCommand(*session,
                          MarkerUndoCommand{commandText,
                                            beforeMarkers,
                                            beforeSelectedMarkerId,
                                            std::move(afterMarkers),
                                            afterSelectedMarkerId});
    statusBar()->showMessage(QStringLiteral("Removed marker."), 2000);
}

bool MainWindow::moveMarker(const QString& markerId, const int targetLogicalRow)
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session || !flitModel_ || targetLogicalRow < 0 || targetLogicalRow >= flitModel_->totalCount()) {
        return false;
    }
    if (session->applyingUnifiedUndoRedo) {
        return false;
    }
    TraceMarker* marker = markerById(*session, markerId);
    if (!marker) {
        return false;
    }

    if (TraceMarker* occupied = markerForLogicalRow(*session, targetLogicalRow);
        occupied && occupied->id != marker->id) {
        statusBar()->showMessage(QStringLiteral("Target row already has a marker."), 2200);
        return false;
    }

    if (marker->logicalRow == targetLogicalRow) {
        session->selectedMarkerId = marker->id;
        refreshMarkerViews();
        statusBar()->showMessage(QStringLiteral("Marker is already on row %1.")
                                     .arg(targetLogicalRow + 1),
                                 1500);
        return true;
    }

    const std::vector<TraceMarker> beforeMarkers = session->markers;
    const QString beforeSelectedMarkerId = session->selectedMarkerId;
    const QString markerIdCopy = marker->id;
    const QString commandText = MarkerCommandText(QStringLiteral("Move marker"), *marker);
    std::vector<TraceMarker> afterMarkers = beforeMarkers;
    TraceMarker* movedMarker = nullptr;
    for (TraceMarker& candidate : afterMarkers) {
        if (candidate.id == markerIdCopy) {
            movedMarker = &candidate;
            break;
        }
    }
    if (!movedMarker) {
        return false;
    }
    const FlitRecord* record = flitModel_->recordForLogicalRow(targetLogicalRow);
    movedMarker->logicalRow = targetLogicalRow;
    movedMarker->timestamp = record ? record->timestamp : 0;
    movedMarker->updatedAt = QDateTime::currentDateTimeUtc();
    applyMarkerSnapshot(*session, afterMarkers, markerIdCopy);
    pushMarkerUndoCommand(*session,
                          MarkerUndoCommand{commandText,
                                            beforeMarkers,
                                            beforeSelectedMarkerId,
                                            std::move(afterMarkers),
                                            markerIdCopy});
    statusBar()->showMessage(QStringLiteral("Moved marker to row %1.").arg(targetLogicalRow + 1), 2200);
    return true;
}

void MainWindow::updateMarker(TraceMarker marker)
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session) {
        return;
    }
    if (session->applyingUnifiedUndoRedo) {
        return;
    }
    TraceMarker* stored = markerById(*session, marker.id);
    if (!stored) {
        return;
    }
    const std::vector<TraceMarker> beforeMarkers = session->markers;
    const QString beforeSelectedMarkerId = session->selectedMarkerId;
    const QString commandText = MarkerCommandText(QStringLiteral("Edit marker"), *stored);
    std::vector<TraceMarker> afterMarkers = beforeMarkers;
    const QString commandMarkerId = marker.id;
    TraceMarker* target = nullptr;
    for (TraceMarker& candidate : afterMarkers) {
        if (candidate.id == commandMarkerId) {
            target = &candidate;
            break;
        }
    }
    if (!target) {
        return;
    }
    marker.logicalRow = stored->logicalRow;
    marker.timestamp = stored->timestamp;
    if (!marker.color.isValid()) {
        marker.color = stored->color;
    }
    *target = std::move(marker);
    const QString afterSelectedMarkerId = commandMarkerId;
    applyMarkerSnapshot(*session, afterMarkers, afterSelectedMarkerId);
    pushMarkerUndoCommand(*session,
                          MarkerUndoCommand{commandText,
                                            beforeMarkers,
                                            beforeSelectedMarkerId,
                                            std::move(afterMarkers),
                                            afterSelectedMarkerId});
}

void MainWindow::selectMarker(const QString& markerId, const bool jumpToRow)
{
    TraceViewSession* session = activeTraceViewSession();
    const TraceMarker* marker = session ? markerById(*session, markerId) : nullptr;
    if (!marker) {
        return;
    }
    session->selectedMarkerId = marker->id;
    refreshMarkerViews();
    if (jumpToRow) {
        jumpToLogicalTraceRow(marker->logicalRow);
    }
}

void MainWindow::toggleMarkerSelection(const QString& markerId, const bool jumpToRow)
{
    TraceViewSession* session = activeTraceViewSession();
    const TraceMarker* marker = session ? markerById(*session, markerId) : nullptr;
    if (!marker) {
        return;
    }
    if (session->selectedMarkerId == marker->id) {
        session->selectedMarkerId.clear();
        refreshMarkerViews();
        statusBar()->showMessage(QStringLiteral("Marker unselected."), 1200);
        return;
    }
    selectMarker(marker->id, jumpToRow);
}

void MainWindow::navigateMarker(const bool forward)
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session || session->markers.empty()) {
        statusBar()->showMessage(QStringLiteral("No markers in the active session."), 1800);
        return;
    }

    std::vector<const TraceMarker*> orderedMarkers;
    orderedMarkers.reserve(session->markers.size());
    for (const TraceMarker& marker : session->markers) {
        orderedMarkers.push_back(&marker);
    }
    std::stable_sort(orderedMarkers.begin(),
                     orderedMarkers.end(),
                     [](const TraceMarker* lhs, const TraceMarker* rhs) {
                         return lhs->logicalRow < rhs->logicalRow;
                     });

    int selectedIndex = -1;
    for (int index = 0; index < static_cast<int>(orderedMarkers.size()); ++index) {
        if (orderedMarkers[static_cast<std::size_t>(index)]->id == session->selectedMarkerId) {
            selectedIndex = index;
            break;
        }
    }

    int nextIndex = 0;
    if (selectedIndex >= 0) {
        nextIndex = forward
            ? (selectedIndex + 1) % static_cast<int>(orderedMarkers.size())
            : (selectedIndex - 1 + static_cast<int>(orderedMarkers.size()))
                % static_cast<int>(orderedMarkers.size());
    } else {
        int currentLogicalRow = -1;
        if (flitView_ && flitModel_ && flitView_->selectionModel()) {
            const QModelIndex current = flitView_->selectionModel()->currentIndex();
            if (current.isValid()) {
                currentLogicalRow = flitModel_->logicalRowAt(current.row());
            }
        }
        if (forward) {
            const auto found = std::find_if(orderedMarkers.cbegin(),
                                            orderedMarkers.cend(),
                                            [currentLogicalRow](const TraceMarker* marker) {
                                                return marker->logicalRow > currentLogicalRow;
                                            });
            nextIndex = found == orderedMarkers.cend()
                ? 0
                : static_cast<int>(std::distance(orderedMarkers.cbegin(), found));
        } else {
            const auto found = std::find_if(orderedMarkers.crbegin(),
                                            orderedMarkers.crend(),
                                            [currentLogicalRow](const TraceMarker* marker) {
                                                return marker->logicalRow < currentLogicalRow;
                                            });
            nextIndex = found == orderedMarkers.crend()
                ? static_cast<int>(orderedMarkers.size()) - 1
                : static_cast<int>(orderedMarkers.size() - 1
                                   - std::distance(orderedMarkers.crbegin(), found));
        }
    }

    const TraceMarker* target = orderedMarkers[static_cast<std::size_t>(nextIndex)];
    selectMarker(target->id, true);
    statusBar()->showMessage(QStringLiteral("%1 marker: %2")
                                 .arg(forward ? QStringLiteral("Next") : QStringLiteral("Previous"),
                                      target->label.isEmpty()
                                          ? QStringLiteral("Marker %1").arg(target->logicalRow + 1)
                                          : target->label),
                             1500);
}

std::vector<TraceMarkerDisplaySummary> MainWindow::markerDisplaySummaries(const std::vector<TraceMarker>& markers) const
{
    std::vector<TraceMarkerDisplaySummary> summaries;
    summaries.reserve(markers.size());
    if (!flitModel_) {
        return summaries;
    }

    for (const TraceMarker& marker : markers) {
        const FlitRecord* record = flitModel_->recordForLogicalRow(marker.logicalRow);
        if (!record) {
            continue;
        }

        TraceMarkerDisplaySummary summary;
        summary.markerId = marker.id;
        summary.channel = ToString(record->channel);
        summary.opcode = record->opcode;
        if (record->channel == FlitChannel::Req || record->channel == FlitChannel::Snp) {
            summary.address = record->address;
        }
        summaries.push_back(std::move(summary));
    }
    return summaries;
}

void MainWindow::refreshMarkerViews()
{
    TraceViewSession* session = activeTraceViewSession();
    const std::vector<TraceMarker> markers = session ? session->markers : std::vector<TraceMarker>{};
    const QString selectedMarkerId = session ? session->selectedMarkerId : QString();
    const std::vector<TraceMarkerDisplaySummary> summaries = markerDisplaySummaries(markers);

    if (markerWidget_) {
        markerWidget_->setMarkers(markers,
                                  selectedMarkerId,
                                  session ? session->markerStickyState : MarkerStickyState{},
                                  summaries);
    }
    if (traceMarkerOverlay_) {
        traceMarkerOverlay_->setMarkers(markers, selectedMarkerId);
    }
    if (timelineWidget_) {
        timelineWidget_->setMarkers(markers, selectedMarkerId);
    }
    if (transactionWidget_) {
        transactionWidget_->setMarkers(markers, selectedMarkerId);
    }
}

void MainWindow::showMarkerDock()
{
    if (markerDock_) {
        markerDock_->toggleView(true);
        markerDock_->raise();
    }
}

void MainWindow::saveMarkers()
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session) {
        return;
    }
    const QString startPath = session->path.isEmpty()
        ? QDir::homePath()
        : QFileInfo(session->path).absolutePath();
    const QString path = QFileDialog::getSaveFileName(this,
                                                      QStringLiteral("Save Markers"),
                                                      startPath,
                                                      QStringLiteral("Marker files (*.markers.json);;JSON files (*.json);;All files (*)"));
    if (!path.isEmpty()) {
        saveMarkersToPath(path);
    }
}

bool MainWindow::saveMarkersToPath(const QString& path)
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session) {
        return false;
    }
    QString error;
    if (markerWidget_) {
        session->markerStickyState = markerWidget_->stickyState();
    }
    if (!SaveTraceMarkersToJson(path,
                                session->markers,
                                session->markerStickyState,
                                session->path,
                                session->label,
                                error)) {
        QMessageBox::warning(this,
                             QStringLiteral("Save Markers"),
                             error.isEmpty() ? QStringLiteral("Failed to save markers.") : error);
        return false;
    }
    statusBar()->showMessage(QStringLiteral("Saved %1 marker%2.")
                                 .arg(session->markers.size())
                                 .arg(session->markers.size() == 1 ? QString() : QStringLiteral("s")),
                             3000);
    return true;
}

void MainWindow::loadMarkers()
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session) {
        return;
    }
    const QString startPath = session->path.isEmpty()
        ? QDir::homePath()
        : QFileInfo(session->path).absolutePath();
    const QString path = QFileDialog::getOpenFileName(this,
                                                      QStringLiteral("Load Markers"),
                                                      startPath,
                                                      QStringLiteral("Marker files (*.markers.json);;JSON files (*.json);;All files (*)"));
    if (!path.isEmpty()) {
        loadMarkersFromPath(path);
    }
}

bool MainWindow::loadMarkersFromPath(const QString& path)
{
    TraceViewSession* session = activeTraceViewSession();
    if (!session || !flitModel_) {
        return false;
    }
    TraceMarkerLoadResult result;
    QString error;
    if (!LoadTraceMarkersFromJson(path, flitModel_->totalCount(), result, error)) {
        QMessageBox::warning(this,
                             QStringLiteral("Load Markers"),
                             error.isEmpty() ? QStringLiteral("Failed to load markers.") : error);
        return false;
    }
    session->markers = std::move(result.markers);
    session->markerStickyState = result.stickyState.value_or(MarkerStickyState{});
    session->selectedMarkerId = session->markers.empty() ? QString() : session->markers.front().id;
    clearMarkerUndoHistory(*session);
    refreshMarkerViews();
    showMarkerDock();
    statusBar()->showMessage(QStringLiteral("Loaded %1 marker%2%3.")
                                 .arg(session->markers.size())
                                 .arg(session->markers.size() == 1 ? QString() : QStringLiteral("s"))
                                 .arg(result.skippedInvalidRows > 0
                                          ? QStringLiteral("; skipped %1 invalid row%2")
                                                .arg(result.skippedInvalidRows)
                                                .arg(result.skippedInvalidRows == 1 ? QString() : QStringLiteral("s"))
                                          : QString()),
                             4000);
    return true;
}

}  // namespace CHIron::Gui
