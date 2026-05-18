#include "cache_widget.hpp"
#include "main_window_internal.hpp"

namespace CHIron::Gui {
using namespace MainWindowDetail;

void MainWindow::buildUi()
{
    using ads::BottomDockWidgetArea;
    using ads::CDockAreaWidget;
    using ads::CDockManager;
    using ads::CDockWidget;
    using ads::CenterDockWidgetArea;
    using ads::LeftDockWidgetArea;
    using ads::RightDockWidgetArea;
    using ads::TopDockWidgetArea;

    setWindowTitle(QStringLiteral("CloganGUI"));
    resize(1860, 980);

    CDockManager::setConfigFlag(CDockManager::FocusHighlighting, true);
    CDockManager::setConfigFlag(CDockManager::EqualSplitOnInsertion, true);
    CDockManager::setConfigFlag(CDockManager::DockAreaDynamicTabsMenuButtonVisibility, true);
    dockManager_ = new CDockManager(this);
    dockManager_->setStyleSheet(
        dockManager_->styleSheet()
        + QStringLiteral(
            "ads--CDockContainerWidget > QSplitter {"
            "  padding: 0px;"
            "}"
            "ads--CDockContainerWidget ads--CDockSplitter::handle, "
            "ads--CDockSplitter::handle {"
            "  background: transparent;"
            "  background-color: transparent;"
            "  border: none;"
            "}"
            "ads--CDockContainerWidget ads--CDockSplitter::handle:horizontal, "
            "ads--CDockSplitter::handle:horizontal {"
            "  width: 5px;"
            "  border-left: 1px solid #E7ECF1;"
            "}"
            "ads--CDockContainerWidget ads--CDockSplitter::handle:vertical, "
            "ads--CDockSplitter::handle:vertical {"
            "  height: 5px;"
            "  border-top: 1px solid #E7ECF1;"
            "}"
            "ads--CDockContainerWidget ads--CDockSplitter::handle:hover, "
            "ads--CDockSplitter::handle:hover {"
            "  background: #F6F9FB;"
            "  background-color: #F6F9FB;"
            "}"));
    dockManager_->setDockWidgetToolBarStyle(Qt::ToolButtonIconOnly, CDockWidget::StateFloating);

    auto makeDockWidget = [this](const QString& title, QWidget* content, const bool forceNoScroll = false) {
        auto* dock = new ads::CDockWidget(dockManager_, title, this);
        dock->setWidget(content, forceNoScroll ? ads::CDockWidget::ForceNoScrollArea
                                               : ads::CDockWidget::AutoScrollArea);
        dock->setMinimumSizeHintMode(ads::CDockWidget::MinimumSizeHintFromContent);
        return dock;
    };

    totalBadge_ = new QLabel(this);
    visibleBadge_ = new QLabel(this);
    reqBadge_ = new QLabel(this);
    rspBadge_ = new QLabel(this);
    datBadge_ = new QLabel(this);
    snpBadge_ = new QLabel(this);
    editStateBadge_ = new QLabel(this);
    outdatedViewsBadge_ = new QLabel(this);

    flitView_ = new QTableView(dockManager_);
    flitView_->setObjectName(QStringLiteral("traceTable"));
    flitView_->setModel(flitModel_);
    flitView_->setSelectionBehavior(QAbstractItemView::SelectRows);
    flitView_->setSelectionMode(QAbstractItemView::SingleSelection);
    flitView_->setAlternatingRowColors(true);
    flitView_->setTextElideMode(Qt::ElideRight);
    flitView_->setContextMenuPolicy(Qt::CustomContextMenu);
    flitView_->verticalHeader()->hide();
    flitView_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    flitView_->verticalHeader()->setMinimumSectionSize(1);
    flitView_->verticalHeader()->setDefaultSectionSize(22);
    flitView_->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    flitView_->horizontalHeader()->setStretchLastSection(false);
    flitView_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    flitView_->horizontalHeader()->setResizeContentsPrecision(64);

    const QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    flitView_->setFont(mono);
    flitView_->setItemDelegateForColumn(FlitTableModel::XactionIndexColumn,
                                        new XactionIndexStateDelegate(flitView_));
    flitView_->setItemDelegateForColumn(FlitTableModel::ChannelColumn, new ChannelBadgeDelegate(flitView_));
    auto* nodeLabelDelegate = new NodeLabelDelegate(flitView_);
    flitView_->setItemDelegateForColumn(FlitTableModel::SourceColumn, nodeLabelDelegate);
    flitView_->setItemDelegateForColumn(FlitTableModel::TargetColumn, nodeLabelDelegate);
    flitView_->viewport()->installEventFilter(this);

    QWidget* tracePanel = new QWidget(dockManager_);
    QVBoxLayout* tracePanelLayout = new QVBoxLayout(tracePanel);
    tracePanelLayout->setContentsMargins(0, 0, 0, 0);
    tracePanelLayout->setSpacing(0);

    QFrame* traceToolbar = new QFrame(tracePanel);
    traceToolbar_ = traceToolbar;
    traceToolbar->setObjectName(QStringLiteral("traceToolbar"));
    QVBoxLayout* traceToolbarLayout = new QVBoxLayout(traceToolbar);
    traceToolbarLayout->setContentsMargins(8, 4, 8, 6);
    traceToolbarLayout->setSpacing(4);

    traceToolbarContent_ = new QWidget(traceToolbar);
    traceToolbarContent_->setObjectName(QStringLiteral("traceToolbarContent"));
    QVBoxLayout* traceToolbarContentLayout = new QVBoxLayout(traceToolbarContent_);
    traceToolbarContentLayout->setContentsMargins(0, 0, 0, 0);
    traceToolbarContentLayout->setSpacing(4);
    traceToolbarLayout->addWidget(traceToolbarContent_);

    auto addToolbarRow = [this,
                          traceToolbarContent = traceToolbarContent_,
                          traceToolbarContentLayout](const QString& title,
                                                     const bool hideWhenFolded = true) -> QHBoxLayout* {
        QWidget* row = new QWidget(traceToolbarContent);
        row->setObjectName(QStringLiteral("traceToolbarRow"));
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(5);

        QLabel* sectionLabel = new QLabel(title, row);
        sectionLabel->setObjectName(QStringLiteral("sectionLabel"));
        sectionLabel->setMinimumWidth(50);
        rowLayout->addWidget(sectionLabel);
        traceToolbarContentLayout->addWidget(row);
        if (hideWhenFolded) {
            traceToolbarFoldableRows_.append(row);
        } else {
            traceToolbarFoldableSearchWidgets_.append(sectionLabel);
        }
        return rowLayout;
    };

    struct ToolbarGroup {
        QFrame* frame = nullptr;
        QHBoxLayout* layout = nullptr;
    };

    auto addToolbarGroup = [traceToolbar](QHBoxLayout* rowLayout, const QString& caption) -> ToolbarGroup {
        ToolbarGroup group;
        group.frame = new QFrame(traceToolbar);
        group.frame->setObjectName(QStringLiteral("traceToolbarGroup"));
        group.layout = new QHBoxLayout(group.frame);
        group.layout->setContentsMargins(8, 4, 8, 4);
        group.layout->setSpacing(4);

        if (!caption.isEmpty()) {
            QLabel* captionLabel = new QLabel(caption, group.frame);
            captionLabel->setObjectName(QStringLiteral("groupCaption"));
            group.layout->addWidget(captionLabel);
        }

        rowLayout->addWidget(group.frame);
        return group;
    };

    auto makeSecondaryLabel = [](QWidget* parent, const QString& text) {
        QLabel* label = new QLabel(text, parent);
        label->setObjectName(QStringLiteral("secondaryLabel"));
        return label;
    };

    auto setHint = [](QWidget* widget, const QString& text) {
        widget->setToolTip(text);
        widget->setStatusTip(text);
    };

    auto addFilterField = [makeSecondaryLabel](QWidget* parent,
                                               QHBoxLayout* rowLayout,
                                               const QString& label,
                                               const QString& placeholder,
                                               const int width) {
        rowLayout->addWidget(makeSecondaryLabel(parent, label));

        QLineEdit* edit = new QLineEdit(parent);
        edit->setPlaceholderText(placeholder);
        edit->setClearButtonEnabled(true);
        edit->setMinimumWidth(width);
        edit->setMaximumWidth(width);
        edit->setFixedHeight(22);
        rowLayout->addWidget(edit);
        return edit;
    };

    auto addToggleGroup = [](QHBoxLayout* rowLayout, std::initializer_list<QToolButton*> buttons) {
        for (QToolButton* button : buttons) {
            rowLayout->addWidget(button);
        }
    };

    reqButton_ = makeToggle(QStringLiteral("REQ"));
    rspButton_ = makeToggle(QStringLiteral("RSP"));
    datButton_ = makeToggle(QStringLiteral("DAT"));
    snpButton_ = makeToggle(QStringLiteral("SNP"));
    txButton_ = makeToggle(QStringLiteral("TX"));
    rxButton_ = makeToggle(QStringLiteral("RX"));
    deniedFlitsButton_ = makeToggle(QStringLiteral("Denied"), false);
    deniedFlitsButton_->setEnabled(false);
    nodeLabelsButton_ = makeToggle(QStringLiteral("Node Labels"));

    QHBoxLayout* summaryRowLayout = addToolbarRow(QStringLiteral("Summary"), false);
    traceToolbarSummaryLayout_ = summaryRowLayout;
    const ToolbarGroup summaryGroup = addToolbarGroup(summaryRowLayout, QStringLiteral("Trace"));
    for (QLabel* badge : {totalBadge_, visibleBadge_, reqBadge_, rspBadge_, datBadge_, snpBadge_, editStateBadge_, outdatedViewsBadge_}) {
        badge->setObjectName(QStringLiteral("metricBadge"));
        summaryGroup.layout->addWidget(badge);
    }
    editStateBadge_->hide();
    outdatedViewsBadge_->hide();
    outdatedViewsBadge_->setObjectName(QStringLiteral("outdatedViewsBadge"));
    outdatedViewsBadge_->setStyleSheet(QStringLiteral(
        "QLabel#outdatedViewsBadge {"
        "  padding: 2px 6px;"
        "  min-height: 18px;"
        "  color: #B42318;"
        "  background: #FEE4E2;"
        "  border: 1px solid #FDA29B;"
        "  border-radius: 3px;"
        "  font-weight: 700;"
        "}"));
    setHint(totalBadge_, QStringLiteral("Total number of flits loaded in the trace."));
    setHint(visibleBadge_, QStringLiteral("Number of flits currently visible after filtering."));
    setHint(reqBadge_, QStringLiteral("Total REQ flits present in the loaded trace."));
    setHint(rspBadge_, QStringLiteral("Total RSP flits present in the loaded trace."));
    setHint(datBadge_, QStringLiteral("Total DAT flits present in the loaded trace."));
    setHint(snpBadge_, QStringLiteral("Total SNP flits present in the loaded trace."));
    setHint(editStateBadge_, QStringLiteral("Current staged trace edit state."));
    setHint(outdatedViewsBadge_, QStringLiteral("Derived views are based on stale indexes or caches."));
    summaryRowLayout->addStretch(1);

    topologyAction_ = new QAction(QStringLiteral("Topology"), this);
    topologyAction_->setToolTip(QStringLiteral("Display CHI topology information stored in the current CLog.B file."));
    topologyAction_->setStatusTip(topologyAction_->toolTip());
    topologyAction_->setEnabled(false);
    connect(topologyAction_, &QAction::triggered, this, &MainWindow::showTopologyDialog);

    QToolButton* topologyButton = new QToolButton(traceToolbar);
    topologyButton->setDefaultAction(topologyAction_);
    topologyButton->setObjectName(QStringLiteral("actionButton"));
    topologyButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    topologyButton->setFixedHeight(22);
    summaryRowLayout->addWidget(topologyButton);

    xactionIndexAction_ = new QAction(QStringLiteral("Build Xaction Index"), this);
    xactionIndexAction_->setToolTip(QStringLiteral("Build Xaction indexing for transactional highlighting."));
    xactionIndexAction_->setStatusTip(xactionIndexAction_->toolTip());
    xactionIndexAction_->setEnabled(false);
    connect(xactionIndexAction_, &QAction::triggered, this, [this]() {
        startXactionIndexing(currentTraceSession_ && currentTraceSession_->isXactionIndexComplete());
    });

    rebuildXactionIndexAction_ = new QAction(QStringLiteral("Rebuild Xaction Index"), this);
    rebuildXactionIndexAction_->setToolTip(QStringLiteral("Delete and rebuild the Xaction index for this trace."));
    rebuildXactionIndexAction_->setStatusTip(rebuildXactionIndexAction_->toolTip());
    rebuildXactionIndexAction_->setEnabled(false);
    connect(rebuildXactionIndexAction_, &QAction::triggered, this, &MainWindow::rebuildXactionIndexing);

    clearXactionIndexAction_ = new QAction(QStringLiteral("Clear Xaction Index"), this);
    clearXactionIndexAction_->setToolTip(QStringLiteral("Delete the persisted Xaction index for this trace."));
    clearXactionIndexAction_->setStatusTip(clearXactionIndexAction_->toolTip());
    clearXactionIndexAction_->setEnabled(false);
    connect(clearXactionIndexAction_, &QAction::triggered, this, &MainWindow::clearXactionIndex);

    QToolButton* xactionIndexButton = new QToolButton(traceToolbar);
    QMenu* xactionIndexMenu = new QMenu(xactionIndexButton);
    xactionIndexMenu->addAction(xactionIndexAction_);
    xactionIndexMenu->addAction(rebuildXactionIndexAction_);
    xactionIndexMenu->addAction(clearXactionIndexAction_);
    xactionIndexButton->setDefaultAction(xactionIndexAction_);
    xactionIndexButton->setMenu(xactionIndexMenu);
    xactionIndexButton->setPopupMode(QToolButton::MenuButtonPopup);
    xactionIndexButton->setObjectName(QStringLiteral("actionButton"));
    xactionIndexButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    xactionIndexButton->setFixedHeight(22);
    summaryRowLayout->addWidget(xactionIndexButton);

    readOnlyModeAction_ = new QAction(QStringLiteral("Read-only"), this);
    readOnlyModeAction_->setCheckable(true);
    readOnlyModeAction_->setChecked(true);
    readOnlyModeAction_->setToolTip(QStringLiteral("Keep the active trace protected from staged table edits."));
    readOnlyModeAction_->setStatusTip(readOnlyModeAction_->toolTip());
    connect(readOnlyModeAction_, &QAction::toggled, this, [this](const bool checked) {
        setActiveTraceReadOnly(checked);
    });

    QToolButton* editModeButton = new QToolButton(traceToolbar);
    editModeButton->setDefaultAction(readOnlyModeAction_);
    editModeButton->setObjectName(QStringLiteral("actionButton"));
    editModeButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    editModeButton->setFixedHeight(22);
    summaryRowLayout->addWidget(editModeButton);

    QPushButton* openButton = new QPushButton(QStringLiteral("Open CLog.B..."), traceToolbar);
    openButton->setObjectName(QStringLiteral("actionButton"));
    openButton->setFixedHeight(22);
    setHint(openButton, QStringLiteral("Open a CLog.B trace file and load its decoded flits into the viewer."));
    summaryRowLayout->addWidget(openButton);

    QPushButton* reloadButton = new QPushButton(QStringLiteral("Reload"), traceToolbar);
    reloadButton->setObjectName(QStringLiteral("actionButton"));
    reloadButton->setFixedHeight(22);
    setHint(reloadButton, QStringLiteral("Reload the current trace source. When no file is loaded, this opens a trace file."));
    summaryRowLayout->addWidget(reloadButton);

    QHBoxLayout* filtersRowLayout = addToolbarRow(QStringLiteral("Filters"));
    const ToolbarGroup channelGroup = addToolbarGroup(filtersRowLayout, QStringLiteral("Channel"));
    addToggleGroup(channelGroup.layout, {reqButton_, rspButton_, datButton_, snpButton_});
    const ToolbarGroup directionGroup = addToolbarGroup(filtersRowLayout, QStringLiteral("Direction"));
    addToggleGroup(directionGroup.layout, {txButton_, rxButton_});
    const ToolbarGroup xactionFilterGroup = addToolbarGroup(filtersRowLayout, QStringLiteral("Xaction"));
    xactionFilterGroup.layout->addWidget(deniedFlitsButton_);
    filtersRowLayout->addStretch(1);

    QHBoxLayout* searchRowLayout = addToolbarRow(QStringLiteral("Search"), false);
    traceToolbarSearchLayout_ = searchRowLayout;
    const ToolbarGroup searchModeGroup = addToolbarGroup(searchRowLayout, QStringLiteral("Mode"));
    traceToolbarFoldableSearchWidgets_.append(searchModeGroup.frame);
    searchModeButton_ = makeToggle(QStringLiteral("Filter"), false);
    searchModeButton_->setMinimumWidth(92);
    searchModeGroup.layout->addWidget(searchModeButton_);
    searchPreviousButton_ = new QToolButton(traceToolbar);
    searchPreviousButton_->setObjectName(QStringLiteral("filterToggle"));
    searchPreviousButton_->setArrowType(Qt::UpArrow);
    searchPreviousButton_->setFixedSize(20, 22);
    searchPreviousButton_->hide();
    searchModeGroup.layout->addWidget(searchPreviousButton_);
    searchNextButton_ = new QToolButton(traceToolbar);
    searchNextButton_->setObjectName(QStringLiteral("filterToggle"));
    searchNextButton_->setArrowType(Qt::DownArrow);
    searchNextButton_->setFixedSize(20, 22);
    searchNextButton_->hide();
    searchModeGroup.layout->addWidget(searchNextButton_);
    searchNavigationLabel_ = new QLabel(QStringLiteral("0/0"), traceToolbar);
    searchNavigationLabel_->setObjectName(QStringLiteral("secondaryLabel"));
    searchNavigationLabel_->setMinimumWidth(48);
    searchNavigationLabel_->setAlignment(Qt::AlignCenter);
    searchNavigationLabel_->hide();
    searchModeGroup.layout->addWidget(searchNavigationLabel_);
    const ToolbarGroup searchGroup = addToolbarGroup(searchRowLayout, QStringLiteral("Fields"));
    traceToolbarFoldableSearchWidgets_.append(searchGroup.frame);
    opcodeFilterEdit_ = addFilterField(searchGroup.frame,
                                       searchGroup.layout,
                                       QStringLiteral("Op"),
                                       QStringLiteral("Opcode"),
                                       96);
    sourceIdFilterEdit_ = addFilterField(searchGroup.frame,
                                         searchGroup.layout,
                                         QStringLiteral("Src"),
                                         QStringLiteral("SrcID"),
                                         60);
    targetIdFilterEdit_ = addFilterField(searchGroup.frame,
                                         searchGroup.layout,
                                         QStringLiteral("Tgt"),
                                         QStringLiteral("TgtID"),
                                         60);
    txnIdFilterEdit_ = addFilterField(searchGroup.frame,
                                      searchGroup.layout,
                                      QStringLiteral("Txn"),
                                      QStringLiteral("TxnID"),
                                      60);
    dbidFilterEdit_ = addFilterField(searchGroup.frame,
                                     searchGroup.layout,
                                     QStringLiteral("DB"),
                                     QStringLiteral("DBID"),
                                     60);
    addressFilterEdit_ = addFilterField(searchGroup.frame,
                                        searchGroup.layout,
                                        QStringLiteral("Addr"),
                                        QStringLiteral("Address"),
                                        96);
    setHint(reqButton_, QStringLiteral("Show or hide Request channel flits."));
    setHint(rspButton_, QStringLiteral("Show or hide Response channel flits."));
    setHint(datButton_, QStringLiteral("Show or hide Data channel flits."));
    setHint(snpButton_, QStringLiteral("Show or hide Snoop channel flits."));
    setHint(txButton_, QStringLiteral("Show or hide transmitted flits."));
    setHint(rxButton_, QStringLiteral("Show or hide received flits."));
    setHint(deniedFlitsButton_,
            QStringLiteral("Show only flits denied by the completed Xaction index."));
    setHint(searchModeButton_,
            QStringLiteral("Search fields filter visible flits. Toggle to highlight matches without hiding non-matching flits."));
    setHint(searchPreviousButton_, QStringLiteral("Jump to the previous highlighted flit. Shortcut: Ctrl+Up."));
    setHint(searchNextButton_, QStringLiteral("Jump to the next highlighted flit. Shortcut: Ctrl+Down."));
    setHint(searchNavigationLabel_, QStringLiteral("Current highlighted flit and total highlighted flits."));
    const QStringList opcodeNames = CLogBTraceLoader::supportedOpcodeNames();
    if (!opcodeNames.isEmpty()) {
        QCompleter* opcodeCompleter = new QCompleter(opcodeNames, opcodeFilterEdit_);
        opcodeCompleter->setCaseSensitivity(Qt::CaseInsensitive);
        opcodeCompleter->setFilterMode(Qt::MatchContains);
        opcodeCompleter->setCompletionMode(QCompleter::PopupCompletion);
        opcodeFilterEdit_->setCompleter(opcodeCompleter);

        const int previewCount = qMin(8, opcodeNames.size());
        QStringList previewNames;
        for (int index = 0; index < previewCount; ++index) {
            previewNames.append(opcodeNames[index]);
        }
        const QString previewSuffix = opcodeNames.size() > previewCount
            ? QStringLiteral(", ...")
            : QString();
        opcodeFilterEdit_->setPlaceholderText(
            QStringLiteral("Opcode / e.g. %1").arg(opcodeNames.front()));
        setHint(opcodeFilterEdit_,
                QStringLiteral(
                    "Filter rows by opcode name or numeric value. Supports decimal or 0x-prefixed hex.\n"
                    "Known opcode names are suggested as you type, for example: %1%2")
                    .arg(previewNames.join(QStringLiteral(", ")), previewSuffix));
    } else {
        setHint(opcodeFilterEdit_, QStringLiteral("Filter rows by opcode name or numeric value. Supports decimal or 0x-prefixed hex."));
    }
    setHint(sourceIdFilterEdit_, QStringLiteral("Filter rows by source node ID. Supports decimal or 0x-prefixed hex."));
    setHint(targetIdFilterEdit_, QStringLiteral("Filter rows by target node ID. Supports decimal or 0x-prefixed hex."));
    setHint(txnIdFilterEdit_, QStringLiteral("Filter rows by transaction ID. Supports decimal or 0x-prefixed hex."));
    setHint(dbidFilterEdit_, QStringLiteral("Filter rows by DBID value. Supports decimal or 0x-prefixed hex."));
    setHint(addressFilterEdit_, QStringLiteral("Filter rows by address text. Supports decimal or 0x-prefixed hex."));
    setHint(nodeLabelsButton_,
            QStringLiteral("Show or hide topology node type labels beside node ID fields."));
    searchRowLayout->addWidget(nodeLabelsButton_);
    traceToolbarFoldableSearchWidgets_.append(nodeLabelsButton_);
    fieldToggleButton_ = new QToolButton(traceToolbar);
    fieldToggleButton_->setText(QStringLiteral("CHI Fields"));
    fieldToggleButton_->setObjectName(QStringLiteral("actionButton"));
    fieldToggleButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    fieldToggleButton_->setFixedHeight(22);
    setHint(fieldToggleButton_,
            QStringLiteral("Select additional CHI flit fields to display as Flits columns and search filters."));
    searchRowLayout->addWidget(fieldToggleButton_);
    traceToolbarFoldableSearchWidgets_.append(fieldToggleButton_);
    setHint(flitView_->horizontalHeader(),
            QStringLiteral("Right-click a Flits column title to switch decoded, decimal, or hexadecimal display."));
    searchRowLayout->addStretch(1);

    fieldToggleDialog_ = new QDialog(this, Qt::Tool);
    fieldToggleDialog_->setWindowTitle(QStringLiteral("CHI Fields"));
    fieldToggleDialog_->resize(720, 360);

    QVBoxLayout* fieldToggleDialogLayout = new QVBoxLayout(fieldToggleDialog_);
    fieldToggleDialogLayout->setContentsMargins(10, 10, 10, 10);
    fieldToggleDialogLayout->setSpacing(8);

    QLabel* fieldToggleDialogLabel = new QLabel(
        QStringLiteral("Enable additional CHI flit fields as Flits columns and search inputs. Create indexes for fields that need faster filtering."),
        fieldToggleDialog_);
    fieldToggleDialogLabel->setObjectName(QStringLiteral("secondaryLabel"));
    fieldToggleDialogLabel->setWordWrap(true);
    fieldToggleDialogLayout->addWidget(fieldToggleDialogLabel);

    fieldToggleScrollArea_ = new QScrollArea(fieldToggleDialog_);
    fieldToggleScrollArea_->setWidgetResizable(true);
    fieldToggleScrollArea_->setFrameShape(QFrame::NoFrame);
    fieldToggleDialogLayout->addWidget(fieldToggleScrollArea_, 1);

    fieldToggleContent_ = new QWidget(fieldToggleScrollArea_);
    fieldToggleContentLayout_ = new QVBoxLayout(fieldToggleContent_);
    fieldToggleContentLayout_->setContentsMargins(0, 0, 0, 0);
    fieldToggleContentLayout_->setSpacing(8);
    fieldToggleScrollArea_->setWidget(fieldToggleContent_);

    QHBoxLayout* fieldToggleActionsLayout = new QHBoxLayout();
    fieldToggleActionsLayout->setContentsMargins(0, 0, 0, 0);
    fieldToggleActionsLayout->addStretch(1);
    fieldToggleClearButton_ = new QPushButton(QStringLiteral("Clear"), fieldToggleDialog_);
    fieldToggleClearButton_->setObjectName(QStringLiteral("actionButton"));
    fieldToggleClearButton_->setFixedHeight(22);
    fieldToggleActionsLayout->addWidget(fieldToggleClearButton_);
    fieldToggleDialogLayout->addLayout(fieldToggleActionsLayout);

    optionalFieldSearchRow_ = new QWidget(traceToolbarContent_);
    optionalFieldSearchRow_->setObjectName(QStringLiteral("traceToolbarRow"));
    optionalFieldSearchRow_->installEventFilter(this);
    optionalFieldSearchLayout_ = new QVBoxLayout(optionalFieldSearchRow_);
    optionalFieldSearchLayout_->setContentsMargins(0, 0, 0, 0);
    optionalFieldSearchLayout_->setSpacing(4);
    optionalFieldSearchRow_->hide();
    traceToolbarContentLayout->addWidget(optionalFieldSearchRow_);
    traceToolbarFoldButton_ = new QToolButton(traceToolbarContent_);
    traceToolbarFoldButton_->setObjectName(QStringLiteral("traceToolbarFoldButton"));
    traceToolbarFoldButton_->setCheckable(true);
    traceToolbarFoldButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    traceToolbarFoldButton_->setArrowType(Qt::UpArrow);
    traceToolbarFoldButton_->setFixedSize(22, 22);
    traceToolbarFoldButton_->setToolTip(QStringLiteral("Fold the Flits toolbar."));
    traceToolbarFoldButton_->setStatusTip(traceToolbarFoldButton_->toolTip());
    searchRowLayout->addWidget(traceToolbarFoldButton_);

    tracePanelLayout->addWidget(traceToolbar);

    sessionTabBar_ = new QTabBar(tracePanel);
    sessionTabBar_->setObjectName(QStringLiteral("sessionTabBar"));
    sessionTabBar_->setExpanding(false);
    sessionTabBar_->setMovable(false);
    sessionTabBar_->setDocumentMode(true);
    sessionTabBar_->setUsesScrollButtons(true);
    sessionTabBar_->hide();
    tracePanelLayout->addWidget(sessionTabBar_);

    QWidget* traceContentHost = new QWidget(tracePanel);
    traceContentStack_ = new QStackedLayout(traceContentHost);
    traceContentStack_->setContentsMargins(0, 0, 0, 0);
    traceContentStack_->setStackingMode(QStackedLayout::StackOne);

    traceEmptyState_ = new QWidget(traceContentHost);
    QVBoxLayout* traceEmptyStateLayout = new QVBoxLayout(traceEmptyState_);
    traceEmptyStateLayout->setContentsMargins(36, 36, 36, 36);
    traceEmptyStateLayout->setSpacing(10);
    traceEmptyStateLayout->addStretch(1);

    QLabel* traceEmptyStateCaption = new QLabel(
        QStringLiteral("Open a CHI trace to populate the packet browser."),
        traceEmptyState_);
    traceEmptyStateCaption->setAlignment(Qt::AlignCenter);
    traceEmptyStateCaption->setStyleSheet(
        QStringLiteral("font-size:14px; font-weight:600; color:#314252;"));
    traceEmptyStateLayout->addWidget(traceEmptyStateCaption, 0, Qt::AlignHCenter);

    traceEmptyStateButton_ = new QPushButton(
        QStringLiteral("Click to open a clog/cldb file ..."),
        traceEmptyState_);
    traceEmptyStateButton_->setCursor(Qt::PointingHandCursor);
    traceEmptyStateButton_->setFlat(true);
    traceEmptyStateButton_->setStyleSheet(
        QStringLiteral("QPushButton { color:#225C8C; background:transparent; border:0; "
                       "font-size:15px; font-weight:700; text-decoration:underline; padding:6px 10px; }"
                       "QPushButton:hover { color:#1A4A71; }"
                       "QPushButton:pressed { color:#143857; }"));
    traceEmptyStateLayout->addWidget(traceEmptyStateButton_, 0, Qt::AlignHCenter);

    QLabel* traceEmptyStateHint = new QLabel(
        QStringLiteral("Supported extensions include .clog, .cldb, .clogb, and .clog.b."),
        traceEmptyState_);
    traceEmptyStateHint->setAlignment(Qt::AlignCenter);
    traceEmptyStateHint->setObjectName(QStringLiteral("secondaryLabel"));
    traceEmptyStateLayout->addWidget(traceEmptyStateHint, 0, Qt::AlignHCenter);
    traceEmptyStateLayout->addStretch(1);

    traceContentStack_->addWidget(traceEmptyState_);
    traceContentStack_->addWidget(flitView_);
    tracePanelLayout->addWidget(traceContentHost, 1);

    detailView_ = new QTableView(dockManager_);
    detailView_->setObjectName(QStringLiteral("fieldTable"));
    detailView_->setModel(detailModel_);
    detailView_->setAlternatingRowColors(true);
    detailView_->verticalHeader()->hide();
    detailView_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    detailView_->verticalHeader()->setMinimumSectionSize(18);
    detailView_->verticalHeader()->setDefaultSectionSize(22);
    detailView_->horizontalHeader()->setStretchLastSection(false);
    detailView_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    detailView_->setFont(mono);
    detailView_->setMinimumWidth(820);
    detailView_->viewport()->installEventFilter(this);

    inspector_ = new QTextBrowser(dockManager_);
    inspector_->setObjectName(QStringLiteral("detailBrowser"));
    inspector_->setOpenExternalLinks(false);
    inspector_->setMinimumWidth(220);
    inspector_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);

    QWidget* inspectorPanel = new QWidget(dockManager_);
    inspectorPanel->setMinimumWidth(220);
    inspectorPanel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    QVBoxLayout* inspectorPanelLayout = new QVBoxLayout(inspectorPanel);
    inspectorPanelLayout->setContentsMargins(0, 0, 0, 0);
    inspectorPanelLayout->setSpacing(0);

    QSplitter* inspectorSplitter = new QSplitter(Qt::Vertical, inspectorPanel);
    inspectorSplitter->setChildrenCollapsible(false);
    inspectorPanelLayout->addWidget(inspectorSplitter, 1);
    inspectorSplitter->addWidget(inspector_);

    QWidget* xactionRowsPanel = new QWidget(inspectorSplitter);
    QVBoxLayout* xactionRowsLayout = new QVBoxLayout(xactionRowsPanel);
    xactionRowsLayout->setContentsMargins(0, 4, 0, 0);
    xactionRowsLayout->setSpacing(4);

    xactionRowsCaption_ = new QLabel(QStringLiteral("Xaction Rows"), xactionRowsPanel);
    xactionRowsCaption_->setObjectName(QStringLiteral("secondaryLabel"));
    xactionRowsCaption_->setStyleSheet(QStringLiteral("font-weight:700; color:#314252; padding-left:4px;"));
    xactionRowsLayout->addWidget(xactionRowsCaption_);

    xactionRowsTable_ = new QTableWidget(0, 7, xactionRowsPanel);
    xactionRowsTable_->setObjectName(QStringLiteral("xactionRowsTable"));
    xactionRowsTable_->setHorizontalHeaderLabels({QStringLiteral("Time"),
                                                  QStringLiteral("Channel"),
                                                  QStringLiteral("Opcode"),
                                                  QStringLiteral("SrcID"),
                                                  QStringLiteral("TgtID"),
                                                  QStringLiteral("TxnID"),
                                                  QStringLiteral("DBID")});
    xactionRowsTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    xactionRowsTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    xactionRowsTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    xactionRowsTable_->setAlternatingRowColors(true);
    xactionRowsTable_->setMinimumWidth(160);
    xactionRowsTable_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Expanding);
    xactionRowsTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    xactionRowsTable_->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
    xactionRowsTable_->verticalHeader()->hide();
    xactionRowsTable_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    xactionRowsTable_->verticalHeader()->setMinimumSectionSize(1);
    xactionRowsTable_->verticalHeader()->setDefaultSectionSize(22);
    xactionRowsTable_->horizontalHeader()->setStretchLastSection(false);
    xactionRowsTable_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    xactionRowsTable_->horizontalHeader()->setMinimumSectionSize(24);
    xactionRowsTable_->setItemDelegateForColumn(1, new ChannelBadgeDelegate(xactionRowsTable_));
    auto* xactionNodeLabelDelegate = new NodeLabelDelegate(xactionRowsTable_);
    xactionRowsTable_->setItemDelegateForColumn(3, xactionNodeLabelDelegate);
    xactionRowsTable_->setItemDelegateForColumn(4, xactionNodeLabelDelegate);
    xactionRowsTable_->setColumnWidth(0, 104);
    xactionRowsTable_->setColumnWidth(1, 96);
    xactionRowsTable_->setColumnWidth(2, 132);
    xactionRowsTable_->setColumnWidth(3, 168);
    xactionRowsTable_->setColumnWidth(4, 168);
    xactionRowsTable_->setColumnWidth(5, 72);
    xactionRowsTable_->setColumnWidth(6, 72);
    xactionRowsTable_->setToolTip(QStringLiteral("Double-click an Xaction row to jump to it in the main trace table."));
    xactionRowsLayout->addWidget(xactionRowsTable_, 1);
    inspectorSplitter->addWidget(xactionRowsPanel);
    inspectorSplitter->setStretchFactor(0, 3);
    inspectorSplitter->setStretchFactor(1, 1);
    inspectorSplitter->setSizes({420, 150});

    QWidget* statisticsPanel = new QWidget(dockManager_);
    QVBoxLayout* statisticsPanelLayout = new QVBoxLayout(statisticsPanel);
    statisticsPanelLayout->setContentsMargins(10, 10, 10, 10);
    statisticsPanelLayout->setSpacing(8);

    QWidget* statisticsContentHost = new QWidget(statisticsPanel);
    statisticsStack_ = new QStackedLayout(statisticsContentHost);
    statisticsStack_->setContentsMargins(0, 0, 0, 0);
    statisticsStack_->setStackingMode(QStackedLayout::StackOne);

    statisticsIdleState_ = new QWidget(statisticsContentHost);
    QVBoxLayout* statisticsIdleLayout = new QVBoxLayout(statisticsIdleState_);
    statisticsIdleLayout->setContentsMargins(28, 28, 28, 28);
    statisticsIdleLayout->setSpacing(10);
    statisticsIdleLayout->addStretch(1);

    QLabel* statisticsIdleCaption = new QLabel(QStringLiteral("Trace Statistics"), statisticsIdleState_);
    statisticsIdleCaption->setAlignment(Qt::AlignCenter);
    statisticsIdleCaption->setStyleSheet(QStringLiteral("font-size:15px; font-weight:700; color:#314252;"));
    statisticsIdleLayout->addWidget(statisticsIdleCaption, 0, Qt::AlignHCenter);

    statisticsHintLabel_ = new QLabel(statisticsIdleState_);
    statisticsHintLabel_->setAlignment(Qt::AlignCenter);
    statisticsHintLabel_->setWordWrap(true);
    statisticsHintLabel_->setObjectName(QStringLiteral("secondaryLabel"));
    statisticsIdleLayout->addWidget(statisticsHintLabel_, 0, Qt::AlignHCenter);

    statisticsCalculateButton_ = new QPushButton(QStringLiteral("Calculate Statistics"), statisticsIdleState_);
    statisticsCalculateButton_->setObjectName(QStringLiteral("actionButton"));
    statisticsCalculateButton_->setFixedHeight(24);
    statisticsCalculateButton_->setMinimumWidth(170);
    statisticsIdleLayout->addWidget(statisticsCalculateButton_, 0, Qt::AlignHCenter);
    statisticsIdleLayout->addStretch(1);

    statisticsLoadingState_ = new QWidget(statisticsContentHost);
    QVBoxLayout* statisticsLoadingLayout = new QVBoxLayout(statisticsLoadingState_);
    statisticsLoadingLayout->setContentsMargins(28, 28, 28, 28);
    statisticsLoadingLayout->setSpacing(10);
    statisticsLoadingLayout->addStretch(1);

    QLabel* statisticsLoadingCaption = new QLabel(QStringLiteral("Calculating statistics..."), statisticsLoadingState_);
    statisticsLoadingCaption->setAlignment(Qt::AlignCenter);
    statisticsLoadingCaption->setStyleSheet(QStringLiteral("font-size:15px; font-weight:700; color:#314252;"));
    statisticsLoadingLayout->addWidget(statisticsLoadingCaption, 0, Qt::AlignHCenter);

    statisticsLoadingLabel_ = new QLabel(QStringLiteral("Preparing the statistics scan."), statisticsLoadingState_);
    statisticsLoadingLabel_->setAlignment(Qt::AlignCenter);
    statisticsLoadingLabel_->setWordWrap(false);
    statisticsLoadingLabel_->setObjectName(QStringLiteral("secondaryLabel"));
    statisticsLoadingLabel_->installEventFilter(this);
    statisticsLoadingLayout->addWidget(statisticsLoadingLabel_, 0, Qt::AlignHCenter);

    statisticsProgressBar_ = new QProgressBar(statisticsLoadingState_);
    statisticsProgressBar_->setRange(0, 1000);
    statisticsProgressBar_->setValue(0);
    statisticsProgressBar_->setTextVisible(false);
    statisticsProgressBar_->setMinimumWidth(260);
    statisticsLoadingLayout->addWidget(statisticsProgressBar_, 0, Qt::AlignHCenter);

    statisticsCancelButton_ = new QPushButton(QStringLiteral("Cancel"), statisticsLoadingState_);
    statisticsCancelButton_->setObjectName(QStringLiteral("actionButton"));
    statisticsCancelButton_->setFixedHeight(24);
    statisticsLoadingLayout->addWidget(statisticsCancelButton_, 0, Qt::AlignHCenter);
    statisticsLoadingLayout->addStretch(1);

    statisticsResultsState_ = new QWidget(statisticsContentHost);
    QVBoxLayout* statisticsResultsLayout = new QVBoxLayout(statisticsResultsState_);
    statisticsResultsLayout->setContentsMargins(0, 0, 0, 0);
    statisticsResultsLayout->setSpacing(8);

    QHBoxLayout* statisticsHeaderLayout = new QHBoxLayout();
    statisticsHeaderLayout->setContentsMargins(0, 0, 0, 0);
    statisticsHeaderLayout->setSpacing(8);

    statisticsSummaryLabel_ = new QLabel(statisticsResultsState_);
    statisticsSummaryLabel_->setWordWrap(true);
    statisticsSummaryLabel_->setStyleSheet(QStringLiteral("font-weight:600; color:#314252;"));
    statisticsHeaderLayout->addWidget(statisticsSummaryLabel_, 1);

    statisticsRefreshButton_ = new QPushButton(QStringLiteral("Recalculate"), statisticsResultsState_);
    statisticsRefreshButton_->setObjectName(QStringLiteral("actionButton"));
    statisticsRefreshButton_->setFixedHeight(22);
    statisticsHeaderLayout->addWidget(statisticsRefreshButton_, 0, Qt::AlignTop);
    statisticsResultsLayout->addLayout(statisticsHeaderLayout);

    statisticsOverviewTable_ = new QTableWidget(0, 2, statisticsResultsState_);
    statisticsOverviewTable_->setHorizontalHeaderLabels({QStringLiteral("Metric"), QStringLiteral("Value")});
    statisticsOverviewTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    statisticsOverviewTable_->setSelectionMode(QAbstractItemView::NoSelection);
    statisticsOverviewTable_->setFocusPolicy(Qt::NoFocus);
    statisticsOverviewTable_->setAlternatingRowColors(true);
    statisticsOverviewTable_->verticalHeader()->hide();
    statisticsOverviewTable_->horizontalHeader()->setStretchLastSection(true);
    statisticsOverviewTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    statisticsOverviewTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    statisticsOverviewTable_->setMinimumHeight(240);
    statisticsResultsLayout->addWidget(statisticsOverviewTable_);

    QLabel* opcodeCountsCaption = new QLabel(QStringLiteral("Opcode Counts"), statisticsResultsState_);
    opcodeCountsCaption->setStyleSheet(QStringLiteral("font-weight:700; color:#18212B;"));
    statisticsResultsLayout->addWidget(opcodeCountsCaption);

    statisticsOpcodeTable_ = new QTableWidget(0, 4, statisticsResultsState_);
    statisticsOpcodeTable_->setHorizontalHeaderLabels({QStringLiteral("Direction"),
                                                       QStringLiteral("Channel"),
                                                       QStringLiteral("Opcode"),
                                                       QStringLiteral("Count")});
    statisticsOpcodeTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    statisticsOpcodeTable_->setSelectionMode(QAbstractItemView::NoSelection);
    statisticsOpcodeTable_->setFocusPolicy(Qt::NoFocus);
    statisticsOpcodeTable_->setAlternatingRowColors(true);
    statisticsOpcodeTable_->verticalHeader()->hide();
    statisticsOpcodeTable_->horizontalHeader()->setStretchLastSection(false);
    statisticsOpcodeTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    statisticsOpcodeTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    statisticsOpcodeTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    statisticsOpcodeTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    statisticsResultsLayout->addWidget(statisticsOpcodeTable_, 1);

    statisticsStack_->addWidget(statisticsIdleState_);
    statisticsStack_->addWidget(statisticsLoadingState_);
    statisticsStack_->addWidget(statisticsResultsState_);
    statisticsPanelLayout->addWidget(statisticsContentHost, 1);

    QWidget* topologyPanel = new QWidget(dockManager_);
    QVBoxLayout* topologyPanelLayout = new QVBoxLayout(topologyPanel);
    topologyPanelLayout->setContentsMargins(10, 10, 10, 10);
    topologyPanelLayout->setSpacing(8);

    topologySummaryLabel_ = new QLabel(topologyPanel);
    topologySummaryLabel_->setObjectName(QStringLiteral("secondaryLabel"));
    topologySummaryLabel_->setWordWrap(true);
    topologyPanelLayout->addWidget(topologySummaryLabel_);

    topologyTable_ = new QTableWidget(0, 4, topologyPanel);
    topologyTable_->setHorizontalHeaderLabels({
        QStringLiteral("Node ID"),
        QStringLiteral("Type"),
        QStringLiteral("Type Value"),
        QStringLiteral("Annotation"),
    });
    topologyTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    topologyTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    topologyTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    topologyTable_->setAlternatingRowColors(true);
    topologyTable_->verticalHeader()->hide();
    topologyTable_->horizontalHeader()->setStretchLastSection(true);
    topologyTable_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    topologyTable_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    topologyTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    topologyTable_->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    topologyPanelLayout->addWidget(topologyTable_, 1);

    QWidget* timelineHost = new QWidget(dockManager_);
    timelineStack_ = new QStackedLayout(timelineHost);
    timelineStack_->setContentsMargins(0, 0, 0, 0);
    timelineStack_->setStackingMode(QStackedLayout::StackOne);
    timelineEmptyWidget_ = new TimelineWidget(timelineHost);
    timelineWidget_ = timelineEmptyWidget_;
    timelineStack_->addWidget(timelineEmptyWidget_);

    QWidget* addressHost = new QWidget(dockManager_);
    addressStack_ = new QStackedLayout(addressHost);
    addressStack_->setContentsMargins(0, 0, 0, 0);
    addressStack_->setStackingMode(QStackedLayout::StackOne);
    addressEmptyWidget_ = new AddressWidget(addressHost);
    addressWidget_ = addressEmptyWidget_;
    addressStack_->addWidget(addressEmptyWidget_);

    QWidget* cacheHost = new QWidget(dockManager_);
    cacheStack_ = new QStackedLayout(cacheHost);
    cacheStack_->setContentsMargins(0, 0, 0, 0);
    cacheStack_->setStackingMode(QStackedLayout::StackOne);
    cacheEmptyWidget_ = new CacheWidget(cacheHost);
    cacheWidget_ = cacheEmptyWidget_;
    cacheStack_->addWidget(cacheEmptyWidget_);

    QWidget* latencyHost = new QWidget(dockManager_);
    latencyStack_ = new QStackedLayout(latencyHost);
    latencyStack_->setContentsMargins(0, 0, 0, 0);
    latencyStack_->setStackingMode(QStackedLayout::StackOne);
    latencyEmptyWidget_ = new LatencyWidget(latencyHost);
    latencyWidget_ = latencyEmptyWidget_;
    latencyStack_->addWidget(latencyEmptyWidget_);

    QWidget* transactionHost = new QWidget(dockManager_);
    transactionStack_ = new QStackedLayout(transactionHost);
    transactionStack_->setContentsMargins(0, 0, 0, 0);
    transactionStack_->setStackingMode(QStackedLayout::StackOne);
    transactionEmptyWidget_ = new TransactionWidget(transactionHost);
    transactionWidget_ = transactionEmptyWidget_;
    transactionStack_->addWidget(transactionEmptyWidget_);

    clipboardWidget_ = new ClipboardWidget(dockManager_);
    connect(clipboardWidget_, &ClipboardWidget::scopeChanged, this, [this](ClipboardScope) {
        bindClipboardWidgetToActiveScope();
    });
    connect(clipboardWidget_, &ClipboardWidget::rowActivated, this, &MainWindow::handleClipboardRowActivated);
    connect(clipboardWidget_, &ClipboardWidget::saveRequested, this, &MainWindow::saveClipboard);

    auto* traceDock = makeDockWidget(QStringLiteral("Flits"), tracePanel, true);
    timelineDock_ = makeDockWidget(QStringLiteral("Timeline"), timelineHost, true);
    addressDock_ = makeDockWidget(QStringLiteral("Address"), addressHost, true);
    cacheDock_ = makeDockWidget(QStringLiteral("Cache"), cacheHost, true);
    latencyDock_ = makeDockWidget(QStringLiteral("Latency"), latencyHost, true);
    latencyDiffWidget_ = new LatencyDiffWidget(dockManager_);
    latencyDiffDock_ = makeDockWidget(QStringLiteral("Latency Diff"), latencyDiffWidget_, true);
    transactionDock_ = makeDockWidget(QStringLiteral("Transaction"), transactionHost, true);
    clipboardDock_ = makeDockWidget(QStringLiteral("Clipboard"), clipboardWidget_, true);
    auto* detailsDock = makeDockWidget(QStringLiteral("Fields"), detailView_);
    auto* inspectorDock = makeDockWidget(QStringLiteral("Inspection"), inspectorPanel);
    statisticsDock_ = makeDockWidget(QStringLiteral("Statistics"), statisticsPanel, true);
    topologyDock_ = makeDockWidget(QStringLiteral("Topology"), topologyPanel, true);

    CDockAreaWidget* traceArea = dockManager_->addDockWidget(CenterDockWidgetArea, traceDock);
    traceArea->setAllowedAreas(ads::OuterDockAreas);
    CDockAreaWidget* rightArea = dockManager_->addDockWidget(RightDockWidgetArea, detailsDock, traceArea);
    CDockAreaWidget* inspectorArea = dockManager_->addDockWidget(BottomDockWidgetArea, inspectorDock, rightArea);
    dockManager_->addDockWidget(BottomDockWidgetArea, timelineDock_, traceArea);
    dockManager_->addDockWidget(BottomDockWidgetArea, addressDock_, traceArea);
    dockManager_->addDockWidget(BottomDockWidgetArea, cacheDock_, traceArea);
    dockManager_->addDockWidget(BottomDockWidgetArea, latencyDock_, traceArea);
    dockManager_->addDockWidget(BottomDockWidgetArea, latencyDiffDock_, traceArea);
    dockManager_->addDockWidget(BottomDockWidgetArea, transactionDock_, traceArea);
    dockManager_->addDockWidget(BottomDockWidgetArea, clipboardDock_, traceArea);
    dockManager_->addDockWidget(BottomDockWidgetArea, statisticsDock_, traceArea);
    dockManager_->addDockWidget(BottomDockWidgetArea, topologyDock_, traceArea);
    latencyDock_->closeDockWidget();
    cacheDock_->closeDockWidget();
    latencyDiffDock_->closeDockWidget();
    transactionDock_->closeDockWidget();
    clipboardDock_->closeDockWidget();
    statisticsDock_->closeDockWidget();
    topologyDock_->closeDockWidget();

    dockManager_->setSplitterSizes(rightArea, {900, 1240});
    dockManager_->setSplitterSizes(inspectorArea, {500, 420});
    defaultLayoutState_ = dockManager_->saveState();

    QSettings settings = MakeLayoutSettings();
    const QByteArray savedGeometry = settings.value(QLatin1String(kGeometrySettingsKey)).toByteArray();
    if (!savedGeometry.isEmpty()) {
        restoreGeometry(savedGeometry);
    }

    const QByteArray savedLayout = settings.value(QLatin1String(kLayoutSettingsKey)).toByteArray();
    if (!savedLayout.isEmpty()) {
        if (!dockManager_->restoreState(savedLayout)) {
            dockManager_->restoreState(defaultLayoutState_);
        }
    }
    updateDerivedWidgetOutdatedTags();

    QToolBar* workspaceToolbar = addToolBar(QStringLiteral("Workspace"));
    workspaceToolbar->setObjectName(QStringLiteral("workspaceToolbar"));
    workspaceToolbar->setMovable(false);
    workspaceToolbar->setFloatable(false);
    workspaceToolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    workspaceToolbar->setIconSize(QSize(16, 16));

    QMenu* windowsMenu = new QMenu(QStringLiteral("&Windows"), this);
    QAction* resetLayoutAction = windowsMenu->addAction(QStringLiteral("Reset To Default Layout"));
    connect(resetLayoutAction, &QAction::triggered, this, &MainWindow::restoreDefaultLayout);
    windowsMenu->addSeparator();
    windowsMenu->addAction(traceDock->toggleViewAction());
    windowsMenu->addAction(timelineDock_->toggleViewAction());
    windowsMenu->addAction(addressDock_->toggleViewAction());
    windowsMenu->addAction(cacheDock_->toggleViewAction());
    windowsMenu->addAction(latencyDock_->toggleViewAction());
    windowsMenu->addAction(latencyDiffDock_->toggleViewAction());
    windowsMenu->addAction(transactionDock_->toggleViewAction());
    windowsMenu->addAction(clipboardDock_->toggleViewAction());
    windowsMenu->addAction(detailsDock->toggleViewAction());
    windowsMenu->addAction(inspectorDock->toggleViewAction());
    windowsMenu->addAction(statisticsDock_->toggleViewAction());
    windowsMenu->addAction(topologyDock_->toggleViewAction());

    QToolButton* windowsButton = new QToolButton(workspaceToolbar);
    windowsButton->setText(QStringLiteral("Windows"));
    windowsButton->setPopupMode(QToolButton::InstantPopup);
    windowsButton->setMenu(windowsMenu);

    QWidgetAction* windowsAction = new QWidgetAction(workspaceToolbar);
    windowsAction->setDefaultWidget(windowsButton);
    workspaceToolbar->addAction(windowsAction);

    QMenu* sessionsMenu = new QMenu(QStringLiteral("&Sessions"), this);
    QAction* reloadActiveSessionAction =
        sessionsMenu->addAction(QStringLiteral("Reload Active Session"));
    sessionsMenu->addSeparator();
    sessionsMenu->addAction(readOnlyModeAction_);
    saveEditedCopyAction_ = sessionsMenu->addAction(QStringLiteral("Save Edited Copy..."));
    saveEditedCopyAction_->setShortcut(QKeySequence::Save);
    saveEditedCopyAction_->setShortcutContext(Qt::WindowShortcut);
    addAction(saveEditedCopyAction_);
    insertBlankFlitAction_ = sessionsMenu->addAction(QStringLiteral("Build Flit Template..."));
    cloneFlitAction_ = sessionsMenu->addAction(QStringLiteral("Clone Selected Flit..."));
    rebuildDerivedViewsAction_ = sessionsMenu->addAction(QStringLiteral("Rebuild Derived Views"));
    latencyDiffAction_ = sessionsMenu->addAction(QStringLiteral("Latency Diff..."));
    latencyDiffAction_->setEnabled(false);
    sessionsMenu->addSeparator();
    QAction* closeCurrentSessionAction =
        sessionsMenu->addAction(QStringLiteral("Close Current Session"));
    QAction* closeOtherSessionsAction =
        sessionsMenu->addAction(QStringLiteral("Close Other Sessions"));
    QAction* closeAllSessionsAction =
        sessionsMenu->addAction(QStringLiteral("Close All Sessions"));
    connect(sessionsMenu, &QMenu::aboutToShow, this, [this,
                                                      reloadActiveSessionAction,
                                                      thisLatencyDiffAction = latencyDiffAction_,
                                                      closeCurrentSessionAction,
                                                      closeOtherSessionsAction,
                                                      closeAllSessionsAction]() {
        const TraceViewSession* session = activeTraceViewSession();
        const bool hasActiveSession = session != nullptr;
        reloadActiveSessionAction->setEnabled(hasActiveSession && !session->path.isEmpty());
        if (thisLatencyDiffAction) {
            thisLatencyDiffAction->setEnabled(traceSessions_.size() >= 2);
        }
        updateEditActions();
        closeCurrentSessionAction->setEnabled(hasActiveSession);
        closeOtherSessionsAction->setEnabled(hasActiveSession && traceSessions_.size() > 1);
        closeAllSessionsAction->setEnabled(!traceSessions_.empty());
    });
    connect(reloadActiveSessionAction, &QAction::triggered, this, [this]() {
        reloadActiveTrace(true);
    });
    connect(closeCurrentSessionAction, &QAction::triggered, this, &MainWindow::closeActiveTraceSession);
    connect(closeOtherSessionsAction, &QAction::triggered, this, [this]() {
        if (activeTraceSessionId_ != 0) {
            closeOtherTraceSessions(activeTraceSessionId_);
        }
    });
    connect(closeAllSessionsAction, &QAction::triggered, this, &MainWindow::closeAllTraceSessions);
    connect(saveEditedCopyAction_, &QAction::triggered, this, [this]() {
        if (TraceViewSession* session = activeTraceViewSession()) {
            saveEditedCopy(*session);
        }
    });
    connect(insertBlankFlitAction_, &QAction::triggered, this, &MainWindow::insertBlankFlitTemplate);
    connect(cloneFlitAction_, &QAction::triggered, this, &MainWindow::cloneSelectedFlitRows);
    connect(rebuildDerivedViewsAction_, &QAction::triggered, this, &MainWindow::rebuildDerivedViewsForActiveSession);
    connect(latencyDiffAction_, &QAction::triggered, this, &MainWindow::showLatencyDiffWizard);

    QToolButton* sessionsButton = new QToolButton(workspaceToolbar);
    sessionsButton->setText(QStringLiteral("Sessions"));
    sessionsButton->setPopupMode(QToolButton::InstantPopup);
    sessionsButton->setMenu(sessionsMenu);

    QWidgetAction* sessionsAction = new QWidgetAction(workspaceToolbar);
    sessionsAction->setDefaultWidget(sessionsButton);
    workspaceToolbar->addAction(sessionsAction);

    QMenu* debugMenu = new QMenu(QStringLiteral("&Debug"), this);
    xactionDebugAction_ = debugMenu->addAction(QStringLiteral("Xaction Highlight Tracking"));
    xactionDebugAction_->setCheckable(true);
    xactionDebugAction_->setChecked(false);
    xactionDebugAction_->setToolTip(
        QStringLiteral("Show verbose Xaction highlight tracking when a flit row is double-clicked."));
    xactionDebugAction_->setStatusTip(xactionDebugAction_->toolTip());

    QToolButton* debugButton = new QToolButton(workspaceToolbar);
    debugButton->setText(QStringLiteral("Debug"));
    debugButton->setPopupMode(QToolButton::InstantPopup);
    debugButton->setMenu(debugMenu);

    QWidgetAction* debugAction = new QWidgetAction(workspaceToolbar);
    debugAction->setDefaultWidget(debugButton);
    workspaceToolbar->addAction(debugAction);

    QMenu* helpMenu = new QMenu(QStringLiteral("&Help"), this);
    QAction* exportBugReportAction = helpMenu->addAction(QStringLiteral("Export Bug Report"));
    connect(exportBugReportAction, &QAction::triggered, this, &MainWindow::exportBugReport);
    QAction* openDiagnosticsFolderAction = helpMenu->addAction(QStringLiteral("Open Diagnostics Folder"));
    connect(openDiagnosticsFolderAction, &QAction::triggered, this, &MainWindow::openDiagnosticsFolder);
    helpMenu->addSeparator();
    QAction* aboutAction = helpMenu->addAction(QStringLiteral("About CloganGUI"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAboutDialog);

    QToolButton* helpButton = new QToolButton(workspaceToolbar);
    helpButton->setText(QStringLiteral("Help"));
    helpButton->setPopupMode(QToolButton::InstantPopup);
    helpButton->setMenu(helpMenu);

    QWidgetAction* helpAction = new QWidgetAction(workspaceToolbar);
    helpAction->setDefaultWidget(helpButton);
    workspaceToolbar->addAction(helpAction);

    filterProgressLabel_ = new QLabel(this);
    filterProgressLabel_->setObjectName(QStringLiteral("secondaryLabel"));
    filterProgressLabel_->hide();

    filterProgressBar_ = new QProgressBar(this);
    filterProgressBar_->setTextVisible(false);
    filterProgressBar_->setFixedWidth(180);
    filterProgressBar_->setFixedHeight(16);
    filterProgressBar_->setRange(0, 1000);
    filterProgressBar_->setValue(0);
    filterProgressBar_->hide();

    xactionIndexProgressLabel_ = new QLabel(this);
    xactionIndexProgressLabel_->setObjectName(QStringLiteral("secondaryLabel"));
    xactionIndexProgressLabel_->hide();

    xactionIndexProgressBar_ = new QProgressBar(this);
    xactionIndexProgressBar_->setTextVisible(false);
    xactionIndexProgressBar_->setFixedWidth(180);
    xactionIndexProgressBar_->setFixedHeight(16);
    xactionIndexProgressBar_->setRange(0, 1000);
    xactionIndexProgressBar_->setValue(0);
    xactionIndexProgressBar_->hide();

    statusBar()->addPermanentWidget(filterProgressLabel_);
    statusBar()->addPermanentWidget(filterProgressBar_);
    statusBar()->addPermanentWidget(xactionIndexProgressLabel_);
    statusBar()->addPermanentWidget(xactionIndexProgressBar_);

    connect(openButton, &QPushButton::clicked, this, &MainWindow::openTraceFile);
    connect(reloadButton, &QPushButton::clicked, this, &MainWindow::reloadTrace);
    connect(traceEmptyStateButton_, &QPushButton::clicked, this, &MainWindow::openTraceFile);
    connect(traceToolbarFoldButton_, &QToolButton::toggled, this, &MainWindow::setTraceToolbarFolded);
    connect(sessionTabBar_, &QTabBar::currentChanged, this, [this](const int index) {
        if (switchingTraceSession_ || index < 0 || !sessionTabBar_) {
            return;
        }
        activateTraceSessionAt(index);
    });
    sessionTabBar_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(sessionTabBar_,
            &QWidget::customContextMenuRequested,
            this,
            &MainWindow::showSessionTabContextMenu);
}

void MainWindow::wireSignals()
{
    auto bindDebouncedFilter = [this](QLineEdit* edit, auto applyFilter) {
        QTimer* timer = new QTimer(edit);
        timer->setSingleShot(true);
        connect(timer, &QTimer::timeout, this, [edit, applyFilter]() {
            applyFilter(edit->text());
        });
        connect(edit, &QLineEdit::textChanged, this, [this, edit, timer, applyFilter](const QString&) {
            if (!currentTraceSession_) {
                timer->stop();
                applyFilter(edit->text());
                scheduleDiagnosticsSnapshotRefresh();
                return;
            }

            if (flitModel_->searchMode() == FlitTableModel::SearchMode::Filter) {
                // Stop the current session-backed filter build immediately while the user keeps typing.
                flitModel_->cancelPendingFilterBuild();
            }
            timer->start(120);
            scheduleDiagnosticsSnapshotRefresh();
        });
    };

    bindDebouncedFilter(opcodeFilterEdit_, [this](const QString& text) {
        flitModel_->setOpcodeFilter(text);
    });
    bindDebouncedFilter(sourceIdFilterEdit_, [this](const QString& text) {
        flitModel_->setSourceIdFilter(text);
    });
    bindDebouncedFilter(targetIdFilterEdit_, [this](const QString& text) {
        flitModel_->setTargetIdFilter(text);
    });
    bindDebouncedFilter(txnIdFilterEdit_, [this](const QString& text) {
        flitModel_->setTxnIdFilter(text);
    });
    bindDebouncedFilter(dbidFilterEdit_, [this](const QString& text) {
        flitModel_->setDbidFilter(text);
    });
    bindDebouncedFilter(addressFilterEdit_, [this](const QString& text) {
        flitModel_->setAddressFilter(text);
    });

    connect(searchModeButton_, &QToolButton::toggled, this, [this](const bool checked) {
        const auto mode = checked
            ? FlitTableModel::SearchMode::Highlight
            : FlitTableModel::SearchMode::Filter;
        flitModel_->setSearchMode(mode);
        searchModeButton_->setText(checked ? QStringLiteral("Highlight")
                                           : QStringLiteral("Filter"));
        const QString hint = checked
            ? QStringLiteral("Search fields highlight matching flits without hiding non-matching flits.")
            : QStringLiteral("Search fields filter visible flits to matching rows.");
        searchModeButton_->setToolTip(hint);
        searchModeButton_->setStatusTip(hint);
        updateSearchNavigationButtons();
        statusBar()->showMessage(checked
                                     ? QStringLiteral("Search highlighting enabled.")
                                     : QStringLiteral("Search filtering enabled."),
                                 2500);
        scheduleDiagnosticsSnapshotRefresh();
    });
    connect(searchPreviousButton_, &QToolButton::clicked, this, [this]() {
        navigateSearchHighlight(false);
    });
    connect(searchNextButton_, &QToolButton::clicked, this, [this]() {
        navigateSearchHighlight(true);
    });
    connect(xactionDebugAction_, &QAction::toggled, this, [this](const bool checked) {
        xactionDebugMode_ = checked;
        statusBar()->showMessage(checked
                                     ? QStringLiteral("Xaction highlight debug enabled.")
                                     : QStringLiteral("Xaction highlight debug disabled."),
                                 2500);
        scheduleDiagnosticsSnapshotRefresh();
    });
    auto* previousHighlightShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Up), this);
    previousHighlightShortcut->setContext(Qt::WindowShortcut);
    connect(previousHighlightShortcut, &QShortcut::activated, this, [this]() {
        navigateSearchHighlight(false);
    });
    auto* nextHighlightShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Down), this);
    nextHighlightShortcut->setContext(Qt::WindowShortcut);
    connect(nextHighlightShortcut, &QShortcut::activated, this, [this]() {
        navigateSearchHighlight(true);
    });

    connect(reqButton_, &QToolButton::toggled, this, [this](const bool checked) {
        flitModel_->setChannelVisible(FlitChannel::Req, checked);
        scheduleDiagnosticsSnapshotRefresh();
    });
    connect(rspButton_, &QToolButton::toggled, this, [this](const bool checked) {
        flitModel_->setChannelVisible(FlitChannel::Rsp, checked);
        scheduleDiagnosticsSnapshotRefresh();
    });
    connect(datButton_, &QToolButton::toggled, this, [this](const bool checked) {
        flitModel_->setChannelVisible(FlitChannel::Dat, checked);
        scheduleDiagnosticsSnapshotRefresh();
    });
    connect(snpButton_, &QToolButton::toggled, this, [this](const bool checked) {
        flitModel_->setChannelVisible(FlitChannel::Snp, checked);
        scheduleDiagnosticsSnapshotRefresh();
    });
    connect(txButton_, &QToolButton::toggled, this, [this](const bool checked) {
        flitModel_->setDirectionVisible(FlitDirection::Tx, checked);
        scheduleDiagnosticsSnapshotRefresh();
    });
    connect(rxButton_, &QToolButton::toggled, this, [this](const bool checked) {
        flitModel_->setDirectionVisible(FlitDirection::Rx, checked);
        scheduleDiagnosticsSnapshotRefresh();
    });
    connect(deniedFlitsButton_, &QToolButton::toggled, this, [this](const bool checked) {
        flitModel_->setXactionDeniedOnlyFilter(checked);
        statusBar()->showMessage(checked
                                     ? QStringLiteral("Showing denied Xaction-index flits only.")
                                     : QStringLiteral("Denied flit filter disabled."),
                                 2500);
        scheduleDiagnosticsSnapshotRefresh();
    });
    connect(nodeLabelsButton_, &QToolButton::toggled, this, [this](const bool checked) {
        flitModel_->setNodeLabelsVisible(checked);
        if (currentTraceSession_) {
            applyFlitColumnWidths();
        } else {
            flitView_->resizeColumnsToContents();
        }
        const QModelIndex current = flitView_->selectionModel()
            ? flitView_->selectionModel()->currentIndex()
            : QModelIndex();
        updateXactionRowsTable(current.isValid() ? flitModel_->recordAt(current.row()) : nullptr);
        statusBar()->showMessage(checked
                                     ? QStringLiteral("Node labels enabled.")
                                     : QStringLiteral("Node labels disabled."),
                                 2500);
        scheduleDiagnosticsSnapshotRefresh();
    });

    wireTraceModelSignals(flitModel_, detailModel_);
    wireTraceSelectionModel();

    connect(flitView_, &QTableView::clicked, this, [this](const QModelIndex& index) {
        if (index.isValid()) {
            updateTimelineSelection();
            updateAddressSelection();
            updateTransactionSelection();
        }
    });

    connect(flitView_, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        if (!index.isValid()) {
            flitModel_->clearTransactionHighlight();
            return;
        }
        const bool highlighted = flitModel_->toggleTransactionHighlightFromVisibleRow(index.row());
        if (!highlighted) {
            statusBar()->showMessage(QStringLiteral("Cleared transactional highlight."), 2500);
            if (xactionDebugMode_) {
                showXactionDebugDialog(index.row(), false);
            }
            return;
        }

        const FlitRecord* record = flitModel_->recordAt(index.row());
        statusBar()->showMessage(
            record && !record->transactionLabel.isEmpty()
                ? QStringLiteral("Highlighted %1.").arg(record->transactionLabel)
                : QStringLiteral("Highlighted transactionally related rows."),
            2500);
        if (xactionDebugMode_) {
            showXactionDebugDialog(index.row(), true);
        }
    });

    connect(flitView_, &QWidget::customContextMenuRequested, this, &MainWindow::showFlitRowContextMenu);

    auto* undoFlitShortcut = new QShortcut(QKeySequence::Undo, flitView_);
    undoFlitShortcut->setContext(Qt::WidgetShortcut);
    connect(undoFlitShortcut, &QShortcut::activated, this, &MainWindow::undoFlitTableEdit);
    auto* undoFlitViewportShortcut = new QShortcut(QKeySequence::Undo, flitView_->viewport());
    undoFlitViewportShortcut->setContext(Qt::WidgetShortcut);
    connect(undoFlitViewportShortcut, &QShortcut::activated, this, &MainWindow::undoFlitTableEdit);

    auto* redoFlitShortcut = new QShortcut(QKeySequence::Redo, flitView_);
    redoFlitShortcut->setContext(Qt::WidgetShortcut);
    connect(redoFlitShortcut, &QShortcut::activated, this, &MainWindow::redoFlitTableEdit);
    auto* redoFlitViewportShortcut = new QShortcut(QKeySequence::Redo, flitView_->viewport());
    redoFlitViewportShortcut->setContext(Qt::WidgetShortcut);
    connect(redoFlitViewportShortcut, &QShortcut::activated, this, &MainWindow::redoFlitTableEdit);

    auto* deleteFlitShortcut = new QShortcut(QKeySequence::Delete, flitView_);
    deleteFlitShortcut->setContext(Qt::WidgetShortcut);
    connect(deleteFlitShortcut, &QShortcut::activated, this, &MainWindow::deleteSelectedFlitRow);
    auto* deleteFlitViewportShortcut = new QShortcut(QKeySequence::Delete, flitView_->viewport());
    deleteFlitViewportShortcut->setContext(Qt::WidgetShortcut);
    connect(deleteFlitViewportShortcut, &QShortcut::activated, this, &MainWindow::deleteSelectedFlitRow);

    connect(detailView_, &QTableView::doubleClicked, this, [this](const QModelIndex& index) {
        showTableCellDialog(detailView_, index);
    });

    connect(xactionRowsTable_, &QTableWidget::cellDoubleClicked, this,
            [this](const int row, int) {
                if (!xactionRowsTable_) {
                    return;
                }
                const QTableWidgetItem* item = xactionRowsTable_->item(row, 0);
                if (!item) {
                    return;
                }
                const int logicalRow = item->data(Qt::UserRole).toInt();
                jumpToLogicalTraceRow(logicalRow);
            });

    connect(flitView_->horizontalHeader(), &QHeaderView::customContextMenuRequested, this,
            [this](const QPoint& pos) {
                QHeaderView* header = flitView_->horizontalHeader();
                const int logicalIndex = header->logicalIndexAt(pos);
                if (logicalIndex < 0) {
                    return;
                }

                if (!flitModel_->supportsDisplayMode(logicalIndex)) {
                    return;
                }

                QMenu menu(header);
                QAction* decodedAction = menu.addAction(QStringLiteral("Decoded"));
                QAction* decimalAction = menu.addAction(QStringLiteral("Decimal"));
                QAction* hexAction = menu.addAction(QStringLiteral("Hexadecimal"));

                decodedAction->setCheckable(true);
                decimalAction->setCheckable(true);
                hexAction->setCheckable(true);

                switch (flitModel_->displayMode(logicalIndex)) {
                case FlitTableModel::DisplayMode::Decoded:
                    decodedAction->setChecked(true);
                    break;
                case FlitTableModel::DisplayMode::Decimal:
                    decimalAction->setChecked(true);
                    break;
                case FlitTableModel::DisplayMode::Hexadecimal:
                    hexAction->setChecked(true);
                    break;
                }

                QAction* selected = menu.exec(header->viewport()->mapToGlobal(pos));
                if (selected == decodedAction) {
                    flitModel_->setDisplayMode(logicalIndex, FlitTableModel::DisplayMode::Decoded);
                    resizeFlitColumnForCurrentTrace(logicalIndex);
                } else if (selected == decimalAction) {
                    flitModel_->setDisplayMode(logicalIndex, FlitTableModel::DisplayMode::Decimal);
                    resizeFlitColumnForCurrentTrace(logicalIndex);
                } else if (selected == hexAction) {
                    flitModel_->setDisplayMode(logicalIndex, FlitTableModel::DisplayMode::Hexadecimal);
                    resizeFlitColumnForCurrentTrace(logicalIndex);
                }
                scheduleDiagnosticsSnapshotRefresh();
            });

    connect(fieldToggleButton_, &QToolButton::clicked, this, &MainWindow::showFieldToggleDialog);
    connect(fieldToggleClearButton_, &QPushButton::clicked, this, [this]() {
        const QStringList fields = flitModel_->availableOptionalFields();
        for (const QString& fieldName : fields) {
            if (flitModel_->isFieldColumnVisible(fieldName)) {
                flitModel_->setFieldColumnVisible(fieldName, false);
            }
        }
        for (auto it = fieldToggleCheckboxes_.cbegin(); it != fieldToggleCheckboxes_.cend(); ++it) {
            QSignalBlocker blocker(it.value());
            it.value()->setChecked(false);
        }
        rebuildOptionalFieldSearchRow();
        refreshNodeLabelDelegates();
        scheduleDiagnosticsSnapshotRefresh();
    });

    connect(statisticsCalculateButton_, &QPushButton::clicked, this, &MainWindow::startStatisticsComputation);
    connect(statisticsRefreshButton_, &QPushButton::clicked, this, &MainWindow::startStatisticsComputation);
    connect(statisticsCancelButton_, &QPushButton::clicked, this, &MainWindow::cancelStatisticsComputation);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if ((flitView_ && watched == flitView_->viewport())
        || (detailView_ && watched == detailView_->viewport())) {
        if (handleTableToolTip(static_cast<QTableView*>(watched->parent()), event)) {
            return true;
        }
    }

    if (watched == optionalFieldSearchRow_
        && event->type() == QEvent::Resize
        && optionalFieldSearchRow_->isVisible()
        && !rebuildingOptionalFieldSearchRow_) {
        rebuildOptionalFieldSearchRow();
    }

    if (watched == statisticsLoadingLabel_
        && event->type() == QEvent::Resize) {
        refreshStatisticsLoadingLabel();
    }

    return QMainWindow::eventFilter(watched, event);
}

QToolButton* MainWindow::makeToggle(const QString& label, const bool checked) const
{
    QToolButton* button = new QToolButton();
    button->setText(label);
    button->setCheckable(true);
    button->setChecked(checked);
    button->setObjectName(QStringLiteral("filterToggle"));
    button->setToolButtonStyle(Qt::ToolButtonTextOnly);
    button->setFixedHeight(22);
    return button;
}

}  // namespace CHIron::Gui

