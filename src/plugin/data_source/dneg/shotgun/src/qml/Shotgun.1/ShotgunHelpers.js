
.pragma library

function handle_response(result_string, title, only_on_error=true, body = "", dialog=null) {
    var result = false

    if(result_string.length) {
        try {
            var data = JSON.parse(result_string)

            // got json.. these are massively inconsistent..
            let has_error = false
            // if("status" in data && status !== null) {
            //     throw "Bad status."
            // }

            if(! "data" in data) {
                throw "Missing data."
            }

            if("error" in data) {
                throw "ShotGrid error."
            }

            if("errors" in data) {
                if(data["errors"][0]["status"] !== null)
                    throw "ShotGrid error."
            }

            // if("status" in data["data"] && data["data"]["status"] !== null && data["data"]["status"] !== "success" ) {
            //     throw "Bad status."
            // }

            result = true

            if(!only_on_error && dialog !== null) {
                dialog.title = title
                if(body) {
                    dialog.text = body + " completed."
                } else {
                    dialog.text = data["data"]["status"]
                }
                dialog.open()
            }
        } catch(err) {
            console.log(err, result_string)
            if(title && dialog !== null) {
                dialog.title = title
                dialog.text = err + "\n" + body + "\n" + result_string
                dialog.open()
            }
            result = false
        }
    }
    return result
}
