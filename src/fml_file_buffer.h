#ifndef FML_FILE_BUFFER_H
#define FML_FILE_BUFFER_H

/**
 *  @file   fml_file_buffer.h
 *  \~russian @brief  Модуль для работы с fml-файлом
 *  \~russian @author angeleyes (mpano91@gmail.com)
 *
 *  \~english @brief  Module for working with fml-file
 *  \~english @author angeleyes (mpano91@gmail.com)
 */

#include <QDebug>
#include <QImage>
#include <string>
#include <QByteArray>
#include <QFile>
#include <QMessageBox>

/**
 * \~russian @brief The fml_file_buffer класс
 *
 * \~english @brief The fml_file_buffer class
 */
class fml_file_buffer
{
public:

    template<typename T>
    static QByteArray create_payload(T* obj) {
        return obj->fml_payload();
    }

    static void save_to_file(QString filename, QByteArray& payload) {
        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qDebug()<<"Unnable to save file";
            return;
        }

        file.write(payload, payload.size());

        file.close();
    }

    template<typename T>
    static void open_file(T* obj, QString filename) {
        QFile file(filename);
        if (!file.open(QFile::ReadOnly)) {
            return;
        }

        QDataStream stream (&file);

        qint16 count = 0;
        stream >> count;
        for (int i = 0; i < count; ++i) {
            QPointF scenePos;
            stream >>scenePos;
            qint32 h,w;
            stream >> h >> w;
            QRectF br;
            stream >> br;
            quint16 format;
            stream >> format;
            quint64 size;
            stream >> size;
            QByteArray zip_img_payload = file.read(size);
            QByteArray img_payload = qUncompress(zip_img_payload);
            QRect br_ = QRect(scenePos.x(), scenePos.y(), w, h);
            obj->addImage(img_payload, w, h, br_, w*4, QImage::Format(format));
        }

        file.close();
    }
};

#endif // FML_FILE_BUFFER_H
