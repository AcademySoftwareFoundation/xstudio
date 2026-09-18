add_src_and_test(video_renderer)

if (${BMD_DECKLINK_PLUGIN})
    add_src_and_test(bmd_decklink)
endif()
