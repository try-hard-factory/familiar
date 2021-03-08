//
// Created by max on 07.03.2021.
//

#include "ImagesArea.h"

#include <cairomm/context.h>
#include <giomm/resource.h>
#include <gdkmm/general.h> // set_source_pixbuf()
#include <glibmm/fileutils.h>
#include <iostream>

ImagesArea::ImagesArea()
{
    try
    {
        m_image = Gdk::Pixbuf::create_from_file("../../src/ui/kot1.png");
        std::cout<< "Height:" << m_image->get_height() << '\n';
        std::cout<< "Width:" << m_image->get_width() << '\n';
    }
    catch(const Gio::ResourceError& ex)
    {
        std::cerr << "ResourceError: " << ex.what() << std::endl;
    }
    catch(const Gdk::PixbufError& ex)
    {
        std::cerr << "PixbufError: " << ex.what() << std::endl;
    }

    // Show at least a quarter of the image.
    if (m_image) {
        set_size_request(m_image->get_width(), m_image->get_height());
    }
}

ImagesArea::~ImagesArea()
{
}

bool ImagesArea::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
    if (!m_image)
        return false;

    Gtk::Allocation allocation = get_allocation();
    const int width = allocation.get_width();
    const int height = allocation.get_height();

    // Draw the image in the middle of the drawing area, or (if the image is
    // larger than the drawing area) draw the middle part of the image.
    Gdk::Cairo::set_source_pixbuf(cr, m_image,
                                  (width - m_image->get_width())/2, (height - m_image->get_height())/2);
    cr->paint();

    return true;
}