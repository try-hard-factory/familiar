#include "menu_structure.h"

const QList<MenuNode>& menuStructure()
{
    static const QList<MenuNode> s = {
        MenuNode::submenu("&File", {
            MenuNode::action("new_scene"),
            MenuNode::action("open"),
            MenuNode::submenu("Open &Recent", {
                MenuNode::dynamic("_build_recent_files"),
            }),
            MenuNode::sep(),
            MenuNode::action("save"),
            MenuNode::action("save_as"),
            MenuNode::action("export_scene"),
            MenuNode::action("export_images"),
            MenuNode::sep(),
            MenuNode::action("quit"),
        }),
        MenuNode::submenu("&Edit", {
            MenuNode::action("undo"),
            MenuNode::action("redo"),
            MenuNode::sep(),
            MenuNode::action("select_all"),
            MenuNode::action("deselect_all"),
            MenuNode::sep(),
            MenuNode::action("cut"),
            MenuNode::action("copy"),
            MenuNode::action("paste"),
            MenuNode::action("delete"),
            MenuNode::sep(),
            MenuNode::action("raise_to_top"),
            MenuNode::action("lower_to_bottom"),
        }),
        MenuNode::submenu("&View", {
            MenuNode::action("fit_scene"),
            MenuNode::action("fit_selection"),
            MenuNode::sep(),
            MenuNode::action("fullscreen"),
            MenuNode::action("always_on_top"),
            MenuNode::action("show_scrollbars"),
            MenuNode::action("show_menubar"),
            MenuNode::action("show_titlebar"),
            MenuNode::sep(),
            MenuNode::action("move_window"),
        }),
        MenuNode::submenu("&Insert", {
            MenuNode::action("insert_images"),
            MenuNode::action("insert_text"),
        }),
        MenuNode::submenu("&Transform", {
            MenuNode::action("crop"),
            MenuNode::action("flip_horizontally"),
            MenuNode::action("flip_vertically"),
            MenuNode::sep(),
            MenuNode::action("reset_scale"),
            MenuNode::action("reset_rotation"),
            MenuNode::action("reset_flip"),
            MenuNode::action("reset_crop"),
            MenuNode::action("reset_transforms"),
        }),
        MenuNode::submenu("&Normalize", {
            MenuNode::action("normalize_height"),
            MenuNode::action("normalize_width"),
            MenuNode::action("normalize_size"),
        }),
        MenuNode::submenu("&Arrange", {
            MenuNode::action("arrange_optimal"),
            MenuNode::action("arrange_horizontal"),
            MenuNode::action("arrange_vertical"),
            MenuNode::action("arrange_square"),
        }),
        MenuNode::submenu("&Images", {
            MenuNode::action("change_opacity"),
            MenuNode::action("grayscale"),
            MenuNode::sep(),
            MenuNode::action("show_color_gamut"),
            MenuNode::action("sample_color"),
        }),
        MenuNode::submenu("&Settings", {
            MenuNode::action("settings"),
            MenuNode::action("keyboard_settings"),
            MenuNode::action("open_settings_dir"),
        }),
        MenuNode::submenu("&Help", {
            MenuNode::action("help"),
            MenuNode::action("about"),
            MenuNode::action("debuglog"),
        }),
    };
    return s;
}
