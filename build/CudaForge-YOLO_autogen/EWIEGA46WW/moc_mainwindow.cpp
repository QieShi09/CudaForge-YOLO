/****************************************************************************
** Meta object code from reading C++ file 'mainwindow.h'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.2.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../mainwindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'mainwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.2.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_MainWindow_t {
    const uint offsetsAndSize[42];
    char stringdata0[471];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(offsetof(qt_meta_stringdata_MainWindow_t, stringdata0) + ofs), len 
static const qt_meta_stringdata_MainWindow_t qt_meta_stringdata_MainWindow = {
    {
QT_MOC_LITERAL(0, 10), // "MainWindow"
QT_MOC_LITERAL(11, 38), // "on_comboBox_layout_currentInd..."
QT_MOC_LITERAL(50, 0), // ""
QT_MOC_LITERAL(51, 5), // "index"
QT_MOC_LITERAL(57, 21), // "on_btn_browse_clicked"
QT_MOC_LITERAL(79, 42), // "on_comboBox_sourceType_curren..."
QT_MOC_LITERAL(122, 45), // "on_comboBox_channelSelect_cur..."
QT_MOC_LITERAL(168, 30), // "on_lineEdit_source_textChanged"
QT_MOC_LITERAL(199, 4), // "arg1"
QT_MOC_LITERAL(204, 27), // "on_btn_back_to_grid_clicked"
QT_MOC_LITERAL(232, 20), // "on_btn_pause_toggled"
QT_MOC_LITERAL(253, 7), // "checked"
QT_MOC_LITERAL(261, 23), // "on_btn_snapshot_clicked"
QT_MOC_LITERAL(285, 20), // "on_btn_start_clicked"
QT_MOC_LITERAL(306, 19), // "on_btn_stop_clicked"
QT_MOC_LITERAL(326, 23), // "onChannelCloseRequested"
QT_MOC_LITERAL(350, 37), // "on_comboBox_speed_currentInde..."
QT_MOC_LITERAL(388, 29), // "on_slider_seek_sliderReleased"
QT_MOC_LITERAL(418, 18), // "onPlaybackFinished"
QT_MOC_LITERAL(437, 10), // "channel_id"
QT_MOC_LITERAL(448, 22) // "onVideoReplayRequested"

    },
    "MainWindow\0on_comboBox_layout_currentIndexChanged\0"
    "\0index\0on_btn_browse_clicked\0"
    "on_comboBox_sourceType_currentIndexChanged\0"
    "on_comboBox_channelSelect_currentIndexChanged\0"
    "on_lineEdit_source_textChanged\0arg1\0"
    "on_btn_back_to_grid_clicked\0"
    "on_btn_pause_toggled\0checked\0"
    "on_btn_snapshot_clicked\0on_btn_start_clicked\0"
    "on_btn_stop_clicked\0onChannelCloseRequested\0"
    "on_comboBox_speed_currentIndexChanged\0"
    "on_slider_seek_sliderReleased\0"
    "onPlaybackFinished\0channel_id\0"
    "onVideoReplayRequested"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_MainWindow[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      15,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,  104,    2, 0x08,    1 /* Private */,
       4,    0,  107,    2, 0x08,    3 /* Private */,
       5,    1,  108,    2, 0x08,    4 /* Private */,
       6,    1,  111,    2, 0x08,    6 /* Private */,
       7,    1,  114,    2, 0x08,    8 /* Private */,
       9,    0,  117,    2, 0x08,   10 /* Private */,
      10,    1,  118,    2, 0x08,   11 /* Private */,
      12,    0,  121,    2, 0x08,   13 /* Private */,
      13,    0,  122,    2, 0x08,   14 /* Private */,
      14,    0,  123,    2, 0x08,   15 /* Private */,
      15,    1,  124,    2, 0x08,   16 /* Private */,
      16,    1,  127,    2, 0x08,   18 /* Private */,
      17,    0,  130,    2, 0x08,   20 /* Private */,
      18,    1,  131,    2, 0x08,   21 /* Private */,
      20,    1,  134,    2, 0x08,   23 /* Private */,

 // slots: parameters
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::QString,    8,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   11,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void, QMetaType::Int,    3,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   19,
    QMetaType::Void, QMetaType::Int,   19,

       0        // eod
};

void MainWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<MainWindow *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->on_comboBox_layout_currentIndexChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->on_btn_browse_clicked(); break;
        case 2: _t->on_comboBox_sourceType_currentIndexChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 3: _t->on_comboBox_channelSelect_currentIndexChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 4: _t->on_lineEdit_source_textChanged((*reinterpret_cast< std::add_pointer_t<QString>>(_a[1]))); break;
        case 5: _t->on_btn_back_to_grid_clicked(); break;
        case 6: _t->on_btn_pause_toggled((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 7: _t->on_btn_snapshot_clicked(); break;
        case 8: _t->on_btn_start_clicked(); break;
        case 9: _t->on_btn_stop_clicked(); break;
        case 10: _t->onChannelCloseRequested((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 11: _t->on_comboBox_speed_currentIndexChanged((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 12: _t->on_slider_seek_sliderReleased(); break;
        case 13: _t->onPlaybackFinished((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 14: _t->onVideoReplayRequested((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        default: ;
        }
    }
}

const QMetaObject MainWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_MainWindow.offsetsAndSize,
    qt_meta_data_MainWindow,
    qt_static_metacall,
    nullptr,
qt_incomplete_metaTypeArray<qt_meta_stringdata_MainWindow_t
, QtPrivate::TypeAndForceComplete<MainWindow, std::true_type>
, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<const QString &, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<bool, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>


>,
    nullptr
} };


const QMetaObject *MainWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *MainWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_MainWindow.stringdata0))
        return static_cast<void*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int MainWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 15)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 15;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 15)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 15;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
