//
// Created by max on 07.03.2021.
//

#ifndef FAMILIAR_MAIN_WINDOW_H
#define FAMILIAR_MAIN_WINDOW_H


#include <gtkmm/application.h>
#include <gtkmm/applicationwindow.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/box.h>
#include <gtkmm/builder.h>

#include <cairomm/context.h>
#include <gdkmm/general.h>

#include "ImagesArea.h"
#include "CanvasArea.h"

class MainWindow : public Gtk::ApplicationWindow {
    Gtk::Box *cont;
    Glib::RefPtr<Gtk::Label> display_label;
    Glib::RefPtr<Gtk::Button> display_btn;
    Glib::RefPtr<Gtk::Builder> ui;
    CanvasArea _area;
public:
    MainWindow()
            : ui{Gtk::Builder::create_from_file("../../src/ui/mainwindow.glade")} {
        if(ui) {
            //ui->get_widget<Gtk::Box>("cont", cont);

//            display_label = Glib::RefPtr<Gtk::Label>::cast_dynamic(
//                    ui->get_object("display_label")
//            );
//
//            display_btn = Glib::RefPtr<Gtk::Button>::cast_dynamic(
//                    ui->get_object("display_button")
//            );
            add(_area);
            //if(cont /*&& display_label && display_btn*/) {
//                cont->add(_area);
//                display_btn->signal_clicked().connect(
//                        [this]() {
//                            display_label->set_text("Hello World");
//                        });
//                add(*cont);
  //          }
        }
        set_title("Simple Gtk::Builder demo");
        set_default_size(800, 800);
        show_all();
    }
};

#endif //FAMILIAR_MAIN_WINDOW_H
