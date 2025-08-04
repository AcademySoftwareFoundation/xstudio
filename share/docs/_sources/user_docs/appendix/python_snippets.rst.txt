.. _python_snippets:

#################
Python 'Snippets'
#################

A Python *snippet* is a self contained Python script that can be executed within xSTUDIO from the Snippets menu or from the Media List menu. The idea is to provide a convenient way to add useful Python scripts to your xSTUDIO environment to automate certain tasks, build playlists or any other interaction with the Session through the Python API

Creating a Snippet
------------------

To create a snippet you simply need to create file with '.py' extension in one of the locations that xSTUDIO scans for snippets. These locations are controlled by the snippets path Preference, which is a list of filesystem paths. By default, one of the paths that xSTUDIO checks is **xStudio/snippets** in your **$HOME** folder. So if you make an **xStudio** folder in your home space (be careful with the letter case) and then a **snippets** folder within, then you can put snippets into that location and xSTUDIO will find them.

So for example, in this screen shot the path **/user_data/ted/snippets** has been added to the snippets path via the Preferences Manager ...

.. figure:: ../images/snippets-01.png
    :alt: Setting the snippets path
    :figclass: align-center
    :align: center

    Setting the snippets search path

Now if I create a file called **my_test_snippet.py** in the default location in my home space here **C:\\Users\\tedwaine\\xStudio\\snippets** and then, under the *Snippets* menu I hit *Reload* then my Snippets menu looks like this:

.. figure:: ../images/snippets-02.png
    :alt: Snippets menu
    :figclass: align-center
    :align: center

Now if I click on 'My Test Snippet' the code in the my_test_snippet.py file is executed in xSTUDIO's python interpreter.

.. note::
    You can also make snippets appear in the Media List right-click Context/More menu. Add another folder name **media** within one of the folders that is scanned for snippets. Any .py files inside the **media** folder will appear in a **Snippets** sub-menu in the Media List menu.

Example Snippet
---------------

You can run any python code you desire from a snippet. Here is a simple example that will randomise the colours of all the *selected* clips within the currently viewed Sequence (if there is a Sequence being viewed in the Viewport)

.. code-block:: python
    :caption: my_test_snippet.py

    from xstudio.api.session.playlist.timeline import Timeline, Clip
    import random
    import colorsys

    def random_clip_colour(item=XSTUDIO.api.session.viewed_container):
        if isinstance(item, Timeline):
            for i in item.selection:
                if isinstance(i, Clip):
                    c = random.randrange(0, 255)

                    rgb = colorsys.hsv_to_rgb(c * (1.0/255.0), 1, 1)
                    i.item_flag = "#FF" + f"{int(rgb[0]*255.0):0{2}x}"+ f"{int(rgb[1]*255.0):0{2}x}"+ f"{int(rgb[2]*255.0):0{2}x}"

    random_clip_colour()

and here's the result!

.. figure:: ../images/snippets-03.png
    :alt: Snippets menu
    :figclass: align-center
    :align: center
    :scale: 60%
