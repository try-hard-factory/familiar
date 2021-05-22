#ifndef FML_FILE_BUFFER_H
#define FML_FILE_BUFFER_H
#include <QDebug>
#include <QImage>
#include <string>
#include <QByteArray>
#include <QFile>
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
        qDebug()<<header.size();
        QFile file(filename);
        if (!file.open(QFile::WriteOnly)) {
           //      ...
            return;
        }

        uint32_t hs = header.size();
        file.write((const char*)&hs, sizeof (hs));
        file.write(header);
        file.write(payload);
        qDebug()<<payload;
        file.close();

//        {
//            QFile file(filename);
//            if (!file.open(QFile::ReadOnly)) {
//                return;
//            }

//            uint32_t hs_ = 0;
//            file.read((char *)&hs_, sizeof(hs_));

//            if (hs_ != hs) exit(1);

//            QByteArray header_b = file.read(hs);
//            if (header != header_b) exit(1);

//            QByteArray payload_ = file.readAll();
//            if (payload != payload_) exit(1);
//            file.close();
//        }
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

        qDebug()<<header;
        QStringList list = header.split(';');
        for (auto& it : list) {
            if (it.isEmpty()) break;
            qDebug()<<it;
            QStringList img_info = it.split(',');
            double x = img_info[0].toDouble();
            double y = img_info[1].toDouble();
            double h = img_info[2].toDouble();
            double w = img_info[3].toDouble();
            size_t sizepix = img_info[4].toUInt();
            QImage::Format format = (QImage::Format)img_info[5].toUInt();
            qDebug()<<x;
            qDebug()<<y;
            qDebug()<<w;
            qDebug()<<h;
            qDebug()<<sizepix;
            qDebug()<<format;
            QByteArray img_payload = file.read(sizepix);
            qDebug()<<img_payload;
            QImage img((uchar*)img_payload.data(), w, h, format);
            obj->addImage(img, {x, y});
        }
        file.close();
    }
};

#endif // FML_FILE_BUFFER_H
