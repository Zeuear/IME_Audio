#include "appleshadoweffect.h"
#include <QPainter>
#include <QImage>
#include <QDebug>

static QImage applyGaussianBlur(const QImage &source, qreal radius)
{
    if (source.isNull() || radius < 1) return source;

    int r = static_cast<int>(radius);
    int size = r * 2 + 1;
    QVector<float> kernel(size);
    float sigma = radius / 3.0;
    float sum = 0;

    for (int i = 0; i < size; ++i) {
        int x = i - r;
        kernel[i] = exp(-(x * x) / (2 * sigma * sigma));
        sum += kernel[i];
    }
    for (int i = 0; i < size; ++i) kernel[i] /= sum;

    int width = source.width();
    int height = source.height();
    QImage result(width, height, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);

    QVector<QRgb> tempBuffer(width * height);

    const uchar* srcBits = source.bits();
    int srcBytesPerLine = source.bytesPerLine();
    QRgb* tempData = tempBuffer.data();

    for (int y = 0; y < height; ++y) {
        const uchar* srcLine = srcBits + y * srcBytesPerLine;
        QRgb* tempLine = tempData + y * width;

        for (int x = r; x < width - r; ++x) {
            float rSum = 0, gSum = 0, bSum = 0, aSum = 0;
            for (int i = -r; i <= r; ++i) {
                QRgb pixel = qRgba(srcLine[(x + i) * 4 + 2], srcLine[(x + i) * 4 + 1],
                    srcLine[(x + i) * 4], srcLine[(x + i) * 4 + 3]);
                float k = kernel[i + r];
                rSum += k * qRed(pixel);
                gSum += k * qGreen(pixel);
                bSum += k * qBlue(pixel);
                aSum += k * qAlpha(pixel);
            }
            tempLine[x] = qRgba(qBound(0, static_cast<int>(rSum), 255),
                qBound(0, static_cast<int>(gSum), 255),
                qBound(0, static_cast<int>(bSum), 255),
                qBound(0, static_cast<int>(aSum), 255));
        }

        for (int x = 0; x < r; ++x) {
            float rSum = 0, gSum = 0, bSum = 0, aSum = 0;
            for (int i = -r; i <= r; ++i) {
                int px = qBound(0, x + i, width - 1);
                QRgb pixel = qRgba(srcLine[px * 4 + 2], srcLine[px * 4 + 1],
                    srcLine[px * 4], srcLine[px * 4 + 3]);
                float k = kernel[i + r];
                rSum += k * qRed(pixel);
                gSum += k * qGreen(pixel);
                bSum += k * qBlue(pixel);
                aSum += k * qAlpha(pixel);
            }
            tempLine[x] = qRgba(qBound(0, static_cast<int>(rSum), 255),
                qBound(0, static_cast<int>(gSum), 255),
                qBound(0, static_cast<int>(bSum), 255),
                qBound(0, static_cast<int>(aSum), 255));
        }

        for (int x = width - r; x < width; ++x) {
            float rSum = 0, gSum = 0, bSum = 0, aSum = 0;
            for (int i = -r; i <= r; ++i) {
                int px = qBound(0, x + i, width - 1);
                QRgb pixel = qRgba(srcLine[px * 4 + 2], srcLine[px * 4 + 1],
                    srcLine[px * 4], srcLine[px * 4 + 3]);
                float k = kernel[i + r];
                rSum += k * qRed(pixel);
                gSum += k * qGreen(pixel);
                bSum += k * qBlue(pixel);
                aSum += k * qAlpha(pixel);
            }
            tempLine[x] = qRgba(qBound(0, static_cast<int>(rSum), 255),
                qBound(0, static_cast<int>(gSum), 255),
                qBound(0, static_cast<int>(bSum), 255),
                qBound(0, static_cast<int>(aSum), 255));
        }
    }

    uchar* dstBits = result.bits();
    int dstBytesPerLine = result.bytesPerLine();

    for (int x = 0; x < width; ++x) {
        uchar* dstLine = dstBits + x * 4;

        for (int y = r; y < height - r; ++y) {
            float rSum = 0, gSum = 0, bSum = 0, aSum = 0;
            for (int i = -r; i <= r; ++i) {
                QRgb pixel = tempData[(y + i) * width + x];
                float k = kernel[i + r];
                rSum += k * qRed(pixel);
                gSum += k * qGreen(pixel);
                bSum += k * qBlue(pixel);
                aSum += k * qAlpha(pixel);
            }
            QRgb color = qRgba(qBound(0, static_cast<int>(rSum), 255),
                qBound(0, static_cast<int>(gSum), 255),
                qBound(0, static_cast<int>(bSum), 255),
                qBound(0, static_cast<int>(aSum), 255));
            memcpy(dstLine + y * dstBytesPerLine, &color, 4);  
        }

        for (int y = 0; y < r; ++y) {
            float rSum = 0, gSum = 0, bSum = 0, aSum = 0;
            for (int i = -r; i <= r; ++i) {
                int py = qBound(0, y + i, height - 1);
                QRgb pixel = tempData[py * width + x];
                float k = kernel[i + r];
                rSum += k * qRed(pixel);
                gSum += k * qGreen(pixel);
                bSum += k * qBlue(pixel);
                aSum += k * qAlpha(pixel);
            }
            QRgb color = qRgba(qBound(0, static_cast<int>(rSum), 255),
                qBound(0, static_cast<int>(gSum), 255),
                qBound(0, static_cast<int>(bSum), 255),
                qBound(0, static_cast<int>(aSum), 255));
            memcpy(dstLine + y * dstBytesPerLine, &color, 4);
        }

        for (int y = height - r; y < height; ++y) {
            float rSum = 0, gSum = 0, bSum = 0, aSum = 0;
            for (int i = -r; i <= r; ++i) {
                int py = qBound(0, y + i, height - 1);
                QRgb pixel = tempData[py * width + x];
                float k = kernel[i + r];
                rSum += k * qRed(pixel);
                gSum += k * qGreen(pixel);
                bSum += k * qBlue(pixel);
                aSum += k * qAlpha(pixel);
            }
            QRgb color = qRgba(qBound(0, static_cast<int>(rSum), 255),
                qBound(0, static_cast<int>(gSum), 255),
                qBound(0, static_cast<int>(bSum), 255),
                qBound(0, static_cast<int>(aSum), 255));
            memcpy(dstLine + y * dstBytesPerLine, &color, 4);
        }
    }

    return result;
}


static QImage applyBoxBlur(const QImage& source, qreal radius, int passes = 3)
{
    if (source.isNull() || radius < 1) return source;

    QImage currentImage = source;
    int r = static_cast<int>(radius / passes);  // Adjust radius per pass
    int size = r * 2 + 1;

    for (int pass = 0; pass < passes; ++pass) {
        int width = currentImage.width();
        int height = currentImage.height();
        QImage result(width, height, QImage::Format_ARGB32_Premultiplied);
        result.fill(Qt::transparent);

        QVector<QRgb> tempBuffer(width * height);

        // Horizontal pass
        const uchar* srcBits = currentImage.bits();
        int srcBytesPerLine = currentImage.bytesPerLine();
        QRgb* tempData = tempBuffer.data();

        for (int y = 0; y < height; ++y) {
            const uchar* srcLine = srcBits + y * srcBytesPerLine;
            QRgb* tempLine = tempData + y * width;

            int rSum = 0, gSum = 0, bSum = 0, aSum = 0;
            for (int x = 0; x < size; ++x) {
                int px = qBound(0, x, width - 1);
                QRgb pixel = qRgba(srcLine[px * 4 + 2], srcLine[px * 4 + 1], srcLine[px * 4], srcLine[px * 4 + 3]);
                rSum += qRed(pixel);
                gSum += qGreen(pixel);
                bSum += qBlue(pixel);
                aSum += qAlpha(pixel);
            }
            tempLine[0] = qRgba(rSum / size, gSum / size, bSum / size, aSum / size);

            for (int x = 1; x < width; ++x) {
                int removeX = qBound(0, x - r - 1, width - 1);
                int addX = qBound(0, x + r, width - 1);
                QRgb removePixel = qRgba(srcLine[removeX * 4 + 2], srcLine[removeX * 4 + 1],
                    srcLine[removeX * 4], srcLine[removeX * 4 + 3]);
                QRgb addPixel = qRgba(srcLine[addX * 4 + 2], srcLine[addX * 4 + 1],
                    srcLine[addX * 4], srcLine[addX * 4 + 3]);
                rSum += qRed(addPixel) - qRed(removePixel);
                gSum += qGreen(addPixel) - qGreen(removePixel);
                bSum += qBlue(addPixel) - qBlue(removePixel);
                aSum += qAlpha(addPixel) - qAlpha(removePixel);
                tempLine[x] = qRgba(rSum / size, gSum / size, bSum / size, aSum / size);
            }
        }

        // Vertical pass
        uchar* dstBits = result.bits();
        int dstBytesPerLine = result.bytesPerLine();

        for (int x = 0; x < width; ++x) {
            uchar* dstLine = dstBits + x * 4;

            int rSum = 0, gSum = 0, bSum = 0, aSum = 0;
            for (int y = 0; y < size; ++y) {
                int py = qBound(0, y, height - 1);
                QRgb pixel = tempData[py * width + x];
                rSum += qRed(pixel);
                gSum += qGreen(pixel);
                bSum += qBlue(pixel);
                aSum += qAlpha(pixel);
            }
            QRgb color = qRgba(rSum / size, gSum / size, bSum / size, aSum / size);
            memcpy(dstLine, &color, 4);

            for (int y = 1; y < height; ++y) {
                int removeY = qBound(0, y - r - 1, height - 1);
                int addY = qBound(0, y + r, height - 1);
                QRgb removePixel = tempData[removeY * width + x];
                QRgb addPixel = tempData[addY * width + x];
                rSum += qRed(addPixel) - qRed(removePixel);
                gSum += qGreen(addPixel) - qGreen(removePixel);
                bSum += qBlue(addPixel) - qBlue(removePixel);
                aSum += qAlpha(addPixel) - qAlpha(removePixel);
                color = qRgba(rSum / size, gSum / size, bSum / size, aSum / size);
                memcpy(dstLine + y * dstBytesPerLine, &color, 4);
            }
        }

        currentImage = result;  // Use the result as input for the next pass
    }

    return currentImage;
}

static QImage applyOptimizedGaussianBlur(const QImage& source, qreal radius)
{
    if (source.isNull() || radius < 1) {
        qDebug() << "Invalid input: source is null or radius < 1";
        return source;
    }

    int r = static_cast<int>(radius);
    int size = r * 2 + 1;
    QVector<float> kernel(size);
    float sigma = radius / 3.0;
    float sum = 0;

    for (int i = 0; i < size; ++i) {
        int x = i - r;
        kernel[i] = exp(-(x * x) / (2 * sigma * sigma));
        sum += kernel[i];
    }
    for (int i = 0; i < size; ++i) kernel[i] /= sum;

    int width = source.width();
    int height = source.height();
    QImage result(width, height, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);

    QVector<QRgb> tempBuffer(width * height);

    const uchar* srcBits = source.bits();
    int srcBytesPerLine = source.bytesPerLine();
    QRgb* tempData = tempBuffer.data();

    for (int y = 0; y < height; ++y) {
        const uchar* srcLine = srcBits + y * srcBytesPerLine;
        QRgb* tempLine = tempData + y * width;

        for (int x = 0; x < width; ++x) {
            float rSum = 0, gSum = 0, bSum = 0, aSum = 0;
            for (int i = -r; i <= r; ++i) {
                int px = qBound(0, x + i, width - 1);
                QRgb pixel = qRgba(srcLine[px * 4 + 2], srcLine[px * 4 + 1],
                    srcLine[px * 4], srcLine[px * 4 + 3]);
                float k = kernel[i + r];
                rSum += k * qRed(pixel);
                gSum += k * qGreen(pixel);
                bSum += k * qBlue(pixel);
                aSum += k * qAlpha(pixel);
            }
            tempLine[x] = qRgba(qBound(0, static_cast<int>(rSum), 255),
                qBound(0, static_cast<int>(gSum), 255),
                qBound(0, static_cast<int>(bSum), 255),
                qBound(0, static_cast<int>(aSum), 255));
        }
    }

    uchar* dstBits = result.bits();
    int dstBytesPerLine = result.bytesPerLine();

    for (int x = 0; x < width; ++x) {
        uchar* dstLine = dstBits + x * 4;

        for (int y = 0; y < height; ++y) {
            float rSum = 0, gSum = 0, bSum = 0, aSum = 0;
            for (int i = -r; i <= r; ++i) {
                int py = qBound(0, y + i, height - 1);
                QRgb pixel = tempData[py * width + x];
                float k = kernel[i + r];
                rSum += k * qRed(pixel);
                gSum += k * qGreen(pixel);
                bSum += k * qBlue(pixel);
                aSum += k * qAlpha(pixel);
            }
            QRgb color = qRgba(qBound(0, static_cast<int>(rSum), 255),
                qBound(0, static_cast<int>(gSum), 255),
                qBound(0, static_cast<int>(bSum), 255),
                qBound(0, static_cast<int>(aSum), 255));
            memcpy(dstLine + y * dstBytesPerLine, &color, 4);
        }
    }

    return result;
}



static void computeIIRCoefficients(qreal sigma, qreal& b0, qreal& b1, qreal& b2, qreal& b3)
{
    qreal q = (sigma > 2.5) ? (0.98711 * sigma - 0.96330) : (3.97156 - 4.14554 * sqrt(1 - 0.26891 * sigma));
    qreal q2 = q * q;
    qreal q3 = q2 * q;
    b0 = 1.57825 + 2.44413 * q + 1.4281 * q2 + 0.422205 * q3;
    b1 = 2.44413 * q + 2.85619 * q2 + 1.26661 * q3;
    b2 = -1.4281 * q2 - 1.26661 * q3;
    b3 = 0.422205 * q3;
    qreal scale = 1.0 / b0;
    b0 = scale; b1 *= scale; b2 *= scale; b3 *= scale;
}

static QImage applyFastGaussianBlur(const QImage& source, qreal radius)
{
    if (source.isNull() || radius < 1) {
        qDebug() << "Invalid input: source is null or radius < 1";
        return source;
    }

    int width = source.width();
    int height = source.height();
    QImage result(width, height, QImage::Format_ARGB32_Premultiplied);
    result.fill(Qt::transparent);

    qreal sigma = radius / 3.0;  // ��֮ǰһ��
    qreal b0, b1, b2, b3;
    computeIIRCoefficients(sigma, b0, b1, b2, b3);

    QVector<QRgb> tempBuffer(width * height);
    const uchar* srcBits = source.bits();
    int srcBytesPerLine = source.bytesPerLine();
    QRgb* tempData = tempBuffer.data();

    for (int y = 0; y < height; ++y) {
        const uchar* srcLine = srcBits + y * srcBytesPerLine;
        QRgb* tempLine = tempData + y * width;

        float r[3] = { 0, 0, 0 }, g[3] = { 0, 0, 0 }, b[3] = { 0, 0, 0 }, a[3] = { 0, 0, 0 };
        for (int x = 0; x < width; ++x) {
            QRgb pixel = qRgba(srcLine[x * 4 + 2], srcLine[x * 4 + 1],
                srcLine[x * 4], srcLine[x * 4 + 3]);
            float rOut = b0 * qRed(pixel) + b1 * r[0] + b2 * r[1] + b3 * r[2];
            float gOut = b0 * qGreen(pixel) + b1 * g[0] + b2 * g[1] + b3 * g[2];
            float bOut = b0 * qBlue(pixel) + b1 * b[0] + b2 * b[1] + b3 * b[2];
            float aOut = b0 * qAlpha(pixel) + b1 * a[0] + b2 * a[1] + b3 * a[2];
            r[2] = r[1]; r[1] = r[0]; r[0] = rOut;
            g[2] = g[1]; g[1] = g[0]; g[0] = gOut;
            b[2] = b[1]; b[1] = b[0]; b[0] = bOut;
            a[2] = a[1]; a[1] = a[0]; a[0] = aOut;
            tempLine[x] = qRgba(qBound(0, static_cast<int>(rOut), 255),
                qBound(0, static_cast<int>(gOut), 255),
                qBound(0, static_cast<int>(bOut), 255),
                qBound(0, static_cast<int>(aOut), 255));
        }

        r[0] = r[1] = r[2] = g[0] = g[1] = g[2] = b[0] = b[1] = b[2] = a[0] = a[1] = a[2] = 0;
        for (int x = width - 1; x >= 0; --x) {
            float rOut = b0 * qRed(tempLine[x]) + b1 * r[0] + b2 * r[1] + b3 * r[2];
            float gOut = b0 * qGreen(tempLine[x]) + b1 * g[0] + b2 * g[1] + b3 * g[2];
            float bOut = b0 * qBlue(tempLine[x]) + b1 * b[0] + b2 * b[1] + b3 * b[2];
            float aOut = b0 * qAlpha(tempLine[x]) + b1 * a[0] + b2 * a[1] + b3 * a[2];
            r[2] = r[1]; r[1] = r[0]; r[0] = rOut;
            g[2] = g[1]; g[1] = g[0]; g[0] = gOut;
            b[2] = b[1]; b[1] = b[0]; b[0] = bOut;
            a[2] = a[1]; a[1] = a[0]; a[0] = aOut;
            tempLine[x] = qRgba(qBound(0, static_cast<int>(rOut), 255),
                qBound(0, static_cast<int>(gOut), 255),
                qBound(0, static_cast<int>(bOut), 255),
                qBound(0, static_cast<int>(aOut), 255));
        }
    }

    uchar* dstBits = result.bits();
    int dstBytesPerLine = result.bytesPerLine();

    for (int x = 0; x < width; ++x) {
        uchar* dstLine = dstBits + x * 4;

        float r[3] = { 0, 0, 0 }, g[3] = { 0, 0, 0 }, b[3] = { 0, 0, 0 }, a[3] = { 0, 0, 0 };
        for (int y = 0; y < height; ++y) {
            QRgb pixel = tempData[y * width + x];
            float rOut = b0 * qRed(pixel) + b1 * r[0] + b2 * r[1] + b3 * r[2];
            float gOut = b0 * qGreen(pixel) + b1 * g[0] + b2 * g[1] + b3 * g[2];
            float bOut = b0 * qBlue(pixel) + b1 * b[0] + b2 * b[1] + b3 * b[2];
            float aOut = b0 * qAlpha(pixel) + b1 * a[0] + b2 * a[1] + b3 * a[2];
            r[2] = r[1]; r[1] = r[0]; r[0] = rOut;
            g[2] = g[1]; g[1] = g[0]; g[0] = gOut;
            b[2] = b[1]; b[1] = b[0]; b[0] = bOut;
            a[2] = a[1]; a[1] = a[0]; a[0] = aOut;
            QRgb color = qRgba(qBound(0, static_cast<int>(rOut), 255),
                qBound(0, static_cast<int>(gOut), 255),
                qBound(0, static_cast<int>(bOut), 255),
                qBound(0, static_cast<int>(aOut), 255));
            memcpy(dstLine + y * dstBytesPerLine, &color, 4);
        }

        r[0] = r[1] = r[2] = g[0] = g[1] = g[2] = b[0] = b[1] = b[2] = a[0] = a[1] = a[2] = 0;
        for (int y = height - 1; y >= 0; --y) {
            QRgb pixel = tempData[y * width + x];
            float rOut = b0 * qRed(pixel) + b1 * r[0] + b2 * r[1] + b3 * r[2];
            float gOut = b0 * qGreen(pixel) + b1 * g[0] + b2 * g[1] + b3 * g[2];
            float bOut = b0 * qBlue(pixel) + b1 * b[0] + b2 * b[1] + b3 * b[2];
            float aOut = b0 * qAlpha(pixel) + b1 * a[0] + b2 * a[1] + b3 * a[2];
            r[2] = r[1]; r[1] = r[0]; r[0] = rOut;
            g[2] = g[1]; g[1] = g[0]; g[0] = gOut;
            b[2] = b[1]; b[1] = b[0]; b[0] = bOut;
            a[2] = a[1]; a[1] = a[0]; a[0] = aOut;
            QRgb color = qRgba(qBound(0, static_cast<int>(rOut), 255),
                qBound(0, static_cast<int>(gOut), 255),
                qBound(0, static_cast<int>(bOut), 255),
                qBound(0, static_cast<int>(aOut), 255));
            memcpy(dstLine + y * dstBytesPerLine, &color, 4);
        }
    }

    return result;

}



AppleShadowEffect::AppleShadowEffect(QObject* parent)
    : QGraphicsEffect(parent),
    m_blurRadius(20.0),
    m_offset(0, 4),
    m_color(40, 40, 40, 60),
    m_opacity(1.0),
    m_innerDistance(0)
{
    m_shadowPixmap = QPixmap();  // ��ʼΪ�գ��ȴ���һ�λ���
}

QRectF AppleShadowEffect::boundingRectFor(const QRectF& rect) const
{
    //qreal delta = m_blurRadius + qMax(qAbs(m_offset.x()), qAbs(m_offset.y()));
    //return rect.adjusted(-delta, -delta, delta, delta);
    return rect;
}

QPixmap AppleShadowEffect::updateShadow(const QRectF& sourceRect)
{
    int width = sourceRect.width() + m_blurRadius * 2;
    int height = sourceRect.height() + m_blurRadius * 2;
    QImage shadowBase(width, height, QImage::Format_ARGB32_Premultiplied);
    shadowBase.fill(Qt::transparent);

    int edge = m_innerDistance;

    QPainter shadowPainter(&shadowBase);
    shadowPainter.setRenderHint(QPainter::Antialiasing);
    shadowPainter.setBrush(m_color);
    shadowPainter.setPen(Qt::NoPen);
    QRectF rect(m_blurRadius, m_blurRadius, sourceRect.width() , sourceRect.height());
    shadowPainter.drawRoundedRect(rect.adjusted(edge, edge, -edge, -edge), 8, 8);

    QImage blurredImage = applyBoxBlur(shadowBase, m_blurRadius, 2);
    return QPixmap::fromImage(blurredImage);
}



void AppleShadowEffect::draw(QPainter* painter)
{
    QRectF sourceRect = sourceBoundingRect();
    if (sourceRect.isEmpty()) {
        drawSource(painter);
        return;
    }

    if (m_shadowPixmap.isNull() ||
        m_shadowPixmap.width() != sourceRect.width() + m_blurRadius * 2 ||
        m_shadowPixmap.height() != sourceRect.height() + m_blurRadius * 2 ||
        m_lastSourceRect != sourceRect) {
        m_shadowPixmap = updateShadow(sourceRect);
        m_lastSourceRect = sourceRect;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    //painter->setOpacity(m_opacity);

    QRectF shadowRect = sourceRect.adjusted(-m_blurRadius, -m_blurRadius, m_blurRadius, m_blurRadius);
    shadowRect.translate(m_offset);
    QRectF sourcePixmapRect(0, 0, m_shadowPixmap.width(), m_shadowPixmap.height());
    painter->drawPixmap(shadowRect, m_shadowPixmap, sourcePixmapRect);

    //painter->setOpacity(m_opacity);
    drawSource(painter);
    painter->restore();
}


ShadowDemoWidget::ShadowDemoWidget(QWidget* parent)
    : QWidget(parent),
    m_shadowEffect(new AppleShadowEffect(this))
{
    // ���ô�������
    setAttribute(Qt::WA_TranslucentBackground);  // ͸����������ʾ��Ӱ
    setWindowFlags(Qt::FramelessWindowHint);     // �ޱ߿򴰿�
    resize(300, 200);                            // Ĭ�ϴ�С

    // ������ӰЧ��
    m_shadowEffect->setBlurRadius(30);
    m_shadowEffect->setOffset(QPointF(0, 4));
    m_shadowEffect->setColor(QColor(0, 0, 0, 150));
    setGraphicsEffect(m_shadowEffect);

    setMouseTracking(true);  // ����������
}

ShadowDemoWidget::~ShadowDemoWidget()
{
}

void ShadowDemoWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // ��������
    QRect rect = this->rect().adjusted(20, 20, -20, -20);  // �����Ӱ�ռ�
    painter.setBrush(QColor(245, 245, 245));         // ǳ�Ұ�͸������
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect, 8, 8);                  // Բ�Ǿ���

    // ����ʾ������
    painter.setPen(QColor(50, 50, 50));
    painter.setFont(QFont("Helvetica", 12));
    painter.drawText(rect, Qt::AlignCenter, "Apple Shadow Demo");
}

void ShadowDemoWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragPosition = event->globalPos() - frameGeometry().topLeft();
        event->accept();
    }
}

void ShadowDemoWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (event->buttons() & Qt::LeftButton) {
        move(event->globalPos() - m_dragPosition);
        event->accept();
    }
}


