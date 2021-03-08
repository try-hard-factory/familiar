//#include "radiobuttons.h"
#include "main_window.h"
#include <gtkmm/application.h>

int main(int argc, char *argv[])
{
    auto app = Gtk::Application::create(argc, argv, "org.gtkmm.example");

    //RadioButtons buttons;
    MainWindow mw;
    //Shows the window and returns when it is closed.
    return app->run(mw);
    //return app->run(buttons);
}

