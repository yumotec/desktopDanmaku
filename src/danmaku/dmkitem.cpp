#include "danmaku/dmkitem.hpp"

#include <assert.h>

using namespace Gdiplus::DllExports;

namespace danmaku
{
    // 将弹幕文本光栅化为位图，供后续绘制使用
    void danmakuItem::rasterize()
    {
        // 断言：当前不应已有位图，避免资源泄漏
        assert(!bitmap_);

        // TODO 拆离字体逻辑，对接全局字体管理
        // TODO 允许自定义字体属性（如字体名称、加粗、斜体等）

        // 创建字符串格式对象（使用默认格式）
        Gdiplus::GpStringFormat *strFmt;
        GdipCreateStringFormat(0, LANG_NEUTRAL, &strFmt);

        // 创建字体族对象
        Gdiplus::GpFontFamily *fontFamily;
        GdipCreateFontFamilyFromName(L"微软雅黑", nullptr, &fontFamily);

        // 创建路径对象，用于容纳文本的几何轮廓
        Gdiplus::GpPath *path;
        GdipCreatePath(Gdiplus::FillModeAlternate, &path);

        // 定义一个极大的矩形区域（65536x65536），确保文本能完全放入
        const Gdiplus::RectF InfRect{0, 0, 65536.f, 65536.f};

        // 将文本字符串转换为路径，指定字体、样式、大小等
        GdipAddPathString(
            path,
            text_.data(), (int)text_.size(),
            fontFamily,
            Gdiplus::FontStyleRegular,
            emSize_,
            &InfRect,
            strFmt
        );

        // 创建画笔（用于绘制边框）和画刷（用于填充文本）
        Gdiplus::GpPen *pen;
        GdipCreatePen1(borderColor_, 2.f, Gdiplus::UnitPixel, &pen);
        Gdiplus::GpSolidFill *brush;
        GdipCreateSolidFill(fillColor_, &brush);

        // 获取路径的世界边界矩形（考虑到边框宽度，传入画笔）
        Gdiplus::Rect pathRect;
        GdipGetPathWorldBoundsI(path, &pathRect, nullptr, pen);

        // 创建与边界矩形等大的位图（32位PARGB格式，支持透明通道）
        GdipCreateBitmapFromScan0(
            pathRect.Width, pathRect.Height,
            0,
            PixelFormat32bppPARGB,
            nullptr,
            &bitmap_
        );
        {
            // 从位图获取图形上下文，用于绘制文本
            Gdiplus::GpGraphics *g;
            GdipGetImageGraphicsContext(bitmap_, &g);

            // 开启抗锯齿，使文本边缘平滑
            GdipSetSmoothingMode(g, Gdiplus::SmoothingModeAntiAlias);

            // 创建矩阵，用于将世界变换平移，使路径的左上角对齐到(0,0)
            Gdiplus::GpMatrix *mat;
            GdipCreateMatrix(&mat);
            GdipTranslateMatrix(mat, -pathRect.X, -pathRect.Y,
                                Gdiplus::MatrixOrderAppend);

            // 应用矩阵到图形上下文的世界变换
            GdipSetWorldTransform(g, mat);

            // 填充路径（文本主体）并绘制路径边框
            GdipDrawPath(g, pen, path);
            GdipFillPath(g, brush, path);

            // 清理本次绘制使用的临时资源
            GdipDeleteGraphics(g);
            GdipDeleteMatrix(mat);
        }

        // 释放所有临时GDI+对象
        GdipDeletePen(pen);
        GdipDeleteBrush(brush);
        GdipDeletePath(path);
        GdipDeleteFontFamily(fontFamily);
        GdipDeleteStringFormat(strFmt);
    }

    // 在指定的图形上下文中绘制弹幕位图
    Gdiplus::Status danmakuItem::draw(Gdiplus::GpGraphics *g, int x, int y)
    {
        // 若尚未光栅化，则立即执行
        if (!bitmap_)
            rasterize();
        // 在位图的指定位置绘制图像
        return GdipDrawImageI(g, bitmap_, x, y);
    }

    // 移动构造函数
    danmakuItem::danmakuItem(danmakuItem &&x) noexcept
    {
        // 防止自移动
        if (this == &x)
            return;

        // 逐成员转移数据
        fillColor_ = x.fillColor_;
        borderColor_ = x.borderColor_;
        text_ = std::move(x.text_);
        bitmap_ = x.bitmap_;
        width_ = x.width_;
        height_ = x.height_;
        emSize_ = x.emSize_;

        // 源对象不再拥有位图所有权，置空防止其析构时释放
        x.bitmap_ = nullptr;
    }

    // 移动赋值操作符
    danmakuItem &danmakuItem::operator=(danmakuItem &&x) noexcept
    {
        // 自赋值检查
        if (this == &x)
            return *this;

        // 释放当前持有的位图资源
        if (bitmap_)
        {
            GdipDisposeImage(bitmap_);
            bitmap_ = nullptr;
        }

        // 转移所有成员
        fillColor_ = x.fillColor_;
        borderColor_ = x.borderColor_;
        text_ = std::move(x.text_);
        bitmap_ = x.bitmap_;
        width_ = x.width_;
        height_ = x.height_;
        emSize_ = x.emSize_;

        // 源对象置空，避免双重释放
        x.bitmap_ = nullptr;

        return *this;
    }

    // 析构函数
    danmakuItem::~danmakuItem()
    {
        if (bitmap_)
            GdipDisposeImage(bitmap_);
    }

    // 使当前缓存的位图失效，下次绘制时会重新光栅化
    void danmakuItem::invalidateCache()
    {
        if (bitmap_)
        {
            GdipDisposeImage(bitmap_);
            bitmap_ = nullptr;
        }
    }
}