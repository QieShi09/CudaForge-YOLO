#ifndef VIDEOWIDGET_H
#define VIDEOWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <cuda_runtime.h>
#include <cuda_gl_interop.h>
#include <atomic>
#include <QPushButton>

extern "C" {
#include <libavutil/pixfmt.h>
}

class QTimer;
class VideoWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit VideoWidget(QWidget *parent = nullptr);
    ~VideoWidget() override;

    // 设置频道 ID (0-based)
    void setChannelId(int id);
    
    // 获取频道 ID
    int getChannelId() const { return m_channelId; }

    // 设置显示的文字 (例如 "NO SIGNAL")
    void setText(const QString &text);
    
    // 更新 CUDA 纹理数据
    void updateTexture(void* device_ptr, int width, int height, int pitch, int format = AV_PIX_FMT_RGBA);

    // 设置共享的数据源指针 (原子操作)
    void setDataSource(std::atomic<void*>* ptr, std::atomic<int>* w, std::atomic<int>* h,
                       std::atomic<int>* p, std::atomic<int>* f);

    // 清除显示内容
    void clear();

    // 设置选中状态
    void setSelected(bool selected);

    // 设置刷新 FPS（用于切换网格/详情模式，单位：帧/秒）
    void setRefreshFPS(int fps);

    // 设置冻结状态（仅停止显示更新，后台继续解码）
    void setFrozen(bool frozen);

    // 设置类别名称文件路径（全局生效，触发下次 paintGL 重新加载）
    static void setClassesFilePath(const QString& path);

    // 设置关闭按钮可见性（详情模式下隐藏）
    void setCloseBtnVisible(bool visible);
    
    // 显示播放结束状态（网格模式下视频源）
    void showPlaybackFinished(bool show);

Q_SIGNALS:
    void closeRequested(); // 点击关闭按钮时发送的信号
    void replayRequested(int channel_id); // 点击重播按钮时发送的信号

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void resizeEvent(QResizeEvent *event) override; // 用于定位按钮
    void hideEvent(QHideEvent *event) override;     // 隐藏时停止刷新
    void showEvent(QShowEvent *event) override;     // 显示时恢复刷新

private:
    QString m_text;
    bool m_isSelected = false;
    int m_channelId = 0; // 频道编号

    // OpenGL 资源
    GLuint m_textureId = 0;
    GLuint m_textureYId = 0;
    GLuint m_textureUVId = 0;
    int m_texWidth = 0;
    int m_texHeight = 0;
    int m_allocatedTexWidth = 0;  // 已分配纹理的宽度
    int m_allocatedTexHeight = 0; // 已分配纹理的高度

    // Shader program for modern OpenGL rendering
    QOpenGLShaderProgram *m_program = nullptr;
    QOpenGLShaderProgram *m_programNv12 = nullptr;
    int m_vertexAttr = -1;
    int m_texCoordAttr = -1;
    int m_vertexAttrNv12 = -1;
    int m_texCoordAttrNv12 = -1;
    
    // CUDA 互操作资源
    cudaGraphicsResource* m_cudaResource = nullptr;
    cudaGraphicsResource* m_cudaResourceY = nullptr;
    cudaGraphicsResource* m_cudaResourceUV = nullptr;
    void* m_currentCudaPtr = nullptr; // 暂存传入的 CUDA 指针
    int m_currentPitch = 0;
    int m_currentFormat = AV_PIX_FMT_NONE;

    // 数据源指针 (来自 DisplayWorker)
    std::atomic<void*>* m_sharedPtr = nullptr;
    std::atomic<int>* m_sharedW = nullptr;
    std::atomic<int>* m_sharedH = nullptr;
    std::atomic<int>* m_sharedPitch = nullptr;
    std::atomic<int>* m_sharedFormat = nullptr;
    QTimer* m_timer = nullptr; // 每个实例独立的定时器
    QPushButton* m_closeBtn = nullptr;  // 关闭按钮
    QPushButton* m_replayBtn = nullptr; // 重播按钮（网格模式视频源）

    int m_refreshFPS = 30; // 刷新频率，默认网格模式 30 FPS

    static QString s_classesFilePath; // 类别名称文件路径（全局共享）
};

#endif // VIDEOWIDGET_H