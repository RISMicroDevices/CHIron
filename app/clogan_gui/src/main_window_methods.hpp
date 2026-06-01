    void buildUi();
    void wireSignals();
    void openTraceFile();
    bool loadTraceFile(const QString& filePath,
                       std::optional<CLog::Parameters> parameterOverride = std::nullopt,
                       quint64 replaceSessionId = 0,
                       bool showErrorDialog = true);
    void resetNoSessionState();
    void reloadTrace();
    bool reloadActiveTrace(bool showErrorDialog = true);
    void closeActiveTraceSession();
    void closeTraceSession(quint64 sessionId);
    void closeOtherTraceSessions(quint64 keepSessionId);
    void closeAllTraceSessions();
    void showSessionTabContextMenu(const QPoint& position);
    TraceViewSession* activeTraceViewSession() noexcept;
    const TraceViewSession* activeTraceViewSession() const noexcept;
    TraceViewSession* traceViewSessionById(quint64 sessionId) noexcept;
    const TraceViewSession* traceViewSessionById(quint64 sessionId) const noexcept;
    TraceViewSession* addTraceViewSession(const QString& sourceLabel,
                                          const QString& sourcePath,
                                          std::optional<CLog::Parameters> parameterOverride);
    void createSessionVisualizationWidgets(TraceViewSession& session);
    void destroySessionVisualizationWidgets(TraceViewSession& session);
    void populateSessionVisualizationWidgets(TraceViewSession& session);
    void activateSessionVisualizationWidgets(TraceViewSession* session);
    bool activateTraceSession(quint64 sessionId);
    bool activateTraceSessionAt(int index);
    void saveActiveSessionUiState();
    void bindActiveSessionUi();
    void refreshSessionTabs();
    void wireTraceModelSignals(FlitTableModel* model, FlitDetailsModel* detailModel);
    void wireTraceSelectionModel();
    void updateEditActions();
    void setActiveTraceReadOnly(bool readOnly);
    bool enableEditingForSession(TraceViewSession& session);
    bool materializeSessionRowsForEditing(TraceViewSession& session);
    bool saveEditedCopy(TraceViewSession& session);
    bool confirmDirtySessionAction(TraceViewSession& session, const QString& actionText);
    bool confirmAllDirtySessions(const QString& actionText);
    void markDerivedViewsOutdated(TraceViewSession& session);
    void setDerivedViewsOutdated(TraceViewSession& session, bool outdated);
    void rebuildDerivedViewsForActiveSession();
    void showFlitRowContextMenu(const QPoint& position);
    TraceMarker* markerById(TraceViewSession& session, const QString& markerId);
    const TraceMarker* markerById(const TraceViewSession& session, const QString& markerId) const;
    TraceMarker* markerForLogicalRow(TraceViewSession& session, int logicalRow);
    const TraceMarker* markerForLogicalRow(const TraceViewSession& session, int logicalRow) const;
    void recordUnifiedRoute(TraceViewSession& session, UnifiedUndoRouteKind kind, const QString& text);
    void pushMarkerUndoCommand(TraceViewSession& session, MarkerUndoCommand command);
    void applyMarkerSnapshot(TraceViewSession& session,
                             std::vector<TraceMarker> markers,
                             const QString& selectedMarkerId);
    void clearMarkerUndoHistory(TraceViewSession& session);
    bool dispatchNativeUndoRedoForFocus(bool redo) const;
    void undoUnifiedEdit();
    void redoUnifiedEdit();
    bool canUndoUnified() const;
    bool canRedoUnified() const;
    QString unifiedUndoText() const;
    QString unifiedRedoText() const;
    void addMarkerAtLogicalRow(int logicalRow);
    void editMarker(const QString& markerId);
    void removeMarker(const QString& markerId);
    bool moveMarker(const QString& markerId, int targetLogicalRow);
    void updateMarker(TraceMarker marker);
    void selectMarker(const QString& markerId, bool jumpToRow);
    void toggleMarkerSelection(const QString& markerId, bool jumpToRow);
    void navigateMarker(bool forward);
    std::vector<TraceMarkerDisplaySummary> markerDisplaySummaries(const std::vector<TraceMarker>& markers) const;
    void refreshMarkerViews();
    void showMarkerDock();
    void saveMarkers();
    bool saveMarkersToPath(const QString& path);
    void loadMarkers();
    bool loadMarkersFromPath(const QString& path);
    void bindClipboardWidgetToActiveScope();
    std::vector<ClipboardEntry>* clipboardEntriesForScope(ClipboardScope scope);
    const std::vector<ClipboardEntry>* clipboardEntriesForScope(ClipboardScope scope) const;
    bool startClipboardRowsInsertAsync(quint64 sourceSessionId,
                                       ClipboardScope scope,
                                       std::vector<std::pair<int, FlitRecord>> rows,
                                       ClipboardInsertOrdering ordering,
                                       int preSkippedDuplicates = 0);
    void insertSelectedFlitToClipboard(ClipboardScope scope);
    void insertSelectedXactionToClipboard(ClipboardScope scope);
    bool insertXactionsWithSelectedAddressToClipboard(ClipboardScope scope,
                                                      ClipboardXactionAddressInsertMode mode);
    void finishClipboardXactionAddressInsert(quint64 generation,
                                             quint64 sourceSessionId,
                                             ClipboardScope scope,
                                             std::vector<std::pair<int, FlitRecord>> rows,
                                             int preSkippedDuplicates,
                                             bool cancelled,
                                             QString errorText);
    void finishClipboardInsertPrepare(quint64 generation,
                                      quint64 sourceSessionId,
                                      std::vector<ClipboardEntry> preparedTarget,
                                      std::vector<ClipboardEntry> preparedAppended,
                                      bool appendOnly,
                                      bool mergeBySource,
                                      std::uint64_t preparedNextSequence,
                                      int inserted,
                                      int skipped,
                                      std::optional<CLogBTraceMetadata> preparedGlobalMetadata,
                                      QString errorText);
    void processClipboardInsertApplyChunk();
    void finishClipboardInsertApply();
    void cancelClipboardXactionAddressInsert();
    void cancelClipboardXactionAddressInsertForSession(quint64 sessionId);
    void updateClipboardInsertProgress(bool active,
                                       const QString& text = QString(),
                                       std::uint64_t completedRecords = 0,
                                       std::uint64_t totalRecords = 0);
    void showClipboardDock();
    void handleClipboardRowActivated(const ClipboardEntry& entry);
    void saveClipboard();
    bool saveClipboardAsCsv(const QString& path, const std::vector<ClipboardEntry>& entries);
    bool saveClipboardAsCLogB(const QString& path, const std::vector<ClipboardEntry>& entries);
    std::optional<CLogBTraceMetadata> metadataForClipboardCLogBSave(const std::vector<ClipboardEntry>& entries) const;
    void insertBlankFlitTemplate();
    void cloneSelectedFlitRows();
    void deleteSelectedFlitRow();
    void undoFlitTableEdit();
    void redoFlitTableEdit();
    QString uniqueSessionLabel(const QString& requestedLabel,
                               const TraceViewSession* ignoreSession = nullptr) const;
    void applyTraceSession(std::shared_ptr<TraceSession> session,
                           const QString& sourceLabel,
                           const QString& sourcePath = QString());
    void applyTraceRows(std::vector<FlitRecord> rows,
                        const QString& sourceLabel,
                        const QString& sourcePath = QString());
    void exportBugReport();
    void openDiagnosticsFolder();
    void showAboutDialog();
    void showTopologyDialog();
    void updateWindowCaption();
    void restoreDefaultLayout();
    void selectFirstRow();
    void updateMetrics();
    void updateInspector(const FlitRecord* record);
    void updateXactionRowsTable(const FlitRecord* record);
    void updateTimelineSelection();
    void updateAddressSelection();
    void updateCacheSelection();
    void updateTransactionSelection();
    void refreshLatencyView();
    void refreshLatencyDiffSessions();
    void showLatencyDiffWizard();
    void refreshTransactionView(TraceViewSession& session);
    void updateDerivedWidgetOutdatedTags();
    void updateTraceEmptyState();
    void refreshTopologyView();
    void updateTopologyAction();
    void updateXactionIndexAction();
    void refreshNodeLabelDelegates();
    void updateFilterProgress(bool active, const QString& text, int value, int maximum);
    void updateXactionIndexProgress(bool active,
                                    const QString& text = QString(),
                                    std::uint64_t completedRecords = 0,
                                    std::uint64_t totalRecords = 0);
    void placeTraceToolbarFoldButton(bool folded);
    void setTraceToolbarFolded(bool folded);
    void updateSearchNavigationButtons();
    void navigateSearchHighlight(bool forward);
    void resetStatisticsView(bool cancelActiveComputation = true);
    void startStatisticsComputation();
    void cancelStatisticsComputation();
    void cancelStatisticsComputationForSession(TraceViewSession& session);
    void startXactionIndexing(bool rebuildExisting = false);
    void rebuildXactionIndexing();
    void clearXactionIndex();
    void cancelXactionIndexing();
    void cancelXactionIndexingForSession(TraceViewSession& session, bool clearPartial);
    void refreshVisibleXactionIndexRows();
    void scheduleVisibleXactionIndexRowsRefresh();
    void finishXactionIndexing(std::shared_ptr<TraceSession> session,
                               quint64 sessionId,
                               quint64 generation,
                               bool succeeded,
                               bool cancelled,
                               CLogBTraceLoadError error);
    void processStatisticsComputation(quint64 sessionId, quint64 generation);
    void applyStatisticsResult(const TraceStatisticsResult& statistics);
    void refreshStatisticsLoadingLabel();
    void updateStatisticsLoadingProgress(const QString& text, int value, int maximum);
    void scheduleDiagnosticsSnapshotRefresh();
    void refreshDiagnosticsSnapshot();
    QString diagnosticsStateSnapshot() const;
    void applyFlitColumnWidths();
    void applyTraceTableRowHeight();
    void resizeFlitColumnForCurrentTrace(int logicalColumn);
    void showFieldToggleDialog();
    void rebuildFieldToggleDialog();
    void createOptionalFieldFastIndex(const QString& fieldName, bool rebuildExisting = false);
    void clearOptionalFieldFastIndex(const QString& fieldName);
    void refreshOptionalFieldIndexButtons(const QString& fieldName = QString());
    void rebuildOptionalFieldSearchRow();
    bool handleTableToolTip(QTableView* view, QEvent* event);
    bool isTableCellElided(QTableView* view, const QModelIndex& index) const;
    void showTableCellDialog(QTableView* view, const QModelIndex& index);
    void showXactionDebugDialog(int visibleRow, bool highlighted);
    void jumpToLogicalTraceRow(int logicalRow);
    QString inspectorHtml(const FlitRecord* record) const;
    QToolButton* makeToggle(const QString& label, bool checked = true) const;
