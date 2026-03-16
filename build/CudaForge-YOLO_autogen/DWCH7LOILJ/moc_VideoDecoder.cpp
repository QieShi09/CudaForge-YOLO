/****************************************************************************
** Meta object code from reading C++ file 'VideoDecoder.hpp'
**
** Created by: The Qt Meta Object Compiler version 68 (Qt 6.2.4)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../../../src/video/VideoDecoder.hpp"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'VideoDecoder.hpp' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 68
#error "This file was generated using the moc from 6.2.4. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_VideoDecoder_t {
    const uint offsetsAndSize[98];
    char stringdata0[629];
};
#define QT_MOC_LITERAL(ofs, len) \
    uint(offsetof(qt_meta_stringdata_VideoDecoder_t, stringdata0) + ofs), len 
static const qt_meta_stringdata_VideoDecoder_t qt_meta_stringdata_VideoDecoder = {
    {
QT_MOC_LITERAL(0, 12), // "VideoDecoder"
QT_MOC_LITERAL(13, 16), // "playbackFinished"
QT_MOC_LITERAL(30, 0), // ""
QT_MOC_LITERAL(31, 10), // "channel_id"
QT_MOC_LITERAL(42, 13), // "startDecoding"
QT_MOC_LITERAL(56, 12), // "stopDecoding"
QT_MOC_LITERAL(69, 14), // "setDisplaySize"
QT_MOC_LITERAL(84, 5), // "width"
QT_MOC_LITERAL(90, 6), // "height"
QT_MOC_LITERAL(97, 13), // "setLowFPSMode"
QT_MOC_LITERAL(111, 3), // "low"
QT_MOC_LITERAL(115, 12), // "setTargetFPS"
QT_MOC_LITERAL(128, 3), // "fps"
QT_MOC_LITERAL(132, 21), // "setAllowOverNativeFPS"
QT_MOC_LITERAL(154, 5), // "allow"
QT_MOC_LITERAL(160, 12), // "getTargetFPS"
QT_MOC_LITERAL(173, 12), // "getNativeFPS"
QT_MOC_LITERAL(186, 9), // "setPaused"
QT_MOC_LITERAL(196, 6), // "paused"
QT_MOC_LITERAL(203, 8), // "setSpeed"
QT_MOC_LITERAL(212, 5), // "speed"
QT_MOC_LITERAL(218, 4), // "seek"
QT_MOC_LITERAL(223, 7), // "int64_t"
QT_MOC_LITERAL(231, 12), // "timestamp_ms"
QT_MOC_LITERAL(244, 11), // "getDuration"
QT_MOC_LITERAL(256, 18), // "getCurrentPosition"
QT_MOC_LITERAL(275, 5), // "isEOF"
QT_MOC_LITERAL(281, 10), // "isFileMode"
QT_MOC_LITERAL(292, 15), // "setChannelEpoch"
QT_MOC_LITERAL(308, 8), // "uint64_t"
QT_MOC_LITERAL(317, 5), // "epoch"
QT_MOC_LITERAL(323, 15), // "initDisplayPool"
QT_MOC_LITERAL(339, 6), // "size_t"
QT_MOC_LITERAL(346, 18), // "releaseDisplayPool"
QT_MOC_LITERAL(365, 24), // "getDisplayBufferFromPool"
QT_MOC_LITERAL(390, 8), // "uint8_t*"
QT_MOC_LITERAL(399, 25), // "returnDisplayBufferToPool"
QT_MOC_LITERAL(425, 3), // "ptr"
QT_MOC_LITERAL(429, 28), // "releaseDisplayBufferCallback"
QT_MOC_LITERAL(458, 6), // "opaque"
QT_MOC_LITERAL(465, 4), // "data"
QT_MOC_LITERAL(470, 21), // "totalDecoderVramBytes"
QT_MOC_LITERAL(492, 29), // "totalStandaloneFrameVramBytes"
QT_MOC_LITERAL(522, 28), // "registerStandaloneFrameAlloc"
QT_MOC_LITERAL(551, 5), // "bytes"
QT_MOC_LITERAL(557, 27), // "registerStandaloneFrameFree"
QT_MOC_LITERAL(585, 14), // "hwDecoderCount"
QT_MOC_LITERAL(600, 14), // "swDecoderCount"
QT_MOC_LITERAL(615, 13) // "maxHwDecoders"

    },
    "VideoDecoder\0playbackFinished\0\0"
    "channel_id\0startDecoding\0stopDecoding\0"
    "setDisplaySize\0width\0height\0setLowFPSMode\0"
    "low\0setTargetFPS\0fps\0setAllowOverNativeFPS\0"
    "allow\0getTargetFPS\0getNativeFPS\0"
    "setPaused\0paused\0setSpeed\0speed\0seek\0"
    "int64_t\0timestamp_ms\0getDuration\0"
    "getCurrentPosition\0isEOF\0isFileMode\0"
    "setChannelEpoch\0uint64_t\0epoch\0"
    "initDisplayPool\0size_t\0releaseDisplayPool\0"
    "getDisplayBufferFromPool\0uint8_t*\0"
    "returnDisplayBufferToPool\0ptr\0"
    "releaseDisplayBufferCallback\0opaque\0"
    "data\0totalDecoderVramBytes\0"
    "totalStandaloneFrameVramBytes\0"
    "registerStandaloneFrameAlloc\0bytes\0"
    "registerStandaloneFrameFree\0hwDecoderCount\0"
    "swDecoderCount\0maxHwDecoders"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_VideoDecoder[] = {

 // content:
      10,       // revision
       0,       // classname
       0,    0, // classinfo
      29,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags, initial metatype offsets
       1,    1,  188,    2, 0x06,    1 /* Public */,

 // slots: name, argc, parameters, tag, flags, initial metatype offsets
       4,    0,  191,    2, 0x0a,    3 /* Public */,
       5,    0,  192,    2, 0x0a,    4 /* Public */,
       6,    2,  193,    2, 0x0a,    5 /* Public */,
       9,    1,  198,    2, 0x0a,    8 /* Public */,
      11,    1,  201,    2, 0x0a,   10 /* Public */,
      13,    1,  204,    2, 0x0a,   12 /* Public */,
      15,    0,  207,    2, 0x10a,   14 /* Public | MethodIsConst  */,
      16,    0,  208,    2, 0x0a,   15 /* Public */,
      17,    1,  209,    2, 0x0a,   16 /* Public */,
      19,    1,  212,    2, 0x0a,   18 /* Public */,
      21,    1,  215,    2, 0x0a,   20 /* Public */,
      24,    0,  218,    2, 0x10a,   22 /* Public | MethodIsConst  */,
      25,    0,  219,    2, 0x10a,   23 /* Public | MethodIsConst  */,
      26,    0,  220,    2, 0x10a,   24 /* Public | MethodIsConst  */,
      27,    0,  221,    2, 0x10a,   25 /* Public | MethodIsConst  */,
      28,    1,  222,    2, 0x0a,   26 /* Public */,
      31,    2,  225,    2, 0x0a,   28 /* Public */,
      33,    0,  230,    2, 0x0a,   31 /* Public */,
      34,    0,  231,    2, 0x0a,   32 /* Public */,
      36,    1,  232,    2, 0x0a,   33 /* Public */,
      38,    2,  235,    2, 0x0a,   35 /* Public */,
      41,    0,  240,    2, 0x0a,   38 /* Public */,
      42,    0,  241,    2, 0x0a,   39 /* Public */,
      43,    1,  242,    2, 0x0a,   40 /* Public */,
      45,    1,  245,    2, 0x0a,   42 /* Public */,
      46,    0,  248,    2, 0x0a,   44 /* Public */,
      47,    0,  249,    2, 0x0a,   45 /* Public */,
      48,    0,  250,    2, 0x0a,   46 /* Public */,

 // signals: parameters
    QMetaType::Void, QMetaType::Int,    3,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int, QMetaType::Int,    7,    8,
    QMetaType::Void, QMetaType::Bool,   10,
    QMetaType::Void, QMetaType::Int,   12,
    QMetaType::Void, QMetaType::Bool,   14,
    QMetaType::Int,
    QMetaType::Double,
    QMetaType::Void, QMetaType::Bool,   18,
    QMetaType::Void, QMetaType::Float,   20,
    QMetaType::Void, 0x80000000 | 22,   23,
    0x80000000 | 22,
    0x80000000 | 22,
    QMetaType::Bool,
    QMetaType::Bool,
    QMetaType::Void, 0x80000000 | 29,   30,
    QMetaType::Void, 0x80000000 | 32, 0x80000000 | 32,    7,    8,
    QMetaType::Void,
    0x80000000 | 35,
    QMetaType::Void, 0x80000000 | 35,   37,
    QMetaType::Void, QMetaType::VoidStar, 0x80000000 | 35,   39,   40,
    0x80000000 | 32,
    0x80000000 | 32,
    QMetaType::Void, 0x80000000 | 32,   44,
    QMetaType::Void, 0x80000000 | 32,   44,
    QMetaType::Int,
    QMetaType::Int,
    QMetaType::Int,

       0        // eod
};

void VideoDecoder::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<VideoDecoder *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->playbackFinished((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->startDecoding(); break;
        case 2: _t->stopDecoding(); break;
        case 3: _t->setDisplaySize((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2]))); break;
        case 4: _t->setLowFPSMode((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 5: _t->setTargetFPS((*reinterpret_cast< std::add_pointer_t<int>>(_a[1]))); break;
        case 6: _t->setAllowOverNativeFPS((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 7: { int _r = _t->getTargetFPS();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 8: { double _r = _t->getNativeFPS();
            if (_a[0]) *reinterpret_cast< double*>(_a[0]) = std::move(_r); }  break;
        case 9: _t->setPaused((*reinterpret_cast< std::add_pointer_t<bool>>(_a[1]))); break;
        case 10: _t->setSpeed((*reinterpret_cast< std::add_pointer_t<float>>(_a[1]))); break;
        case 11: _t->seek((*reinterpret_cast< std::add_pointer_t<int64_t>>(_a[1]))); break;
        case 12: { int64_t _r = _t->getDuration();
            if (_a[0]) *reinterpret_cast< int64_t*>(_a[0]) = std::move(_r); }  break;
        case 13: { int64_t _r = _t->getCurrentPosition();
            if (_a[0]) *reinterpret_cast< int64_t*>(_a[0]) = std::move(_r); }  break;
        case 14: { bool _r = _t->isEOF();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 15: { bool _r = _t->isFileMode();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 16: _t->setChannelEpoch((*reinterpret_cast< std::add_pointer_t<uint64_t>>(_a[1]))); break;
        case 17: _t->initDisplayPool((*reinterpret_cast< std::add_pointer_t<size_t>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<size_t>>(_a[2]))); break;
        case 18: _t->releaseDisplayPool(); break;
        case 19: { uint8_t* _r = _t->getDisplayBufferFromPool();
            if (_a[0]) *reinterpret_cast< uint8_t**>(_a[0]) = std::move(_r); }  break;
        case 20: _t->returnDisplayBufferToPool((*reinterpret_cast< std::add_pointer_t<uint8_t*>>(_a[1]))); break;
        case 21: _t->releaseDisplayBufferCallback((*reinterpret_cast< std::add_pointer_t<void*>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<uint8_t*>>(_a[2]))); break;
        case 22: { size_t _r = _t->totalDecoderVramBytes();
            if (_a[0]) *reinterpret_cast< size_t*>(_a[0]) = std::move(_r); }  break;
        case 23: { size_t _r = _t->totalStandaloneFrameVramBytes();
            if (_a[0]) *reinterpret_cast< size_t*>(_a[0]) = std::move(_r); }  break;
        case 24: _t->registerStandaloneFrameAlloc((*reinterpret_cast< std::add_pointer_t<size_t>>(_a[1]))); break;
        case 25: _t->registerStandaloneFrameFree((*reinterpret_cast< std::add_pointer_t<size_t>>(_a[1]))); break;
        case 26: { int _r = _t->hwDecoderCount();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 27: { int _r = _t->swDecoderCount();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 28: { int _r = _t->maxHwDecoders();
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (VideoDecoder::*)(int );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&VideoDecoder::playbackFinished)) {
                *result = 0;
                return;
            }
        }
    }
}

const QMetaObject VideoDecoder::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_meta_stringdata_VideoDecoder.offsetsAndSize,
    qt_meta_data_VideoDecoder,
    qt_static_metacall,
    nullptr,
qt_incomplete_metaTypeArray<qt_meta_stringdata_VideoDecoder_t
, QtPrivate::TypeAndForceComplete<VideoDecoder, std::true_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>
, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<bool, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<bool, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<double, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<bool, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<float, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<int64_t, std::false_type>, QtPrivate::TypeAndForceComplete<int64_t, std::false_type>, QtPrivate::TypeAndForceComplete<int64_t, std::false_type>, QtPrivate::TypeAndForceComplete<bool, std::false_type>, QtPrivate::TypeAndForceComplete<bool, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<uint64_t, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<size_t, std::false_type>, QtPrivate::TypeAndForceComplete<size_t, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<uint8_t *, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<uint8_t *, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<void *, std::false_type>, QtPrivate::TypeAndForceComplete<uint8_t *, std::false_type>, QtPrivate::TypeAndForceComplete<size_t, std::false_type>, QtPrivate::TypeAndForceComplete<size_t, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<size_t, std::false_type>, QtPrivate::TypeAndForceComplete<void, std::false_type>, QtPrivate::TypeAndForceComplete<size_t, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>, QtPrivate::TypeAndForceComplete<int, std::false_type>


>,
    nullptr
} };


const QMetaObject *VideoDecoder::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *VideoDecoder::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_VideoDecoder.stringdata0))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int VideoDecoder::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 29)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 29;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 29)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 29;
    }
    return _id;
}

// SIGNAL 0
void VideoDecoder::playbackFinished(int _t1)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
