print("==== START ====")
def dump_client_names(items=XSTUDIO.api.session.selected_media):
    for item in items:
        try:
            print(item.metadata["metadata"]["shotgun"]["version"]["attributes"]["sg_client_filename"])
        except:
            pass

dump_client_names()
print("==== END ====")
