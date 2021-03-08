//
// Created by max on 07.03.2021.
//

#ifndef FAMILIAR_RADIOBUTTONS_H
#define FAMILIAR_RADIOBUTTONS_H

#include <gtkmm/box.h>
#include <gtkmm/window.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/separator.h>

class RadioButtons : public Gtk::Window
{
public:
    RadioButtons();
    virtual ~RadioButtons();

protected:
    //Signal handlers:
    void on_button_clicked();

    //Child widgets:
    Gtk::Box m_Box_Top, m_Box1, m_Box2;
    Gtk::RadioButton m_RadioButton1, m_RadioButton2, m_RadioButton3;
    Gtk::Separator m_Separator;
    Gtk::Button m_Button_Close;
};


#endif //FAMILIAR_RADIOBUTTONS_H
