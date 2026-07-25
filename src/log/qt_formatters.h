#pragma once

// fmtquill::formatter specializations for the Qt value types that show up
// constantly in familiar's geometry/UI code, plus quill::Codec bindings so
// they can be passed straight to FLOG_*. All are cheap-to-copy value types,
// so formatting is deferred to the backend thread via DeferredFormatCodec
// instead of formatting eagerly on the calling (GUI) thread.

#include "quill/DeferredFormatCodec.h"
#include "quill/bundled/fmt/format.h"

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QTransform>
#include <QUrl>

template<>
struct fmtquill::formatter<QString>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const QString& value, format_context& ctx) const
    {
        return fmtquill::format_to(ctx.out(), "{}", value.toStdString());
    }
};

template<>
struct quill::Codec<QString> : quill::DeferredFormatCodec<QString>
{
};

template<>
struct fmtquill::formatter<QPointF>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const QPointF& value, format_context& ctx) const
    {
        return fmtquill::format_to(ctx.out(), "({}, {})", value.x(), value.y());
    }
};

template<>
struct quill::Codec<QPointF> : quill::DeferredFormatCodec<QPointF>
{
};

template<>
struct fmtquill::formatter<QRectF>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const QRectF& value, format_context& ctx) const
    {
        return fmtquill::format_to(ctx.out(),
                                    "({}, {}, {}x{})",
                                    value.x(),
                                    value.y(),
                                    value.width(),
                                    value.height());
    }
};

template<>
struct quill::Codec<QRectF> : quill::DeferredFormatCodec<QRectF>
{
};

template<>
struct fmtquill::formatter<QSizeF>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const QSizeF& value, format_context& ctx) const
    {
        return fmtquill::format_to(ctx.out(), "{}x{}", value.width(), value.height());
    }
};

template<>
struct quill::Codec<QSizeF> : quill::DeferredFormatCodec<QSizeF>
{
};

template<>
struct fmtquill::formatter<QColor>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const QColor& value, format_context& ctx) const
    {
        return fmtquill::format_to(ctx.out(), "{}", value.name(QColor::HexArgb).toStdString());
    }
};

template<>
struct quill::Codec<QColor> : quill::DeferredFormatCodec<QColor>
{
};

template<>
struct fmtquill::formatter<QUrl>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const QUrl& value, format_context& ctx) const
    {
        return fmtquill::format_to(ctx.out(), "{}", value.toString().toStdString());
    }
};

template<>
struct quill::Codec<QUrl> : quill::DeferredFormatCodec<QUrl>
{
};

template<>
struct fmtquill::formatter<QTransform>
{
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

    auto format(const QTransform& value, format_context& ctx) const
    {
        return fmtquill::format_to(ctx.out(),
                                    "[{} {} {}; {} {} {}; {} {} {}]",
                                    value.m11(),
                                    value.m12(),
                                    value.m13(),
                                    value.m21(),
                                    value.m22(),
                                    value.m23(),
                                    value.m31(),
                                    value.m32(),
                                    value.m33());
    }
};

template<>
struct quill::Codec<QTransform> : quill::DeferredFormatCodec<QTransform>
{
};
