/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 6.2.4
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDoubleSpinBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QHBoxLayout *horizontalLayout_main;
    QGroupBox *groupBox_monitor;
    QVBoxLayout *verticalLayout_monitor;
    QStackedWidget *stackedWidget_view;
    QWidget *page_grid;
    QGridLayout *gridLayout_video;
    QWidget *page_analysis;
    QVBoxLayout *verticalLayout_analysis;
    QWidget *widget_analysis_display;
    QVBoxLayout *layout_analysis_video_placeholder;
    QFrame *frame_controls;
    QVBoxLayout *verticalLayout_controls;
    QWidget *widget_sidebar_top;
    QVBoxLayout *verticalLayout_sidebar_top;
    QGroupBox *groupBox_global;
    QFormLayout *formLayout;
    QLabel *label_layout;
    QComboBox *comboBox_layout;
    QLabel *label_conf;
    QDoubleSpinBox *doubleSpinBox_conf;
    QLabel *label_iou;
    QDoubleSpinBox *doubleSpinBox_iou;
    QGroupBox *groupBox_channel;
    QFormLayout *formLayout_2;
    QLabel *label_sourceType;
    QComboBox *comboBox_sourceType;
    QLabel *label_sourcePath;
    QHBoxLayout *horizontalLayout_source;
    QLineEdit *lineEdit_source;
    QPushButton *btn_browse;
    QLabel *label_selectChannel;
    QComboBox *comboBox_channelSelect;
    QPushButton *btn_start;
    QPushButton *btn_stop;
    QSpacerItem *verticalSpacer;
    QWidget *widget_sidebar_analysis;
    QVBoxLayout *verticalLayout_sidebar_analysis;
    QGroupBox *groupBox_video_controls;
    QVBoxLayout *verticalLayout_video_ctrl;
    QLabel *label_live_indicator;
    QPushButton *btn_pause;
    QPushButton *btn_snapshot;
    QLabel *label_seek;
    QSlider *slider_seek;
    QHBoxLayout *horizontalLayout_speed;
    QLabel *label_speed;
    QComboBox *comboBox_speed;
    QPushButton *btn_back_to_grid;
    QTabWidget *tabWidget_details;
    QWidget *tab_detections;
    QVBoxLayout *verticalLayout_detections;
    QTableWidget *tableWidget_detections;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(1280, 932);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName(QString::fromUtf8("centralwidget"));
        horizontalLayout_main = new QHBoxLayout(centralwidget);
        horizontalLayout_main->setSpacing(12);
        horizontalLayout_main->setObjectName(QString::fromUtf8("horizontalLayout_main"));
        horizontalLayout_main->setContentsMargins(10, 10, 10, 10);
        groupBox_monitor = new QGroupBox(centralwidget);
        groupBox_monitor->setObjectName(QString::fromUtf8("groupBox_monitor"));
        verticalLayout_monitor = new QVBoxLayout(groupBox_monitor);
        verticalLayout_monitor->setObjectName(QString::fromUtf8("verticalLayout_monitor"));
        verticalLayout_monitor->setContentsMargins(4, 24, 4, 4);
        stackedWidget_view = new QStackedWidget(groupBox_monitor);
        stackedWidget_view->setObjectName(QString::fromUtf8("stackedWidget_view"));
        page_grid = new QWidget();
        page_grid->setObjectName(QString::fromUtf8("page_grid"));
        gridLayout_video = new QGridLayout(page_grid);
        gridLayout_video->setObjectName(QString::fromUtf8("gridLayout_video"));
        gridLayout_video->setContentsMargins(0, 8, 0, 0);
        stackedWidget_view->addWidget(page_grid);
        page_analysis = new QWidget();
        page_analysis->setObjectName(QString::fromUtf8("page_analysis"));
        verticalLayout_analysis = new QVBoxLayout(page_analysis);
        verticalLayout_analysis->setSpacing(0);
        verticalLayout_analysis->setObjectName(QString::fromUtf8("verticalLayout_analysis"));
        verticalLayout_analysis->setContentsMargins(0, 0, 0, 0);
        widget_analysis_display = new QWidget(page_analysis);
        widget_analysis_display->setObjectName(QString::fromUtf8("widget_analysis_display"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(widget_analysis_display->sizePolicy().hasHeightForWidth());
        widget_analysis_display->setSizePolicy(sizePolicy);
        layout_analysis_video_placeholder = new QVBoxLayout(widget_analysis_display);
        layout_analysis_video_placeholder->setObjectName(QString::fromUtf8("layout_analysis_video_placeholder"));

        verticalLayout_analysis->addWidget(widget_analysis_display);

        stackedWidget_view->addWidget(page_analysis);

        verticalLayout_monitor->addWidget(stackedWidget_view);


        horizontalLayout_main->addWidget(groupBox_monitor);

        frame_controls = new QFrame(centralwidget);
        frame_controls->setObjectName(QString::fromUtf8("frame_controls"));
        frame_controls->setMinimumSize(QSize(280, 0));
        frame_controls->setMaximumSize(QSize(320, 16777215));
        frame_controls->setFrameShape(QFrame::StyledPanel);
        frame_controls->setFrameShadow(QFrame::Raised);
        verticalLayout_controls = new QVBoxLayout(frame_controls);
        verticalLayout_controls->setObjectName(QString::fromUtf8("verticalLayout_controls"));
        widget_sidebar_top = new QWidget(frame_controls);
        widget_sidebar_top->setObjectName(QString::fromUtf8("widget_sidebar_top"));
        verticalLayout_sidebar_top = new QVBoxLayout(widget_sidebar_top);
        verticalLayout_sidebar_top->setObjectName(QString::fromUtf8("verticalLayout_sidebar_top"));
        verticalLayout_sidebar_top->setContentsMargins(0, 0, 0, 0);
        groupBox_global = new QGroupBox(widget_sidebar_top);
        groupBox_global->setObjectName(QString::fromUtf8("groupBox_global"));
        formLayout = new QFormLayout(groupBox_global);
        formLayout->setObjectName(QString::fromUtf8("formLayout"));
        label_layout = new QLabel(groupBox_global);
        label_layout->setObjectName(QString::fromUtf8("label_layout"));

        formLayout->setWidget(0, QFormLayout::LabelRole, label_layout);

        comboBox_layout = new QComboBox(groupBox_global);
        comboBox_layout->addItem(QString());
        comboBox_layout->addItem(QString());
        comboBox_layout->addItem(QString());
        comboBox_layout->setObjectName(QString::fromUtf8("comboBox_layout"));

        formLayout->setWidget(0, QFormLayout::FieldRole, comboBox_layout);

        label_conf = new QLabel(groupBox_global);
        label_conf->setObjectName(QString::fromUtf8("label_conf"));

        formLayout->setWidget(1, QFormLayout::LabelRole, label_conf);

        doubleSpinBox_conf = new QDoubleSpinBox(groupBox_global);
        doubleSpinBox_conf->setObjectName(QString::fromUtf8("doubleSpinBox_conf"));
        doubleSpinBox_conf->setMaximum(1.000000000000000);
        doubleSpinBox_conf->setSingleStep(0.050000000000000);
        doubleSpinBox_conf->setValue(0.250000000000000);

        formLayout->setWidget(1, QFormLayout::FieldRole, doubleSpinBox_conf);

        label_iou = new QLabel(groupBox_global);
        label_iou->setObjectName(QString::fromUtf8("label_iou"));

        formLayout->setWidget(2, QFormLayout::LabelRole, label_iou);

        doubleSpinBox_iou = new QDoubleSpinBox(groupBox_global);
        doubleSpinBox_iou->setObjectName(QString::fromUtf8("doubleSpinBox_iou"));
        doubleSpinBox_iou->setMaximum(1.000000000000000);
        doubleSpinBox_iou->setSingleStep(0.050000000000000);
        doubleSpinBox_iou->setValue(0.450000000000000);

        formLayout->setWidget(2, QFormLayout::FieldRole, doubleSpinBox_iou);


        verticalLayout_sidebar_top->addWidget(groupBox_global);

        groupBox_channel = new QGroupBox(widget_sidebar_top);
        groupBox_channel->setObjectName(QString::fromUtf8("groupBox_channel"));
        formLayout_2 = new QFormLayout(groupBox_channel);
        formLayout_2->setObjectName(QString::fromUtf8("formLayout_2"));
        label_sourceType = new QLabel(groupBox_channel);
        label_sourceType->setObjectName(QString::fromUtf8("label_sourceType"));

        formLayout_2->setWidget(1, QFormLayout::LabelRole, label_sourceType);

        comboBox_sourceType = new QComboBox(groupBox_channel);
        comboBox_sourceType->addItem(QString());
        comboBox_sourceType->addItem(QString());
        comboBox_sourceType->addItem(QString());
        comboBox_sourceType->addItem(QString());
        comboBox_sourceType->setObjectName(QString::fromUtf8("comboBox_sourceType"));

        formLayout_2->setWidget(1, QFormLayout::FieldRole, comboBox_sourceType);

        label_sourcePath = new QLabel(groupBox_channel);
        label_sourcePath->setObjectName(QString::fromUtf8("label_sourcePath"));

        formLayout_2->setWidget(2, QFormLayout::LabelRole, label_sourcePath);

        horizontalLayout_source = new QHBoxLayout();
        horizontalLayout_source->setObjectName(QString::fromUtf8("horizontalLayout_source"));
        lineEdit_source = new QLineEdit(groupBox_channel);
        lineEdit_source->setObjectName(QString::fromUtf8("lineEdit_source"));

        horizontalLayout_source->addWidget(lineEdit_source);

        btn_browse = new QPushButton(groupBox_channel);
        btn_browse->setObjectName(QString::fromUtf8("btn_browse"));
        btn_browse->setMaximumSize(QSize(30, 16777215));

        horizontalLayout_source->addWidget(btn_browse);


        formLayout_2->setLayout(2, QFormLayout::FieldRole, horizontalLayout_source);

        label_selectChannel = new QLabel(groupBox_channel);
        label_selectChannel->setObjectName(QString::fromUtf8("label_selectChannel"));

        formLayout_2->setWidget(0, QFormLayout::LabelRole, label_selectChannel);

        comboBox_channelSelect = new QComboBox(groupBox_channel);
        comboBox_channelSelect->setObjectName(QString::fromUtf8("comboBox_channelSelect"));

        formLayout_2->setWidget(0, QFormLayout::FieldRole, comboBox_channelSelect);


        verticalLayout_sidebar_top->addWidget(groupBox_channel);

        btn_start = new QPushButton(widget_sidebar_top);
        btn_start->setObjectName(QString::fromUtf8("btn_start"));
        btn_start->setMinimumSize(QSize(0, 40));

        verticalLayout_sidebar_top->addWidget(btn_start);

        btn_stop = new QPushButton(widget_sidebar_top);
        btn_stop->setObjectName(QString::fromUtf8("btn_stop"));
        btn_stop->setMinimumSize(QSize(0, 40));

        verticalLayout_sidebar_top->addWidget(btn_stop);

        verticalSpacer = new QSpacerItem(20, 12, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_sidebar_top->addItem(verticalSpacer);


        verticalLayout_controls->addWidget(widget_sidebar_top);

        widget_sidebar_analysis = new QWidget(frame_controls);
        widget_sidebar_analysis->setObjectName(QString::fromUtf8("widget_sidebar_analysis"));
        verticalLayout_sidebar_analysis = new QVBoxLayout(widget_sidebar_analysis);
        verticalLayout_sidebar_analysis->setObjectName(QString::fromUtf8("verticalLayout_sidebar_analysis"));
        verticalLayout_sidebar_analysis->setContentsMargins(0, 0, 0, 0);
        groupBox_video_controls = new QGroupBox(widget_sidebar_analysis);
        groupBox_video_controls->setObjectName(QString::fromUtf8("groupBox_video_controls"));
        verticalLayout_video_ctrl = new QVBoxLayout(groupBox_video_controls);
        verticalLayout_video_ctrl->setObjectName(QString::fromUtf8("verticalLayout_video_ctrl"));
        label_live_indicator = new QLabel(groupBox_video_controls);
        label_live_indicator->setObjectName(QString::fromUtf8("label_live_indicator"));
        label_live_indicator->setAlignment(Qt::AlignCenter);

        verticalLayout_video_ctrl->addWidget(label_live_indicator);

        btn_pause = new QPushButton(groupBox_video_controls);
        btn_pause->setObjectName(QString::fromUtf8("btn_pause"));
        btn_pause->setMinimumSize(QSize(0, 30));
        QIcon icon;
        QString iconThemeName = QString::fromUtf8("media-playback-pause");
        if (QIcon::hasThemeIcon(iconThemeName)) {
            icon = QIcon::fromTheme(iconThemeName);
        } else {
            icon.addFile(QString::fromUtf8("."), QSize(), QIcon::Normal, QIcon::Off);
        }
        btn_pause->setIcon(icon);
        btn_pause->setCheckable(true);

        verticalLayout_video_ctrl->addWidget(btn_pause);

        btn_snapshot = new QPushButton(groupBox_video_controls);
        btn_snapshot->setObjectName(QString::fromUtf8("btn_snapshot"));
        btn_snapshot->setMinimumSize(QSize(0, 30));
        QIcon icon1;
        iconThemeName = QString::fromUtf8("camera-photo");
        if (QIcon::hasThemeIcon(iconThemeName)) {
            icon1 = QIcon::fromTheme(iconThemeName);
        } else {
            icon1.addFile(QString::fromUtf8("."), QSize(), QIcon::Normal, QIcon::Off);
        }
        btn_snapshot->setIcon(icon1);

        verticalLayout_video_ctrl->addWidget(btn_snapshot);

        label_seek = new QLabel(groupBox_video_controls);
        label_seek->setObjectName(QString::fromUtf8("label_seek"));

        verticalLayout_video_ctrl->addWidget(label_seek);

        slider_seek = new QSlider(groupBox_video_controls);
        slider_seek->setObjectName(QString::fromUtf8("slider_seek"));
        slider_seek->setOrientation(Qt::Horizontal);

        verticalLayout_video_ctrl->addWidget(slider_seek);

        horizontalLayout_speed = new QHBoxLayout();
        horizontalLayout_speed->setObjectName(QString::fromUtf8("horizontalLayout_speed"));
        label_speed = new QLabel(groupBox_video_controls);
        label_speed->setObjectName(QString::fromUtf8("label_speed"));

        horizontalLayout_speed->addWidget(label_speed);

        comboBox_speed = new QComboBox(groupBox_video_controls);
        comboBox_speed->addItem(QString());
        comboBox_speed->addItem(QString());
        comboBox_speed->addItem(QString());
        comboBox_speed->setObjectName(QString::fromUtf8("comboBox_speed"));

        horizontalLayout_speed->addWidget(comboBox_speed);


        verticalLayout_video_ctrl->addLayout(horizontalLayout_speed);


        verticalLayout_sidebar_analysis->addWidget(groupBox_video_controls);

        btn_back_to_grid = new QPushButton(widget_sidebar_analysis);
        btn_back_to_grid->setObjectName(QString::fromUtf8("btn_back_to_grid"));
        btn_back_to_grid->setMinimumSize(QSize(0, 40));

        verticalLayout_sidebar_analysis->addWidget(btn_back_to_grid);


        verticalLayout_controls->addWidget(widget_sidebar_analysis);

        tabWidget_details = new QTabWidget(frame_controls);
        tabWidget_details->setObjectName(QString::fromUtf8("tabWidget_details"));
        sizePolicy.setHeightForWidth(tabWidget_details->sizePolicy().hasHeightForWidth());
        tabWidget_details->setSizePolicy(sizePolicy);
        tab_detections = new QWidget();
        tab_detections->setObjectName(QString::fromUtf8("tab_detections"));
        verticalLayout_detections = new QVBoxLayout(tab_detections);
        verticalLayout_detections->setObjectName(QString::fromUtf8("verticalLayout_detections"));
        verticalLayout_detections->setContentsMargins(0, 8, 0, 0);
        tableWidget_detections = new QTableWidget(tab_detections);
        if (tableWidget_detections->columnCount() < 3)
            tableWidget_detections->setColumnCount(3);
        QTableWidgetItem *__qtablewidgetitem = new QTableWidgetItem();
        tableWidget_detections->setHorizontalHeaderItem(0, __qtablewidgetitem);
        QTableWidgetItem *__qtablewidgetitem1 = new QTableWidgetItem();
        tableWidget_detections->setHorizontalHeaderItem(1, __qtablewidgetitem1);
        QTableWidgetItem *__qtablewidgetitem2 = new QTableWidgetItem();
        tableWidget_detections->setHorizontalHeaderItem(2, __qtablewidgetitem2);
        tableWidget_detections->setObjectName(QString::fromUtf8("tableWidget_detections"));
        tableWidget_detections->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableWidget_detections->setColumnCount(3);

        verticalLayout_detections->addWidget(tableWidget_detections);

        tabWidget_details->addTab(tab_detections, QString());

        verticalLayout_controls->addWidget(tabWidget_details);


        horizontalLayout_main->addWidget(frame_controls);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName(QString::fromUtf8("menubar"));
        menubar->setGeometry(QRect(0, 0, 1280, 25));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName(QString::fromUtf8("statusbar"));
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        stackedWidget_view->setCurrentIndex(0);
        comboBox_layout->setCurrentIndex(1);
        tabWidget_details->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "CudaForge-YOLO - Steel Defect Detection System", nullptr));
        MainWindow->setStyleSheet(QCoreApplication::translate("MainWindow", "/* Gem-gray + Jade Light Theme */\n"
"QMainWindow, QWidget {\n"
"    background-color: #ecf3f0;\n"
"    color: #334155;\n"
"    font-family: \"Segoe UI\", \"Microsoft YaHei\", sans-serif;\n"
"    font-size: 10pt;\n"
"}\n"
"\n"
"QWidget#centralwidget {\n"
"    background: qlineargradient(x1:0, y1:0, x2:1, y2:1,\n"
"        stop:0 #eef4f1,\n"
"        stop:0.6 #e8f0ed,\n"
"        stop:1 #dfe9e5);\n"
"}\n"
"\n"
"QMenuBar {\n"
"    background: #dfe9e5;\n"
"    border: none;\n"
"    border-bottom: 1px solid #c5d7d0;\n"
"    color: #2f4350;\n"
"    font-weight: 700;\n"
"}\n"
"\n"
"QWidget#customTitleBarMain {\n"
"    background: #dfe9e5;\n"
"    border: none;\n"
"    border-bottom: 2px solid #c5d7d0;\n"
"}\n"
"QLabel#mainTitleLabel {\n"
"    color: #2f4350;\n"
"    font-size: 13px;\n"
"    font-weight: 800;\n"
"    letter-spacing: 0.5px;\n"
"}\n"
"QPushButton#titleMinButton,\n"
"QPushButton#titleMaxButton,\n"
"QPushButton#titleCloseButtonMain {\n"
"    min-width: 24px;\n"
"    max-width: 24px;\n"
"    min-height: 2"
                        "4px;\n"
"    max-height: 24px;\n"
"    border-radius: 6px;\n"
"    font-size: 12px;\n"
"    padding: 0px;\n"
"    font-weight: 700;\n"
"}\n"
"QPushButton#titleMinButton,\n"
"QPushButton#titleMaxButton {\n"
"    background: #edf4f1;\n"
"    color: #355260;\n"
"    border: 1px solid #b9ccc5;\n"
"}\n"
"QPushButton#titleMinButton:hover,\n"
"QPushButton#titleMaxButton:hover {\n"
"    background: #dcebe5;\n"
"    border: 1px solid #a7bfb6;\n"
"}\n"
"QPushButton#titleCloseButtonMain {\n"
"    background: #fbe3e1;\n"
"    color: #B91C1C;\n"
"    border: 1px solid #edc9c5;\n"
"}\n"
"QPushButton#titleCloseButtonMain:hover {\n"
"    background: #f7d4d0;\n"
"    border: 1px solid #e5b9b2;\n"
"}\n"
"QMenuBar::item {\n"
"    background: transparent;\n"
"    padding: 6px 10px;\n"
"    border-radius: 6px;\n"
"}\n"
"QMenuBar::item:selected {\n"
"    background: rgba(224, 237, 231, 0.95);\n"
"}\n"
"\n"
"QFrame#frame_controls {\n"
"    background: rgba(235, 243, 239, 0.95);\n"
"    border: 1px solid #c3d4ce;\n"
"    border-radiu"
                        "s: 12px;\n"
"}\n"
"\n"
"QGroupBox {\n"
"    border: 1px solid #c2d3cd;\n"
"    border-radius: 10px;\n"
"    margin-top: 32px;\n"
"    font-weight: 700;\n"
"    background-color: rgba(241, 248, 245, 0.95);\n"
"}\n"
"QGroupBox::title {\n"
"    subcontrol-origin: margin;\n"
"    subcontrol-position: top left;\n"
"    left: 12px;\n"
"    top: 0px;\n"
"    padding: 3px 10px;\n"
"    color: #2e4c57;\n"
"    background-color: rgba(223, 236, 230, 0.95);\n"
"    border: 1px solid #b8ccc4;\n"
"    border-radius: 8px;\n"
"    font-size: 10pt;\n"
"}\n"
"\n"
"QPushButton {\n"
"    background-color: rgba(236, 246, 242, 0.98);\n"
"    color: #2f4652;\n"
"    border: 1px solid #b9cdc6;\n"
"    padding: 7px 14px;\n"
"    border-radius: 7px;\n"
"    font-weight: 700;\n"
"}\n"
"QPushButton:hover {\n"
"    background-color: rgba(224, 239, 233, 0.98);\n"
"    border: 1px solid #9ebdb3;\n"
"}\n"
"QPushButton:pressed {\n"
"    background-color: rgba(212, 230, 222, 0.98);\n"
"}\n"
"\n"
"QPushButton#btn_start {\n"
"    background-colo"
                        "r: #7faea0;\n"
"    color: #ffffff;\n"
"    border: 1px solid #6d998c;\n"
"}\n"
"QPushButton#btn_start:hover { background-color: #6f9f91; }\n"
"\n"
"QPushButton#btn_stop {\n"
"    background-color: #d26d6d;\n"
"    color: #ffffff;\n"
"    border: 1px solid #be5f5f;\n"
"}\n"
"QPushButton#btn_stop:hover { background-color: #c95f5f; }\n"
"\n"
"QSpinBox, QDoubleSpinBox, QComboBox, QTextEdit, QLineEdit {\n"
"    background-color: rgba(255, 255, 255, 0.93);\n"
"    border: 1px solid #bfd0ca;\n"
"    color: #324654;\n"
"    padding: 4px;\n"
"    border-radius: 6px;\n"
"}\n"
"\n"
"QTabWidget::pane {\n"
"    border: 1px solid #c4d5cf;\n"
"    border-radius: 8px;\n"
"    background: rgba(236, 245, 241, 0.85);\n"
"}\n"
"QTabBar::tab {\n"
"    background: transparent;\n"
"    color: #617686;\n"
"    padding: 7px 14px;\n"
"    border-bottom: none;\n"
"    border-radius: 6px;\n"
"    font-weight: 700;\n"
"}\n"
"QTabBar::tab:selected {\n"
"    color: #2f5c52;\n"
"    background: rgba(218, 235, 229, 0.95);\n"
"    border: 1px"
                        " solid #b3cbc2;\n"
"}\n"
"\n"
"QPushButton#btn_back_to_grid,\n"
"QPushButton#btn_pause,\n"
"QPushButton#btn_snapshot {\n"
"    background-color: #7faea0;\n"
"    color: #ffffff;\n"
"    border: 1px solid #6d998c;\n"
"    font-weight: 700;\n"
"}\n"
"QPushButton#btn_back_to_grid:hover,\n"
"QPushButton#btn_pause:hover,\n"
"QPushButton#btn_snapshot:hover {\n"
"    background-color: #6f9f91;\n"
"    border: 1px solid #608a7e;\n"
"}\n"
"\n"
"QTableWidget {\n"
"    background: rgba(247, 252, 250, 0.95);\n"
"    border: 1px solid #c3d4ce;\n"
"    gridline-color: #d9e6e1;\n"
"    alternate-background-color: #f1f8f5;\n"
"}\n"
"QHeaderView::section {\n"
"    background: rgba(223, 236, 230, 0.95);\n"
"    color: #3d5563;\n"
"    border: none;\n"
"    border-right: 1px solid #c6d6d0;\n"
"    padding: 6px;\n"
"    font-weight: 700;\n"
"}", nullptr));
        groupBox_monitor->setTitle(QCoreApplication::translate("MainWindow", "Real-time Monitoring (4-Channel)", nullptr));
        widget_analysis_display->setStyleSheet(QCoreApplication::translate("MainWindow", "background: qlineargradient(x1:0, y1:0, x2:1, y2:1,\n"
"    stop:0 #e7efec,\n"
"    stop:1 #dbe7e2);\n"
"border: 1px dashed #aabfb7;\n"
"border-radius: 10px;", nullptr));
        groupBox_global->setTitle(QCoreApplication::translate("MainWindow", "Global Settings", nullptr));
        label_layout->setText(QCoreApplication::translate("MainWindow", "View Layout:", nullptr));
        comboBox_layout->setItemText(0, QCoreApplication::translate("MainWindow", "1x1 Single Focus", nullptr));
        comboBox_layout->setItemText(1, QCoreApplication::translate("MainWindow", "2x2 Quad View", nullptr));
        comboBox_layout->setItemText(2, QCoreApplication::translate("MainWindow", "3x3 Grid View", nullptr));

        label_conf->setText(QCoreApplication::translate("MainWindow", "Conf Thres:", nullptr));
        label_iou->setText(QCoreApplication::translate("MainWindow", "IOU Thres:", nullptr));
        groupBox_channel->setTitle(QCoreApplication::translate("MainWindow", "Channel Settings", nullptr));
        label_sourceType->setText(QCoreApplication::translate("MainWindow", "Source Type:", nullptr));
        comboBox_sourceType->setItemText(0, QCoreApplication::translate("MainWindow", "Video File (MP4/AVI)", nullptr));
        comboBox_sourceType->setItemText(1, QCoreApplication::translate("MainWindow", "RTSP Stream", nullptr));
        comboBox_sourceType->setItemText(2, QCoreApplication::translate("MainWindow", "USB Camera", nullptr));
        comboBox_sourceType->setItemText(3, QCoreApplication::translate("MainWindow", "Image File (JPG/PNG)", nullptr));

        label_sourcePath->setText(QCoreApplication::translate("MainWindow", "Source Path:", nullptr));
        lineEdit_source->setPlaceholderText(QCoreApplication::translate("MainWindow", "File path", nullptr));
        btn_browse->setText(QCoreApplication::translate("MainWindow", "...", nullptr));
        label_selectChannel->setText(QCoreApplication::translate("MainWindow", "Select CAM:", nullptr));
#if QT_CONFIG(tooltip)
        comboBox_channelSelect->setToolTip(QCoreApplication::translate("MainWindow", "Select which camera to configure", nullptr));
#endif // QT_CONFIG(tooltip)
        btn_start->setText(QCoreApplication::translate("MainWindow", "START DETECTION", nullptr));
        btn_stop->setText(QCoreApplication::translate("MainWindow", "STOP", nullptr));
        groupBox_video_controls->setTitle(QCoreApplication::translate("MainWindow", "Video Controls", nullptr));
        label_live_indicator->setStyleSheet(QCoreApplication::translate("MainWindow", "color: #ff4444; font-weight: bold;", nullptr));
        label_live_indicator->setText(QCoreApplication::translate("MainWindow", "\360\237\224\264 LIVE STREAM", nullptr));
        btn_pause->setText(QCoreApplication::translate("MainWindow", "Pause", nullptr));
        btn_snapshot->setText(QCoreApplication::translate("MainWindow", "Snapshot", nullptr));
        label_seek->setText(QCoreApplication::translate("MainWindow", "Progress:", nullptr));
        label_speed->setText(QCoreApplication::translate("MainWindow", "Speed:", nullptr));
        comboBox_speed->setItemText(0, QCoreApplication::translate("MainWindow", "0.5x", nullptr));
        comboBox_speed->setItemText(1, QCoreApplication::translate("MainWindow", "1.0x", nullptr));
        comboBox_speed->setItemText(2, QCoreApplication::translate("MainWindow", "2.0x", nullptr));

        btn_back_to_grid->setText(QCoreApplication::translate("MainWindow", "Back to Grid", nullptr));
        QTableWidgetItem *___qtablewidgetitem = tableWidget_detections->horizontalHeaderItem(0);
        ___qtablewidgetitem->setText(QCoreApplication::translate("MainWindow", "Class", nullptr));
        QTableWidgetItem *___qtablewidgetitem1 = tableWidget_detections->horizontalHeaderItem(1);
        ___qtablewidgetitem1->setText(QCoreApplication::translate("MainWindow", "Conf", nullptr));
        QTableWidgetItem *___qtablewidgetitem2 = tableWidget_detections->horizontalHeaderItem(2);
        ___qtablewidgetitem2->setText(QCoreApplication::translate("MainWindow", "Location", nullptr));
        tabWidget_details->setTabText(tabWidget_details->indexOf(tab_detections), QCoreApplication::translate("MainWindow", "Detections", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
