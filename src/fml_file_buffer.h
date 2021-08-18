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
    static std::string create_header(T* obj) {
        return obj->fml_header();
    }

    template<typename T>
    static QByteArray create_payload(T* obj) {
        return obj->fml_payload();
    }

    static void save_to_file(QString filename, QByteArray& header, QByteArray& payload) {
        QFile file(filename);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            qDebug()<<"Unnable to save file";
            return;
        }

        uint32_t hs = header.size();
        file.write((const char*)&hs, sizeof (hs));
        file.write(header);
        file.write(payload, payload.size());

        file.close();
    }

    template<typename T>
    static void open_file(T* obj, QString filename) {
        QFile file(filename);
        if (!file.open(QFile::ReadOnly)) {
            return;
        }

        uint32_t hs = 0;
        file.read((char *)&hs, sizeof(hs));

        QByteArray header_b = file.read(hs);
        QString header(header_b);

        QStringList list = header.split(';');
        for (auto& it : list) {
            if (it.isEmpty()) break;
//            qDebug()<<it;
            QStringList img_info = it.split(',');
            double x = img_info[0].toDouble();
            double y = img_info[1].toDouble();
            double h = img_info[2].toDouble();
            double w = img_info[3].toDouble();
            double bh = img_info[4].toDouble();
            double bw = img_info[5].toDouble();
            size_t sizepix = img_info[6].toUInt();
            QImage::Format format = (QImage::Format)img_info[7].toUInt();
//            qDebug()<<x;
//            qDebug()<<y;
//            qDebug()<<w;
//            qDebug()<<h;
//            qDebug()<<sizepix;
//            qDebug()<<format;
            QByteArray img_payload = file.read(sizepix);
            QRect br = QRect(x, y, bw, bh);
            obj->addImage(img_payload, w, h, br, w*4, format);
        }

        file.close();
    }
};

#endif // FML_FILE_BUFFER_H
