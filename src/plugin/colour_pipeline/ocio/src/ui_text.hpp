// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <string>
#include <vector>
#include "xstudio/ui/keyboard.hpp"

struct UiText {

    std::string RED       = "Red";
    std::string GREEN     = "Green";
    std::string BLUE      = "Blue";
    std::string ALPHA     = "Alpha";
    std::string LUMINANCE = "Luminance";
    std::string RGB       = "RGB";
    std::string R         = "R";
    std::string G         = "G";
    std::string B         = "B";
    std::string A         = "A";
    std::string L         = "L";

    struct ChannelHotkey {
        int key;
        int modifier;
        std::string name;
        std::string description;
        std::string channel_name;
    };

    std::vector<ChannelHotkey> channel_hotkeys = {
        {int('R'),
         xstudio::ui::NoModifier,
         "Toggle Red Channel",
         "Toggles red colour channel only",
         "Red"},
        {int('G'),
         xstudio::ui::NoModifier,
         "Toggle Green Channel",
         "Toggles green colour channel only",
         "Green"},
        {int('B'),
         xstudio::ui::NoModifier,
         "Toggle Blue Channel",
         "Toggles blue colour channel only",
         "Blue"},
        {int('A'),
         xstudio::ui::NoModifier,
         "Toggle Alpha Channel",
         "Toggles alpha colour channel only",
         "Alpha"},
        {int('L'),
         xstudio::ui::ControlModifier,
         "Toggle Luminance",
         "Toggle luminance view mode",
         "Luminance"},
        {int('C'),
         xstudio::ui::NoModifier,
         "Rever to RGB Mode",
         "Returns to regular RGB colour view mode",
         "RGB"}};

    std::string DISPLAY                 = "Display";
    std::string DISPLAY_SHORT           = "Disp";
    std::string VIEW                    = "View";
    std::string EXPOSURE                = "Exposure (E)";
    std::string EXPOSURE_SHORT          = "Exp (E)";
    std::string GAMMA                   = "Gamma";
    std::string GAMMA_SHORT             = "Gam";
    std::string ENABLE_GAMMA            = "Gamma Control";
    std::string ENABLE_GAMMA_SHORT      = "Enbl. Gam.";
    std::string SATURATION              = "Saturation";
    std::string SATURATION_SHORT        = "Sat";
    std::string ENABLE_SATURATION       = "Saturation Control";
    std::string ENABLE_SATURATION_SHORT = "Enbl. Sat.";
    std::string CHANNEL                 = "Channel";
    std::string CHANNEL_SHORT           = "Chan";
    std::string SOURCE_CS               = "Source Colour Space";
    std::string SOURCE_CS_SHORT         = "Source cs";
    std::string CMS_OFF                 = "Bypass Colour Management";
    std::string CMS_OFF_SHORT           = "CMS OFF";
    std::string CMS_OFF_ICON            = "--";
    std::string PREF_VIEW               = "OCIO Preferred View";
    std::string VIEW_MODE               = "Global OCIO View";
    std::string GLOBAL_VIEW_SHORT       = "Global view";
    std::string SOURCE_CS_MODE          = "Auto adjust source";
    std::string SOURCE_CS_MODE_SHORT    = "Adjust source";


    std::string DEFAULT_VIEW                   = "Default";
    std::string AUTOMATIC_VIEW                 = "Automatic";
    std::vector<std::string> PREF_VIEW_OPTIONS = {
        DEFAULT_VIEW,
        AUTOMATIC_VIEW,
        // New config style
        "Client",
        "Client graded",
        "Client neutral",
        "Client alt",
        "Un-tone-mapped",
        "Raw",
        // Old config style
        "Film",
        "Film primary",
        "Film neutral",
        "Film alt",
        "Linear",
        "Gamma22",
        // Common views
        "DNEG",
        "Log"};


    std::string CS_MSG_CMS_SELECT_CLR_TIP =
        "Select colour channel to display. You can also use R,G,B,A,Ctrl+L hotkeys.";
    std::string CS_MSG_CMS_SET_EXP_TIP   = "Set viewer Exposure in f-stops. Double click to "
                                           "toggle between last set value and default of 0.0.";
    std::string CS_MSG_CMS_SET_GAMMA_TIP = "Set viewer Gamma. Double click to "
                                           "toggle between last set value and default of 1.0.";
    std::string CS_MSG_CMS_SET_SATURATION_TIP =
        "Set viewer Saturation. Double click to "
        "toggle between last set value and default of 1.0.";

    std::vector<std::string> MENU_PATH_CHANNEL = {
        "menu_bar|Channel",
        "popout_viewer_context_menu_colour_channel|Channel",
        "viewer_context_menu_colour_channel|Channel"};
    std::vector<std::string> MENU_PATH_DISPLAY = {
        "menu_bar|OCIO Display", "viewer_context_menu_colour_items|OCIO Display"};
    std::vector<std::string> MENU_PATH_POPOUT_VIEWER_DISPLAY = {
        "menu_bar|OCIO Pop-Out Viewer Display",
        "popout_viewer_context_menu_colour_items|OCIO Display (2nd Viewer)"};
    std::vector<std::string> MENU_PATH_VIEW = {
        "menu_bar|OCIO View",
        "popout_viewer_context_menu_colour_items|OCIO View",
        "viewer_context_menu_colour_items|OCIO View"};
    std::vector<std::string> MENU_PATH_SOURCE_CS = {"Colour|Source colour space"};
    std::vector<std::string> MENU_PATH_CMS_OFF   = {"menu_bar"};
    std::vector<std::string> MENU_PATH_PREF_VIEW = {"Colour|OCIO Preferred View"};
    std::vector<std::string> MENU_PATH_VIEW_MODE = {"menu_bar"};

    std::string DISPLAY_TOOLTIP = "Select the colour profile for your display.  See User "
                                  "Documentation > OCIO for more information.";
    std::string VIEW_TOOLTIP = "Select from available grades and looks. See User Documentation "
                               "> OCIO for more information.";
    std::string SOURCE_CS_TOOLTIP = "Select from available colourspaces. See User "
                                    "Documentation > OCIO for more information.";
    std::string CS_BYPASS_TOOLTIP = "Turn off colour management";
    std::string PREF_VIEW_TOOLTIP = "Set preferred view";
    std::string GLOBAL_VIEW_TOOLTIP =
        "Enable global view to affect every loaded media when changing the OCIO view.";
    std::string SOURCE_CS_MODE_TOOLTIP =
        "Automatically use the most appropriate source colour space for the selected view.";

    std::vector<std::string> OCIO_LOAD_ERROR = {"Error could not load OCIO config"};
};
