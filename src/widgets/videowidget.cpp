#include "videowidget.h"
#include <QDebug>
#include <QTimer>
#include <QResizeEvent>
#include <algorithm> // for std::max
#include "src/core/DetectionResults.hpp"
#include <QFile>
#include <QTextStream>
#include <cstdio>
#include <cuda_runtime.h>

// 静态成员：类别名称文件路径
QString VideoWidget::s_classesFilePath = "src/engines/class.txt";

void VideoWidget::setClassesFilePath(const QString& path)
{
    s_classesFilePath = path;
}

VideoWidget::VideoWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    m_text = "CAM 01 · READY";
    // 初始化关闭按钮
    m_closeBtn = new QPushButton("×", this);
    m_closeBtn->setFixedSize(24, 24);
    m_closeBtn->setCursor(Qt::PointingHandCursor);
    m_closeBtn->setStyleSheet(
        "QPushButton { background: rgba(206, 106, 106, 210); color: white; border: 1px solid rgba(168,80,80,220); border-radius: 12px; font-weight: bold; font-size: 15px; }"
        "QPushButton:hover { background: rgba(196, 88, 88, 230); }"
    );
    m_closeBtn->hide(); // 默认隐藏，只有有源时才显示

    connect(m_closeBtn, &QPushButton::clicked, this, [this](){
        this->clear(); // 点击时先清理自身状态
        Q_EMIT closeRequested();
    });
    
    // 初始化重播按钮（网格模式下视频源播放结束时显示）
    m_replayBtn = new QPushButton("↻", this); // 圆圈箭头 Unicode
    m_replayBtn->setFixedSize(24, 24);
    m_replayBtn->setCursor(Qt::PointingHandCursor);
    m_replayBtn->setStyleSheet(
        "QPushButton { background: rgba(106, 152, 139, 220); color: white; border: 1px solid rgba(86,128,117,220); border-radius: 12px; font-size: 14px; padding: 0px; margin: 0px; }"
        "QPushButton:hover { background: rgba(90, 139, 125, 240); }"
    );
    m_replayBtn->hide(); // 默认隐藏
    
    connect(m_replayBtn, &QPushButton::clicked, this, [this](){
        m_replayBtn->hide();
        Q_EMIT replayRequested(m_channelId);
    });
}

VideoWidget::~VideoWidget()
{
    makeCurrent();
    if (m_cudaResource) cudaGraphicsUnregisterResource(m_cudaResource);
    if (m_textureId) glDeleteTextures(1, &m_textureId);
    delete m_program;
    doneCurrent();
}

void VideoWidget::setChannelId(int id)
{
    m_channelId = id;
    m_text = QString("CAM %1 · READY").arg(m_channelId + 1, 2, 10, QChar('0'));
    update();
}

void VideoWidget::setText(const QString &text)
{
    m_text = text;
    update();
}

void VideoWidget::updateTexture(void* device_ptr, int width, int height, int pitch)
{
    // 暂存数据，等待 paintGL 时在 OpenGL 上下文中处理
    m_currentCudaPtr = device_ptr;
    m_texWidth = width;
    m_texHeight = height;
    m_currentPitch = pitch;
    m_text.clear(); // 收到视频帧，必须清除 "WAITING" 文字
    update(); // 触发重绘
}

void VideoWidget::setDataSource(std::atomic<void*>* ptr, std::atomic<int>* w, std::atomic<int>* h, std::atomic<int>* p)
{
    // 如果源没变（例如 Grid 切换时重新绑定），直接返回，避免闪烁
    if (m_sharedPtr == ptr && ptr != nullptr) return;

    // 执行“静默清理”：释放 GPU 资源但不设置 WAITING 文字
    if (m_timer) m_timer->stop();
    if (isValid() && context()) {
        makeCurrent();
        if (m_cudaResource) {
            cudaGraphicsUnregisterResource(m_cudaResource);
            m_cudaResource = nullptr;
        }
        if (m_textureId) {
            glDeleteTextures(1, &m_textureId);
            m_textureId = 0;
        }
        doneCurrent();
    }
    m_allocatedTexWidth = 0;
    m_allocatedTexHeight = 0;

    // 1. 立即重置当前渲染指针和纹理尺寸信息
    m_currentCudaPtr = nullptr;
    m_sharedPtr = ptr;
    m_sharedW = w;
    m_sharedH = h;
    m_sharedPitch = p;
    
    m_text = "LOADING..."; // 切换时显示加载中
    m_closeBtn->show();    // 显示交互按钮

    // 使用定时器代替 paintGL 内部的递归 update()
    // 修复：去掉 static，使用成员变量 m_timer，确保每个通道独立刷新
    if (!m_timer) {
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, QOverload<>::of(&VideoWidget::update));
    }
    
    // 使用配置的刷新频率启动定时器（默认 30 FPS -> 33ms）
    m_timer->start(1000 / std::max(1, m_refreshFPS));
    update(); // 立即触发一次重绘以显示 LOADING
}

void VideoWidget::clear()
{
    // 1. 停止刷新定时器 (非 GL 操作，始终执行)
    if (m_timer) m_timer->stop();

    // 2. 释放 GPU 资源 (仅在上下文有效时执行)
    if (isValid() && context()) {
        makeCurrent();
        if (m_cudaResource) {
            cudaGraphicsUnregisterResource(m_cudaResource);
            m_cudaResource = nullptr;
        }
        if (m_textureId) {
            glDeleteTextures(1, &m_textureId);
            m_textureId = 0;
        }
        doneCurrent();
    }

    // 3. 断开所有数据源引用 (非 GL 操作，始终执行)
    m_sharedPtr = nullptr;
    m_sharedW = nullptr;
    m_sharedH = nullptr;
    m_sharedPitch = nullptr;
    m_currentCudaPtr = nullptr; 
    
    // 关键：重置已分配尺寸，强制下次打开时重建 OpenGL 资源
    m_allocatedTexWidth = 0;
    m_allocatedTexHeight = 0;

    // 4. 重置 UI 状态 (始终执行，确保 Grid 切换时文字正确)
    m_closeBtn->hide();
    m_replayBtn->hide(); // 隐藏重播按钮
    m_text = QString("CAM %1 · READY").arg(m_channelId + 1, 2, 10, QChar('0'));

    update();
}

void VideoWidget::setSelected(bool selected)
{
    m_isSelected = selected;
    update();
}

void VideoWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.90f, 0.94f, 0.92f, 1.0f);
    
    // 启用纹理
    glEnable(GL_TEXTURE_2D);

    // Modern OpenGL: Shaders
    m_program = new QOpenGLShaderProgram(this);

    // 顶点着色器：一个简单的直通着色器
    const char *vsrc =
        "attribute vec4 vertex;\n"
        "attribute vec2 texCoord;\n"
        "varying vec2 texc;\n"
        "void main(void)\n"
        "{\n"
        "    gl_Position = vertex;\n"
        "    texc = texCoord;\n"
        "}\n";
    m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vsrc);

    // 片段着色器：从纹理中采样颜色
    const char *fsrc =
        "uniform sampler2D texture;\n"
        "varying vec2 texc;\n"
        "void main(void)\n"
        "{\n"
        "    gl_FragColor = texture2D(texture, texc);\n"
        "}\n";
    m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fsrc);

    m_program->link();
    m_vertexAttr = m_program->attributeLocation("vertex");
    m_texCoordAttr = m_program->attributeLocation("texCoord");
}

void VideoWidget::resizeEvent(QResizeEvent *event)
{
    QOpenGLWidget::resizeEvent(event);
    // 将关闭按钮固定在右上角
    m_closeBtn->move(width() - m_closeBtn->width() - 5, 5);
    // 将重播按钮放在关闭按钮左侧
    int x_close = width() - m_closeBtn->width() - 5;
    int replay_x = x_close - m_replayBtn->width() - 6;
    m_replayBtn->move(replay_x, 5);
}

void VideoWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
}

void VideoWidget::hideEvent(QHideEvent *event)
{
    // 当 Widget 不可见时（如切换到详情页），停止定时器
    // 这样 paintGL 不再触发，彻底停止了该通道的 CUDA 拷贝开销，实现“休息”
    if (m_timer) m_timer->stop();
    QOpenGLWidget::hideEvent(event);
}

void VideoWidget::showEvent(QShowEvent *event)
{
    // 重新显示时，如果已经有数据源，恢复刷新
    if (m_timer && m_sharedPtr) m_timer->start(1000 / std::max(1, m_refreshFPS));
    QOpenGLWidget::showEvent(event);
}

void VideoWidget::setRefreshFPS(int fps)
{
    if (fps <= 0) return;
    m_refreshFPS = fps;
    if (m_timer) {
        // 如果正在运行，立即应用新间隔
        if (m_timer->isActive()) m_timer->start(1000 / std::max(1, m_refreshFPS));
    }
}

void VideoWidget::setFrozen(bool frozen)
{
    // 冻结/解冻显示（仅停止刷新，后台继续解码）
    if (!m_timer) return;
    
    if (frozen) {
        m_timer->stop(); // 停止刷新，保持当前显示的帧
    } else {
        // 恢复刷新，自动显示最新的帧
        m_timer->start(1000 / std::max(1, m_refreshFPS));
    }
}

void VideoWidget::setCloseBtnVisible(bool visible)
{
    if (m_closeBtn) m_closeBtn->setVisible(visible);
}

void VideoWidget::showPlaybackFinished(bool show)
{
    // 显示/隐藏重播按钮（仅网格模式下视频源使用）
    if (m_replayBtn) {
        m_replayBtn->setVisible(show);
        if (show) {
            // 统一固定在关闭按钮左侧
            int x_close = width() - m_closeBtn->width() - 5;
            int replay_x = x_close - m_replayBtn->width() - 6;
            m_replayBtn->move(replay_x, 5);
        }
    }
}

void VideoWidget::paintGL()
{
    // 每一帧开始都必须清空缓冲区，防止 Grid 切换时的文字/画面残留
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 主动拉取最新数据
    void* ptr = nullptr;
    if (m_sharedPtr && (ptr = m_sharedPtr->load(std::memory_order_acquire))) {
        m_currentCudaPtr = ptr;
        m_texWidth = m_sharedW->load(std::memory_order_relaxed);
        m_texHeight = m_sharedH->load(std::memory_order_relaxed);
        m_currentPitch = m_sharedPitch->load(std::memory_order_relaxed);
        
        // 只要拿到了有效的指针且尺寸正常，就立即清除 LOADING 或 WAITING
        if (m_texWidth > 0 && m_texHeight > 0) {
            if (m_text == "LOADING..." || m_text.contains("WAITING")) {
                m_text.clear();
            }
        }
    } else {
        // 如果没有有效源，确保当前指针为空
        m_currentCudaPtr = nullptr;
    }

    // 1. 处理 CUDA -> OpenGL 纹理拷贝
    if (m_currentCudaPtr && m_texWidth > 0 && m_texHeight > 0) {
        // 仅当纹理未创建，或视频分辨率发生变化时，才重建纹理
        if (m_textureId == 0 || m_texWidth != m_allocatedTexWidth || m_texHeight != m_allocatedTexHeight) {
            // 如果已有资源，先释放
            if (m_cudaResource) cudaGraphicsUnregisterResource(m_cudaResource);
            if (m_textureId) glDeleteTextures(1, &m_textureId);

            // 创建新纹理
            glGenTextures(1, &m_textureId);
            glBindTexture(GL_TEXTURE_2D, m_textureId);
            // 设置纹理参数
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // 分配显存 (RGBA)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_texWidth, m_texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindTexture(GL_TEXTURE_2D, 0);

            // 更新已分配的纹理尺寸记录
            m_allocatedTexWidth = m_texWidth;
            m_allocatedTexHeight = m_texHeight;

            // 将新纹理注册到 CUDA
            cudaError_t regErr = cudaGraphicsGLRegisterImage(&m_cudaResource, m_textureId, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard);
            if (regErr != cudaSuccess) {
                fprintf(stderr, "[VideoWidget ch=%d] cudaGraphicsGLRegisterImage failed: %s (tex=%u %dx%d)\n",
                        m_channelId, cudaGetErrorString(regErr), m_textureId, m_texWidth, m_texHeight);
                m_cudaResource = nullptr;
            }
        }

        if (!m_cudaResource) {
            // 注册失败，跳过 CUDA 拷贝
        } else {
        // 映射资源
        cudaError_t mapErr = cudaGraphicsMapResources(1, &m_cudaResource, 0);
        if (mapErr != cudaSuccess) {
            fprintf(stderr, "[VideoWidget ch=%d] cudaGraphicsMapResources failed: %s (ptr=%p)\n",
                    m_channelId, cudaGetErrorString(mapErr), (void*)m_currentCudaPtr);
        } else {
            cudaArray* textureArray;
            cudaGraphicsSubResourceGetMappedArray(&textureArray, m_cudaResource, 0, 0);

            // 执行拷贝 (Device -> Array)
            cudaError_t cpyErr = cudaMemcpy2DToArray(textureArray, 0, 0, m_currentCudaPtr, m_currentPitch, m_texWidth * 4, m_texHeight, cudaMemcpyDeviceToDevice);
            if (cpyErr != cudaSuccess) {
                fprintf(stderr, "[VideoWidget ch=%d] cudaMemcpy2DToArray failed: %s (ptr=%p pitch=%d %dx%d)\n",
                        m_channelId, cudaGetErrorString(cpyErr),
                        (void*)m_currentCudaPtr, m_currentPitch, m_texWidth, m_texHeight);
            }

            // 解除映射
            cudaGraphicsUnmapResources(1, &m_cudaResource, 0);
        }
        } // end m_cudaResource check
    }
    
    // 2. 使用原生 OpenGL 绘制纹理背景
    if (m_textureId != 0) {
        glBindTexture(GL_TEXTURE_2D, m_textureId);

        // 使用 Shader Program 绘制
        m_program->bind();

        // 定义顶点和纹理坐标
        GLfloat vertices[] = {
            -1.0f, -1.0f,   1.0f, -1.0f,   1.0f,  1.0f,  -1.0f,  1.0f
        };
        
        // 默认纹理坐标 (0,0 是左下)。
        // 如果发现画面是倒的（头朝下），就把这里的 0.0 和 1.0 对调。
        GLfloat texCoords[] = {
            0.0f, 1.0f,   1.0f, 1.0f,   1.0f, 0.0f,   0.0f, 0.0f
        };

        glVertexAttribPointer(m_vertexAttr, 2, GL_FLOAT, GL_FALSE, 0, vertices);
        glVertexAttribPointer(m_texCoordAttr, 2, GL_FLOAT, GL_FALSE, 0, texCoords);

        glEnableVertexAttribArray(m_vertexAttr);
        glEnableVertexAttribArray(m_texCoordAttr);

        glDrawArrays(GL_QUADS, 0, 4);

        glDisableVertexAttribArray(m_vertexAttr);
        glDisableVertexAttribArray(m_texCoordAttr);

        m_program->release();
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    // 3. 使用 QPainter 绘制覆盖层 (文字、边框、检测框)
    // QOpenGLWidget 允许在 GL 绘制后使用 QPainter
    QPainter painter(this);

    // 绘制检测框（使用类别名称，如果可用）
    if (m_texWidth > 0 && m_texHeight > 0) {
        auto dets = DetectionResults::getInstance().get(m_channelId);
        if (!dets.empty()) {
            float scaleX = static_cast<float>(width()) / static_cast<float>(m_texWidth);
            float scaleY = static_cast<float>(height()) / static_cast<float>(m_texHeight);

            QPen boxPen(QColor(0, 255, 0));
            boxPen.setWidth(2);
            painter.setPen(boxPen);
            painter.setFont(QFont("Segoe UI", 10, QFont::Bold));

            // 载入类别名称（路径改变时自动重新加载）
            static std::vector<QString> classNames;
            static bool triedLoad = false;
            static QString loadedPath;
            if (!triedLoad || loadedPath != s_classesFilePath) {
                triedLoad = true;
                loadedPath = s_classesFilePath;
                classNames.clear();
                QFile f(s_classesFilePath);
                if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&f);
                    while (!in.atEnd()) {
                        QString line = in.readLine().trimmed();
                        if (!line.isEmpty()) classNames.push_back(line);
                    }
                    f.close();
                }
            }

            for (const auto& d : dets) {
                if (d.w <= 0.0f || d.h <= 0.0f) continue;
                QRectF rect(d.x * scaleX, d.y * scaleY, d.w * scaleX, d.h * scaleY);
                painter.drawRect(rect);
                QString label;
                if (d.class_id >= 0 && static_cast<size_t>(d.class_id) < classNames.size()) {
                    label = QString("%1 %2").arg(classNames[d.class_id]).arg(d.conf, 0, 'f', 2);
                } else {
                    label = QString("%1 %2").arg(d.class_id).arg(d.conf, 0, 'f', 2);
                }
                painter.drawText(rect.topLeft() + QPointF(2, 12), label);
            }
        }
    }
    
    if (!m_text.isEmpty()) {
        painter.setPen(QColor(58, 90, 101));
        painter.setFont(QFont("Segoe UI", 13, QFont::DemiBold));
        painter.drawText(rect(), Qt::AlignCenter, m_text);
    }

    // 绘制选中边框
    if (m_isSelected) {
        QPen pen(QColor(104, 154, 140));
        pen.setWidth(3);
        painter.setPen(pen);
        painter.drawRect(rect().adjusted(1, 1, -2, -2));
    } else {
        QPen pen(QColor(173, 193, 186));
        pen.setWidth(1);
        painter.setPen(pen);
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
}