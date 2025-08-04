.. _media_items:

xSTUDIO Media Item Structure
----------------------------

In VFX and Animation pipelines it is common to have multiple encodings of the same render, scan, playblast etc on disk. For example, a CG render may live on the filesystem as a set of high resolution, high bit-depth EXR images that were output by a renderer or compositing application. There may also be post-render generated lighter-weight compressed encodings of these EXR images as a quicktime movie that will stream much more easily into a player like xSTUDIO. Or maybe there are identical EXR sequences that are half or quarter resolution (usually called *proxies*). Sometimes there will be *many* parallel encodings of the same output in order to support specific client deliverables and internal pipeline requirements. It entirely depends on the studio's proprietary pipeline.

When reviewing images, xSTUDIO users may wish to see one or another of these encodings depending on the review activity (animation dailies might only neewd a lightweight encoding, lighting dailies might want *all the pixels*). 

xSTUDIO's core data model was designed to reflect this common practice at VFX and Animation studios ands as such a *Media Item* in xSTUDIO (meaning the object that appears in a playlist as an individual, discreet item) can be composed of one **or more** *Media Sources* internally. xSTUDIO's interface and also API provides features for quickly switching which type of Media Source is active for the Media Item (such as thew 'Source' button in the Viewport toolbar) 

For xSTUDIO users to be able to take advantage of this feature, xSTUDIO must be extended with some pipeline integration plugins that know how to find the various encodings of a given output version and build the Media Item accordingly when the user wants to load it. It is possible to build and add Media with multiple Media Sources via the Python or C++ APIs.

xSTUDIO users working outside of an established pipeline and associated infrasturcture, this feature of xSTUDIO can be ignored as Media items wrapping a single Media Source is handled seamlessly.

