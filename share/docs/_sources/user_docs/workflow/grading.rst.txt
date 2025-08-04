.. _grading:

Grading Tool
============

.. figure:: ../images/grading-01.png
   :alt: xSTUDIO Grading tool in action
   :figclass: align-center
   :align: center
   :scale: 50 %

   The grading tool in action


xSTUDIO provides a grading tool allowing for the creation and manipulation of **SOP** grades that can be applied to a media item or a single frame or some frame range within the media item's ovwerall duration. Mask shapes can be created very easily to mask in or out an area of the image. In conjunction with xSTUDIO's notes system, this allows you to make creative grading notes with lots of control to record and share ideas. Multiple grades can also be 'stacked' on a single media item if so desired.

Creating a Grade
----------------

With the media frame of interest on the screen and the Grading tool open (this can either be as a floating window or within a panel in an xSTUDIO :ref:`interface layout <interface_layouts>`) you can either hit the **+** button to create a new grade layour or just start moving the colour wheel handles in the grading interface.

Editing a Grade
---------------

When you want to change an existing grade, click on the grade layer item in the list that is on the left hand side of the Grading panel.
* Delete the selected grade with the trashcan button
* Set the grade to be a single frame or the full duration of the media clip with the corresponding button

Polygon Grade Masks
-------------------

* Add a mask shape by hitting the 'Polygon' button
* When adding a polygon, click into the viewport to place verts for the polygon border. Clicking on the initial point will close the shape and make the polygon.
* The polygon can be edited by dragging its points. 
* Double Click on the polygon boundary to add a new point
* SHIFT+Click when the mouse is hopvered over a polygon point will delete it.

Ellipse Grade Masks
-------------------

* Add an oval mask shape with the 'Ellipse' button
* Handles on the viewport overlay

.. note::
    Use the softness control (drag left/right) to control the fall-off at the edge of ellipse and polygon masks. The mask effect can be *inverted* with the corresponding button.

Grading Walkthrough
-------------------

This video demostrates grading regions of a plate with polygons and an ellipse and adding a grade to the whole image:

.. raw:: html
    
   <center><video src="../../_static/grading-01.webm" width="720" height="366" controls></video></center>

