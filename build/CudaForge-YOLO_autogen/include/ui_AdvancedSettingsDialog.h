/********************************************************************************
** Form generated from reading UI file 'AdvancedSettingsDialog.ui'
**
** Created by: Qt User Interface Compiler version 6.2.4
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ADVANCEDSETTINGSDIALOG_H
#define UI_ADVANCEDSETTINGSDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AdvancedSettingsDialog
{
public:
    QVBoxLayout *verticalLayoutRoot;
    QWidget *customTitleBar;
    QHBoxLayout *horizontalLayoutTitle;
    QLabel *lblTitle;
    QSpacerItem *horizontalSpacerTitle;
    QPushButton *btnClose;
    QWidget *dialogBody;
    QVBoxLayout *verticalLayoutBody;
    QTabWidget *tabsMain;
    QWidget *tabDashboard;
    QVBoxLayout *dashTabLayout;
    QScrollArea *dashScrollArea;
    QWidget *dashPage;
    QVBoxLayout *dashOuterLayout;
    QHBoxLayout *dashColumnsLayout;
    QVBoxLayout *leftColumnLayout;
    QWidget *cardGpuMem;
    QVBoxLayout *vlCardGpuMem;
    QLabel *titleGpuMem;
    QFormLayout *formGpuMem;
    QLabel *txtGpuUsage;
    QProgressBar *m_barGpuMem;
    QLabel *txtGpuDetail;
    QLabel *m_lblGpuMem;
    QWidget *cardSlotPool;
    QVBoxLayout *vlCardSlotPool;
    QLabel *titleSlotPool;
    QFormLayout *formSlotPool;
    QLabel *txtSlotUsage;
    QProgressBar *m_barSlotPool;
    QLabel *txtSlotDetail;
    QLabel *m_lblSlotPool;
    QWidget *cardVram;
    QVBoxLayout *vlCardVram;
    QLabel *titleVram;
    QFormLayout *formVram;
    QLabel *txtVramSlot;
    QLabel *m_lblVramSlot;
    QLabel *txtVramCtx;
    QLabel *m_lblVramCtx;
    QLabel *txtVramDec;
    QLabel *m_lblVramDecoder;
    QLabel *txtVramOther;
    QLabel *m_lblVramOther;
    QLabel *txtDecCount;
    QLabel *m_lblDecoderCount;
    QWidget *cardArena;
    QVBoxLayout *vlCardArena;
    QLabel *titleArena;
    QFormLayout *formArena;
    QLabel *txtInputArena;
    QLabel *m_lblInputArena;
    QLabel *txtOutputArena;
    QLabel *m_lblOutputArena;
    QSpacerItem *leftSpacer;
    QVBoxLayout *middleColumnLayout;
    QWidget *cardThroughput;
    QVBoxLayout *vlCardThroughput;
    QLabel *titleThroughput;
    QFormLayout *formThroughput;
    QLabel *txtInput;
    QLabel *m_lblDecodeFps;
    QLabel *txtInfer;
    QLabel *m_lblInferFps;
    QLabel *txtDisplay;
    QLabel *m_lblDisplayFps;
    QLabel *txtDetections;
    QLabel *m_lblDetections;
    QLabel *txtDqPush;
    QLabel *m_lblDqPush;
    QLabel *txtDqDrop;
    QLabel *m_lblDqDrop;
    QWidget *cardWorker;
    QVBoxLayout *vlCardWorker;
    QLabel *titleWorker;
    QFormLayout *formWorker;
    QLabel *txtIdle;
    QLabel *m_lblWorkerIdle;
    QLabel *txtBatch;
    QLabel *m_lblBatchUtil;
    QLabel *txtSlotWait;
    QLabel *m_lblSlotWait;
    QLabel *txtPreproc;
    QLabel *m_lblPreprocTime;
    QLabel *txtPeak;
    QLabel *m_lblPeakSlots;
    QLabel *txtCtx;
    QLabel *m_lblCtxPool;
    QSpacerItem *middleSpacer;
    QVBoxLayout *rightColumnLayout;
    QWidget *cardDetQueue;
    QVBoxLayout *vlCardDetQueue;
    QLabel *titleDetQueue;
    QFormLayout *formDetQueue;
    QLabel *txtDetFill;
    QProgressBar *m_barDetQueue;
    QLabel *txtDetDetail;
    QLabel *m_lblDetQueue;
    QWidget *cardBottleneck;
    QVBoxLayout *vlCardBottleneck;
    QLabel *titleBottleneck;
    QScrollArea *bottleneckScroll;
    QWidget *bottleneckContent;
    QVBoxLayout *bottleneckContentLayout;
    QLabel *m_lblBottleneck;
    QSpacerItem *bottleneckSpacer;
    QSpacerItem *rightSpacer;
    QWidget *cardLoadTest;
    QVBoxLayout *vlCardLoadTest;
    QLabel *titleLoadTest;
    QHBoxLayout *hlLoadParams;
    QLabel *m_lblLoadLayout;
    QComboBox *m_comboLoadLayout;
    QLabel *txtFps;
    QSpinBox *m_spinLoadFps;
    QLabel *txtDuration;
    QSpinBox *m_spinLoadDuration;
    QPushButton *m_btnLoadTest;
    QPushButton *m_btnStopTest;
    QSpacerItem *loadSpacer;
    QLabel *m_lblLoadTest;
    QHBoxLayout *dashCopyLayout;
    QSpacerItem *dashCopySpacer;
    QPushButton *m_btnCopyDash;
    QWidget *tabSettings;
    QVBoxLayout *settingsLayout;
    QWidget *cardPipelineParams;
    QVBoxLayout *vlCardParams;
    QLabel *titleParams;
    QFormLayout *formPipelineParams;
    QLabel *txtSlots;
    QSpinBox *m_spinSlots;
    QLabel *txtWorkers;
    QSpinBox *m_spinWorkerCount;
    QLabel *txtStreams;
    QSpinBox *m_spinInferenceStreams;
    QLabel *txtBatchParam;
    QSpinBox *m_spinBatch;
    QLabel *txtCtxPoolParam;
    QSpinBox *m_spinContextPool;
    QLabel *txtStats;
    QSpinBox *m_spinStatsInterval;
    QWidget *cardModelClasses;
    QVBoxLayout *vlCardModel;
    QLabel *titleModel;
    QFormLayout *formModelClasses;
    QLabel *txtModel;
    QWidget *rowModel;
    QHBoxLayout *hlModel;
    QLineEdit *m_editModelPath;
    QPushButton *m_btnBrowseModel;
    QLabel *txtClasses;
    QWidget *rowClasses;
    QHBoxLayout *hlClasses;
    QLineEdit *m_editClassesPath;
    QPushButton *m_btnBrowseClasses;
    QHBoxLayout *rowActions;
    QSpacerItem *actionSpacer;
    QPushButton *m_btnDefaults;
    QPushButton *m_btnApply;
    QSpacerItem *settingsSpacer;

    void setupUi(QDialog *AdvancedSettingsDialog)
    {
        if (AdvancedSettingsDialog->objectName().isEmpty())
            AdvancedSettingsDialog->setObjectName(QString::fromUtf8("AdvancedSettingsDialog"));
        AdvancedSettingsDialog->resize(1180, 680);
        verticalLayoutRoot = new QVBoxLayout(AdvancedSettingsDialog);
        verticalLayoutRoot->setSpacing(0);
        verticalLayoutRoot->setObjectName(QString::fromUtf8("verticalLayoutRoot"));
        verticalLayoutRoot->setContentsMargins(0, 0, 0, 0);
        customTitleBar = new QWidget(AdvancedSettingsDialog);
        customTitleBar->setObjectName(QString::fromUtf8("customTitleBar"));
        horizontalLayoutTitle = new QHBoxLayout(customTitleBar);
        horizontalLayoutTitle->setObjectName(QString::fromUtf8("horizontalLayoutTitle"));
        horizontalLayoutTitle->setContentsMargins(12, 6, 8, 6);
        lblTitle = new QLabel(customTitleBar);
        lblTitle->setObjectName(QString::fromUtf8("lblTitle"));

        horizontalLayoutTitle->addWidget(lblTitle);

        horizontalSpacerTitle = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayoutTitle->addItem(horizontalSpacerTitle);

        btnClose = new QPushButton(customTitleBar);
        btnClose->setObjectName(QString::fromUtf8("btnClose"));

        horizontalLayoutTitle->addWidget(btnClose);


        verticalLayoutRoot->addWidget(customTitleBar);

        dialogBody = new QWidget(AdvancedSettingsDialog);
        dialogBody->setObjectName(QString::fromUtf8("dialogBody"));
        verticalLayoutBody = new QVBoxLayout(dialogBody);
        verticalLayoutBody->setSpacing(10);
        verticalLayoutBody->setObjectName(QString::fromUtf8("verticalLayoutBody"));
        verticalLayoutBody->setContentsMargins(12, 10, 12, 12);
        tabsMain = new QTabWidget(dialogBody);
        tabsMain->setObjectName(QString::fromUtf8("tabsMain"));
        tabDashboard = new QWidget();
        tabDashboard->setObjectName(QString::fromUtf8("tabDashboard"));
        dashTabLayout = new QVBoxLayout(tabDashboard);
        dashTabLayout->setObjectName(QString::fromUtf8("dashTabLayout"));
        dashTabLayout->setContentsMargins(0, 0, 0, 0);
        dashScrollArea = new QScrollArea(tabDashboard);
        dashScrollArea->setObjectName(QString::fromUtf8("dashScrollArea"));
        dashScrollArea->setWidgetResizable(true);
        dashScrollArea->setFrameShape(QFrame::NoFrame);
        dashPage = new QWidget();
        dashPage->setObjectName(QString::fromUtf8("dashPage"));
        dashOuterLayout = new QVBoxLayout(dashPage);
        dashOuterLayout->setSpacing(10);
        dashOuterLayout->setObjectName(QString::fromUtf8("dashOuterLayout"));
        dashColumnsLayout = new QHBoxLayout();
        dashColumnsLayout->setSpacing(12);
        dashColumnsLayout->setObjectName(QString::fromUtf8("dashColumnsLayout"));
        leftColumnLayout = new QVBoxLayout();
        leftColumnLayout->setSpacing(10);
        leftColumnLayout->setObjectName(QString::fromUtf8("leftColumnLayout"));
        cardGpuMem = new QWidget(dashPage);
        cardGpuMem->setObjectName(QString::fromUtf8("cardGpuMem"));
        cardGpuMem->setProperty("card", QVariant(true));
        vlCardGpuMem = new QVBoxLayout(cardGpuMem);
        vlCardGpuMem->setObjectName(QString::fromUtf8("vlCardGpuMem"));
        vlCardGpuMem->setContentsMargins(10, 10, 10, 10);
        titleGpuMem = new QLabel(cardGpuMem);
        titleGpuMem->setObjectName(QString::fromUtf8("titleGpuMem"));

        vlCardGpuMem->addWidget(titleGpuMem);

        formGpuMem = new QFormLayout();
        formGpuMem->setObjectName(QString::fromUtf8("formGpuMem"));
        txtGpuUsage = new QLabel(cardGpuMem);
        txtGpuUsage->setObjectName(QString::fromUtf8("txtGpuUsage"));

        formGpuMem->setWidget(0, QFormLayout::LabelRole, txtGpuUsage);

        m_barGpuMem = new QProgressBar(cardGpuMem);
        m_barGpuMem->setObjectName(QString::fromUtf8("m_barGpuMem"));
        m_barGpuMem->setMaximum(100);
        m_barGpuMem->setValue(0);

        formGpuMem->setWidget(0, QFormLayout::FieldRole, m_barGpuMem);

        txtGpuDetail = new QLabel(cardGpuMem);
        txtGpuDetail->setObjectName(QString::fromUtf8("txtGpuDetail"));

        formGpuMem->setWidget(1, QFormLayout::LabelRole, txtGpuDetail);

        m_lblGpuMem = new QLabel(cardGpuMem);
        m_lblGpuMem->setObjectName(QString::fromUtf8("m_lblGpuMem"));

        formGpuMem->setWidget(1, QFormLayout::FieldRole, m_lblGpuMem);


        vlCardGpuMem->addLayout(formGpuMem);


        leftColumnLayout->addWidget(cardGpuMem);

        cardSlotPool = new QWidget(dashPage);
        cardSlotPool->setObjectName(QString::fromUtf8("cardSlotPool"));
        cardSlotPool->setProperty("card", QVariant(true));
        vlCardSlotPool = new QVBoxLayout(cardSlotPool);
        vlCardSlotPool->setObjectName(QString::fromUtf8("vlCardSlotPool"));
        vlCardSlotPool->setContentsMargins(10, 10, 10, 10);
        titleSlotPool = new QLabel(cardSlotPool);
        titleSlotPool->setObjectName(QString::fromUtf8("titleSlotPool"));

        vlCardSlotPool->addWidget(titleSlotPool);

        formSlotPool = new QFormLayout();
        formSlotPool->setObjectName(QString::fromUtf8("formSlotPool"));
        txtSlotUsage = new QLabel(cardSlotPool);
        txtSlotUsage->setObjectName(QString::fromUtf8("txtSlotUsage"));

        formSlotPool->setWidget(0, QFormLayout::LabelRole, txtSlotUsage);

        m_barSlotPool = new QProgressBar(cardSlotPool);
        m_barSlotPool->setObjectName(QString::fromUtf8("m_barSlotPool"));
        m_barSlotPool->setMaximum(100);
        m_barSlotPool->setValue(0);

        formSlotPool->setWidget(0, QFormLayout::FieldRole, m_barSlotPool);

        txtSlotDetail = new QLabel(cardSlotPool);
        txtSlotDetail->setObjectName(QString::fromUtf8("txtSlotDetail"));

        formSlotPool->setWidget(1, QFormLayout::LabelRole, txtSlotDetail);

        m_lblSlotPool = new QLabel(cardSlotPool);
        m_lblSlotPool->setObjectName(QString::fromUtf8("m_lblSlotPool"));

        formSlotPool->setWidget(1, QFormLayout::FieldRole, m_lblSlotPool);


        vlCardSlotPool->addLayout(formSlotPool);


        leftColumnLayout->addWidget(cardSlotPool);

        cardVram = new QWidget(dashPage);
        cardVram->setObjectName(QString::fromUtf8("cardVram"));
        cardVram->setProperty("card", QVariant(true));
        vlCardVram = new QVBoxLayout(cardVram);
        vlCardVram->setObjectName(QString::fromUtf8("vlCardVram"));
        vlCardVram->setContentsMargins(10, 10, 10, 10);
        titleVram = new QLabel(cardVram);
        titleVram->setObjectName(QString::fromUtf8("titleVram"));

        vlCardVram->addWidget(titleVram);

        formVram = new QFormLayout();
        formVram->setObjectName(QString::fromUtf8("formVram"));
        txtVramSlot = new QLabel(cardVram);
        txtVramSlot->setObjectName(QString::fromUtf8("txtVramSlot"));

        formVram->setWidget(0, QFormLayout::LabelRole, txtVramSlot);

        m_lblVramSlot = new QLabel(cardVram);
        m_lblVramSlot->setObjectName(QString::fromUtf8("m_lblVramSlot"));

        formVram->setWidget(0, QFormLayout::FieldRole, m_lblVramSlot);

        txtVramCtx = new QLabel(cardVram);
        txtVramCtx->setObjectName(QString::fromUtf8("txtVramCtx"));

        formVram->setWidget(1, QFormLayout::LabelRole, txtVramCtx);

        m_lblVramCtx = new QLabel(cardVram);
        m_lblVramCtx->setObjectName(QString::fromUtf8("m_lblVramCtx"));

        formVram->setWidget(1, QFormLayout::FieldRole, m_lblVramCtx);

        txtVramDec = new QLabel(cardVram);
        txtVramDec->setObjectName(QString::fromUtf8("txtVramDec"));

        formVram->setWidget(2, QFormLayout::LabelRole, txtVramDec);

        m_lblVramDecoder = new QLabel(cardVram);
        m_lblVramDecoder->setObjectName(QString::fromUtf8("m_lblVramDecoder"));

        formVram->setWidget(2, QFormLayout::FieldRole, m_lblVramDecoder);

        txtVramOther = new QLabel(cardVram);
        txtVramOther->setObjectName(QString::fromUtf8("txtVramOther"));

        formVram->setWidget(3, QFormLayout::LabelRole, txtVramOther);

        m_lblVramOther = new QLabel(cardVram);
        m_lblVramOther->setObjectName(QString::fromUtf8("m_lblVramOther"));

        formVram->setWidget(3, QFormLayout::FieldRole, m_lblVramOther);

        txtDecCount = new QLabel(cardVram);
        txtDecCount->setObjectName(QString::fromUtf8("txtDecCount"));

        formVram->setWidget(4, QFormLayout::LabelRole, txtDecCount);

        m_lblDecoderCount = new QLabel(cardVram);
        m_lblDecoderCount->setObjectName(QString::fromUtf8("m_lblDecoderCount"));

        formVram->setWidget(4, QFormLayout::FieldRole, m_lblDecoderCount);


        vlCardVram->addLayout(formVram);


        leftColumnLayout->addWidget(cardVram);

        cardArena = new QWidget(dashPage);
        cardArena->setObjectName(QString::fromUtf8("cardArena"));
        cardArena->setProperty("card", QVariant(true));
        vlCardArena = new QVBoxLayout(cardArena);
        vlCardArena->setObjectName(QString::fromUtf8("vlCardArena"));
        vlCardArena->setContentsMargins(10, 10, 10, 10);
        titleArena = new QLabel(cardArena);
        titleArena->setObjectName(QString::fromUtf8("titleArena"));

        vlCardArena->addWidget(titleArena);

        formArena = new QFormLayout();
        formArena->setObjectName(QString::fromUtf8("formArena"));
        txtInputArena = new QLabel(cardArena);
        txtInputArena->setObjectName(QString::fromUtf8("txtInputArena"));

        formArena->setWidget(0, QFormLayout::LabelRole, txtInputArena);

        m_lblInputArena = new QLabel(cardArena);
        m_lblInputArena->setObjectName(QString::fromUtf8("m_lblInputArena"));

        formArena->setWidget(0, QFormLayout::FieldRole, m_lblInputArena);

        txtOutputArena = new QLabel(cardArena);
        txtOutputArena->setObjectName(QString::fromUtf8("txtOutputArena"));

        formArena->setWidget(1, QFormLayout::LabelRole, txtOutputArena);

        m_lblOutputArena = new QLabel(cardArena);
        m_lblOutputArena->setObjectName(QString::fromUtf8("m_lblOutputArena"));

        formArena->setWidget(1, QFormLayout::FieldRole, m_lblOutputArena);


        vlCardArena->addLayout(formArena);


        leftColumnLayout->addWidget(cardArena);

        leftSpacer = new QSpacerItem(20, 4, QSizePolicy::Minimum, QSizePolicy::Fixed);

        leftColumnLayout->addItem(leftSpacer);


        dashColumnsLayout->addLayout(leftColumnLayout);

        middleColumnLayout = new QVBoxLayout();
        middleColumnLayout->setSpacing(10);
        middleColumnLayout->setObjectName(QString::fromUtf8("middleColumnLayout"));
        cardThroughput = new QWidget(dashPage);
        cardThroughput->setObjectName(QString::fromUtf8("cardThroughput"));
        cardThroughput->setProperty("card", QVariant(true));
        vlCardThroughput = new QVBoxLayout(cardThroughput);
        vlCardThroughput->setObjectName(QString::fromUtf8("vlCardThroughput"));
        vlCardThroughput->setContentsMargins(10, 10, 10, 10);
        titleThroughput = new QLabel(cardThroughput);
        titleThroughput->setObjectName(QString::fromUtf8("titleThroughput"));

        vlCardThroughput->addWidget(titleThroughput);

        formThroughput = new QFormLayout();
        formThroughput->setObjectName(QString::fromUtf8("formThroughput"));
        txtInput = new QLabel(cardThroughput);
        txtInput->setObjectName(QString::fromUtf8("txtInput"));

        formThroughput->setWidget(0, QFormLayout::LabelRole, txtInput);

        m_lblDecodeFps = new QLabel(cardThroughput);
        m_lblDecodeFps->setObjectName(QString::fromUtf8("m_lblDecodeFps"));

        formThroughput->setWidget(0, QFormLayout::FieldRole, m_lblDecodeFps);

        txtInfer = new QLabel(cardThroughput);
        txtInfer->setObjectName(QString::fromUtf8("txtInfer"));

        formThroughput->setWidget(1, QFormLayout::LabelRole, txtInfer);

        m_lblInferFps = new QLabel(cardThroughput);
        m_lblInferFps->setObjectName(QString::fromUtf8("m_lblInferFps"));

        formThroughput->setWidget(1, QFormLayout::FieldRole, m_lblInferFps);

        txtDisplay = new QLabel(cardThroughput);
        txtDisplay->setObjectName(QString::fromUtf8("txtDisplay"));

        formThroughput->setWidget(2, QFormLayout::LabelRole, txtDisplay);

        m_lblDisplayFps = new QLabel(cardThroughput);
        m_lblDisplayFps->setObjectName(QString::fromUtf8("m_lblDisplayFps"));

        formThroughput->setWidget(2, QFormLayout::FieldRole, m_lblDisplayFps);

        txtDetections = new QLabel(cardThroughput);
        txtDetections->setObjectName(QString::fromUtf8("txtDetections"));

        formThroughput->setWidget(3, QFormLayout::LabelRole, txtDetections);

        m_lblDetections = new QLabel(cardThroughput);
        m_lblDetections->setObjectName(QString::fromUtf8("m_lblDetections"));

        formThroughput->setWidget(3, QFormLayout::FieldRole, m_lblDetections);

        txtDqPush = new QLabel(cardThroughput);
        txtDqPush->setObjectName(QString::fromUtf8("txtDqPush"));

        formThroughput->setWidget(4, QFormLayout::LabelRole, txtDqPush);

        m_lblDqPush = new QLabel(cardThroughput);
        m_lblDqPush->setObjectName(QString::fromUtf8("m_lblDqPush"));

        formThroughput->setWidget(4, QFormLayout::FieldRole, m_lblDqPush);

        txtDqDrop = new QLabel(cardThroughput);
        txtDqDrop->setObjectName(QString::fromUtf8("txtDqDrop"));

        formThroughput->setWidget(5, QFormLayout::LabelRole, txtDqDrop);

        m_lblDqDrop = new QLabel(cardThroughput);
        m_lblDqDrop->setObjectName(QString::fromUtf8("m_lblDqDrop"));

        formThroughput->setWidget(5, QFormLayout::FieldRole, m_lblDqDrop);


        vlCardThroughput->addLayout(formThroughput);


        middleColumnLayout->addWidget(cardThroughput);

        cardWorker = new QWidget(dashPage);
        cardWorker->setObjectName(QString::fromUtf8("cardWorker"));
        cardWorker->setProperty("card", QVariant(true));
        vlCardWorker = new QVBoxLayout(cardWorker);
        vlCardWorker->setObjectName(QString::fromUtf8("vlCardWorker"));
        vlCardWorker->setContentsMargins(10, 10, 10, 10);
        titleWorker = new QLabel(cardWorker);
        titleWorker->setObjectName(QString::fromUtf8("titleWorker"));

        vlCardWorker->addWidget(titleWorker);

        formWorker = new QFormLayout();
        formWorker->setObjectName(QString::fromUtf8("formWorker"));
        txtIdle = new QLabel(cardWorker);
        txtIdle->setObjectName(QString::fromUtf8("txtIdle"));

        formWorker->setWidget(0, QFormLayout::LabelRole, txtIdle);

        m_lblWorkerIdle = new QLabel(cardWorker);
        m_lblWorkerIdle->setObjectName(QString::fromUtf8("m_lblWorkerIdle"));

        formWorker->setWidget(0, QFormLayout::FieldRole, m_lblWorkerIdle);

        txtBatch = new QLabel(cardWorker);
        txtBatch->setObjectName(QString::fromUtf8("txtBatch"));

        formWorker->setWidget(1, QFormLayout::LabelRole, txtBatch);

        m_lblBatchUtil = new QLabel(cardWorker);
        m_lblBatchUtil->setObjectName(QString::fromUtf8("m_lblBatchUtil"));

        formWorker->setWidget(1, QFormLayout::FieldRole, m_lblBatchUtil);

        txtSlotWait = new QLabel(cardWorker);
        txtSlotWait->setObjectName(QString::fromUtf8("txtSlotWait"));

        formWorker->setWidget(2, QFormLayout::LabelRole, txtSlotWait);

        m_lblSlotWait = new QLabel(cardWorker);
        m_lblSlotWait->setObjectName(QString::fromUtf8("m_lblSlotWait"));

        formWorker->setWidget(2, QFormLayout::FieldRole, m_lblSlotWait);

        txtPreproc = new QLabel(cardWorker);
        txtPreproc->setObjectName(QString::fromUtf8("txtPreproc"));

        formWorker->setWidget(3, QFormLayout::LabelRole, txtPreproc);

        m_lblPreprocTime = new QLabel(cardWorker);
        m_lblPreprocTime->setObjectName(QString::fromUtf8("m_lblPreprocTime"));

        formWorker->setWidget(3, QFormLayout::FieldRole, m_lblPreprocTime);

        txtPeak = new QLabel(cardWorker);
        txtPeak->setObjectName(QString::fromUtf8("txtPeak"));

        formWorker->setWidget(4, QFormLayout::LabelRole, txtPeak);

        m_lblPeakSlots = new QLabel(cardWorker);
        m_lblPeakSlots->setObjectName(QString::fromUtf8("m_lblPeakSlots"));

        formWorker->setWidget(4, QFormLayout::FieldRole, m_lblPeakSlots);

        txtCtx = new QLabel(cardWorker);
        txtCtx->setObjectName(QString::fromUtf8("txtCtx"));

        formWorker->setWidget(5, QFormLayout::LabelRole, txtCtx);

        m_lblCtxPool = new QLabel(cardWorker);
        m_lblCtxPool->setObjectName(QString::fromUtf8("m_lblCtxPool"));

        formWorker->setWidget(5, QFormLayout::FieldRole, m_lblCtxPool);


        vlCardWorker->addLayout(formWorker);


        middleColumnLayout->addWidget(cardWorker);

        middleSpacer = new QSpacerItem(20, 4, QSizePolicy::Minimum, QSizePolicy::Fixed);

        middleColumnLayout->addItem(middleSpacer);


        dashColumnsLayout->addLayout(middleColumnLayout);

        rightColumnLayout = new QVBoxLayout();
        rightColumnLayout->setSpacing(10);
        rightColumnLayout->setObjectName(QString::fromUtf8("rightColumnLayout"));
        cardDetQueue = new QWidget(dashPage);
        cardDetQueue->setObjectName(QString::fromUtf8("cardDetQueue"));
        cardDetQueue->setProperty("card", QVariant(true));
        vlCardDetQueue = new QVBoxLayout(cardDetQueue);
        vlCardDetQueue->setObjectName(QString::fromUtf8("vlCardDetQueue"));
        vlCardDetQueue->setContentsMargins(10, 10, 10, 10);
        titleDetQueue = new QLabel(cardDetQueue);
        titleDetQueue->setObjectName(QString::fromUtf8("titleDetQueue"));

        vlCardDetQueue->addWidget(titleDetQueue);

        formDetQueue = new QFormLayout();
        formDetQueue->setObjectName(QString::fromUtf8("formDetQueue"));
        txtDetFill = new QLabel(cardDetQueue);
        txtDetFill->setObjectName(QString::fromUtf8("txtDetFill"));

        formDetQueue->setWidget(0, QFormLayout::LabelRole, txtDetFill);

        m_barDetQueue = new QProgressBar(cardDetQueue);
        m_barDetQueue->setObjectName(QString::fromUtf8("m_barDetQueue"));
        m_barDetQueue->setMaximum(100);
        m_barDetQueue->setValue(0);

        formDetQueue->setWidget(0, QFormLayout::FieldRole, m_barDetQueue);

        txtDetDetail = new QLabel(cardDetQueue);
        txtDetDetail->setObjectName(QString::fromUtf8("txtDetDetail"));

        formDetQueue->setWidget(1, QFormLayout::LabelRole, txtDetDetail);

        m_lblDetQueue = new QLabel(cardDetQueue);
        m_lblDetQueue->setObjectName(QString::fromUtf8("m_lblDetQueue"));

        formDetQueue->setWidget(1, QFormLayout::FieldRole, m_lblDetQueue);


        vlCardDetQueue->addLayout(formDetQueue);


        rightColumnLayout->addWidget(cardDetQueue);

        cardBottleneck = new QWidget(dashPage);
        cardBottleneck->setObjectName(QString::fromUtf8("cardBottleneck"));
        cardBottleneck->setProperty("card", QVariant(true));
        vlCardBottleneck = new QVBoxLayout(cardBottleneck);
        vlCardBottleneck->setObjectName(QString::fromUtf8("vlCardBottleneck"));
        vlCardBottleneck->setContentsMargins(10, 10, 10, 10);
        titleBottleneck = new QLabel(cardBottleneck);
        titleBottleneck->setObjectName(QString::fromUtf8("titleBottleneck"));

        vlCardBottleneck->addWidget(titleBottleneck);

        bottleneckScroll = new QScrollArea(cardBottleneck);
        bottleneckScroll->setObjectName(QString::fromUtf8("bottleneckScroll"));
        bottleneckScroll->setWidgetResizable(true);
        bottleneckScroll->setFrameShape(QFrame::NoFrame);
        bottleneckContent = new QWidget();
        bottleneckContent->setObjectName(QString::fromUtf8("bottleneckContent"));
        bottleneckContentLayout = new QVBoxLayout(bottleneckContent);
        bottleneckContentLayout->setObjectName(QString::fromUtf8("bottleneckContentLayout"));
        bottleneckContentLayout->setContentsMargins(0, 0, 0, 0);
        m_lblBottleneck = new QLabel(bottleneckContent);
        m_lblBottleneck->setObjectName(QString::fromUtf8("m_lblBottleneck"));
        m_lblBottleneck->setWordWrap(true);
        m_lblBottleneck->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);

        bottleneckContentLayout->addWidget(m_lblBottleneck);

        bottleneckSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        bottleneckContentLayout->addItem(bottleneckSpacer);

        bottleneckScroll->setWidget(bottleneckContent);

        vlCardBottleneck->addWidget(bottleneckScroll);


        rightColumnLayout->addWidget(cardBottleneck);

        rightSpacer = new QSpacerItem(20, 4, QSizePolicy::Minimum, QSizePolicy::Fixed);

        rightColumnLayout->addItem(rightSpacer);


        dashColumnsLayout->addLayout(rightColumnLayout);


        dashOuterLayout->addLayout(dashColumnsLayout);

        cardLoadTest = new QWidget(dashPage);
        cardLoadTest->setObjectName(QString::fromUtf8("cardLoadTest"));
        cardLoadTest->setProperty("card", QVariant(true));
        vlCardLoadTest = new QVBoxLayout(cardLoadTest);
        vlCardLoadTest->setObjectName(QString::fromUtf8("vlCardLoadTest"));
        vlCardLoadTest->setContentsMargins(10, 10, 10, 10);
        titleLoadTest = new QLabel(cardLoadTest);
        titleLoadTest->setObjectName(QString::fromUtf8("titleLoadTest"));

        vlCardLoadTest->addWidget(titleLoadTest);

        hlLoadParams = new QHBoxLayout();
        hlLoadParams->setObjectName(QString::fromUtf8("hlLoadParams"));
        m_lblLoadLayout = new QLabel(cardLoadTest);
        m_lblLoadLayout->setObjectName(QString::fromUtf8("m_lblLoadLayout"));

        hlLoadParams->addWidget(m_lblLoadLayout);

        m_comboLoadLayout = new QComboBox(cardLoadTest);
        m_comboLoadLayout->addItem(QString());
        m_comboLoadLayout->addItem(QString());
        m_comboLoadLayout->addItem(QString());
        m_comboLoadLayout->setObjectName(QString::fromUtf8("m_comboLoadLayout"));

        hlLoadParams->addWidget(m_comboLoadLayout);

        txtFps = new QLabel(cardLoadTest);
        txtFps->setObjectName(QString::fromUtf8("txtFps"));

        hlLoadParams->addWidget(txtFps);

        m_spinLoadFps = new QSpinBox(cardLoadTest);
        m_spinLoadFps->setObjectName(QString::fromUtf8("m_spinLoadFps"));
        m_spinLoadFps->setMinimum(1);
        m_spinLoadFps->setMaximum(9999);
        m_spinLoadFps->setValue(30);

        hlLoadParams->addWidget(m_spinLoadFps);

        txtDuration = new QLabel(cardLoadTest);
        txtDuration->setObjectName(QString::fromUtf8("txtDuration"));

        hlLoadParams->addWidget(txtDuration);

        m_spinLoadDuration = new QSpinBox(cardLoadTest);
        m_spinLoadDuration->setObjectName(QString::fromUtf8("m_spinLoadDuration"));
        m_spinLoadDuration->setMinimum(5);
        m_spinLoadDuration->setMaximum(300);
        m_spinLoadDuration->setValue(30);

        hlLoadParams->addWidget(m_spinLoadDuration);

        m_btnLoadTest = new QPushButton(cardLoadTest);
        m_btnLoadTest->setObjectName(QString::fromUtf8("m_btnLoadTest"));

        hlLoadParams->addWidget(m_btnLoadTest);

        m_btnStopTest = new QPushButton(cardLoadTest);
        m_btnStopTest->setObjectName(QString::fromUtf8("m_btnStopTest"));
        m_btnStopTest->setEnabled(false);

        hlLoadParams->addWidget(m_btnStopTest);

        loadSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        hlLoadParams->addItem(loadSpacer);


        vlCardLoadTest->addLayout(hlLoadParams);

        m_lblLoadTest = new QLabel(cardLoadTest);
        m_lblLoadTest->setObjectName(QString::fromUtf8("m_lblLoadTest"));
        m_lblLoadTest->setWordWrap(true);

        vlCardLoadTest->addWidget(m_lblLoadTest);


        dashOuterLayout->addWidget(cardLoadTest);

        dashCopyLayout = new QHBoxLayout();
        dashCopyLayout->setObjectName(QString::fromUtf8("dashCopyLayout"));
        dashCopySpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        dashCopyLayout->addItem(dashCopySpacer);

        m_btnCopyDash = new QPushButton(dashPage);
        m_btnCopyDash->setObjectName(QString::fromUtf8("m_btnCopyDash"));

        dashCopyLayout->addWidget(m_btnCopyDash);


        dashOuterLayout->addLayout(dashCopyLayout);

        dashScrollArea->setWidget(dashPage);

        dashTabLayout->addWidget(dashScrollArea);

        tabsMain->addTab(tabDashboard, QString());
        tabSettings = new QWidget();
        tabSettings->setObjectName(QString::fromUtf8("tabSettings"));
        settingsLayout = new QVBoxLayout(tabSettings);
        settingsLayout->setObjectName(QString::fromUtf8("settingsLayout"));
        cardPipelineParams = new QWidget(tabSettings);
        cardPipelineParams->setObjectName(QString::fromUtf8("cardPipelineParams"));
        cardPipelineParams->setProperty("card", QVariant(true));
        vlCardParams = new QVBoxLayout(cardPipelineParams);
        vlCardParams->setObjectName(QString::fromUtf8("vlCardParams"));
        vlCardParams->setContentsMargins(10, 10, 10, 10);
        titleParams = new QLabel(cardPipelineParams);
        titleParams->setObjectName(QString::fromUtf8("titleParams"));

        vlCardParams->addWidget(titleParams);

        formPipelineParams = new QFormLayout();
        formPipelineParams->setObjectName(QString::fromUtf8("formPipelineParams"));
        txtSlots = new QLabel(cardPipelineParams);
        txtSlots->setObjectName(QString::fromUtf8("txtSlots"));

        formPipelineParams->setWidget(0, QFormLayout::LabelRole, txtSlots);

        m_spinSlots = new QSpinBox(cardPipelineParams);
        m_spinSlots->setObjectName(QString::fromUtf8("m_spinSlots"));
        m_spinSlots->setMinimum(16);
        m_spinSlots->setMaximum(2000);
        m_spinSlots->setValue(200);

        formPipelineParams->setWidget(0, QFormLayout::FieldRole, m_spinSlots);

        txtWorkers = new QLabel(cardPipelineParams);
        txtWorkers->setObjectName(QString::fromUtf8("txtWorkers"));

        formPipelineParams->setWidget(1, QFormLayout::LabelRole, txtWorkers);

        m_spinWorkerCount = new QSpinBox(cardPipelineParams);
        m_spinWorkerCount->setObjectName(QString::fromUtf8("m_spinWorkerCount"));
        m_spinWorkerCount->setMinimum(0);
        m_spinWorkerCount->setMaximum(16);
        m_spinWorkerCount->setValue(0);

        formPipelineParams->setWidget(1, QFormLayout::FieldRole, m_spinWorkerCount);

        txtStreams = new QLabel(cardPipelineParams);
        txtStreams->setObjectName(QString::fromUtf8("txtStreams"));

        formPipelineParams->setWidget(2, QFormLayout::LabelRole, txtStreams);

        m_spinInferenceStreams = new QSpinBox(cardPipelineParams);
        m_spinInferenceStreams->setObjectName(QString::fromUtf8("m_spinInferenceStreams"));
        m_spinInferenceStreams->setMinimum(1);
        m_spinInferenceStreams->setMaximum(8);
        m_spinInferenceStreams->setValue(2);

        formPipelineParams->setWidget(2, QFormLayout::FieldRole, m_spinInferenceStreams);

        txtBatchParam = new QLabel(cardPipelineParams);
        txtBatchParam->setObjectName(QString::fromUtf8("txtBatchParam"));

        formPipelineParams->setWidget(3, QFormLayout::LabelRole, txtBatchParam);

        m_spinBatch = new QSpinBox(cardPipelineParams);
        m_spinBatch->setObjectName(QString::fromUtf8("m_spinBatch"));
        m_spinBatch->setMinimum(1);
        m_spinBatch->setMaximum(32);
        m_spinBatch->setValue(16);

        formPipelineParams->setWidget(3, QFormLayout::FieldRole, m_spinBatch);

        txtCtxPoolParam = new QLabel(cardPipelineParams);
        txtCtxPoolParam->setObjectName(QString::fromUtf8("txtCtxPoolParam"));

        formPipelineParams->setWidget(4, QFormLayout::LabelRole, txtCtxPoolParam);

        m_spinContextPool = new QSpinBox(cardPipelineParams);
        m_spinContextPool->setObjectName(QString::fromUtf8("m_spinContextPool"));
        m_spinContextPool->setMinimum(0);
        m_spinContextPool->setMaximum(8);
        m_spinContextPool->setValue(0);

        formPipelineParams->setWidget(4, QFormLayout::FieldRole, m_spinContextPool);

        txtStats = new QLabel(cardPipelineParams);
        txtStats->setObjectName(QString::fromUtf8("txtStats"));

        formPipelineParams->setWidget(5, QFormLayout::LabelRole, txtStats);

        m_spinStatsInterval = new QSpinBox(cardPipelineParams);
        m_spinStatsInterval->setObjectName(QString::fromUtf8("m_spinStatsInterval"));
        m_spinStatsInterval->setMinimum(1);
        m_spinStatsInterval->setMaximum(60);
        m_spinStatsInterval->setValue(5);

        formPipelineParams->setWidget(5, QFormLayout::FieldRole, m_spinStatsInterval);


        vlCardParams->addLayout(formPipelineParams);


        settingsLayout->addWidget(cardPipelineParams);

        cardModelClasses = new QWidget(tabSettings);
        cardModelClasses->setObjectName(QString::fromUtf8("cardModelClasses"));
        cardModelClasses->setProperty("card", QVariant(true));
        vlCardModel = new QVBoxLayout(cardModelClasses);
        vlCardModel->setObjectName(QString::fromUtf8("vlCardModel"));
        vlCardModel->setContentsMargins(10, 10, 10, 10);
        titleModel = new QLabel(cardModelClasses);
        titleModel->setObjectName(QString::fromUtf8("titleModel"));

        vlCardModel->addWidget(titleModel);

        formModelClasses = new QFormLayout();
        formModelClasses->setObjectName(QString::fromUtf8("formModelClasses"));
        txtModel = new QLabel(cardModelClasses);
        txtModel->setObjectName(QString::fromUtf8("txtModel"));

        formModelClasses->setWidget(0, QFormLayout::LabelRole, txtModel);

        rowModel = new QWidget(cardModelClasses);
        rowModel->setObjectName(QString::fromUtf8("rowModel"));
        hlModel = new QHBoxLayout(rowModel);
        hlModel->setObjectName(QString::fromUtf8("hlModel"));
        hlModel->setContentsMargins(0, 0, 0, 0);
        m_editModelPath = new QLineEdit(rowModel);
        m_editModelPath->setObjectName(QString::fromUtf8("m_editModelPath"));

        hlModel->addWidget(m_editModelPath);

        m_btnBrowseModel = new QPushButton(rowModel);
        m_btnBrowseModel->setObjectName(QString::fromUtf8("m_btnBrowseModel"));

        hlModel->addWidget(m_btnBrowseModel);


        formModelClasses->setWidget(0, QFormLayout::FieldRole, rowModel);

        txtClasses = new QLabel(cardModelClasses);
        txtClasses->setObjectName(QString::fromUtf8("txtClasses"));

        formModelClasses->setWidget(1, QFormLayout::LabelRole, txtClasses);

        rowClasses = new QWidget(cardModelClasses);
        rowClasses->setObjectName(QString::fromUtf8("rowClasses"));
        hlClasses = new QHBoxLayout(rowClasses);
        hlClasses->setObjectName(QString::fromUtf8("hlClasses"));
        hlClasses->setContentsMargins(0, 0, 0, 0);
        m_editClassesPath = new QLineEdit(rowClasses);
        m_editClassesPath->setObjectName(QString::fromUtf8("m_editClassesPath"));

        hlClasses->addWidget(m_editClassesPath);

        m_btnBrowseClasses = new QPushButton(rowClasses);
        m_btnBrowseClasses->setObjectName(QString::fromUtf8("m_btnBrowseClasses"));

        hlClasses->addWidget(m_btnBrowseClasses);


        formModelClasses->setWidget(1, QFormLayout::FieldRole, rowClasses);


        vlCardModel->addLayout(formModelClasses);


        settingsLayout->addWidget(cardModelClasses);

        rowActions = new QHBoxLayout();
        rowActions->setObjectName(QString::fromUtf8("rowActions"));
        actionSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        rowActions->addItem(actionSpacer);

        m_btnDefaults = new QPushButton(tabSettings);
        m_btnDefaults->setObjectName(QString::fromUtf8("m_btnDefaults"));

        rowActions->addWidget(m_btnDefaults);

        m_btnApply = new QPushButton(tabSettings);
        m_btnApply->setObjectName(QString::fromUtf8("m_btnApply"));

        rowActions->addWidget(m_btnApply);


        settingsLayout->addLayout(rowActions);

        settingsSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        settingsLayout->addItem(settingsSpacer);

        tabsMain->addTab(tabSettings, QString());

        verticalLayoutBody->addWidget(tabsMain);


        verticalLayoutRoot->addWidget(dialogBody);


        retranslateUi(AdvancedSettingsDialog);

        m_btnApply->setDefault(true);


        QMetaObject::connectSlotsByName(AdvancedSettingsDialog);
    } // setupUi

    void retranslateUi(QDialog *AdvancedSettingsDialog)
    {
        lblTitle->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Advanced Settings", nullptr));
        btnClose->setText(QCoreApplication::translate("AdvancedSettingsDialog", "\303\227", nullptr));
        titleGpuMem->setText(QCoreApplication::translate("AdvancedSettingsDialog", "GPU Memory / GPU \346\230\276\345\255\230", nullptr));
        titleGpuMem->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "cardTitle", nullptr)));
        txtGpuUsage->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Usage / \344\275\277\347\224\250\347\216\207:", nullptr));
        txtGpuUsage->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        txtGpuDetail->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Detail / \350\257\246\346\203\205:", nullptr));
        txtGpuDetail->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblGpuMem->setText(QCoreApplication::translate("AdvancedSettingsDialog", "-- / -- MiB", nullptr));
        titleSlotPool->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Slot Pool / \346\216\250\347\220\206\345\206\205\345\255\230\346\247\275\346\261\240", nullptr));
        titleSlotPool->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "cardTitle", nullptr)));
        txtSlotUsage->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Used / \345\267\262\345\215\240\347\224\250:", nullptr));
        txtSlotUsage->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        txtSlotDetail->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Detail / \350\257\246\346\203\205:", nullptr));
        txtSlotDetail->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblSlotPool->setText(QCoreApplication::translate("AdvancedSettingsDialog", "-- / --", nullptr));
        titleVram->setText(QCoreApplication::translate("AdvancedSettingsDialog", "VRAM Breakdown / \346\230\276\345\255\230\346\213\206\345\210\206(\344\274\260\347\256\227)", nullptr));
        titleVram->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "cardTitle", nullptr)));
        txtVramSlot->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Tensor Arenas / \345\274\240\351\207\217\346\261\240:", nullptr));
        txtVramSlot->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblVramSlot->setText(QCoreApplication::translate("AdvancedSettingsDialog", "--", nullptr));
        txtVramCtx->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Context / \344\270\212\344\270\213\346\226\207:", nullptr));
        txtVramCtx->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblVramCtx->setText(QCoreApplication::translate("AdvancedSettingsDialog", "--", nullptr));
        txtVramDec->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Decoder / \350\247\243\347\240\201\345\231\250:", nullptr));
        txtVramDec->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblVramDecoder->setText(QCoreApplication::translate("AdvancedSettingsDialog", "--", nullptr));
        txtVramOther->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Other / \345\205\266\344\273\226:", nullptr));
        txtVramOther->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblVramOther->setText(QCoreApplication::translate("AdvancedSettingsDialog", "--", nullptr));
        txtDecCount->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Decoders (HW/SW):", nullptr));
        txtDecCount->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblDecoderCount->setText(QCoreApplication::translate("AdvancedSettingsDialog", "--", nullptr));
        titleArena->setText(QCoreApplication::translate("AdvancedSettingsDialog", "GPU Arenas / GPU \346\230\276\345\255\230\346\261\240", nullptr));
        titleArena->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "cardTitle", nullptr)));
        txtInputArena->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Input Arena:", nullptr));
        txtInputArena->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblInputArena->setText(QCoreApplication::translate("AdvancedSettingsDialog", "--", nullptr));
        txtOutputArena->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Output Arena:", nullptr));
        txtOutputArena->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblOutputArena->setText(QCoreApplication::translate("AdvancedSettingsDialog", "--", nullptr));
        titleThroughput->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Throughput / \345\220\236\345\220\220\351\207\217 (\346\257\217 2 \347\247\222\351\207\207\346\240\267)", nullptr));
        titleThroughput->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "cardTitle", nullptr)));
        txtInput->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Input FPS / \350\276\223\345\205\245\351\200\237\347\216\207:", nullptr));
        txtInput->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblDecodeFps->setText(QCoreApplication::translate("AdvancedSettingsDialog", "0", nullptr));
        txtInfer->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Infer FPS / \346\216\250\347\220\206\345\270\247\347\216\207:", nullptr));
        txtInfer->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblInferFps->setText(QCoreApplication::translate("AdvancedSettingsDialog", "0", nullptr));
        txtDisplay->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Display FPS / \346\230\276\347\244\272\345\270\247\347\216\207:", nullptr));
        txtDisplay->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblDisplayFps->setText(QCoreApplication::translate("AdvancedSettingsDialog", "0", nullptr));
        txtDetections->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Detections / \346\243\200\346\265\213\347\233\256\346\240\207:", nullptr));
        txtDetections->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblDetections->setText(QCoreApplication::translate("AdvancedSettingsDialog", "0", nullptr));
        txtDqPush->setText(QCoreApplication::translate("AdvancedSettingsDialog", "SlotQ Push / \346\216\250\345\205\245\351\200\237\347\216\207:", nullptr));
        txtDqPush->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblDqPush->setText(QCoreApplication::translate("AdvancedSettingsDialog", "0", nullptr));
        txtDqDrop->setText(QCoreApplication::translate("AdvancedSettingsDialog", "SlotQ Dropped / \344\270\242\345\270\247:", nullptr));
        txtDqDrop->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblDqDrop->setText(QCoreApplication::translate("AdvancedSettingsDialog", "0", nullptr));
        titleWorker->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Worker Efficiency / \346\216\250\347\220\206\347\272\277\347\250\213\346\225\210\347\216\207", nullptr));
        titleWorker->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "cardTitle", nullptr)));
        txtIdle->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Idle / \347\251\272\351\227\262\347\216\207:", nullptr));
        txtIdle->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblWorkerIdle->setText(QCoreApplication::translate("AdvancedSettingsDialog", "--", nullptr));
        txtBatch->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Avg Batch / \345\271\263\345\235\207\346\211\271\351\207\217:", nullptr));
        txtBatch->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblBatchUtil->setText(QCoreApplication::translate("AdvancedSettingsDialog", "--", nullptr));
        txtSlotWait->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Slot Wait / \346\247\275\347\255\211\345\276\205:", nullptr));
        txtSlotWait->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblSlotWait->setText(QCoreApplication::translate("AdvancedSettingsDialog", "--", nullptr));
        txtPreproc->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Preprocess / \351\242\204\345\244\204\347\220\206:", nullptr));
        txtPreproc->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblPreprocTime->setText(QCoreApplication::translate("AdvancedSettingsDialog", "--", nullptr));
        txtPeak->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Peak Slots / \345\263\260\345\200\274\345\215\240\347\224\250:", nullptr));
        txtPeak->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblPeakSlots->setText(QCoreApplication::translate("AdvancedSettingsDialog", "--", nullptr));
        txtCtx->setText(QCoreApplication::translate("AdvancedSettingsDialog", "TRT Ctx Pool / \344\270\212\344\270\213\346\226\207\346\261\240:", nullptr));
        txtCtx->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblCtxPool->setText(QCoreApplication::translate("AdvancedSettingsDialog", "--", nullptr));
        titleDetQueue->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Slot Queue / \346\247\275\351\230\237\345\210\227", nullptr));
        titleDetQueue->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "cardTitle", nullptr)));
        txtDetFill->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Fill / \345\241\253\345\205\205\347\216\207:", nullptr));
        txtDetFill->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        txtDetDetail->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Detail / \350\257\246\346\203\205:", nullptr));
        txtDetDetail->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_lblDetQueue->setText(QCoreApplication::translate("AdvancedSettingsDialog", "-- / --", nullptr));
        titleBottleneck->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Bottleneck / \347\223\266\351\242\210\345\210\206\346\236\220", nullptr));
        titleBottleneck->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "cardTitle", nullptr)));
        m_lblBottleneck->setText(QCoreApplication::translate("AdvancedSettingsDialog", "--", nullptr));
        titleLoadTest->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Real Decode Test / \347\234\237\345\256\236\350\247\243\347\240\201\345\216\213\346\265\213", nullptr));
        titleLoadTest->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "cardTitle", nullptr)));
        m_lblLoadLayout->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Layout/\345\270\203\345\261\200:", nullptr));
        m_lblLoadLayout->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_comboLoadLayout->setItemText(0, QCoreApplication::translate("AdvancedSettingsDialog", "1x1 (1 ch)", nullptr));
        m_comboLoadLayout->setItemText(1, QCoreApplication::translate("AdvancedSettingsDialog", "2x2 (4 ch)", nullptr));
        m_comboLoadLayout->setItemText(2, QCoreApplication::translate("AdvancedSettingsDialog", "3x3 (9 ch)", nullptr));

        txtFps->setText(QCoreApplication::translate("AdvancedSettingsDialog", "FPS:", nullptr));
        txtFps->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        txtDuration->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Dur/\346\227\266\351\225\277:", nullptr));
        txtDuration->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_spinLoadDuration->setSuffix(QCoreApplication::translate("AdvancedSettingsDialog", " s", nullptr));
        m_btnLoadTest->setText(QCoreApplication::translate("AdvancedSettingsDialog", "\342\226\266 Start", nullptr));
        m_btnStopTest->setText(QCoreApplication::translate("AdvancedSettingsDialog", "\342\226\240 Stop", nullptr));
        m_lblLoadTest->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Ready / \345\260\261\347\273\252", nullptr));
        m_btnCopyDash->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Copy Dashboard / \345\244\215\345\210\266\344\273\252\350\241\250\347\233\230", nullptr));
        tabsMain->setTabText(tabsMain->indexOf(tabDashboard), QCoreApplication::translate("AdvancedSettingsDialog", "Dashboard / \344\273\252\350\241\250\347\233\230", nullptr));
        titleParams->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Pipeline Parameters / \347\256\241\347\272\277\345\217\202\346\225\260", nullptr));
        titleParams->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "cardTitle", nullptr)));
        txtSlots->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Input Arena Frames / \350\276\223\345\205\245\346\261\240\345\270\247\346\225\260:", nullptr));
        txtSlots->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        txtWorkers->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Worker Count / \346\216\250\347\220\206\347\272\277\347\250\213:", nullptr));
        txtWorkers->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_spinWorkerCount->setSpecialValueText(QCoreApplication::translate("AdvancedSettingsDialog", "Auto / \350\207\252\345\212\250", nullptr));
        txtStreams->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Parallel Infer / \345\271\266\350\241\214\346\216\250\347\220\206\346\225\260:", nullptr));
        txtStreams->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        txtBatchParam->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Worker Max Batch / \346\234\200\345\244\247\346\211\271\351\207\217:", nullptr));
        txtBatchParam->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        txtCtxPoolParam->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Context Pool / \344\270\212\344\270\213\346\226\207\346\261\240:", nullptr));
        txtCtxPoolParam->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_spinContextPool->setSpecialValueText(QCoreApplication::translate("AdvancedSettingsDialog", "Auto / \350\207\252\345\212\250", nullptr));
        txtStats->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Stats Interval / \347\273\237\350\256\241\351\227\264\351\232\224:", nullptr));
        txtStats->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_spinStatsInterval->setSuffix(QCoreApplication::translate("AdvancedSettingsDialog", " s", nullptr));
        titleModel->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Model & Classes / \346\250\241\345\236\213\344\270\216\347\261\273\345\210\253", nullptr));
        titleModel->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "cardTitle", nullptr)));
        txtModel->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Model / \346\250\241\345\236\213\350\267\257\345\276\204:", nullptr));
        txtModel->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_editModelPath->setPlaceholderText(QCoreApplication::translate("AdvancedSettingsDialog", "path/to/model.engine", nullptr));
        m_btnBrowseModel->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Browse...", nullptr));
        txtClasses->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Classes / \347\261\273\345\210\253\350\267\257\345\276\204:", nullptr));
        txtClasses->setProperty("role", QVariant(QCoreApplication::translate("AdvancedSettingsDialog", "rowTitle", nullptr)));
        m_editClassesPath->setPlaceholderText(QCoreApplication::translate("AdvancedSettingsDialog", "path/to/classes.txt", nullptr));
        m_btnBrowseClasses->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Browse...", nullptr));
        m_btnDefaults->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Restore Defaults / \346\201\242\345\244\215\351\273\230\350\256\244", nullptr));
        m_btnApply->setText(QCoreApplication::translate("AdvancedSettingsDialog", "Apply && Save / \345\272\224\347\224\250\345\271\266\344\277\235\345\255\230", nullptr));
        tabsMain->setTabText(tabsMain->indexOf(tabSettings), QCoreApplication::translate("AdvancedSettingsDialog", "Settings / \350\256\276\347\275\256", nullptr));
        (void)AdvancedSettingsDialog;
    } // retranslateUi

};

namespace Ui {
    class AdvancedSettingsDialog: public Ui_AdvancedSettingsDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ADVANCEDSETTINGSDIALOG_H
