
.. _release_notes:

Release Notes
=============


======
v1.2.0
======

Version 1.2.0 introduces the ShotBrowser - a fast, powerful and easy to use interface to find all published media and other production data in shotgun and ivy. 
Numerous other new features and improvements are also included.

**Major New feature - Shot Browser**

  - New interface for browsing and loading Playlists, Shots, Reference and reviewing published Notes. 
  - Hit the “S” hotkey or use the 'cloud' botton visible to top left of viewer to reveal.
  - Quickly build your own custom queries for finding renders, scans and other outputs. 
  - Share your queries with colleagues with simple cut/paste action.
  - Choose your preferred 'leaf' to load (dneg movie, review movie, EXRs etc.)
  - Includes “Transfer Media” feature to initiate site transfers with a single click.
  - “Compare With >” and “Replace With >” features allow you to switch media in the session. (Right click on Media items to see this menu)
  - “Live Versions…” displays a list of all other versions for whichever shot you have selected in xStudio. (Right click on Media items to see this menu)
  - “Live Notes…” displays all notes for whichever shot you have selected in xStudio. (Right click on Media items to see this menu)

**Other New features**

  - Set Default Compare Mode as a preference (set to “A/B” for example).
  - New 'HUD' overlay that shows EXR data window and image boundary.
  - Share selected media with colleagues with a clickable link or paste-able shell command. (Right click on Media via 'Copy' sub-menu)
  - New “File > Add Media From Clipboard” lets you paste paths directly to a playlist.
  - Masking improvements - set custom mask ratio and 'M' hotkey toggles mask on/off.
  - Optimised media loading - load very large playlists quickly without slowing down the app.

**Major Bug Fixes**

  - Multi-channel EXRs including DI mattes now display correctly.

======
v1.1.0
======

Version 1.1.0 includes some small feature improvements and several bug fixes as follows.

**New features**

  - Bilinear pixel mode enabled to reduce aliasing with an option in preferences to select the filter mode.
  - Support for 32 bit EXR added, allowing more AOVs to be viewed correctly.
  - Auto align frame numbers of multiple sources in A/B compare mode (with new 'auto align' toggle in media info bar)
  - Preference option to allow cycling through media selection to 'wrap around' via Up/Down arrow hotkeys.

**Bug fixes**

  - Now correctly supports single frame quicktime sources.
  - Respects the embedded timecode in quicktimes and correctly converts to a frame number.
  - Alpha channel display fixed.
  - Resolved stuttering playback on virtual GPU workstations affecting some Montreal based users
  - Resolved issue where selecting an unplayable 'Source' would lock the Media source.
  - Intermittent 'unexpected message' error on save and shotgun publish playlist issue resolved.
  - Refined audio volume / mute button action
  - Luminance view mode hotkey fixed

======
v1.0.0
======

Overview
--------

Version 1.0 of xStudio should be used instead of our legacy 'flip' and 'slip' viewers. Note that xStudio already provides a 
much richer and fully featured interface compared to those apps, as you will discover as you use it. This version, however, 
is not intended to replace the review workflows currently provided by 'dnReview' and you should continue to use dnReview 
for those scenarios until later releases of xStudio.

**Playlists**

  - Create any number of playlists
  - Drag and drop to reorder and organise media
  - Load Shotgun playlists from within xStudio (using the Load layout)
  - Create and update Shotgun playlists from xStudio

**Media**

  - Display virtually any image format (EXR, TIF, JPG, MOV, etc)
  - Drag and drop media from the filesystem or Ivy Browser
  - Audio playback

**Notes**

  - Add notes and annotations to media
  - Sketch, Shapes and Laser modes
  - Adjustable colour, opacity and size
  - Publish notes and annotations directly to Shotgun

**The Viewer**
  
  - Colour accurate (OCIO v2 colour management)
  - Adjust exposure and playback rate
  - Zoom/pan image, RGBA channel display
  - A/B and String "Compare Modes"
  - Adjustable masking and guide-lines
  - Quickly switch media source (e.g. EXR, high quality pro-res quicktime, movie)
|
What can I expect from future releases?
***************************************
Many more exciting features will be released over the next year, including:

- Better Shotgun and Ivy integration (version switching, etc)
- Multi-track NLE timeline with auto-conform
- Session-syncing for collaborative review sessions
- Colour-correction and image-transform tools (rotate/move/scale etc)
- And much more!