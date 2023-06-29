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
      "working_colorspace": "linear",

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