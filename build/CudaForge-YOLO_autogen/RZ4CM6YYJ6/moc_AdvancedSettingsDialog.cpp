/****************************************************************************
** Meta object code from reading C++ file 'AdvancedSettingsDialog.hpp'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.2.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/widgets/AdvancedSettingsDialog.hpp"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'AdvancedSettingsDialog.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.2.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_AdvancedSettingsDialog_t {
    const uint offsetsAndSize[34];
    char stringdata0[225];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(offsetof(qt_meta_stringdata_AdvancedSettingsDialog_t, stringdata0) + ofs), len 
static const qt_meta_stringdata_AdvancedSettingsDialog_t qt_meta_stringdata_AdvancedSettingsDialog = {
    {
QT_MOC_LITERAL(0, 22), // "AdvancedSettingsDialog"
QT_MOC_LITERAL(23, 15), // "settingsApplied"
QT_MOC_LITERAL(39, 0), // ""
QT_MOC_LITERAL(40, 8), // "Settings"
QT_MOC_LITERAL(49, 1), // "s"
QT_MOC_LITERAL(51, 22), // "loadTestStartRequested"
QT_MOC_LITERAL(74, 13), // "useRealDecode"
QT_MOC_LITERAL(88, 11), // "numChannels"
QT_MOC_LITERAL(100, 9), // "targetFps"
QT_MOC_LITERAL(110, 10), // "layoutMode"
QT_MOC_LITERAL(121, 21), // "loadTestStopRequested"
QT_MOC_LITERAL(143, 7), // "onApply"
QT_MOC_LITERAL(151, 17), // "onRestoreDefaults"
QT_MOC_LITERAL(169, 16), // "refreshDashboard"
QT_MOC_LITERAL(186, 11), // "runLoadTest"
QT_MOC_LITERAL(198, 12), // "stopLoadTest"
QT_MOC_LITERAL(211, 13) // "copyDashboard"

    },
    "AdvancedSettingsDialog\0settingsApplied\0"
    "\0Settings\0s\0loadTestStartRequested\0"
    "useRealDecode\0numChannels\0targetFps\0"
    "layoutMode\0loadTestStopRequested\0"
    "onApply\0onRestoreDefaults\0refreshDashboard\0"
    "runLoadTest\0stopLoadTest\0copyDashboard"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_AdvancedSettingsDialog[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
       9,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       3,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,   68,    2, 0x06,    1 /* Public */,
       5,    4,   71,    2, 0x06,    3 /* Public */,
      10,    0,   80,    2, 0x06,    8 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
      11,    0,   81,    2, 0x08,    9 /* Private */,
      12,    0,   82,    2, 0x08,   10 /* Private */,
      13,    0,   83,    2, 0x08,   11 /* Private */,
      14,    0,   84,    2, 0x08,   12 /* Private */,
      15,    0,   85,    2, 0x08,   13 /* Private */,
      16,    0,   86,    2, 0x08,   14 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3,    4,
    QMetaType::Void, QMetaType::Bool, QMetaType::Int, QMetaType::Int, QMetaType::Int,    6,    7,    8,    9,
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,

       0        // eod
};

void AdvancedSettingsDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AdvancedSettingsDialog *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->settingsApplied((*reinterpret_cast< std::add_pointer_t<Settings>>(_a[1]))); break;
        case 1: _t->loadTestStartRequested((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[4]))); break;
        case 2: _t->loadTestStopRequested(); break;
        case 3: _t->onApply(); break;
        case 4: _t->onRestoreDefaults(); break;
        case 5: _t->refreshDashboard(); break;
        case 6: _t->runLoadTest(); break;
        case 7: _t->stopLoadTest(); break;
        case 8: _t->copyDashboard(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (AdvancedSettingsDialog::*)(const Settings & );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&AdvancedSettingsDialog::settingsApplied)) {
                *result = 0;
                return;
            }
        }
        {
            using _t = void (AdvancedSettingsDialog::*)(bool , int , int , int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&AdvancedSettingsDialog::loadTestStartRequested)) {
                *result = 1;
                return;
            }
        }
        {
            using _t = void (AdvancedSettingsDialog::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&AdvancedSettingsDialog::loadTestStopRequested)) {
                *result = 2;
                return;
            }
        }
    }
}

const QMetaObject AdvancedSettingsDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_AdvancedSettingsDialog.offsetsAndSize,
    qt_meta_data_AdvancedSettingsDialog,
    qt_static_metacall,
    nullptr,
qt_incomplete_metaTypeArray<qt_meta_stringdata_AdvancedSettingsDialog_t
, QtPrivate::TypeAndForceComplete<AdvancedSettingsDialog, std::true_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<const Settings &, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<bool, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>
, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>


>,
    nullptr
} };


const QMetaObject *AdvancedSettingsDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AdvancedSettingsDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_AdvancedSettingsDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int AdvancedSettingsDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 9)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 9;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 9)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 9;
    }
    return _id;
}

// SIGNAL 0
void AdvancedSettingsDialog::settingsApplied(const Settings & _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}

// SIGNAL 1
void AdvancedSettingsDialog::loadTestStartRequested(bool _t1, int _t2, int _t3, int _t4)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t3))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t4))) };
    QMetaObject::activate(this, &staticMetaObject, 1, _a);
}

// SIGNAL 2
void AdvancedSettingsDialog::loadTestStopRequested()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
