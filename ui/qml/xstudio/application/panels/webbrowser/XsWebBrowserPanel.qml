// SPDX-License-Identifier: Apache-2.0
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtWebEngine
import QtWebChannel

import xStudio 1.0

// Embedded browser panel. Only registered as a view when xSTUDIO is built
// with BUILD_WEBENGINE (see webEngineAvailable in qml_setup.cpp). Pages loaded
// here can reach the native xstudioWebBridge object via QWebChannel.
Item {

    id: panel
    anchors.fill: parent

    // Home page comes from preferences. The shipped default is the bundled
    // channel test page, so a fresh panel proves the WebChannel round trip
    // without any external URL.
    XsPreference {
        id: homeUrlPref
        path: "/ui/qml/web_browser_home_url"
    }
    readonly property string testPageUrl: "qrc:/application/panels/webbrowser/webchannel_test.html"
    property string defaultUrl: homeUrlPref.value ? homeUrlPref.value : testPageUrl

    // The page the panel is on, kept in the layout's user_data so the panel
    // comes back to it on the next run. XsStoredPanelProperties restores it
    // before Component.onCompleted, so the first load is already the right
    // one; with nothing stored it stays empty and the home page loads.
    property string currentUrl: ""
    XsStoredPanelProperties {
        propertyNames: ["currentUrl"]
    }

    // Load once here. Binding webView.url instead reloads the page every
    // time the preference model re-evaluates.
    Component.onCompleted: webView.url = currentUrl ? currentUrl : defaultUrl

    property int btnHeight: XsStyleSheet.widgetStdHeight + 4
    property int padding: XsStyleSheet.panelPadding

    XsGradientRectangle {
        anchors.fill: parent
    }

    // xstudioWebBridge is a C++ context property, so it can't carry the
    // WebChannel.id attached property that registeredObjects needs. Register
    // it explicitly under the name pages will look up in channel.objects.
    WebChannel {
        id: channel
        Component.onCompleted: channel.registerObject("xstudioWebBridge", xstudioWebBridge)
    }

    ColumnLayout {

        anchors.fill: parent
        anchors.margins: padding
        spacing: padding

        RowLayout {

            Layout.fillWidth: true
            Layout.preferredHeight: btnHeight
            spacing: 4

            XsSimpleButton {
                text: "<"
                Layout.preferredHeight: btnHeight
                enabled: webView.canGoBack
                opacity: enabled ? 1.0 : 0.4
                onClicked: webView.goBack()
            }

            XsSimpleButton {
                text: ">"
                Layout.preferredHeight: btnHeight
                enabled: webView.canGoForward
                opacity: enabled ? 1.0 : 0.4
                onClicked: webView.goForward()
            }

            XsSimpleButton {
                text: webView.loading ? "Stop" : "Reload"
                Layout.preferredHeight: btnHeight
                onClicked: webView.loading ? webView.stop() : webView.reload()
            }

            XsTextField {
                id: urlField
                Layout.fillWidth: true
                Layout.preferredHeight: btnHeight
                horizontalAlignment: TextInput.AlignLeft
                leftPadding: 6
                rightPadding: 6
                bgColor: XsStyleSheet.widgetBgNormalColor
                text: webView.url
                onAccepted: webView.url = text.trim()
            }

            XsSimpleButton {
                text: "Home"
                Layout.preferredHeight: btnHeight
                onClicked: webView.url = defaultUrl
            }
        }

        WebEngineView {
            id: webView
            Layout.fillWidth: true
            Layout.fillHeight: true
            webChannel: channel
            onUrlChanged: currentUrl = url.toString()
            // Inject Qt's channel client into every page so a web tool needs no
            // library of its own: it just checks for window.QWebChannel. Pages
            // served over http can't load qrc: resources themselves.
            Component.onCompleted: {
                userScripts.collection = [{
                    name: "qwebchannel",
                    sourceUrl: "qrc:///qtwebchannel/qwebchannel.js",
                    injectionPoint: WebEngineScript.DocumentCreation,
                    worldId: WebEngineScript.MainWorld
                }]
            }
            // Surface the page's console in the app log so bridge problems
            // are visible without devtools.
            onJavaScriptConsoleMessage: (level, message, lineNumber, sourceID) => {
                console.log("[web] " + sourceID + ":" + lineNumber + ": " + message)
            }
        }

        XsText {
            Layout.fillWidth: true
            elide: Text.ElideRight
            color: XsStyleSheet.hintColor
            text: webView.loading
                ? "Loading… " + Math.round(webView.loadProgress) + "%"
                : webView.title
        }
    }
}
