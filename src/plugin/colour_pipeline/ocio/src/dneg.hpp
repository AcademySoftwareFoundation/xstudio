// SPDX-License-Identifier: Apache-2.0
#include <regex>

#include <OpenColorIO/OpenColorIO.h> //NOLINT


/*
This function checks the playback machines yaml file to determine if the playback
machine display is a cinema screen or HDR TV.
*/
std::string get_playback_display() {
    const std::string CONST_PLAYBACK_FILE = "/var/playback/sys-config.yaml";

    std::map<std::string, std::string> CONST_DISPLAY = {
        {"cinema", "DCI-P3"}, {"hdr", "HDR"}, {"playback", "Playback"}};

    std::fstream data_load;
    data_load.open(CONST_PLAYBACK_FILE, std::ios::in);

    if (data_load.is_open()) {
        std::string temp;
        while (std::getline(data_load, temp)) {
            if (temp.find("cinema") != std::string::npos) {
                return CONST_DISPLAY["cinema"];
            } else if (temp.find("hdr") != std::string::npos) {
                return CONST_DISPLAY["hdr"];
            }
        }
        data_load.close();
    }

    return CONST_DISPLAY["playback"];
}


std::string dneg_ocio_default_display(
    const OCIO::ConstConfigRcPtr &ocio_config, const std::string &device) {
    std::string display = "";
    std::map<std::string, std::vector<std::string>> display_views;
    std::vector<std::string> displays;

    std::map<std::string, std::string> CONST_DISPLAY = {
        {"srgb", "sRGB"}, {"eizo", "EIZO"}, {"hdr", "HDR"}, {"playback", "Playback"}};

    // Helper lambda to check for string in displays vector
    auto check_displays = [&displays](std::string &check_string) {
        if (std::find(displays.begin(), displays.end(), check_string) != displays.end()) {
            return true;
        }

        return false;
    };

    // Get the Hostname of the machine
    const char *hostname_env = std::getenv("HOSTNAME");
    std::string hostname;
    std::string hostname_lower;

    if (hostname_env && *hostname_env) {
        hostname       = hostname_env;
        hostname_lower = hostname;
    }
    hostname[0] = std::toupper(hostname[0]);

    std::transform(
        hostname_lower.begin(), hostname_lower.end(), hostname_lower.begin(), ::tolower);

    // Get ocio config and
    const std::string default_display = ocio_config->getDefaultDisplay();

    // Parse display views
    for (int i = 0; i < ocio_config->getNumDisplays(); ++i) {
        const std::string display = ocio_config->getDisplay(i);
        displays.push_back(display);

        display_views[display] = std::vector<std::string>();
        for (int j = 0; j < ocio_config->getNumViews(display.c_str()); ++j) {
            const std::string view = ocio_config->getView(display.c_str(), j);
            display_views[display].push_back(view);
        }
    }

    // Check for India sRGB lock
    const char *site_name_env   = std::getenv("DN_SITE");
    const bool srgb_calibration = (bool)std::getenv("DN_SRGB_CALIBRATION");
    std::string site_name;

    if (site_name_env && *site_name_env) {
        site_name = site_name_env;
    }

    if (((site_name == "mumbai") || (site_name == "chennai")) && srgb_calibration) {
        // Return sRGB
        return CONST_DISPLAY["srgb"];
    }

    // At-desk monitor Logic
    std::string device_upper = xstudio::utility::to_upper(device);

    // EIZO monitors
    if (device_upper.find(CONST_DISPLAY["eizo"]) != std::string::npos) {
        /*
         NOTE: this rule is kept for backward compatibility only, a time
         where each EIZO model had its specific calibration.
         Some OCIO configs have a display per Device model, whereas others
         have one for all EIZO monitors
        */
        if (check_displays(CONST_DISPLAY["eizo"])) {
            display = CONST_DISPLAY["eizo"];
        }
        /*
          Try to match the model exactly something like this is expected
         'Eizo CG247X (DFP-0)', in the OCIO config, this would be EIZO247X
        */
        else {
            std::regex expression("CG([0-9A-Z]+)");
            std::smatch match;
            if (std::regex_search(device_upper, match, expression)) {
                std::string config_name = "EIZO" + match.str(1);
                if (check_displays(config_name)) {
                    display = config_name;
                }
            }
        }

        if (display.empty()) {
            display = default_display;
            spdlog::warn("Could not find OCIO Display for device " + device);
        }
    }
    // DELL monitors
    else if (device_upper.find("DELL") != std::string::npos) {
        if (check_displays(CONST_DISPLAY["srgb"])) {
            display = CONST_DISPLAY["srgb"];
        } else {
            display = default_display;
        }
    }

    // Playback room logic

    // KONA video cards
    else if (device_upper.find("KONA") != std::string::npos) {
        if (check_displays(CONST_DISPLAY["hdr"])) {
            display = CONST_DISPLAY["hdr"];
        } else {
            display = default_display;
            spdlog::warn("Could not set HDR as the display for device " + device);
        }
    }

    /*
    Fallback: Projectors
    New style is to have a 'Playback' display. Fallback to old-style where
    we check whether the hostname is one of the display options.
    Note that for DCI, playback display default view is a straight
    DCI-P3 output (playback characterization tranforms are identity matrix
    and 1D LUT).
    */

    else if (
        hostname_lower.find("playback") != std::string::npos &&
        check_displays(CONST_DISPLAY["playback"])) {
        display = get_playback_display();
    }

    else if (check_displays(hostname)) {
        display = hostname;
    }

    // Fall back to default
    if (display.empty()) {
        display = default_display;
    }

    return display;
}
