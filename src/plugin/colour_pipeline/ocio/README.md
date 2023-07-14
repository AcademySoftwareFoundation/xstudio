# XStudio OCIO Plugin

This is the documentation for xStudio builtin OpenColorIO plugin.

## Metadata format

    {

      // General settings
      //
      // Path to the media, used for input space detection rules
      "path": "/path/to/file",
      // Specify OCIO config to use
      // Use an absolute path or special keys:
      // __curent__ will use OCIO::GetCurrentConfig()
      // __raw__ will use OCIO::Config::Create()
      "ocio_config": "__current__",
      // Dict containing OCIO context key / value
      "ocio_context": {
        "SHOT" : "myshot",
        "SHOW" : "myshow",
        "MYVAR": "myvalue",
      },
      // Input detection, use either colorspace or display/view pair
      // input_colorspace supports colon separated values, the first
      // colorspace, role or alias found in the config will be used.
      "input_colorspace": "scene_linear:linear",
      "input_display": "Rec709",
      "input_view": "Film",
      // View that should be used when the preferred view mode is set
      // to automatic, this allows per-media view assignment.
      "automatic_view": "Film",
      // Custom active displays and views, comma or colon separated
      // list. Note that the OCIO_ACTIVE_DISPLAYS and _VIEWS will
      // always take precedence as per OCIO implementation.
      "active_displays": "sRGB:Rec709",
      "active_views": "Film",

      // DNEG specifics
      //
      // Identify colour workflow updates
      "pipeline_version": "1",
      // Use custom rules to assign OCIO display from monitor name
      "viewing_rules": true,
      // Turn CDL looks into dynamic transforms
      // Format is look_name: backing_file_name
      "dynamic_cdl": {
        "primary": "GRD_primary",
        "neutral": "GRD_neutral",
      }
    }